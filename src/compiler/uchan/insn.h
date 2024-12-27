#pragma once

#include <stdio.h>
#include "token.h"

#define MAX_OPERANDS 3

enum LabelKind {
  kLabelToken, // 関数名など、コンパイル対象コードに現れるラベル
  kLabelAutoL, // 自動生成されたジャンプ用ラベル
  kLabelAutoS, // 自動生成された文字列リテラル用ラベル
  kLabelStr,   // 文字列リテラルで表現されるラベル
  kLabelCase,  // switch-case の case
};

struct Label {
  enum LabelKind kind;
  union {
    struct Token *token;
    int label_no;
    const char *label_str;
    struct {
      int label_no;
      int case_val;
    } case_;
  };
};

void PrintLabel(FILE *out, struct Label *label);

enum InsnKind {
  kInsnNormal, // 通常のアセンブラ命令
  kInsnInterp, // アセンブラ内インタープリター命令
  kInsnInterpResult, // インタープリターの結果を取得
};

enum OperandKind {
  kOprNone, // オペランド末尾を示すマーカー
  kOprInt,
  kOprLabel,
  kOprReg,
  kOprBaseOff,
  kOprBaseLabel,
};

struct Operand {
  enum OperandKind kind;
  union {
    int val_int;
    struct Label val_label;
    const char *val_reg;
    struct {
      const char *base;
      int offset;
    } val_base_off;
    struct {
      const char *base;
      struct Label label;
    } val_base_label;
  };
};

struct Instruction {
  enum InsnKind kind;
  const char *opcode;
  struct Operand operands[MAX_OPERANDS];
};

void SetInsnNoOpr(struct Instruction *insn,
                  enum InsnKind kind, const char *opcode);

enum AsmLineKind {
  kAsmLineDeleted,
  kAsmLineInsn,
  kAsmLineLabel,
  kAsmLineIndentedComment,
  kAsmLineRaw,
};

struct AsmLine {
  enum AsmLineKind kind;
  union {
    struct Label label;
    struct Instruction insn;
    char comment[64];
    char raw[64];
  };
};

void OptimizeAsmLines(struct AsmLine *lines, int num_lines);
