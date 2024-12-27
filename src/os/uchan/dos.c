#include "mmio.h"
#include "delay.h"
#include "lcd.h"

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

void lcd_putsn(char *s, int n) {
  while (n-- > 0) {
    lcd_putc(*s++);
  }
}

void lcd_clear() {
  lcd_cmd(0x01);
}

void lcd_puts_addr(int addr, char *s) {
  lcd_cmd(0x80 + addr);
  lcd_puts(s);
}

void putchar(char c) {
  lcd_putc(c);
  uart_putc(c);
}

void puts(char *s) {
  while (*s) {
    putchar(*s++);
  }
}

void lcd_cursor_at(int row, int col) {
  lcd_cmd(0x80 | ((((row & 1) << 6) | (row >> 1) * 20) + col));
}

void uart_cursor_at(int row, int col) {
  uart_puts("\x1B["); // CSI
  uart_putc('1' + row);
  uart_putc(';');
  if (col < 10) {
    uart_putc('1' + col);
  } else {
    uart_putc('1');
    uart_putc('0' + (col - 10));
  }
  uart_putc('H');
}

// カーソルを row 行 col 列に移動
// row, col: 0 始まり
void cursor_at(int row, int col) {
  lcd_cursor_at(row, col);
  uart_cursor_at(row, col);
}

// カーソルを row 行 col 列に移動し、その位置の文字を消去
void cursor_at_erace(int row, int col) {
  lcd_cursor_at(row, col);
  lcd_putc(' ');
  lcd_cursor_at(row, col);

  uart_cursor_at(row, col);
  uart_puts("\x1B[X");
}

void clear() {
  lcd_clear();
  uart_puts("\x1B[H\x1B[J");
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

void int2hex(int val, char *s, int n) {
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
}

// 先頭 0 の数を返す
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
  for (i = 0; s[i] == '0'; ++i) {
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

  lcd_puts_addr(0x00, "CMD");
  if (digit10 > 0) {
    putchar('0' + digit10);
  }
  putchar('0' + cmd);
  puts(" -> ");

  if (r1 < 0) {
    puts("timeout");
  } else {
    int2hex(r1, buf, 2);
    buf[2] = '\0';
    puts(buf);
  }

  lcd_puts_addr(0x40, msg);
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

  //delay_ms(500);
  clear();

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

// buf へ 1 ブロック読み込み
int sd_read_block(unsigned char *buf, int block_addr, int ccs) {
  int i;
  int r1;

  int addr_hi;
  int addr_lo;
  if (ccs) {
    addr_lo = block_addr;
    addr_hi = 0;
  } else {
    // バイトアドレスに変換
    addr_lo = block_addr << 9;
    addr_hi = block_addr >> (16 - 9);
  }

  assert_cs();
  send_sd_cmd(17, addr_hi, addr_lo, 0); // CMD17 (SEND_SINGLE_BLOCK)
  r1 = wait_sd_r1();
  if (r1 != 0) {
    show_sd_cmd_error(17, r1, "FAILED");
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

int load_exe(unsigned int pmem_addr, unsigned int dmem_addr, unsigned int exe_lba, int ccs) {
  int i;
  char buf[5];
  int *head = dmem_addr;
  char *block_buf = dmem_addr + 512;

  if (sd_read_block(head, exe_lba, ccs) < 0) {
    return -1;
  }
  int pmem_len = head[0]; // # of words
  int dmem_len = head[1]; // # of bytes

  int num_dmem_block = (dmem_len + 511) >> 9;
  int pmem_lba = exe_lba + num_dmem_block;

  int byte_index = 512;
  for (i = 0; i < pmem_len; ++i) { // insn[0]=0~2, insn[169]=507~509
    if (byte_index >= 510) { // のこり 3 バイト未満なので、次のクラスタを読む
      char insn_buf0 = block_buf[510];
      char insn_buf1 = block_buf[511];
      if (sd_read_block(block_buf, pmem_lba++, ccs) < 0) {
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
    if (sd_read_block(block_buf, exe_lba + i, ccs) < 0) {
      return -1;
    }
    block_buf += 512;
  }

  return 0;
}

int get_key() {
  while (1) {
    if ((kbc_status & 0xff) != 0) {
      return kbc_queue;
    }
    if ((uart3_flag & 0x01) != 0) {
      break;
    }
  }

  char c = uart_getc();
  if (c != 0x1b) {
    return c;
  } else { // escape sequence
    c = uart_getc();
    if (c != '[') {
      return c;
    } else { // CSI
      c = uart_getc();
      if ('A' <= c & c <= 'D') {
        return 0x1c + c - 'A';
      }
      return c;
    }
  }
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

unsigned int rootdir_sec;

int foreach_dir_entry(char *block_buf, int ccs, int (*proc_entry)(), void *arg) {
  int find_loop;
  for (find_loop = 0; find_loop < BPB_RootEntCnt >> 4; ++find_loop) {
    if (sd_read_block(block_buf, rootdir_sec + find_loop, ccs) < 0) {
      puts("failed to read block");
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
      putchar(0x01);
    } else {
      putchar(*name);
    }
  }
}

unsigned int read16(char *buf) {
  return *buf | (buf[1] << 8);
}

int print_file_name(char *dir_entry, int *i) {
  if (*dir_entry == 0xe5 || *dir_entry == 0x00 || (dir_entry[11] & 0x0e) != 0) {
    return 0;
  }
  int is_dir = dir_entry[11] & 0x10;
  int rownum = *i;

  // i addr          i_bin expr
  // 0 0x80=0x80+0   0 0   0*64 + 0*20
  // 1 0xc0=0x80+64  0 1   1*64 + 0*20
  // 2 0x94=0x80+20  1 0   0*64 + 1*20
  // 3 0xd4=0x80+84  1 1   1*64 + 1*20
  if (rownum == 0) {
    clear();
  } else {
    cursor_at(rownum, 0);
  }

  print_filename_trimspace(dir_entry, dir_entry + 7);
  if (strncmp(dir_entry + 8, "   ", 3) != 0) { // 拡張子あり
    putchar('.');
    print_filename_trimspace(dir_entry + 8, dir_entry + 10);
  }
  if (is_dir) {
    putchar('/');
  }

  // ファイルサイズ表示
  cursor_at(rownum, 13);
  unsigned int siz_lo = read16(dir_entry + 28);
  unsigned int siz_hi = read16(dir_entry + 30);
  char buf[6];
  if (siz_hi == 0) {
    int2dec(siz_lo, buf, 5);
    buf[5] = 0;
    puts(buf);
    putchar('B');
  } else {
    puts("TOOBIG");
  }

  if (rownum == 3) {
    cursor_at(3, 19);
    lcd_putc(0x02);
    uart_puts("\xe2\xa4\xb6");
    while (get_key() != 0x1e);
  }
  *i = (rownum + 1) & 3;

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

int load_exe_by_filename(int (*app_main)(), char *block_buf, char *filename, int ccs) {
  char fn83[11];
  filename_to_fn83(filename, fn83);

  char *file_entry = foreach_dir_entry(block_buf, ccs, find_file, fn83);
  if (file_entry == 0 && fn83[8] == ' ') {
    fn83[8]  = 'E';
    fn83[9]  = 'X';
    fn83[10] = 'E';
    file_entry = foreach_dir_entry(block_buf, ccs, find_file, fn83);
  }

  if (file_entry == 0) {
    puts("No such file");
    return -1;
  } else {
    unsigned int exe_clus = read16(file_entry + 26);
    unsigned int exe_lba = clus_to_sec(exe_clus);
    if (load_exe(app_main, block_buf, exe_lba, ccs) < 0) {
      puts("failed to load app");
      return -1;
    }

    puts("File loaded\n");
  }
  return 0;
}

void run_app(int (*app_main)(), char *block_buf) {
  char buf[5];
  int appinfo[2] = { // アプリに渡す構成情報
    (int)app_main, // 0: .text の開始アドレス
    (int)syscall,  // 1: システムコールのエントリアドレス
  };

  __builtin_set_gp(block_buf);
  int ret_code = app_main(appinfo);
  __builtin_set_gp(0x100);

  cursor_at(3, 0);
  int2hex(ret_code, buf, 4);
  buf[4] = 0;
  putchar(0x7e);
  puts(buf);
}

unsigned int sdinfo;
unsigned int cap_mib;

void print_sdinfo() {
  char buf[5];
  puts("SDv");
  if (sdinfo & 0x0001) {
    puts("2+");
  } else {
    puts("1 ");
  }
  if (sdinfo & 0x0002) {
    puts("HC");
  } else {
    puts("SC");
  }

  puts(" ");
  int2dec(cap_mib, buf, 4);
  buf[4] = 0;
  puts(buf);
  puts("MB");
}

void cat_file(char *filename, char *block_buf, int ccs) {
  char fn83[11];
  filename_to_fn83(filename, fn83);
  char *file_entry = foreach_dir_entry(block_buf, ccs, find_file, fn83);

  cursor_at(1, 0);
  if (file_entry == 0) {
    puts("no such file");
    return;
  }
  unsigned int siz_lo = read16(file_entry + 28);
  unsigned int siz_hi = read16(file_entry + 30);

  unsigned int clus = read16(file_entry + 26);
  unsigned int lba = clus_to_sec(clus);
  if (sd_read_block(block_buf, lba, ccs) < 0) {
    puts("failed to load file");
    return;
  }

  if (siz_hi > 0) {
    siz_lo = 0xffff;
  }

  int row;
  int len;
  for (row = 1; siz_lo > 0 & row < 4; ++row) {
    cursor_at(row, 0);
    if (siz_lo >= 20) {
      len = 20;
    } else {
      len = siz_lo;
    }
    lcd_putsn(block_buf, len);
    block_buf += len;
    siz_lo -= len;
  }
}

void proc_cmd(char *cmd, int (*app_main)(), char *block_buf, int ccs) {
  int i;
  char buf[5];
  cursor_at(1, 0);
  if (strncmp(cmd, "ls", 3) == 0) {
    i = 0;
    foreach_dir_entry(block_buf, ccs, print_file_name, &i);
  } else if (strncmp(cmd, "ld ", 3) == 0) {
    load_exe_by_filename(app_main, block_buf, cmd + 3, ccs);
  } else if (strncmp(cmd, "run", 4) == 0) {
    run_app(app_main, block_buf);
  } else if (strncmp(cmd, "sdinfo", 4) == 0) {
    print_sdinfo();
  } else if (strncmp(cmd, "cat ", 4) == 0) {
    cat_file(cmd + 4, block_buf, ccs);
  } else {
    if (load_exe_by_filename(app_main, block_buf, cmd, ccs) >= 0) {
      run_app(app_main, block_buf);
    }
  }
}

int main() {
  int i;
  unsigned int block_len;
  char buf[5];
  unsigned int csd[9]; // 末尾は 16 ビットの CRC
  int (*app_main)() = 0x2000;
  unsigned char *block_buf = 0x2000;

  lcd_init();
  // チルダ
  // 0  _ * _ _ _
  // 1  * _ * _ *
  // 2  _ _ _ * _
  // 3  _ _ _ _ _
  // 4  _ _ _ _ _
  // 5  _ _ _ _ _
  // 6  _ _ _ _ _
  // 7  _ _ _ _ _
  lcd_cmd(0x48);
  lcd_puts("\x08\x15\x02");
  for (i = 0; i < 5; ++i) {
    putchar(0);
  }
  // 行が続くことを表す記号
  // 0  _ _ _ _ _
  // 1  _ _ _ _ _
  // 2  _ _ _ * _
  // 3  _ _ _ * _
  // 4  _ _ _ * _
  // 5  _ * _ * _
  // 6  * * * * _
  // 7  _ * _ _ _
  lcd_cmd(0x50);
  lcd_putsn("\0\0\x02\x02\x02\x0a\x1e\x08", 8);

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
  int ccs = (sdinfo & 2) != 0;
  if (sd_read_block(block_buf, 0, ccs) < 0) {
    return 1;
  }
  cursor_at(1, 0);

  // MBR か PBR の判定
  if (!has_signature_55AA(block_buf)) {
    puts("not found 55 AA");
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

      if (sd_read_block(block_buf, PartitionSector, ccs) < 0) {
        return 1;
      }
      break;
    }
    if (i == 4) {
      puts("MBR with no FAT16 pt");
      return 1;
    }
  } else {
    puts("Unknown BS");
    return 1;
  }

  if (!is_valid_PBR(block_buf)) {
    puts("no valid PBR");
    return 1;
  }

  set_bpb_values(block_buf);
  if (BPB_BytsPerSec != 512) {
    puts("BPB_BytsPerSec!=512");
    return 1;
  }

  rootdir_sec = PartitionSector + BPB_ResvdSecCnt + BPB_NumFATs * BPB_FATSz16;
  char cmd[21];
  int cmd_i = 0;
  int caps = 0;
  int shift = 0;

  while (1) {
    int key = get_key();
    if (cmd_i == 0 && key < 0x80) {
      clear();
    }

    if (key == '\n') { // Enter
      if (cmd_i > 0) {
        cmd[cmd_i] = '\0';
        proc_cmd(cmd, app_main, block_buf, ccs);
      }
      cmd_i = 0;
    } else if (key == '\b') {
      if (cmd_i > 0) {
        cmd_i--;
        cursor_at_erace(0, cmd_i);
      }
    } else if (key == 0x0E) { // Shift
      shift = 1;
      led_port |= 0x02;
    } else if (key == 0x8E) { // Shift (break)
      shift = 0;
      led_port &= 0xfd;
    } else if (key == 0x0F) { // Caps
      caps = !caps;
      led_port = (led_port & 0xfe) | caps;
    } else if (0x20 <= key & key < 0x80 & cmd_i < 20) {
      if (caps ^ shift) {
        key = shift_map[key - 0x20];
      }
      putchar(key);
      cmd[cmd_i++] = key;
    }
  }

  return 0;
}

int syscall(int funcnum, int *args) {
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
  }
  return ret;
}
