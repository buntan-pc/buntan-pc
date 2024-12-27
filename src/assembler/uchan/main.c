#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_OPERAND 128
#define MAX_BP 512
#define MAX_LABEL 512
#define MAX_LINE 512

// 文字列をすべて小文字にする
void ToLower(char *s) {
  while (*s) {
    *s = tolower(*s);
    s++;
  }
}

struct AsmLine {
  char *label;
  char *mnemonic;
  char *operands[MAX_OPERAND];
  int num_opr;
};

// line をニーモニックとオペランドに分割する
void SplitOpcode(char *line, struct AsmLine *al) {
  al->label = al->mnemonic = NULL;
  al->num_opr = 0;

  if (strtok(line, ";#") == NULL) {
    return;
  }

  char *colon = strchr(line, ':');
  if (colon) {
    al->label = line;
    *colon = '\0';
    line = colon + 1;
  }

  if ((al->mnemonic = strtok(line, " \t\n")) == NULL) {
    return;
  }
  ToLower(al->mnemonic);
  int i;
  for (i = 0; i < MAX_OPERAND; ++i) {
    if ((al->operands[i] = strtok(NULL, ",\n")) == NULL) {
      break;
    }
    al->operands[i] += strspn(al->operands[i], " \t");
    for (char *last = al->operands[i] + strlen(al->operands[i]) - 1;
         last > al->operands[i] && strchr(" \t", *last) != NULL;
         last--) {
      *last = '\0';
    }
  }
  al->num_opr = i;
}

struct LabelAddr {
  const char *label;
  int addr;
};

enum BPType {
  BP_IP_REL12,
  BP_IP_REL14,
  BP_ABS16,
};

struct Backpatch {
  int ip;
  const char *label;
  enum BPType type;
};

void InitBackpatch(struct Backpatch *bp,
                   int ip, const char *label, enum BPType type) {
  bp->ip = ip;
  bp->label = label;
  bp->type = type;
}

void NewBackpatch(struct Backpatch *backpatches, int *num_backpatches,
                  int ip, const char *label, enum BPType bp_type) {
  if (*num_backpatches == MAX_BP) {
    fprintf(stderr, "too many backpatches\n");
    exit(1);
  }
  InitBackpatch(backpatches + (*num_backpatches)++, ip, strdup(label), bp_type);
}

enum AddrBase {
  AB_ZERO,
  AB_FP,
  AB_GP,
  AB_RSVD,
};

// X+imm 形式のオペランドを読み取る
// 戻り値: X の種類を表す数値
enum AddrBase ParseAddrOffset(char *opr, uint16_t *off, struct LabelAddr *labels, int num_labels) {
  char *plus = strchr(opr, '+');
  char *off_s = opr;
  enum AddrBase ab = AB_ZERO;
  if (plus) {
    *plus = '\0';
    off_s = plus + 1;
    ToLower(opr);
    if (strcmp(opr, "z") == 0) {
      ab = AB_ZERO;
    } else if (strcmp(opr, "zero") == 0) {
      ab = AB_ZERO;
    } else if (strcmp(opr, "fp") == 0) {
      ab = AB_FP;
    } else if (strcmp(opr, "gp") == 0) {
      ab = AB_GP;
    } else {
      ab = AB_RSVD;
    }
  }
  if (isdigit(*off_s)) {
    char *endptr;
    *off = strtol(off_s, &endptr, 0);

    if (*endptr) {
      fprintf(stderr, "failed to parse addr offset: '%s'\n", endptr);
      exit(1);
    }
  } else {
    int i = 0;
    for (; i < num_labels; ++i) {
      if (strcmp(labels[i].label, off_s) == 0) {
        break;
      }
    }
    if (i == num_labels) {
      fprintf(stderr, "no such label: '%s'\n", off_s);
      exit(1);
    }
    *off = labels[i].addr;
  }

  return ab;
}

int ExpectFP(char *opr) {
  ToLower(opr);
  if (strcmp(opr, "fp") != 0) {
    return -1;
  }
  return 0;
}

// i 番目のオペランドを文字列として取得
char *GetOperand(char *mnemonic, char **operands, int n, int i) {
  if (n <= i) {
    fprintf(stderr, "too few operands for '%s': %d\n", mnemonic, n);
    exit(1);
  }
  return operands[i];
}

// i 番目のオペランドを long 値として取得（整数変換できなければバックパッチ）
long GetOperandLong(char *operand, struct Backpatch *backpatches,
                    int *num_backpatches, int ip, enum BPType bp_type) {
  char *endptr;
  long value = strtol(operand, &endptr, 0);

  if (endptr == operand) {
    InitBackpatch(backpatches + *num_backpatches, ip, strdup(operand), bp_type);
    (*num_backpatches)++;
    return 0;
  } else if (*endptr) {
    fprintf(stderr, "failed conversion to long: '%s'\n", endptr);
    exit(1);
  }

  return value;
}

// i 番目のオペランドを long 値として取得
long GetOperandLongNoBP(char *operand) {
  char *endptr;
  long value = strtol(operand, &endptr, 0);

  if (*endptr) {
    fprintf(stderr, "failed conversion to long: '%s'\n", endptr);
    exit(1);
  }

  return value;
}

#define GET_STR(i) GetOperand((al->mnemonic), (al->operands), (al->num_opr), (i))
#define GET_LONG(i, bp_type) GetOperandLong(\
    GetOperand((al->mnemonic), (al->operands), (al->num_opr), (i)), \
    (backpatches), &(num_backpatches), (ip), (bp_type))
#define GET_LONG_NO_BP(i) GetOperandLongNoBP(\
    GetOperand((al->mnemonic), (al->operands), (al->num_opr), (i)))

// db 命令のオペランドを解釈し、メモリに書き、バイト数を返す
int DataByte(uint8_t *dmem, char **operands, int num_opr) {
  char *endptr;
  for (int i = 0; i < num_opr; i++) {
    dmem[i] = strtol(operands[i], &endptr, 0);
    if (*endptr) {
      fprintf(stderr, "failed conversion to long: '%s'\n", endptr);
      exit(1);
    }
  }
  return num_opr;
}

// 即値付きロードストア命令
// insn: 機械語テンプレート
// opr: "X+off" 形式の文字列
// mask: 即値のビットマスク
uint32_t GenLoadStoreImm(uint32_t insn, char *operand, uint16_t mask, struct LabelAddr *labels, int num_labels) {
  uint16_t off;
  enum AddrBase ab = ParseAddrOffset(operand, &off, labels, num_labels);
  return insn | (ab << 12) | (mask & off);
}

enum PopReg {
  POP_FP,
  POP_GP,
  POP_ISR,
};

enum PopReg ParsePopReg(char *opr) {
  ToLower(opr);
  if (strcmp(opr, "fp") == 0) {
    return POP_FP;
  } else if (strcmp(opr, "gp") == 0) {
    return POP_GP;
  } else if (strcmp(opr, "isr") == 0) {
    return POP_ISR;
  }

  fprintf(stderr, "invalid register for POP X: '%s'\n", opr);
  exit(1);
}

struct Interp {
  int16_t stack[16];
};

void Push(struct Interp *interp, int16_t v) {
  for (int i = 14; i >= 0; i--) {
    interp->stack[i + 1] = interp->stack[i];
  }
  interp->stack[0] = v;
}

int16_t Pop(struct Interp *interp) {
  int16_t top = interp->stack[0];
  for (int i = 0; i <= 14; i++) {
    interp->stack[i] = interp->stack[i + 1];
  }
  return top;
}

int16_t Top(struct Interp *interp) {
  return interp->stack[0];
}

// ラベルとアドレスの組を設定し、マップファイルに書く。
// ラベルが設定されれば 1 を、ラベルが NULL なら 0 を返す。
int SetLabel(struct LabelAddr *la, const char *label, int addr, FILE *map_file) {
  if (label) {
    la->label = strdup(label);
    la->addr = addr;

    if (map_file) {
      fprintf(map_file, "0x%04x %s\n", addr, label);
    }
    return 1;
  }
  return 0;
}

// .data セクションを処理し、バイト数を返す
//
// input_file: ソースコード
// map_file: マップファイル
// dmem: データメモリへのポインタ
// line: 行バッファ
// al: "section .data" 行
//   関数の終了時には al に次のセクション定義行が格納された状態になっている
// labels: 発見したラベルとそのときのデータメモリアドレスの組を記録
int ProcessDataSection(FILE *input_file, FILE *map_file,
                       uint8_t *dmem, char *line, struct AsmLine *al,
                       struct LabelAddr *labels, int *num_labels, int exe_mode) {
  int size = 0;
  int first_label = 1;
  while (fgets(line, MAX_LINE, input_file) != NULL) {
    SplitOpcode(line, al);
    if (first_label && al->label) {
      if (exe_mode && strcmp(al->label, "pmem_len") != 0) {
        // exe_mode の場合、先頭 4 バイトを pmem_len & dmem_len に強制する
        SetLabel(labels + 0, "pmem_len", 0x0000, map_file);
        SetLabel(labels + 1, "dmem_len", 0x0002, map_file);
        *num_labels = 2;
        size = 4;
      }
      first_label = 0;
    }

    *num_labels += SetLabel(labels + *num_labels, al->label,
                            size, map_file);
    if (al->mnemonic == NULL) {
      continue;
    }

    if (strcmp(al->mnemonic, "section") == 0) {
      if (strcmp(al->operands[0], ".data") != 0) {
        // .data セクションの終了
        if (first_label && exe_mode) {
          // exe_mode の場合、先頭 4 バイトを pmem_len & dmem_len に強制する
          SetLabel(labels + 0, "pmem_len", 0x0000, map_file);
          SetLabel(labels + 1, "dmem_len", 0x0002, map_file);
          *num_labels = 2;
        }
        break;
      }
    } else if (strcmp(al->mnemonic, "db") == 0) {
      size += DataByte(dmem + size, al->operands, al->num_opr);
      if (size & 1) {
        size++; // 偶数アドレスにそろえる
      }
    } else {
      fprintf(stderr, "'%s' is not supported in .data\n", al->mnemonic);
      exit(1);
    }
  }

  return size;
}

// .text セクションを処理し、命令数を返す
//
// input_file: ソースコード
// map_file: マップファイル
// pmem: プログラムメモリへのポインタ
// line: 行バッファ
// al: "section .text" 行
// labels: 発見したラベルとそのときの IP の組を記録
int ProcessTextSection(FILE *input_file, FILE *map_file,
                       uint32_t *pmem, char *line, struct AsmLine *al,
                       struct LabelAddr *labels, int *num_labels) {
  int ip = 0;

  struct Backpatch backpatches[MAX_BP];
  int num_backpatches = 0;

  struct Interp interp;

  while (fgets(line, MAX_LINE, input_file) != NULL) {
    SplitOpcode(line, al);
    *num_labels += SetLabel(labels + *num_labels, al->label, ip, map_file);
    if (al->mnemonic == NULL) {
      continue;
    }

    if (al->mnemonic[0] == '.') {
      // 内蔵インタプリタ用命令
      if (strcmp(al->mnemonic + 1, "push") == 0) {
        Push(&interp, GET_LONG_NO_BP(0));
      } else if (strcmp(al->mnemonic + 1, "sign") == 0) {
        Push(&interp, (uint16_t)Pop(&interp) ^ 0x8000);
      } else if (strcmp(al->mnemonic + 1, "add") == 0) {
        int16_t rhs = Pop(&interp);
        Push(&interp, Pop(&interp) + rhs);
      } else if (strcmp(al->mnemonic + 1, "sub") == 0) {
        int16_t rhs = Pop(&interp);
        Push(&interp, Pop(&interp) - rhs);
      } else if (strcmp(al->mnemonic + 1, "mul") == 0) {
        int16_t rhs = Pop(&interp);
        Push(&interp, Pop(&interp) * rhs);
      } else if (strcmp(al->mnemonic + 1, "eq") == 0) {
        int16_t rhs = Pop(&interp);
        Push(&interp, Pop(&interp) == rhs);
      } else if (strcmp(al->mnemonic + 1, "neq") == 0) {
        int16_t rhs = Pop(&interp);
        Push(&interp, Pop(&interp) != rhs);
      } else if (strcmp(al->mnemonic + 1, "lt") == 0) {
        int16_t rhs = Pop(&interp);
        Push(&interp, Pop(&interp) < rhs);
      } else if (strcmp(al->mnemonic + 1, "le") == 0) {
        int16_t rhs = Pop(&interp);
        Push(&interp, Pop(&interp) <= rhs);
      } else if (strcmp(al->mnemonic + 1, "bt") == 0) {
        uint16_t rhs = Pop(&interp);
        Push(&interp, (uint16_t)Pop(&interp) < rhs);
      } else if (strcmp(al->mnemonic + 1, "be") == 0) {
        uint16_t rhs = Pop(&interp);
        Push(&interp, (uint16_t)Pop(&interp) <= rhs);
      } else if (strcmp(al->mnemonic + 1, "not") == 0) {
        Push(&interp, ~(uint16_t)Pop(&interp));
      } else if (strcmp(al->mnemonic + 1, "and") == 0) {
        int16_t rhs = Pop(&interp);
        Push(&interp, Pop(&interp) & rhs);
      } else if (strcmp(al->mnemonic + 1, "or") == 0) {
        int16_t rhs = Pop(&interp);
        Push(&interp, Pop(&interp) | rhs);
      } else if (strcmp(al->mnemonic + 1, "xor") == 0) {
        int16_t rhs = Pop(&interp);
        Push(&interp, Pop(&interp) ^ rhs);
      } else if (strcmp(al->mnemonic + 1, "shr") == 0) {
        int16_t rhs = Pop(&interp);
        Push(&interp, (uint16_t)Pop(&interp) >> rhs);
      } else if (strcmp(al->mnemonic + 1, "sar") == 0) {
        int16_t rhs = Pop(&interp);
        Push(&interp, (int16_t)Pop(&interp) >> rhs);
      } else if (strcmp(al->mnemonic + 1, "shl") == 0) {
        int16_t rhs = Pop(&interp);
        Push(&interp, (int16_t)Pop(&interp) << rhs);
      }
      continue;
    }

    if (strcmp(al->mnemonic, "push") == 0) {
      char *opr = GET_STR(0);
      if (strchr(opr, '+') == NULL) {
        if (opr[0] == '$') {
          if (strcmp(opr + 1, "top") == 0) {
            uint16_t stack_top = Top(&interp);
            pmem[ip] = 0x30000 | stack_top;
          } else {
            fprintf(stderr, "unknown variable: '%s'\n", opr);
            exit(1);
          }
        } else {
          pmem[ip] = 0x30000 | GET_LONG(0, BP_ABS16);
        }
      } else {
        uint16_t off;
        enum AddrBase ab = ParseAddrOffset(GET_STR(0), &off, labels, *num_labels);
        pmem[ip] = 0x14000 | (ab << 12) | (0xfff & off);
      }
    } else if (strcmp(al->mnemonic, "jmp") == 0) {
      NewBackpatch(backpatches, &num_backpatches, ip, GET_STR(0), BP_IP_REL12);
      pmem[ip] = 0x04000;
    } else if (strcmp(al->mnemonic, "call") == 0) {
      if (al->num_opr == 0) {
        pmem[ip] = 0x1c801;
      } else {
        // ジャンプ先ラベルを探す
        struct LabelAddr *call_to = NULL;
        char *call_label = GET_STR(0);
        for (int i = 0; i < *num_labels; ++i) {
          if (strcmp(labels[i].label, call_label) == 0) {
            call_to = labels + i;
            break;
          }
        }

        pmem[ip] = 0x00000;
        if (call_to) { // 後方（既知のラベル）への call
          pmem[ip] |= 0x3fffu & (call_to->addr - ip - 1);
        } else {
          NewBackpatch(backpatches, &num_backpatches, ip, GET_STR(0), BP_IP_REL14);
        }
      }
    } else if (strcmp(al->mnemonic, "jz") == 0) {
      NewBackpatch(backpatches, &num_backpatches, ip, GET_STR(0), BP_IP_REL12);
      pmem[ip] = 0x06000;
    } else if (strcmp(al->mnemonic, "ld1") == 0) {
      pmem[ip] = GenLoadStoreImm(0x08000, GET_STR(0), 0xfff, labels, *num_labels);
    } else if (strcmp(al->mnemonic, "st1") == 0) {
      pmem[ip] = GenLoadStoreImm(0x0c000, GET_STR(0), 0xfff, labels, *num_labels);
    } else if (strcmp(al->mnemonic, "ld") == 0) {
      pmem[ip] = GenLoadStoreImm(0x10000, GET_STR(0), 0xffe, labels, *num_labels);
    } else if (strcmp(al->mnemonic, "st") == 0) {
      pmem[ip] = GenLoadStoreImm(0x10001, GET_STR(0), 0xffe, labels, *num_labels);
    } else if (strcmp(al->mnemonic, "add") == 0) {
      if (al->num_opr == 0) {
        pmem[ip] = 0x1c060;
      } else {
        char *base = GET_STR(0);
        if (ExpectFP(base)) {
          fprintf(stderr, "base of ADD must be FP: '%s'\n", base);
          exit(1);
        }
        uint16_t addend = GET_LONG_NO_BP(1);
        pmem[ip] = 0x05000 | (0xfff & addend);
      }
    } else if (strcmp(al->mnemonic, "nop") == 0) {
      pmem[ip] = 0x1c000;
    } else if (strcmp(al->mnemonic, "pop") == 0) {
      if (al->num_opr == 0) {
        pmem[ip] = 0x1c04f;
      } else {
        enum PopReg pr = ParsePopReg(GET_STR(0));
        pmem[ip] = 0x1c820 | pr;
      }
    } else if (strcmp(al->mnemonic, "inc") == 0) {
      pmem[ip] = 0x1c001;
    } else if (strcmp(al->mnemonic, "inc2") == 0) {
      pmem[ip] = 0x1c002;
    } else if (strcmp(al->mnemonic, "not") == 0) {
      pmem[ip] = 0x1c004;
    } else if (strcmp(al->mnemonic, "sign") == 0) {
      pmem[ip] = 0x1c005;
    } else if (strcmp(al->mnemonic, "exts") == 0) {
      pmem[ip] = 0x1c006;
    } else if (strcmp(al->mnemonic, "and") == 0) {
      pmem[ip] = 0x1c050;
    } else if (strcmp(al->mnemonic, "or") == 0) {
      pmem[ip] = 0x1c051;
    } else if (strcmp(al->mnemonic, "xor") == 0) {
      pmem[ip] = 0x1c052;
    } else if (strcmp(al->mnemonic, "shr") == 0) {
      pmem[ip] = 0x1c054;
    } else if (strcmp(al->mnemonic, "sar") == 0) {
      pmem[ip] = 0x1c055;
    } else if (strcmp(al->mnemonic, "shl") == 0) {
      pmem[ip] = 0x1c056;
    } else if (strcmp(al->mnemonic, "sub") == 0) {
      pmem[ip] = 0x1c061;
    } else if (strcmp(al->mnemonic, "mul") == 0) {
      pmem[ip] = 0x1c062;
    } else if (strcmp(al->mnemonic, "eq") == 0) {
      pmem[ip] = 0x1c068;
    } else if (strcmp(al->mnemonic, "neq") == 0) {
      pmem[ip] = 0x1c069;
    } else if (strcmp(al->mnemonic, "lt") == 0) {
      pmem[ip] = 0x1c06a;
    } else if (strcmp(al->mnemonic, "le") == 0) {
      pmem[ip] = 0x1c06b;
    } else if (strcmp(al->mnemonic, "bt") == 0) {
      pmem[ip] = 0x1c06c;
    } else if (strcmp(al->mnemonic, "be") == 0) {
      pmem[ip] = 0x1c06d;
    } else if (strcmp(al->mnemonic, "dup") == 0) {
      if (al->num_opr == 0) {
        pmem[ip] = 0x1c080;
      } else {
        long n = GET_LONG_NO_BP(0);
        if (n == 0) {
          pmem[ip] = 0x1c080;
        } else if (n == 1) {
          pmem[ip] = 0x1c08f;
        } else {
          fprintf(stderr, "DUP takes 0 or 1: %ld\n", n);
          exit(1);
        }
      }
    } else if (strcmp(al->mnemonic, "ret") == 0) {
      pmem[ip] = 0x1c800;
    } else if (strcmp(al->mnemonic, "cpop") == 0) {
      char *target = GET_STR(0);
      if (ExpectFP(target)) {
        fprintf(stderr, "CPOP takes FP: '%s'\n", target);
        exit(1);
      }
      pmem[ip] = 0x1c802;
    } else if (strcmp(al->mnemonic, "cpush") == 0) {
      char *target = GET_STR(0);
      if (ExpectFP(target)) {
        fprintf(stderr, "CPUSH takes FP: '%s'\n", target);
        exit(1);
      }
      pmem[ip] = 0x1c803;
    } else if (strcmp(al->mnemonic, "ldd") == 0) {
      pmem[ip] = 0x1c808;
    } else if (strcmp(al->mnemonic, "ldd1") == 0) {
      pmem[ip] = 0x1c809;
    } else if (strcmp(al->mnemonic, "sta") == 0) { // store, remaining address
      pmem[ip] = 0x1c80c;
    } else if (strcmp(al->mnemonic, "sta1") == 0) { // store, remaining address
      pmem[ip] = 0x1c80d;
    } else if (strcmp(al->mnemonic, "std") == 0) { // store, remaining data
      pmem[ip] = 0x1c80e;
    } else if (strcmp(al->mnemonic, "std1") == 0) { // store, remaining data
      pmem[ip] = 0x1c80f;
    } else if (strcmp(al->mnemonic, "db") == 0) {
      fprintf(stderr, "db is not supported in .text section\n");
      exit(1);
    } else if (strcmp(al->mnemonic, "int") == 0) {
      pmem[ip] = 0x1c810;
    } else if (strcmp(al->mnemonic, "iret") == 0) {
      pmem[ip] = 0x1c812;
    } else if (strcmp(al->mnemonic, "spha") == 0) {
      pmem[ip] = 0x1c824;
    } else if (strcmp(al->mnemonic, "spla") == 0) {
      pmem[ip] = 0x1c825;
    } else {
      fprintf(stderr, "unknown mnemonic: %s\n", al->mnemonic);
      exit(1);
    }

    ip++;
  }

  for (int i = 0; i < num_backpatches; i++) {
    int l = 0;
    for (; l < *num_labels; l++) {
      if (strcmp(backpatches[i].label, labels[l].label)) {
        continue;
      }

      int ins_pc = backpatches[i].ip;
      uint32_t ins = pmem[ins_pc];
      int diff_pc = labels[l].addr - ins_pc - 1;
      switch (backpatches[i].type) {
      case BP_IP_REL12:
        if (diff_pc < -0x0800 || 0x0800 <= diff_pc) {
          fprintf(stderr, "jump target is too far (BP_IP_REL12): diff=%d\n", diff_pc);
          exit(1);
        }
        ins = (ins & 0x3f000u) | (diff_pc & 0xfffu);
        break;
      case BP_IP_REL14:
        if (diff_pc < -0x2000 || 0x2000 <= diff_pc) {
          fprintf(stderr, "jump target is too far (BP_IP_REL14): diff=%d\n", diff_pc);
          exit(1);
        }
        ins = (ins & 0x3c000u) | (diff_pc & 0x3fffu);
        break;
      case BP_ABS16:
        ins = (ins & 0x30000u) | (labels[l].addr & 0xffffu);
        break;
      }
      pmem[ins_pc] = ins;
      break;
    }
    if (l == *num_labels) {
      fprintf(stderr, "unknown label: %s\n", backpatches[i].label);
      exit(1);
    }
  }

  return ip;
}

#define ARG_FILE(name) \
  do { \
    i++; \
    if (i >= argc) { \
      fprintf(stderr, "<" #name "> is not specified\n"); \
      exit(1); \
    } \
    name ## _filename = argv[i]; \
  } while (0)


int main(int argc, char **argv) {
  const char *input_filename = NULL;
  const char *dmem_filename = NULL;
  const char *pmem_filename = NULL;
  const char *map_filename = NULL;
  const char *exe_filename = NULL;

  for (int i = 1; i < argc; i++) {
    if (input_filename) {
      fprintf(stderr, "multiple inputs are not supported: '%s'\n", argv[i]);
      exit(1);
    } else if (strcmp(argv[i], "--dmem") == 0) {
      ARG_FILE(dmem);
    } else if (strcmp(argv[i], "--pmem") == 0) {
      ARG_FILE(pmem);
    } else if (strcmp(argv[i], "--map") == 0) {
      ARG_FILE(map);
    } else if (strcmp(argv[i], "-o") == 0) {
      ARG_FILE(exe);
    } else {
      input_filename = argv[i];
    }
  }

  FILE *input_file = stdin;
  FILE *pmem_file = NULL, *dmem_file = NULL, *map_file = NULL;
  FILE *exe_file = NULL;
  if (input_filename && strcmp(input_filename, "-") != 0) {
    input_file = fopen(input_filename, "r");
  }
  if (pmem_filename) {
    pmem_file = fopen(pmem_filename, "w");
  }
  if (dmem_filename) {
    dmem_file = fopen(dmem_filename, "w");
  }
  if (map_filename) {
    map_file = fopen(map_filename, "w");
  }
  if (exe_filename) {
    exe_file = fopen(exe_filename, "w");
  }

  char line[MAX_LINE];
  struct AsmLine al;

  uint8_t dmem[16 * 1024]; // 16KBytes
  uint32_t pmem[16 * 1024]; // 16KWords
  struct LabelAddr labels[MAX_LABEL];
  int num_labels = 0;
  int dmem_size = 0, num_insn = 0;

  if (fgets(line, MAX_LINE, input_file) != NULL) {
    SplitOpcode(line, &al);
    if (strcmp(al.mnemonic, "section") != 0 ||
        strcmp(al.operands[0], ".data") != 0) {
      fprintf(stderr, "first line must be 'section .data'\n");
      exit(1);
    }
    dmem_size = ProcessDataSection(input_file, map_file, dmem, line, &al,
                                   labels, &num_labels, exe_file != NULL);

    if (strcmp(al.mnemonic, "section") != 0 ||
        strcmp(al.operands[0], ".text") != 0) {
      fprintf(stderr, ".text section must come just after .data\n");
      exit(1);
    }
    num_insn = ProcessTextSection(input_file, map_file, pmem, line, &al,
                                  labels, &num_labels);
  }

  if (dmem_file) {
    for (int i = 0; i < dmem_size; i += 2) {
      fprintf(dmem_file, "%02X%02X\n", dmem[i + 1], dmem[i]);
    }
  }
  if (pmem_file) {
    for (int i = 0; i < num_insn; i++) {
      fprintf(pmem_file, "%05X\n", pmem[i]);
    }
  }

  if (exe_file) {
    // 先頭に pmem_len と dmem_len があることを確かめる
    for (int i = 0; i < 4; ++i) {
      if (dmem[i] != 0) {
        fprintf(stderr, "first 4 bytes of dmem must be zero\n");
        exit(1);
      }
    }
    int has_pl = 0, has_dl = 0;
    for (int i = 0; i < num_labels; ++i) {
      if (strcmp(labels[i].label, "pmem_len") == 0) {
        if (labels[i].addr != 0x00) {
          fprintf(stderr, "pmem_len must be placed at 0x0000\n");
          exit(1);
        }
        has_pl = 1;
      } else if (strcmp(labels[i].label, "dmem_len") == 0) {
        if (labels[i].addr != 0x02) {
          fprintf(stderr, "dmem_len must be placed at 0x0002\n");
          exit(1);
        }
        has_dl = 1;
      }
    }
    if (!has_pl | !has_dl) {
      fprintf(stderr, "exe mode needs pmem_len & dmem_len at the beginning of .data\n");
      exit(1);
    }
    dmem[0] = num_insn & 0xff;
    dmem[1] = (num_insn >> 8) & 0xff;
    dmem[2] = dmem_size & 0xff;
    dmem[3] = (dmem_size >> 8) & 0xff;
    fwrite(dmem, 1, (dmem_size + 511) & 0xfe00, exe_file);
    for (int i = 0; i < num_insn; i++) {
      uint8_t buf[3] = {
        pmem[i] & 0xff,
        (pmem[i] >> 8) & 0xff,
        (pmem[i] >> 16) & 0xff
      };
      fwrite(buf, 1, 3, exe_file);
    }
  }
  return 0;
}
