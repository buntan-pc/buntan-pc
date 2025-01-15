// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2024 Kota UCHIDA
 */
#include "mmio.h"
#include "delay.h"

char *shift_map = " !\x22#$%&'()*+<=>?" // 0x20: !"#$%&'()*+,-./
                  "0!\x22#$%&'()*+<=>?" // 0x30:0123456789:;<=>?
                  "`ABCDEFGHIJKLMNO"    // 0x40:@ABCDEFGHIJKLMNO
                  "PQRSTUVWXYZ{|}~_"    // 0x50:PQRSTUVWXYZ[¥]^_
                  "`ABCDEFGHIJKLMNO"    // 0x60:`abcdefghijklmno
                  "PQRSTUVWXYZ{|}~\x7f";// 0x70:pqrstuvwxyz{|}~

int uart_getc() {
  while ((uart3_flag & 0x01) == 0);
  return uart3_data;
}

void uart_putc(char c) {
  while ((uart3_flag & 0x04) == 0);
  uart3_data = c;
}

void uart_puts(char *s) {
  while (*s) {
    uart_putc(*s++);
  }
}

void uart_putsn(char *s, int n) {
  while (n-- > 0) {
    uart_putc(*s++);
  }
}

void uart_cursor_at_col(int col) {
  uart_puts("\x1B["); // CSI
  if (col < 10) {
    uart_putc('1' + col);
  } else {
    uart_putc('1');
    uart_putc('0' + (col - 10));
  }
  uart_putc('G');
}

// カーソル上の文字を消す
void uart_del_char() {
  uart_puts("\x1B[X");
}

void assert_cs() {
  spi_ctl = 0;
}

void deassert_cs() {
  spi_ctl = 2;
}

void wait_spi_ready() {
  while ((spi_ctl & 1) == 0);
}

void send_spi(int v) {
  spi_dat = v;
  wait_spi_ready();
}

int recv_spi_16() {
  int v;
  send_spi(0xff);
  v = spi_dat << 8;
  send_spi(0xff);
  v = spi_dat | v;
  return v;
}

// SD の R1 レスポンスは最上位ビットが 0
//
// 戻り値
// >=0: SD からのレスポンス
// <0:  レスポンス受信がタイムアウト
int wait_sd_response() {
  int i;
  int resp = spi_dat;
  for (i = 0; i < 8; i++) {
    if ((resp & 0x80) == 0) {
      return resp;
    }
    send_spi(0xff);
    resp = spi_dat;
  }
  return -resp;
}

int wait_sd_r1() {
  int r1 = wait_sd_response();
  send_spi(0xff);
  return r1;
}

int wait_sd_r1_32(int *high, int *low) {
  int r1 = wait_sd_response();
  *high = recv_spi_16();
  *low = recv_spi_16();
  send_spi(0xff);
  return r1;
}

// 先頭 0 の数を返す
int int2hex(int val, char *s, int n) {
  int i;
  for (i = 0; i < n; i++) {
    int v = val & 0xf;
    val = val >> 4;
    if (v >= 10) {
      v += 'A' - 10;
    } else {
      v += '0';
    }
    s[n - 1 - i] = v;
  }
  for (i = 0; s[i] == '0'; ++i);
  return i;
}

/* 整数を長さ n の 10 進数文字列へ変換する。先頭の 0 は空白文字で埋められる。
 *
 * @param val 文字列へ変換する整数
 * @param s   文字列バッファ
 * @param n   s の文字数
 * @return 先頭の 0 の数。val=0 の場合は n-1 を返す。
 */
int int2dec(int val, char *s, int n) {
  int i;
  int dec_arr[6];
  int digit;
  dec_arr[0] = 1;
  for (i = 1; i < n; i++) {
    dec_arr[i] = dec_arr[i - 1] * 10;
  }

  for (i = 0; i < n; i++) {
    int dec = dec_arr[n - i - 1];
    int digit = 0;
    while (val >= dec) {
      val -= dec;
      digit++;
    }
    s[i] = digit + '0';
  }
  for (i = 0; i < n - 1 & s[i] == '0'; ++i) {
    s[i] = ' ';
  }
  return i;
}

void send_sd_cmd(char cmd, int arg_high, int arg_low, char crc) {
  send_spi(0x40 + cmd);
  send_spi(arg_high >> 8);
  send_spi(arg_high & 0xff);
  send_spi(arg_low >> 8);
  send_spi(arg_low & 0xff);
  send_spi(crc); // CRC-7 + stop bit
}

void show_sd_cmd_error(char cmd, int r1, char *msg) {
  char buf[3];
  int digit10 = 0;
  while (cmd >= 10) {
    digit10++;
    cmd -= 10;
  }

  uart_puts("CMD");
  if (digit10 > 0) {
    uart_putc('0' + digit10);
  }
  uart_putc('0' + cmd);
  uart_puts(" -> ");

  if (r1 < 0) {
    uart_puts("timeout");
  } else {
    int2hex(r1, buf, 2);
    buf[2] = '\0';
    uart_puts(buf);
  }
  uart_putc('\n');

  uart_puts(msg);
}

// return minus value on error
// bit 0: V2 (0: SDv1, 1: SDv2 or later)
// bit 1: CCS (Card Capacity Status)
int sd_init() {
  int i;
  int r1;
  int hi; // response high 16 bits
  int lo; // response low 16 bits
  int sdv; // 1 => SDSC, 2 => SDHC/SDXC or later

  deassert_cs(); // CS = 1
  for (i = 0; i < 10; i++) {
    send_spi(0xff);
  }

  assert_cs(); // CS = 0
  send_sd_cmd(0, 0, 0, 0x95); // CMD0
  r1 = wait_sd_r1();
  deassert_cs(); // CS = 1

  if (r1 != 0x01) { // 正常に Idle State にならなかった
    show_sd_cmd_error(0, r1, "FAILED");
    return -1;
  }

  assert_cs(); // CS = 0
  send_sd_cmd(8, 0x0000, 0x01aa, 0x87); // CMD8
  // CMD8 は CRC を正しくしないとエラーになる
  r1 = wait_sd_r1_32(&hi, &lo);
  deassert_cs(); // CS = 1
  if (r1 < 0) {
    show_sd_cmd_error(0, r1, "FAILED");
    return -1;
  } else if (r1 & 4) { // illegal command => SDSC か、あるいは SD ではない何か
    sdv = 1;
  } else if ((r1 & 0xfe) == 0) { // SDHC/SDXC or later
    sdv = 2;
  } else {
    show_sd_cmd_error(0, r1, "FAILED");
    return -1;
  }

  if (sdv >= 2) {
    if (r1 != 0x01) { // 正常に Idle State にならなかった
      show_sd_cmd_error(8, r1, "FAILED");
      return -1;
    }
    if (lo != 0x01aa) {
      show_sd_cmd_error(8, r1, "FAILED (01AA)");
      return -1;
    }
  }

  for (i = 0; i < 200; i++) {
    assert_cs(); // CS = 0
    send_sd_cmd(55, 0, 0, 0); // CMD55
    r1 = wait_sd_r1();
    deassert_cs(); // CS = 1

    if (r1 != 1) {
      show_sd_cmd_error(55, r1, "FAILED");
      return -1;
    }

    assert_cs(); // CS = 0
    hi = (sdv >= 2) << 14; // HCS=1 if sd version >= 2
    send_sd_cmd(41, hi, 0, 0);
    r1 = wait_sd_r1();
    deassert_cs(); // CS = 1
    if (r1 == 0x01) {        // In Idle State
      continue;
    } else if (r1 == 0x00) { // Idle State を抜けた
      break;
    } else {                 // その他の状態
      show_sd_cmd_error(41, r1, "FAILED");
      return -1;
    }

    delay_ms(10);
  }

  if (r1 != 0) {
    return -1;
  }

  assert_cs();
  send_sd_cmd(58, 0, 0, 0); // Read OCR
  r1 = wait_sd_r1_32(&hi, &lo);
  deassert_cs();

  int res;
  res = 0;
  if (sdv >= 2) {
    res = res | 0x0001;
  }
  if ((hi & 0xc000) == 0xc000) {
    res = res | 0x0002;
  }
  return res;
}

int sd_get_read_bl_len(int *csd) {
  return csd[2] & 0x000f; // [83:80]
}

int sd_get_capacity_mib_csdv1(unsigned int *csd) {
  int c_size;
  int c_size_mult;
  int read_bl_len;
  int shift;

  c_size = (csd[3] & 0x03ff) << 2;        // [73:64]
  c_size = c_size | (csd[4] >> 14); // [63:62]
  c_size_mult = (csd[4] & 3) << 1;                  // [49:48]
  c_size_mult = c_size_mult | (csd[5] >> 15); // [47]
  read_bl_len = sd_get_read_bl_len(csd);

  // memory capacity = (C_SIZE + 1) * 2^(C_SIZE_MULT + 2 + READ_BL_LEN)
  shift =
    20                  // 1MiB == 2^20
    - read_bl_len       // ブロック数（の指数）
    - (c_size_mult + 2);// 計数（の指数）
  // read_bl_len == 10
  // c_size_mult == 7
  // shift == 1
  // c_size == 0x0f02 == 3842
  //
  // BLOCK_LEN = 1024
  // MULT = 512
  // BLOCKNR = 3843 * MULT = 1921.5 KiB
  // cap = BLOCKNR * BLOCK_LEN = 1921.5 MiB

  if (shift >= 0) {
    return (c_size + 1) >> shift;
  } else {
    return (c_size + 1) << -shift;
  }
}

int sd_get_capacity_mib_csdv2(unsigned int *csd) {
  unsigned int c_size_h;
  unsigned int c_size_l;
  int cap_mib;

  // C_SIZE: [69:48]
  // csd[0] = [127:112]
  // csd[3] = [79:64]
  // csd[4] = [63:48]
  c_size_h = (csd[3] & 0x003f); // [69:64]
  c_size_l = csd[4];            // [63:48]

  cap_mib = c_size_l >> 1;
  cap_mib = cap_mib | (c_size_h << 15);
  return cap_mib;
}

// CSD レジスタを取得する
int sd_read_csd(unsigned int *csd) {
  int i;
  int r1;

  assert_cs();
  send_sd_cmd(9, 0, 0, 0); // CMD9 (SEND_CSD)
  r1 = wait_sd_r1();
  if (r1 != 0) {
    return -1;
  }

  while (spi_dat != 0xfe) { // Start Block トークンを探す
    send_spi(0xff);
  }
  for (i = 0; i < 9; i++) {
    csd[i] = recv_spi_16();
  }
  deassert_cs();
  return 0;
}

// MiB 単位の容量
int sd_get_capacity_mib(unsigned int *csd) {
  int csdv;

  csdv = csd[0] >> 14;
  if (csdv == 0) { // Ver.1
    return sd_get_capacity_mib_csdv1(csd);
  } else if (csdv >= 1) { // Ver.2 or Ver.3
    return sd_get_capacity_mib_csdv2(csd);
  }
  return -1;
}

int sd_set_block_len_512() {
  int r1;

  assert_cs(); // CS = 0
  send_sd_cmd(16, 0, 512, 0); // CMD16
  r1 = wait_sd_r1();
  deassert_cs(); // CS = 1
  if (r1 != 0) {
    show_sd_cmd_error(16, r1, "FAILED");
    return -1;
  }
  return 0;
}

int sd_ccs;

// buf へ 1 ブロック読み込み
int sd_read_block(unsigned char *buf, int block_addr) {
  int i;
  int r1;

  int addr_hi;
  int addr_lo;
  if (sd_ccs) {
    addr_lo = block_addr;
    addr_hi = 0;
  } else {
    // バイトアドレスに変換
    addr_lo = block_addr << 9;
    addr_hi = block_addr >> (16 - 9);
  }

  assert_cs();
  send_sd_cmd(17, addr_hi, addr_lo, 0); // CMD17 (READ_SINGLE_BLOCK)
  r1 = wait_sd_r1();
  if (r1 != 0) {
    show_sd_cmd_error(17, r1, "FAILED");
    deassert_cs();
    return -1;
  }

  while (spi_dat != 0xfe) { // Start Block トークンを探す
    send_spi(0xff);
  }
  for (i = 0; i < 512; i++) {
    send_spi(0xff);
    buf[i] = spi_dat;
  }
  deassert_cs();
  return 0;
}

// SD へ 1 ブロック書き込み
int sd_write_block(unsigned char *buf, int block_addr) {
  int i;
  int r1;

  int addr_hi;
  int addr_lo;
  if (sd_ccs) {
    addr_lo = block_addr;
    addr_hi = 0;
  } else {
    // バイトアドレスに変換
    addr_lo = block_addr << 9;
    addr_hi = block_addr >> (16 - 9);
  }

  assert_cs();
  send_sd_cmd(24, addr_hi, addr_lo, 0); // CMD24 (WRITE_BLOCK)
  r1 = wait_sd_r1();
  if (r1 != 0) {
    show_sd_cmd_error(24, r1, "FAILED");
    deassert_cs();
    return -1;
  }

  // 1 バイト以上空ける
  send_spi(0xff);

  // Start Block トークン
  send_spi(0xfe);

  // データ本体を送る
  for (i = 0; i < 512; i++) {
    send_spi(buf[i]);
  }

  // ダミーの CRC（2 バイト）
  send_spi(0x00);
  send_spi(0x00);

  // ステータスを受け取る
  send_spi(0xff);
  deassert_cs();

  int data_resp = spi_dat & 0x1f;
  if ((data_resp & 1) == 0) {
    show_sd_cmd_error(24, r1, "RESP INVALID");
    return -1;
  } else if (data_resp == 0x0b) {
    show_sd_cmd_error(24, r1, "CRC ERROR");
    return -1;
  } else if (data_resp == 0x0d) {
    show_sd_cmd_error(24, r1, "WRITE ERROR");
    return -1;
  } else if (data_resp == 0x05) {
    // ビジー状態が解除されるのを待つ
    assert_cs();
    send_spi(0xff);
    while (spi_dat == 0) {
      send_spi(0xff);
    }
    deassert_cs();
    return 0;
  }
  return 0;
}

int has_signature_55AA(unsigned char *block_buf) {
  return block_buf[510] == 0x55 & block_buf[511] == 0xAA;
}

int is_valid_PBR(unsigned char *block_buf) {
  return (block_buf[0] == 0xEB && block_buf[2] == 0x90)
      || (block_buf[0] == 0xE9);
}

unsigned int BPB_BytsPerSec;
unsigned int BPB_SecPerClus;
unsigned int BPB_ResvdSecCnt;
unsigned int BPB_NumFATs;
unsigned int BPB_RootEntCnt;
unsigned int BPB_FATSz16;
unsigned int RootEntSector;
unsigned int FirstDataSector;
unsigned int PartitionSector;

void set_bpb_values(unsigned char *block_buf) {
  BPB_BytsPerSec = block_buf[11] | (block_buf[12] << 8);
  BPB_SecPerClus = block_buf[13];
  BPB_ResvdSecCnt = block_buf[14] | (block_buf[15] << 8);
  BPB_NumFATs = block_buf[16];
  BPB_RootEntCnt = block_buf[17] | (block_buf[18] << 8);
  BPB_FATSz16 = block_buf[22] | (block_buf[23] << 8);
  FirstDataSector =
    PartitionSector
    + BPB_ResvdSecCnt
    + BPB_NumFATs*BPB_FATSz16
    + (BPB_RootEntCnt >> 4);
}

unsigned int clus_to_sec(unsigned int clus) {
  return FirstDataSector + (clus - 2)*BPB_SecPerClus;
}

int load_exe(unsigned int pmem_addr, unsigned int dmem_addr, unsigned int exe_lba) {
  int i;
  char buf[5];
  int *head = dmem_addr;
  char *block_buf = dmem_addr + 512;

  if (sd_read_block(head, exe_lba) < 0) {
    return -1;
  }
  int pmem_len = head[0]; // # of words
  int dmem_len = head[1]; // # of bytes

  int num_dmem_block = (dmem_len + 511) >> 9;
  int pmem_lba = exe_lba + num_dmem_block;

  int byte_index = 512;
  for (i = 0; i < pmem_len; ++i) { // insn[0]=0~2, insn[169]=507~509
    if (byte_index >= 510) { // のこり 3 バイト未満なので、次のブロックを読む
      char insn_buf0 = block_buf[510];
      char insn_buf1 = block_buf[511];
      if (sd_read_block(block_buf, pmem_lba++) < 0) {
        return -1;
      }
      if (byte_index == 512) {
        byte_index = 0;
      } else {
        if (byte_index == 510) {
          __builtin_write_pmem(pmem_addr, block_buf[0], insn_buf0 | (insn_buf1 << 8));
          byte_index = 1;
        } else { // byte_index == 511
          __builtin_write_pmem(pmem_addr, block_buf[1], insn_buf1 | (block_buf[0] << 8));
          byte_index = 2;
        }
        ++pmem_addr;
        //++i;
        continue;
      }
    }
    __builtin_write_pmem(pmem_addr, block_buf[byte_index+2],
                         (block_buf[byte_index+1] << 8) | block_buf[byte_index]);
    ++pmem_addr;
    byte_index += 3;
  }

  for (i = 1; i < num_dmem_block; ++i) {
    if (sd_read_block(block_buf, exe_lba + i) < 0) {
      return -1;
    }
    block_buf += 512;
  }

  return 0;
}

int strncmp(char *a, char *b, int n) {
  int i;
  for (i = 0; i < n; i++) {
    int v = a[i] - b[i];
    if (v != 0 | a[i] == 0) {
      return v;
    }
  }
  return 0;
}

int chartoi(char c) {
  if ('0' <= c & c <= '9') {
    return c - '0';
  }
  if (c >= 'a') {
    c -= 0x20;
  }
  if ('A' <= c & c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

int strtoi(char *s, char **endptr, int base) {
  int v = 0;
  int i;
  while (1) {
    i = chartoi(*s);
    if (i < 0 | i >= base) {
      break;
    }
    ++s;
    v = base*v + i;
  }
  *endptr = s;
  return v;
}

void strcpy(char *dst, char *src) {
  while (*src) {
    *dst++ = *src++;
  }
  *dst = 0;
}

int toupper(int c) {
  if ('a' <= c & c <= 'z') {
    return c - 0x20;
  } else {
    return c;
  }
}

/*
 * ルートディレクトリエントリを走査する。
 *
 * @param block_buf  作業用バッファ（セクタサイズ以上のメモリ領域）
 * @param entry_sec  最後に処理したエントリが存在するセクタ番号
 * @param proc_entry ディレクトリエントリを受け取って処理する関数
 * @param arg        proc_entry の引数に渡す値
 */
int foreach_dir_entry(char *block_buf, unsigned int *entry_sec,
                      int (*proc_entry)(), void *arg) {
  int find_loop;
  for (find_loop = 0; find_loop < BPB_RootEntCnt >> 4; ++find_loop) {
    if (entry_sec) {
      *entry_sec = RootEntSector + find_loop;
    }
    if (sd_read_block(block_buf, RootEntSector + find_loop) < 0) {
      uart_puts("failed to read block\n");
      return 0;
    }
    for (int i = 0; i < 16; ++i) {
      char *dir_entry = block_buf + (i << 5);
      if (*dir_entry == 0x00) { // これ以降、有効なエントリは無い
        return 0;
      }
      int res = proc_entry(dir_entry, arg);
      if (res != 0) {
        return res;
      }
    }
  }
  return 0;
}

void print_filename_trimspace(char *name, char *end) {
  while (*end == ' ') {
    --end;
  }
  for (; name <= end; ++name) {
    if (*name == 0x7e) { // チルダ
      uart_putc(0x01);
    } else {
      uart_putc(*name);
    }
  }
}

unsigned int read16(char *buf) {
  return *buf | (buf[1] << 8);
}

int print_file_name(char *dir_entry, void *dummy) {
  if (*dir_entry == 0xe5 || *dir_entry == 0x00 || (dir_entry[11] & 0x0e) != 0) {
    return 0;
  }
  int is_dir = dir_entry[11] & 0x10;

  print_filename_trimspace(dir_entry, dir_entry + 7);
  if (strncmp(dir_entry + 8, "   ", 3) != 0) { // 拡張子あり
    uart_putc('.');
    print_filename_trimspace(dir_entry + 8, dir_entry + 10);
  }
  if (is_dir) {
    uart_putc('/');
  }

  // ファイルサイズ表示
  uart_cursor_at_col(13);
  unsigned int siz_lo = read16(dir_entry + 28);
  unsigned int siz_hi = read16(dir_entry + 30);
  char buf[6];
  if (siz_hi == 0) {
    int2dec(siz_lo, buf, 5);
    buf[5] = 0;
    uart_puts(buf);
    uart_putc('B');
  } else {
    uart_puts("TOOBIG");
  }
  uart_putc('\n');

  return 0;
}

char *find_file(char *dir_entry, char *fn83) {
  if (*dir_entry == 0xe5 || *dir_entry == 0x00) {
    return 0;
  }

  if (strncmp(fn83, dir_entry, 11) == 0) {
    return dir_entry;
  }
  return 0;
}

void filename_to_fn83(char *filename, char *fn83) {
  int i;
  for (i = 0; i < 11; ++i) {
    fn83[i] = ' ';
  }

  char *p = filename;
  char *q = fn83;
  for (i = 0; i < 8 & *p != 0; ++i) {
    char c = *p++;
    if (c == '.') {
      break;
    } else {
      *q++ = toupper(c);
    }
  }
  if (i == 8) {
    while (*p != 0) {
      if (*p++ == '.') {
        break;
      }
    }
  }
  q = fn83 + 8;
  for (i = 0; i < 3 & *p != 0; ++i) {
    *q++ = toupper(*p++);
  }
}

int load_exe_by_filename(int (*app_main)(), char *block_buf, char *filename) {
  char fn83[11];
  filename_to_fn83(filename, fn83);

  char *file_entry = foreach_dir_entry(block_buf, 0, find_file, fn83);
  if (file_entry == 0 && fn83[8] == ' ') {
    fn83[8]  = 'E';
    fn83[9]  = 'X';
    fn83[10] = 'E';
    file_entry = foreach_dir_entry(block_buf, 0, find_file, fn83);
  }

  if (file_entry == 0) {
    uart_puts("No such file\n");
    return -1;
  } else {
    unsigned int exe_clus = read16(file_entry + 26);
    unsigned int exe_lba = clus_to_sec(exe_clus);
    if (load_exe(app_main, block_buf, exe_lba) < 0) {
      uart_puts("failed to load app\n");
      return -1;
    }

    uart_puts("File loaded\n");
  }
  return 0;
}

int build_argv(char *cmd, char **argv, int n) {
  int argc = 0;

  while (1) {
    while (*cmd == ' ') {
      ++cmd;
    }
    if (*cmd == '\0') {
      return argc;
    }
    *argv++ = cmd;
    ++argc;
    while (*cmd != ' ') {
      if (*cmd == '\0') {
        return argc;
      }
      ++cmd;
    }
    *cmd++ = '\0';
  }
  return argc;
}

void run_app(int (*app_main)(), char *block_buf, int argc, char **argv) {
  char buf[5];
  int appinfo[4] = { // アプリに渡す構成情報
    (int)app_main, // 0: .text の開始アドレス
    (int)syscall,  // 1: システムコールのエントリアドレス
    argc,          // 2: argc
    (int)argv,     // 3: argv
  };

  __builtin_set_gp(block_buf);
  int ret_code = app_main(appinfo);
  __builtin_set_gp(0x100);

  int2hex(ret_code, buf, 4);
  buf[4] = 0;
  uart_puts("\xe2\x86\x92"); // 右矢印
  uart_puts(buf);
  uart_putc('\n');
}

char uart1_recv_byte() {
  while ((uart_flag & 0x01) == 0);
  return uart_data;
}

unsigned int uart1_recv_word() {
  unsigned int v = uart1_recv_byte();
  return (v << 8) | uart1_recv_byte();
}

// UART からプログラムを受信して app_main と dmem が指すメモリに配置する
void recv_program(int (*app_main)(), unsigned int *dmem) {
  char buf[4];

  // 受信バッファを空にする
  while ((uart_flag & 0x01) != 0) {
    uart_data;
  }

  uart_puts("receiving a program from UART1\n");
  unsigned int pmem_len = uart1_recv_word();
  unsigned int dmem_bytes = uart1_recv_word();
  // 受信中に文字列表示などの重たい処理をするとデータを取りこぼす
  unsigned int i;
  unsigned int pmem_addr = app_main;
  for (i = 0; i < pmem_len; ++i) {
    char insn_hi = uart1_recv_byte();
    __builtin_write_pmem(pmem_addr++, insn_hi, uart1_recv_word());
  }

  for (i = 0; i < dmem_bytes; i += 2) {
    *dmem++ = uart1_recv_word();
  }

  // ので、表示処理は後でやる
  int2hex(pmem_len, buf, 4);
  uart_puts("pmem_len:   ");
  uart_putsn(buf, 4);
  int2hex(dmem_bytes, buf, 4);
  uart_puts("\ndmem_bytes: ");
  uart_putsn(buf, 4);
  uart_putc('\n');
}

unsigned int sdinfo;
unsigned int cap_mib;

void print_msg_dec(char *msg, int val) {
  char buf[5];
  uart_puts(msg);
  uart_puts(": ");
  int nzero = int2dec(val, buf, 5);
  uart_putsn(buf + nzero, 5 - nzero);
  uart_putc('\n');
}

void print_sdinfo() {
  char buf[5];
  uart_puts("SDv");
  if (sdinfo & 0x0001) {
    uart_puts("2+");
  } else {
    uart_puts("1 ");
  }
  if (sdinfo & 0x0002) {
    uart_puts("HC");
  } else {
    uart_puts("SC");
  }

  uart_puts(" ");
  int2dec(cap_mib, buf, 4);
  buf[4] = 0;
  uart_puts(buf);
  uart_puts("MB\n");
}

void print_partinfo() {
  print_msg_dec("BPB_BytsPerSec ", BPB_BytsPerSec);
  print_msg_dec("BPB_SecPerClus ", BPB_SecPerClus);
  print_msg_dec("BPB_ResvdSecCnt", BPB_ResvdSecCnt);
  print_msg_dec("BPB_NumFATs    ", BPB_NumFATs);
  print_msg_dec("BPB_RootEntCnt ", BPB_RootEntCnt);
  print_msg_dec("BPB_FATSz16    ", BPB_FATSz16);
  print_msg_dec("FirstDataSector", FirstDataSector);
  print_msg_dec("PartitionSector", PartitionSector);
}

void cat_file(char *filename, char *block_buf) {
  char fn83[11];
  filename_to_fn83(filename, fn83);
  char *file_entry = foreach_dir_entry(block_buf, 0, find_file, fn83);

  if (file_entry == 0) {
    uart_puts("no such file");
    return;
  }
  unsigned int siz_lo = read16(file_entry + 28);
  unsigned int siz_hi = read16(file_entry + 30);

  unsigned int clus = read16(file_entry + 26);
  unsigned int lba = clus_to_sec(clus);
  if (sd_read_block(block_buf, lba) < 0) {
    uart_puts("failed to load file\n");
    return;
  }

  int len = siz_lo;
  if (siz_hi > 0 || siz_lo >= 512) {
    len = 512;
  }
  uart_putsn(block_buf, len);
}

void proc_cmd(char *cmd, int (*app_main)(), char *block_buf) {
  if (strncmp(cmd, "ls", 3) == 0) {
    foreach_dir_entry(block_buf, 0, print_file_name, 0);
  } else if (strncmp(cmd, "sdinfo", 7) == 0) {
    print_sdinfo();
  } else if (strncmp(cmd, "partinfo", 9) == 0) {
    print_partinfo();
  } else if (strncmp(cmd, "cat ", 4) == 0) {
    cat_file(cmd + 4, block_buf);
  } else if (strncmp(cmd, "recv", 5) == 0) {
    recv_program(app_main, block_buf);
  } else if (strncmp(cmd, "run", 3) == 0 && (cmd[3] == '\0' | cmd[3] == ' ')) {
    char *argv[8];
    int argc = build_argv(cmd, argv, 8);
    run_app(app_main, block_buf, argc, argv);
  } else {
    char *argv[8];
    int argc = build_argv(cmd, argv, 8);
    if (load_exe_by_filename(app_main, block_buf, argv[0]) >= 0) {
      run_app(app_main, block_buf, argc, argv);
    }
  }
}

// 空クラスタを探し、そのクラスタ番号を返す。
//
// @retval 0 空クラスタが見つからなかった
// @retval 1 空クラスタの検索中に何らかのエラーが発生した
unsigned int find_free_cluster(unsigned int *block_buf) {
  for (int fat_sec_off = 0; fat_sec_off < BPB_FATSz16; ++fat_sec_off) {
    unsigned int fat_sec = PartitionSector + BPB_ResvdSecCnt + fat_sec_off;
    if (sd_read_block(block_buf, fat_sec) < 0) {
      return 1;
    }

    for (int i = 0; i < 256; ++i) {
      if (block_buf[i] == 0) {
        return (fat_sec_off << 8) + i;
      }
    }
  }
  return 0;
}

int main() {
  int i;
  unsigned int block_len;
  char buf[5];
  unsigned int csd[9]; // 末尾は 16 ビットの CRC
  int (*app_main)() = 0x2000;
  unsigned char *block_buf = 0x2000;

  sdinfo = sd_init();
  if (sdinfo < 0) {
    return 1;
  }
  if (sd_read_csd(csd) < 0) {
    return 1;
  }
  cap_mib = sd_get_capacity_mib(csd);
  block_len = sd_get_read_bl_len(csd);

  print_sdinfo(sdinfo, cap_mib);

  if (block_len != 9) {
    if (sd_set_block_len_512() < 0) {
      return 1;
    }
  }

  // sdinfo bit1 が CCS
  sd_ccs = (sdinfo & 2) != 0;
  if (sd_read_block(block_buf, 0) < 0) {
    return 1;
  }

  // MBR か PBR の判定
  if (!has_signature_55AA(block_buf)) {
    uart_puts("not found 55 AA");
    return 1;
  }
  if (!is_valid_PBR(block_buf)) {
    for (i = 0; i < 4; ++i) {
      int item_offset = 446 + (i << 4);
      if ((block_buf[item_offset] & 0x7f) != 0) {
        continue;
      }
      int part_type = block_buf[item_offset + 4];
      if ((part_type & 0xef) != 0x0e) {
        continue;
      }
      // FAT16 パーティションを見つけたので、PBR を読む

      PartitionSector = block_buf[item_offset + 8] | (block_buf[item_offset + 9] << 8);
      // LBA Start は 4 バイトだが、上位 16 ビットは無視
      // （32MiB までにあるパーティションのみ正常に読める）

      if (sd_read_block(block_buf, PartitionSector) < 0) {
        return 1;
      }
      break;
    }
    if (i == 4) {
      uart_puts("MBR with no FAT16 pt");
      return 1;
    }
  } else {
    uart_puts("Unknown BS");
    return 1;
  }

  if (!is_valid_PBR(block_buf)) {
    uart_puts("no valid PBR");
    return 1;
  }

  set_bpb_values(block_buf);
  if (BPB_BytsPerSec != 512) {
    uart_puts("BPB_BytsPerSec!=512");
    return 1;
  }

  RootEntSector = PartitionSector + BPB_ResvdSecCnt + BPB_NumFATs * BPB_FATSz16;
  char cmd[64];
  int cmd_i = 0;

  uart_puts("> ");

  while (1) {
    int key = uart_getc();

    if (key == '\n') { // Enter
      uart_putc('\n');
      if (cmd_i > 0) {
        cmd[cmd_i] = '\0';
        proc_cmd(cmd, app_main, block_buf);
      }
      cmd_i = 0;
      uart_puts("> ");
    } else if (key == '\b') {
      if (cmd_i > 0) {
        cmd_i--;
        uart_putc('\b');
        uart_del_char();
      }
    } else if (0x20 <= key & key < 0x80 & cmd_i < 63) {
      cmd[cmd_i++] = key;
      uart_putc(key);
    }
  }

  return 0;
}

int syscall(int funcnum, int *args) {
  __builtin_set_gp(0x0100);

  int ret = -1;
  switch (funcnum) {
  case 0: // get OS version
    ret = 1;
    break;
  case 1: // put string to the standard output
    {
      char *s = args[0];
      int len = args[1];
      ret = 0;
      if (len < 0) {
        while (*s) {
          uart_putc(*s++);
          ++ret;
        }
      } else {
        while (ret++ < len) {
          uart_putc(*s++);
        }
      }
    }
    break;
  case 2: // get string from the standard input
    {
      char *s = args[0];
      int len = args[1] - 1;
      int i = 0;
      while (1) {
        int c = uart_getc();
        uart_putc(c);
        if (c == '\n') {
          s[i] = '\0';
          break;
        } else if (c == '\b') {
          if (i > 0) {
            --i;
            uart_del_char();
          }
        } else if (i < len) {
          s[i++] = c;
        }
      }
    }
    break;
  case 3: // open a file in FAT filesystem on the main SD card
    {
      char *filename = args[0];
      unsigned int *file_entry = args[1];
      char block_buf[512];

      // file_entry の先頭 11 バイトを fn83 用バッファとして使う
      filename_to_fn83(filename, file_entry);
      unsigned int *ent = foreach_dir_entry(block_buf, 0, find_file, file_entry);
      if (ent != 0) {
        int i;
        for (i = 0; i < 16; ++i) {
          *file_entry++ = *ent++;
        }
        ret = 0;
      }
    }
    break;
  case 4: // read a block
    {
      char *file_entry = args[0];
      char *block_buf = args[1];  // read buffer (512 バイトの倍数の大きさ)
      unsigned int off = args[2]; // offset in blocks
      unsigned int len = args[3]; // length in blocks

      unsigned int clus = read16(file_entry + 26);
      unsigned int sec = clus_to_sec(clus); // セクタ番号

      if (off + len > BPB_SecPerClus) {
        // 1 クラスタを超える場所の読み込みは未実装
        ret = -2;
        break;
      }

      if (sd_read_block(block_buf, sec + off) < 0) {
        // 読み込みでエラー
        ret = -1;
        break;
      }

      ret = 0;
    }
    break;
  case 5: // write a block
    {
      char *file_entry = args[0];
      char *block_buf = args[1];  // read buffer (512 バイトの倍数の大きさ)
      unsigned int off = args[2]; // offset in blocks
      unsigned int len = args[3]; // length in blocks

      unsigned int clus = read16(file_entry + 26);
      unsigned int sec = clus_to_sec(clus); // セクタ番号

      if (off + len > BPB_SecPerClus) {
        // 1 クラスタを超える場所の書き込みは未実装
        ret = -2;
        break;
      }

      if (sd_write_block(block_buf, sec + off) < 0) {
        // 書き込みエラー
        ret = -1;
        break;
      }

      ret = 0;
    }
    break;
  case 6: // write a file entry
    // 同名ファイルのエントリを上書きするか、新規に作る
    {
      unsigned int *file_entry = args[0];

      /* mode flags
       * 1: 同名ファイルを上書き
       * 2: 新規作成
       */
      unsigned int mode = args[1];

      char block_buf[512];
      unsigned int ent_sec;
      unsigned int *ent = foreach_dir_entry(block_buf, &ent_sec, find_file, file_entry);
      if (ent) {
        if ((mode & 1) == 0) {
          // 上書きフラグが指定されていないのでエラー
          break;
        }
      } else { // 同名のエントリがない
        // このとき、block_buf には必ず 1 つ以上の空エントリがあるはず。
        // なぜなら find_file が「ファイルが無い」ことを確定させるために
        // ent[0] == 0 なエントリに出会うまで検索し続けるから。

        if ((mode & 2) == 0) {
          // 新規作成フラグが指定されていないのでエラー
          break;
        }

        // 空クラスタを探して割り当てる
        unsigned int clus = find_free_cluster(block_buf);
        if (clus <= 1) {
          break;
        }
        *((unsigned int *)block_buf + (clus & 255)) = 0xFFFF;
        if (sd_write_block(block_buf, PartitionSector + BPB_ResvdSecCnt + (clus >> 8)) < 0) {
          break;
        }

        file_entry[10] = 0;    // First cluster high
        file_entry[13] = clus; // First cluster low

        // 改めて空エントリを含むブロックを読み出し、この後の SD 書き込みに備える
        if (sd_read_block(block_buf, ent_sec) < 0) {
          break;
        }
        for (int i = 0; i < 16; ++i) {
          ent = block_buf + (i << 5);
          if (*ent == 0x00) { // 空エントリ
            break;
          }
        }
      }

      // ファイルエントリを更新し、ディスクに書き戻す
      for (int i = 0; i < 16; ++i) {
        *ent++ = *file_entry++;
      }
      if (sd_write_block(block_buf, ent_sec) < 0) {
        break;
      }
      ret = 0;
    }
    break;
  case 7:
    ret = int2hex(args[0], args[1], args[2]);
    break;
  case 8:
    ret = int2dec(args[0], args[1], args[2]);
    break;
  }
  __builtin_set_gp(0x2000);
  return ret;
}
