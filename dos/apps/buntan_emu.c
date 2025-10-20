#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int buntan_main(int *info);

// 先頭 0 の数を返す
static int int2hex(int val, char *s, int n) {
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
static int int2dec(int val, char *s, int n) {
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

int buntan_syscall(int num, int *args) {
  switch (num) {
  case 0: // sys_get_os_version
    return 0;
  case 1: // sys_put_string
    {
      char *s = (char*)args[0];
      int len = args[1];
      if (len >= 0) {
        return printf("%.*s", len, s);
      } else {
        return printf("%s", s);
      }
    }
  case 2: // sys_get_string
    {
      char *s = (char*)args[0];
      int len = args[1];
      if (len < 0) {
        return -1;
      } else {
        if (fgets(s, len, stdin) == NULL) {
          return -1;
        }
        len = strlen(s);
        if (s[len - 1] == '\n') {
          --len;
          s[len] = '\0';
        }
        return len;
      }
    }
  case 3: // sys_open_entry_fatsd
    {
      /*
       * ファイルエントリの構造
       *
       * FAT のファイルエントリは本来 32 バイト。
       * BuntanPC では int は 16 ビットなので、int[16] の配列として作られる。
       * 一方 buntan_emu.c の世界では int は 32 ビットを仮定しているため、
       * file_entry は 64 バイトのデータ構造となる。
       *
       * 本来のファイルエントリ    buntan_emu.c でのエントリ
       *  0  ------------------     0  ------------------   file_entry[0]
       *    | 8.3 filename     |      | normal filename  |
       *    |                  |   16 |------------------|
       *    |                  |      | file descriptor  |
       *    |                  |   20 |------------------|
       *    |                  |      | 空き領域         |
       * 11 |------------------|   21 |------------------|  file_entry[5]
       *    | DIR_Attr         |      | DIR_Attr         |
       *    |                  |   22 |------------------|
       *    |                  |      | 空き領域         |
       * 12 |------------------|   24 |------------------|  file_entry[6]
       *    | DIR_NTRes        |      | DIR_NTRes        |
       *    |                  |   26 |------------------|
       *    |                  |      | 空き領域         |
       * 14 |------------------|   28 |------------------|  file_entry[7]
       *            ...                       ...
       */
      char *filename = (char*)args[0];
      int *file_entry = (int*)args[1];

      memset(file_entry, 0, sizeof(int) * 16);
      strcpy((char*)file_entry, filename);

      struct stat sb;
      if (stat(filename, &sb) < 0) {
        return -1;
      }

      file_entry[14] = sb.st_size & 0xffffu;
      file_entry[15] = (sb.st_size >> 16) & 0xffffu;
      return 0;
    }
  case 4: // sys_read_block_fatsd
    {
      int *file_entry = (int*)args[0];
      uint8_t *block_buf = (uint8_t*)args[1];
      int off = args[2];
      int len = args[3];

      memset(block_buf, 0, 512 * len);

      unsigned int file_size = file_entry[14] | (file_entry[15] << 16);
      if (off + len > file_size) {
        return -2;
      }
      if (lseek(file_entry[4], 512 * off, SEEK_SET) < 0) {
        return -1;
      }
      if (read(file_entry[4], block_buf, 512 * len) < 0) {
        return -1;
      }
      return 0;
    }
  case 5: // sys_write_block_fatsd
    {
      int *file_entry = (int*)args[0];
      uint8_t *block_buf = (uint8_t*)args[1];
      int off = args[2];
      int len = args[3];

      unsigned int file_size = file_entry[14] | (file_entry[15] << 16);
      if (off + len > file_size) {
        return -2;
      }
      if (lseek(file_entry[4], 512 * off, SEEK_SET) < 0) {
        return -1;
      }
      if (write(file_entry[4], block_buf, 512 * len) < 0) {
        return -1;
      }
      return 0;
    }
  case 6: // sys_write_entry_fatsd
    {
      int *file_entry = (int*)args[0];
      unsigned int mode = args[1];

      int flag = O_RDWR;
      flag |= (mode & 2) ? O_CREAT : 0;
      flag |= (mode & 1) ? 0 : O_EXCL;

      file_entry[4] = open((char*)file_entry, flag, 0644);
      if (file_entry[4] < 0) {
        return -1;
      }
      return 0;
    }
  case 7: // sys_int2hex
    return int2hex(args[0], (char*)args[1], args[2]);
  case 8: // sys_int2dec
    return int2dec(args[0], (char*)args[1], args[2]);
  case 9: // sys_getc
    return getchar();
  default:
    printf("syscall %d is not implemented\n", num);
    return -1;
  }
}

int main(int argc, char **argv) {
  int buntan_info[4] = {
    0x0000, // .text address
    (int)buntan_syscall,
    argc,
    (int)argv,
  };
  return buntan_main(buntan_info);
}
