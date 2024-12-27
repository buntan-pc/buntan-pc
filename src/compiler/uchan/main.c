#include <assert.h>
#include <ctype.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "insn.h"
#include "symbol.h"
#include "token.h"
#include "type.h"

#define INDENT "\t"
#define MAX_STRING 256
#define MAX_LINE (32*1024/2)
#define MAX_SRC_LEN (128*1024)
#define GVAR_OFFSET 0x100
#define MAX_PATHLEN 256

char *src;
void Locate(char *p) {
  char *start = p, *end = p;

  while (src < start && *(start - 1) != '\n') {
    start--;
  }

  while (*end && *end != '\n') {
    end++;
  }

  fprintf(stderr, "%.*s\n", (int)(end - start), start);
  fprintf(stderr, "%*s\n", (int)(p - start + 1), "^");
}

struct JumpLabels {
  int lbreak;
  int lcontinue;
};

struct GenContext {
  struct Scope *scope;
  int frame_size;
  int num_label;
  int num_strings;
  struct Token *strings[MAX_STRING];
  struct JumpLabels jump_labels;
  int num_line;
  struct AsmLine asm_lines[MAX_LINE];
  int print_ast;
  int is_isr; // 現在コンパイルしている関数は ISR である
};

int GenLabel(struct GenContext *ctx) {
  return ++ctx->num_label; // L_n ラベルは 1 始まり
}

struct AsmLine *GenAsmLine(struct GenContext *ctx, enum AsmLineKind kind) {
  if (ctx->num_line >= MAX_LINE) {
    fprintf(stderr, "too many asm lines\n");
    exit(1);
  }
  struct AsmLine *l = &ctx->asm_lines[ctx->num_line++];
  l->kind = kind;
  return l;
}

struct Instruction *Insn(struct GenContext *ctx, const char *opcode) {
  struct AsmLine *line = GenAsmLine(ctx, kAsmLineInsn);
  struct Instruction *i = &line->insn;
  SetInsnNoOpr(i, kInsnNormal, opcode);
  return i;
}

struct Instruction *InsnInt(struct GenContext *ctx, const char *opcode, int opr1) {
  struct Instruction *i = Insn(ctx, opcode);
  i->operands[0].kind = kOprInt;
  i->operands[0].val_int = opr1;
  return i;
}

struct Instruction *InsnReg(struct GenContext *ctx, const char *opcode, const char *opr1) {
  struct Instruction *i = Insn(ctx, opcode);
  i->operands[0].kind = kOprReg;
  i->operands[0].val_reg = opr1;
  return i;
}

struct Instruction *InsnRegInt(struct GenContext *ctx, const char *opcode, const char *opr1, int opr2) {
  struct Instruction *i = Insn(ctx, opcode);
  i->operands[0].kind = kOprReg;
  i->operands[0].val_reg = opr1;
  i->operands[1].kind = kOprInt;
  i->operands[1].val_int = opr2;
  return i;
}

struct Instruction *InsnBaseOff(struct GenContext *ctx, const char *opcode, const char *base, int off) {
  struct Instruction *i = Insn(ctx, opcode);
  i->operands[0].kind = kOprBaseOff;
  i->operands[0].val_base_off.base = base;
  i->operands[0].val_base_off.offset = off;
  return i;
}

struct Instruction *InsnLabelStr(struct GenContext *ctx, const char *opcode, const char *label) {
  struct Instruction *i = Insn(ctx, opcode);
  i->operands[0].kind = kOprLabel;
  i->operands[0].val_label.kind = kLabelStr;
  i->operands[0].val_label.label_str = label;
  return i;
}

struct Instruction *InsnLabel(struct GenContext *ctx, const char *opcode, struct Label *label) {
  struct Instruction *i = Insn(ctx, opcode);
  i->operands[0].kind = kOprLabel;
  i->operands[0].val_label = *label;
  return i;
}

struct Instruction *InsnLabelToken(struct GenContext *ctx, const char *opcode, struct Token *label) {
  struct Instruction *i = Insn(ctx, opcode);
  i->operands[0].kind = kOprLabel;
  i->operands[0].val_label.kind = kLabelToken;
  i->operands[0].val_label.token = label;
  return i;
}

struct Instruction *InsnLabelAuto(struct GenContext *ctx, const char *opcode, enum LabelKind kind, int label) {
  struct Instruction *i = Insn(ctx, opcode);
  i->operands[0].kind = kOprLabel;
  i->operands[0].val_label.kind = kind;
  i->operands[0].val_label.label_no = label;
  return i;
}

struct Instruction *InsnLabelAutoL(struct GenContext *ctx, const char *opcode, int label) {
  return InsnLabelAuto(ctx, opcode, kLabelAutoL, label);
}

struct Instruction *InsnLabelAutoS(struct GenContext *ctx, const char *opcode, const char *base, int label) {
  struct Instruction *i = Insn(ctx, opcode);
  i->operands[0].kind = kOprBaseLabel;
  i->operands[0].val_base_label.base = base;
  i->operands[0].val_base_label.label.kind = kLabelAutoS;
  i->operands[0].val_base_label.label.label_no = label;
  return i;
}

struct Instruction *InsnLabelCase(struct GenContext *ctx, const char *opcode, int label_no, int case_val) {
  struct Instruction *i = Insn(ctx, opcode);
  i->operands[0].kind = kOprLabel;
  i->operands[0].val_label.kind = kLabelCase;
  i->operands[0].val_label.case_.label_no = label_no;
  i->operands[0].val_label.case_.case_val = case_val;
  return i;
}

struct Label *AddLabel(struct GenContext *ctx) {
  struct AsmLine *line = GenAsmLine(ctx, kAsmLineLabel);
  return &line->label;
}

struct Label *AddLabelToken(struct GenContext *ctx, struct Token *t) {
  struct Label *l = AddLabel(ctx);
  l->kind = kLabelToken;
  l->token = t;
  return l;
}

struct Label *AddLabelAutoL(struct GenContext *ctx, int no) {
  struct Label *l = AddLabel(ctx);
  l->kind = kLabelAutoL;
  l->label_no = no;
  return l;
}

struct Label *AddLabelStr(struct GenContext *ctx, const char *s) {
  struct Label *l = AddLabel(ctx);
  l->kind = kLabelStr;
  l->label_str = s;
  return l;
}

struct Label *AddLabelCase(struct GenContext *ctx, int no, int val) {
  struct Label *l = AddLabel(ctx);
  l->kind = kLabelCase;
  l->case_.label_no = no;
  l->case_.case_val = val;
  return l;
}

#define PRINT_NODE_COMMENT(ctx, n, s) \
  do { \
    if ((ctx)->print_ast) { \
      struct AsmLine *l = GenAsmLine((ctx), kAsmLineIndentedComment); \
      snprintf(l->comment, sizeof(l->comment), \
              "Node[%d]: %s", (n->index), (s)); \
    } \
  } while (0)

enum ValueClass {
  VC_RVAL,
  VC_LVAL,
  VC_NO_NEED,
};

void ConvertToBoolean(struct GenContext *ctx) {
  InsnInt(ctx, "push", 0);
  Insn(ctx, "neq");
}

#define GEN_BOOL   0x01u
#define GEN_INTERP 0x02u

// 可能であればアセンブラ内蔵インタープリター上に値を生成する
//#define REQ_INTERP 0x01u
// 自身が kInsnNormal の命令を発行する場合、その直前の命令を kInsnInterpResult にする
//#define REQ_GET_INTERP_RESULT 0x02u

struct Instruction *GetLastInsn(struct GenContext *ctx) {
  struct AsmLine *last_line = &(ctx)->asm_lines[(ctx)->num_line - 1];
  if (last_line->kind != kAsmLineInsn) {
    fprintf(stderr, "cannot get the last instruction: the last line is not kAsmLineInsn.\n");
    exit(1);
  }
  return &last_line->insn;
}

// 命令が 'ld fp+0' であれば真を返す
int IsLdFp0(struct AsmLine *al) {
  if (al->kind != kAsmLineInsn || strcmp("ld", al->insn.opcode) != 0) {
    return 0;
  }
  struct Operand *operands = al->insn.operands;
  if (operands[0].kind == kOprBaseOff && operands[1].kind == kOprNone &&
      operands[0].val_base_off.base != NULL &&
      strcmp("fp", operands[0].val_base_off.base) == 0 &&
      operands[0].val_base_off.offset == 0) {
    return 1;
  }
  return 0;
}

// 命令が 'add fp,N' であれば真を返す
int IsAddFp(struct AsmLine *al) {
  if (al->kind != kAsmLineInsn || strcmp("add", al->insn.opcode) != 0) {
    return 0;
  }
  struct Operand *operands = al->insn.operands;
  if (operands[0].kind == kOprReg && strcmp("fp", operands[0].val_reg) == 0 &&
      operands[1].kind == kOprInt && operands[2].kind == kOprNone) {
    return 1;
  }
  return 0;
}

// 命令が 'push X+imm' であれば真を返す
int IsPushBaseImm(struct AsmLine *al) {
  if (al->kind != kAsmLineInsn || strcmp("push", al->insn.opcode) != 0) {
    return 0;
  }
  struct Operand *operands = al->insn.operands;
  if (operands[0].kind == kOprBaseOff &&
      operands[1].kind == kOprNone) {
    return 1;
  }
  return 0;
}

// 最終行が push X+imm なら ld/st X+imm に置き換える。そうでなければ lda/sta を発行する。
// store: 0=ld, 1=st
// byt: 0=word, 1=byte
// pop: 1=スタックにデータを残さない
void EmitOptimalLdSt(struct GenContext *ctx, int store, int byt, int pop) {
  struct AsmLine *last_line = &ctx->asm_lines[ctx->num_line - 1];
  if (IsPushBaseImm(last_line)) {
    if (store) {
      last_line->insn.opcode = byt ? "st1" : "st";
    } else {
      last_line->insn.opcode = byt ? "ld1" : "ld";
      if (pop) {
        Insn(ctx, "pop");
      }
    }
  } else {
    Insn(ctx, store ? (byt ? "std1" : "std") : (byt ? "ldd1" : "ldd"));
    if (pop) {
      Insn(ctx, "pop");
    }
  }
}

unsigned Generate(struct GenContext *ctx, struct Node *node, enum ValueClass value_class, int req_onstack);

const char *GetBaseReg(struct Symbol *sym) {
  return sym->kind == kSymLVar ? "fp"
       : (sym->attr & SYM_ATTR_FIXED_ADDR) ? NULL
       : "gp";
}

void GenVarInit(struct GenContext *ctx, struct Symbol *var) {
  struct Node *init = var->def->rhs;
  if (init == NULL) {
    return;
  }

  const char *base = GetBaseReg(var);
  const char *insn = "st";
  if ((var->type->kind == kTypeArray && SizeofType(var->type->base) == 1) ||
      (SizeofType(var->type) == 1)) {
    insn = "st1";
  }

  if (init->kind != kNodeIList) {
    Generate(ctx, init, VC_RVAL, 1);
    InsnBaseOff(ctx, insn, base, var->offset);
  } else {
    assert(var->type->kind == kTypeArray);
    struct Node *n = init->rhs;
    uint16_t offset = var->offset;
    int inc = SizeofType(var->type->base);
    while (n) {
      Generate(ctx, n, VC_RVAL, 1);
      InsnBaseOff(ctx, insn, base, offset);
      offset += inc;
      n = n->next;
    }
  }
}

void GenNodeId(struct GenContext *ctx, struct Symbol *sym, enum ValueClass value_class) {
  assert(sym->type);
  const char *base = GetBaseReg(sym);

  if (sym->type->kind == kTypeArray) {
    if (value_class == VC_LVAL) {
      fprintf(stderr, "array itself cannot be l-value: %.*s\n",
              sym->name->len, sym->name->raw);
      exit(1);
    }
    InsnBaseOff(ctx, "push", base, sym->offset);
  } else {
    if (value_class == VC_LVAL) {
      InsnBaseOff(ctx, "push", base, sym->offset);
    } else {
      // value_class == VC_NO_NEED だとしても ld 命令は発効する。
      // 読み込みの副作用のあるメモリマップトレジスタの可能性がある。
      if (SizeofType(sym->type) == 1) {
        InsnBaseOff(ctx, "ld1", base, sym->offset);
      } else {
        InsnBaseOff(ctx, "ld", base, sym->offset);
      }
    }
  }
}

// 生成された値が boolean value なら true、それ以外は false を返す。
// req_onstack: 演算スタック上に値が必要（インタプリタ上ではダメ）であれば 1
unsigned Generate(struct GenContext *ctx, struct Node *node, enum ValueClass value_class, int req_onstack) {
  //if (node->has_const_value) {
  //  InsnInt(ctx, "push", node->const_value);
  //  return node->const_value == 0 || node->const_value == 1;
  //}

  unsigned gen_result = 0;
  unsigned lhs_res, rhs_res;
  int lhs_is_uint = node->lhs && node->lhs->type && IS_UNSIGNED_INT(node->lhs->type);
  int rhs_is_uint = node->rhs && node->rhs->type && IS_UNSIGNED_INT(node->rhs->type);

  // arr[imm] または ptr[imm] の最適化
  if (node->kind == kNodeAdd &&
      node->lhs->kind == kNodeId && (
        node->lhs->type->kind == kTypeArray ||
        node->lhs->type->kind == kTypePtr) &&
      node->rhs->kind == kNodeInteger) {
    struct Symbol *sym = FindSymbol(ctx->scope, node->lhs->token);
    if (!sym) {
      fprintf(stderr, "unknown symbol\n");
      Locate(node->token->raw);
      exit(1);
    }

    size_t element_size = SizeofType(node->lhs->type->base);
    int index = node->rhs->token->value.as_int;
    if (node->lhs->type->kind == kTypeArray) {
      uint16_t offset = sym->offset + element_size * index;
      if ((offset & 0xf000) == 0) {
        InsnBaseOff(ctx, "push", GetBaseReg(sym), offset);
        return gen_result;
      }
      // オフセットが 12 ビットに収まらない場合は最適化を諦める
    } else { // kTypePtr
      Generate(ctx, node->lhs, VC_RVAL, 1);
      InsnInt(ctx, "push", element_size * index);
      Insn(ctx, "add");
      return gen_result;
    }
  }

  if (100 <= node->kind && node->kind < 200) { // standard binary expression
    struct Instruction *lhs_insn, *rhs_insn;

    switch (value_class) {
    case VC_RVAL:
      lhs_res = Generate(ctx, node->lhs, VC_RVAL, 0);
      if (node->lhs->type->kind == kTypeChar &&
          (node->lhs->type->attr & TYPE_ATTR_SIGNED) != 0) { // signed char
        Insn(ctx, "exts")->kind = GetLastInsn(ctx)->kind;
      }
      lhs_insn = GetLastInsn(ctx);

      rhs_res = Generate(ctx, node->rhs, VC_RVAL, 0);
      if (node->rhs->type->kind == kTypeChar &&
          (node->rhs->type->attr & TYPE_ATTR_SIGNED) != 0) { // signed char
        Insn(ctx, "exts")->kind = GetLastInsn(ctx)->kind;
      }
      rhs_insn = GetLastInsn(ctx);

      gen_result |= (lhs_res & rhs_res & GEN_INTERP);
      if ((lhs_res & GEN_INTERP) && !(rhs_res & GEN_INTERP)) {
        assert(lhs_insn->kind == kInsnInterp);
        lhs_insn->kind = kInsnInterpResult;
      } else if (!(lhs_res & GEN_INTERP) && (rhs_res & GEN_INTERP)) {
        assert(rhs_insn->kind == kInsnInterp);
        rhs_insn->kind = kInsnInterpResult;
      }
      break;
    case VC_LVAL:
      fprintf(stderr, "standard binary expression cannot be an lvalue\n");
      Locate(node->token->raw);
      exit(1);
    case VC_NO_NEED:
      node->type = NewType(kTypeVoid);
      Generate(ctx, node->lhs, VC_NO_NEED, 0);
      Generate(ctx, node->rhs, VC_NO_NEED, 0);
      return 0;
    }
  }

  if ((kNodeLT <= node->kind && node->kind <= kNodeNEq)
      || node->kind == kNodeLAnd || node->kind == kNodeLOr) {
    gen_result |= GEN_BOOL;
  }

  switch (node->kind) {
  case kNodeInteger:
    if (value_class == VC_RVAL) {
      if (req_onstack) {
        InsnInt(ctx, "push", node->token->value.as_int & 0xffff);
      } else {
        InsnInt(ctx, "push", node->token->value.as_int)->kind = kInsnInterp;
        gen_result |= GEN_INTERP;
      }
    }
    break;
  case kNodeId:
    {
      struct Symbol *sym = FindSymbol(ctx->scope, node->token);
      if (!sym) {
        fprintf(stderr, "unknown symbol\n");
        Locate(node->token->raw);
        exit(1);
      }
      switch (sym->kind) {
      case kSymHead:
        fprintf(stderr, "this must not happen");
        exit(1);
        break;
      case kSymGVar:
        GenNodeId(ctx, sym, value_class);
        break;
      case kSymLVar:
        if (value_class == VC_NO_NEED) {
          node->type = NewType(kTypeVoid);
          break;
        }
        GenNodeId(ctx, sym, value_class);
        break;
      case kSymFunc:
        if (value_class == VC_NO_NEED) {
          node->type = NewType(kTypeVoid);
          break;
        }
        InsnLabelToken(ctx, "push", sym->name);
        break;
      case kSymBif:
        fprintf(stderr, "cannot take address of a built-in function\n");
        Locate(node->token->raw);
        exit(1);
        break;
      }
      if (value_class == VC_NO_NEED) {
        node->type = NewType(kTypeVoid);
        Insn(ctx, "pop");
      }
    }
    break;
  case kNodeString:
    if (value_class != VC_NO_NEED) {
      if (ctx->num_strings == MAX_STRING) {
        fprintf(stderr, "too many string constants\n");
        Locate(node->token->raw);
        exit(1);
      }
      ctx->strings[ctx->num_strings] = node->token;
      InsnLabelAutoS(ctx, "push", "gp", ctx->num_strings);
      ctx->num_strings++;
    }
    break;
  case kNodeInc:
  case kNodeDec:
    if (node->lhs) { // 後置インクリメント 'exp ++'
      PRINT_NODE_COMMENT(ctx, node, "Inc/Dec (postfix))");
      Generate(ctx, node->lhs, VC_RVAL, 1);
      if (value_class == VC_NO_NEED) {
        InsnInt(ctx, "push", 1);
        node->type = NewType(kTypeVoid);
      } else {
        InsnInt(ctx, "dup", 0);
        InsnInt(ctx, "push", 1);
      }
      Insn(ctx, node->kind == kNodeInc ? "add" : "sub");
      Generate(ctx, node->lhs, VC_LVAL, 1);
      EmitOptimalLdSt(ctx, 1, SizeofType(node->lhs->type) == 1, 1);
    } else { // 前置インクリメント '++ exp'
      PRINT_NODE_COMMENT(ctx, node, "Inc/Dec (prefix))");
      Generate(ctx, node->rhs, VC_RVAL, 1);
      InsnInt(ctx, "push", 1);
      Insn(ctx, node->kind == kNodeInc ? "add" : "sub");
      Generate(ctx, node->rhs, VC_LVAL, 1);
      int byt = SizeofType(node->rhs->type) == 1;
      if (value_class == VC_NO_NEED) {
        node->type = NewType(kTypeVoid);
        EmitOptimalLdSt(ctx, 1, byt, 1);
      } else {
        Insn(ctx, byt ? "std1" : "std");
      }
    }
    break;
  case kNodeRef:
    Generate(ctx, node->rhs, value_class == VC_NO_NEED ? VC_NO_NEED : VC_LVAL, 1);
    break;
  case kNodeDeref:
    {
      Generate(ctx, node->rhs, VC_RVAL, 1);
      int byt = SizeofType(node->rhs->type->base) == 1;
      if (value_class == VC_RVAL) {
        EmitOptimalLdSt(ctx, 0, byt, 0);
      } else if (value_class == VC_NO_NEED) {
        node->type = NewType(kTypeVoid);
        EmitOptimalLdSt(ctx, 0, byt, 1);
      }
    }
    break;
  case kNodeNot:
    PRINT_NODE_COMMENT(ctx, node, "Not");
    if (value_class == VC_NO_NEED) {
      Generate(ctx, node->rhs, VC_NO_NEED, 0);
    } else {
      rhs_res = Generate(ctx, node->rhs, VC_RVAL, 0);
      Insn(ctx, "not")->kind = kInsnInterp;
      if (!(rhs_res & GEN_INTERP)) {
        GetLastInsn(ctx)->kind = kInsnNormal;
      } else if (req_onstack) {
        GetLastInsn(ctx)->kind = kInsnInterpResult;
      } else {
        gen_result |= GEN_INTERP;
      }
    }
    break;
  case kNodeCall:
    {
      PRINT_NODE_COMMENT(ctx, node, "Call");

      struct Node *args[10];
      int num_args = 0;
      for (struct Node *arg = node->rhs; arg; arg = arg->next) {
        if (num_args >= 10) {
          fprintf(stderr, "the number of arguments must be < 10\n");
          Locate(arg->token->raw);
          exit(1);
        }
        args[num_args++] = arg;
      }
      for (int i = 0; i < num_args; i++) {
        Generate(ctx, args[num_args - 1 - i], VC_RVAL, 1);
      }
      if (node->lhs->kind == kNodeId) {
        struct Symbol *sym = FindSymbol(ctx->scope, node->lhs->token);
        int is_bif = sym && sym->kind == kSymBif;
        if (is_bif && strcmp(sym->name->raw, "__builtin_set_gp") == 0) {
          InsnReg(ctx, "pop", "gp");
        } else if (is_bif && strcmp(sym->name->raw, "__builtin_write_pmem") == 0) {
          Insn(ctx, "spha");
          Insn(ctx, "spla");
        } else if (is_bif && strcmp(sym->name->raw, "__builtin_set_isr") == 0) {
          InsnReg(ctx, "pop", "isr");
        } else if (sym && (sym->kind == kSymGVar || sym->kind == kSymLVar) && sym->type->kind == kTypePtr) {
          Generate(ctx, node->lhs, VC_RVAL, 1);
          Insn(ctx, "call");
        } else {
          InsnLabelToken(ctx, "call", node->lhs->token);
        }
      } else {
        fprintf(stderr, "not implemented call of non-id expression\n");
        Locate(node->lhs->token->raw);
        exit(1);
      }
      if (value_class == VC_NO_NEED) {
        Insn(ctx, "pop");
      }
    }
    break;
  case kNodeCast:
    if (value_class == VC_NO_NEED) {
      Generate(ctx, node->rhs, VC_NO_NEED, 1);
      node->type = NewType(kTypeVoid);
    } else {
      Generate(ctx, node->rhs, value_class, 1);
    }
    break;
  case kNodeVoid:
    break;
  case kNodeLAnd:
    {
      int label_false = GenLabel(ctx);
      int label_end = GenLabel(ctx);
      PRINT_NODE_COMMENT(ctx, node, "LAnd (eval lhs)");
      if (!(Generate(ctx, node->lhs, VC_RVAL, 1) & GEN_BOOL)) {
        ConvertToBoolean(ctx);
      }
      InsnLabelAutoL(ctx, "jz", label_false);
      PRINT_NODE_COMMENT(ctx, node, "LAnd (eval rhs)");
      if (!(Generate(ctx, node->rhs, VC_RVAL, 1) & GEN_BOOL)) {
        ConvertToBoolean(ctx);
      }
      InsnLabelAutoL(ctx, "jz", label_false);
      PRINT_NODE_COMMENT(ctx, node, "LAnd (true)");
      if (value_class != VC_NO_NEED) {
        InsnInt(ctx, "push", 1);
        InsnLabelAutoL(ctx, "jmp", label_end);
      }
      AddLabelAutoL(ctx, label_false);
      if (value_class != VC_NO_NEED) {
        InsnInt(ctx, "push", 0);
        AddLabelAutoL(ctx, label_end);
      }
    }
    break;
  case kNodeLOr:
    {
      int label_true = GenLabel(ctx);
      int label_end = GenLabel(ctx);
      PRINT_NODE_COMMENT(ctx, node, "LOr (eval lhs)");
      Generate(ctx, node->lhs, VC_RVAL, 1);
      Insn(ctx, "not");
      InsnLabelAutoL(ctx, "jz", label_true);
      PRINT_NODE_COMMENT(ctx, node, "LOr (eval rhs)");
      Generate(ctx, node->rhs, VC_RVAL, 1);
      Insn(ctx, "not");
      InsnLabelAutoL(ctx, "jz", label_true);
      PRINT_NODE_COMMENT(ctx, node, "LOr (false)");
      if (value_class != VC_NO_NEED) {
        InsnInt(ctx, "push", 0);
        InsnLabelAutoL(ctx, "jmp", label_end);
      }
      AddLabelAutoL(ctx, label_true);
      if (value_class != VC_NO_NEED) {
        InsnInt(ctx, "push", 1);
        AddLabelAutoL(ctx, label_end);
      }
    }
    break;
  case kNodeAssign:
    PRINT_NODE_COMMENT(ctx, node, "Assign");
    Generate(ctx, node->rhs, VC_RVAL, 1);
    Generate(ctx, node->lhs, VC_LVAL, 1);
    {
      int byt = SizeofType(node->lhs->type) == 1;
      if (value_class == VC_LVAL) {
        Insn(ctx, byt ? "sta1" : "sta");
      } else if (value_class == VC_RVAL) {
        Insn(ctx, byt ? "std1" : "std");
      } else { // VC_NO_NEED
        EmitOptimalLdSt(ctx, 1, byt, 1);
      }
    }
    break;

  // standard binary expression
  case kNodeAdd:
    PRINT_NODE_COMMENT(ctx, node, "Add");
    if (node->lhs->type->kind == kTypePtr ||
        node->lhs->type->kind == kTypeArray) {
      size_t element_size = SizeofType(node->lhs->type->base);
      if (element_size > 1) {
        InsnInt(ctx, "push", (int)element_size);
        Insn(ctx, "mul");
      }
    }
    Insn(ctx, "add");
    break;
  case kNodeSub:
    PRINT_NODE_COMMENT(ctx, node, "Sub");
    Insn(ctx, "sub");
    break;
  case kNodeMul:
    PRINT_NODE_COMMENT(ctx, node, "Mul");
    Insn(ctx, "mul");
    break;
  case kNodeLT:
    PRINT_NODE_COMMENT(ctx, node, "LT");
    if (lhs_is_uint || rhs_is_uint) {
      Insn(ctx, "bt");
    } else {
      Insn(ctx, "lt");
    }
    break;
  case kNodeLE:
    PRINT_NODE_COMMENT(ctx, node, "LE");
    if (lhs_is_uint || rhs_is_uint) {
      Insn(ctx, "be");
    } else {
      Insn(ctx, "le");
    }
    break;
  case kNodeEq:
    PRINT_NODE_COMMENT(ctx, node, "Eq");
    Insn(ctx, "eq");
    break;
  case kNodeNEq:
    PRINT_NODE_COMMENT(ctx, node, "NEq");
    Insn(ctx, "neq");
    break;
  case kNodeAnd:
    PRINT_NODE_COMMENT(ctx, node, "And");
    Insn(ctx, "and");
    break;
  case kNodeOr:
    PRINT_NODE_COMMENT(ctx, node, "Or");
    Insn(ctx, "or");
    break;
  case kNodeXor:
    PRINT_NODE_COMMENT(ctx, node, "Xor");
    Insn(ctx, "xor");
    break;
  case kNodeRShift:
  case kNodeLShift:
    PRINT_NODE_COMMENT(ctx, node, "R/LShift");
    if (node->kind == kNodeLShift) {
      Insn(ctx, "shl");
    } else if (node->lhs->type->attr & TYPE_ATTR_SIGNED) {
      Insn(ctx, "sar");
    } else { // unsigned shift
      Insn(ctx, "shr");
    }
    break;
  // end of standard binary expression

  case kNodeDefFunc:
    {
      ctx->scope = node->scope;
      ctx->frame_size = node->scope->var_offset;

      struct Token *func_name = node->token;
      AddLabelToken(ctx, func_name);

      ctx->is_isr = 0;
      if (strncmp(func_name->raw, "_ISR", 4) == 0) {
        ctx->is_isr = 1;
      }

      int line_add_fp = ctx->num_line;
      // add fp のオペランド値は、関数定義の処理後に上書きされる
      InsnRegInt(ctx, "add", "fp", 0 /* ダミーの値 */);

      int num_param = 0;
      for (struct Node *param = node->cond; param; param = param->next) {
        struct Symbol *sym = FindSymbol(ctx->scope, param->lhs->token);
        InsnBaseOff(ctx, "st", "fp", sym->offset);
        ++num_param;
      }

      Generate(ctx, node->rhs, VC_RVAL, 1);
      int line_body_last = ctx->num_line;

      // 引数が 1 つしかなく、その引数が 1 回しか評価されない場合の最適化
      if (num_param == 1 && IsLdFp0(ctx->asm_lines + line_add_fp + 2)) {
        // add fp,-2; st fp+0; ld fp+0 という流れである
        int num_ld = 0; // 2 行目以降で ld fp+0 が登場する回数
        for (int line = line_add_fp + 3; line < line_body_last; ++line) {
          if (IsLdFp0(ctx->asm_lines + line)) {
            ++num_ld;
          }
        }
        if (num_ld == 0) {
          // st fp+0, ld fp+0 の対を削除
          ctx->asm_lines[line_add_fp + 1].kind = kAsmLineDeleted;
          ctx->asm_lines[line_add_fp + 2].kind = kAsmLineDeleted;
          ctx->frame_size -= 2;
        }
      }

      struct AsmLine *add_fp = &ctx->asm_lines[line_add_fp];
      if (ctx->frame_size > 0) {
        add_fp->insn.operands[1].val_int = -ctx->frame_size;
        for (int line = line_add_fp + 1; line < line_body_last; ++line) {
          if (IsAddFp(ctx->asm_lines + line)) {
            ctx->asm_lines[line].insn.operands[1].val_int = ctx->frame_size;
          }
        }
      } else {
        add_fp->kind = kAsmLineDeleted;
        for (int line = line_add_fp + 1; line < line_body_last; ++line) {
          if (IsAddFp(ctx->asm_lines + line)) {
            ctx->asm_lines[line].kind = kAsmLineDeleted;
          }
        }
      }
    }
    break;
  case kNodeBlock:
    ctx->scope = node->scope;
    if (ctx->frame_size < node->scope->var_offset) {
      ctx->frame_size = node->scope->var_offset;
    }
    for (struct Node *n = node->rhs; n; n = n->next) {
      Generate(ctx, n, VC_NO_NEED, 1);
    }
    ctx->scope = node->scope->parent;
    break;
  case kNodeReturn:
    if (node->lhs) {
      Generate(ctx, node->lhs, VC_RVAL, 1);
    }
    if (ctx->frame_size > 0) {
      InsnRegInt(ctx, "add", "fp", 0 /* ダミーの値 */);
    }
    Insn(ctx, ctx->is_isr ? "iret" : "ret");
    break;
  case kNodeDefVar:
    {
      int is_global = ctx->scope->parent == NULL;
      struct Symbol *sym = FindSymbol(ctx->scope, node->lhs->token);
      assert(sym->type);

      if (is_global) {
        assert(sym->offset != 0);
      } else {
        GenVarInit(ctx, sym);
      }
    }
    break;
  case kNodeIf:
    {
      int label_else = node->rhs ? GenLabel(ctx) : -1;
      int label_end = GenLabel(ctx);
      PRINT_NODE_COMMENT(ctx, node, "If (cond eval)");
      if (!(Generate(ctx, node->cond, VC_RVAL, 1) & GEN_BOOL)) {
        PRINT_NODE_COMMENT(ctx, node, "If (cond to bool)");
        ConvertToBoolean(ctx);
      }
      InsnLabelAutoL(ctx, "jz", node->rhs ? label_else : label_end);
      PRINT_NODE_COMMENT(ctx, node, "If (then)");
      Generate(ctx, node->lhs, VC_RVAL, 1);
      if (node->rhs) {
        InsnLabelAutoL(ctx, "jmp", label_end);
        PRINT_NODE_COMMENT(ctx, node, "If (else)");
        AddLabelAutoL(ctx, label_else);
        Generate(ctx, node->rhs, VC_RVAL, 1);
      }
      PRINT_NODE_COMMENT(ctx, node, "If (end)");
      AddLabelAutoL(ctx, label_end);
    }
    break;
  case kNodeSwitch:
    {
      struct Node *stmt = node->rhs->rhs;
      int label_case = GenLabel(ctx);
      int label_end = GenLabel(ctx);

      struct JumpLabels old_labels = ctx->jump_labels;
      ctx->jump_labels.lbreak = label_end;

      int no_default = 1;
      while (stmt) {
        if (stmt->kind == kNodeCase) {
          if (stmt->cond) {
            Generate(ctx, node->cond, VC_RVAL, 1);
            Generate(ctx, stmt->cond, VC_RVAL, 1);
            Insn(ctx, "neq");
            InsnLabelCase(ctx, "jz", label_case, stmt->cond->token->value.as_int);
          } else { // default ラベル
            InsnLabelAutoL(ctx, "jmp", label_end);
            no_default = 0;
          }
        }
        stmt = stmt->next;
      }
      if (no_default) {
        InsnLabelAutoL(ctx, "jmp", label_end);
      }

      stmt = node->rhs->rhs;
      while (stmt) {
        if (stmt->kind == kNodeCase) {
          if (stmt->cond) {
            AddLabelCase(ctx, label_case, stmt->cond->token->value.as_int);
          } else { // default ラベル
            AddLabelAutoL(ctx, label_end);
          }
        } else {
          Generate(ctx, stmt, VC_NO_NEED, 1);
        }
        stmt = stmt->next;
      }
      if (no_default) {
        AddLabelAutoL(ctx, label_end);
      }

      ctx->jump_labels = old_labels;
    }
    break;
  case kNodeCase:
    break;
  case kNodeFor:
    {
      int label_cond = GenLabel(ctx);
      int label_end = GenLabel(ctx);
      int label_next = GenLabel(ctx);

      struct JumpLabels old_labels = ctx->jump_labels;
      ctx->jump_labels.lbreak = label_end;
      ctx->jump_labels.lcontinue = label_next;

      PRINT_NODE_COMMENT(ctx, node, "For (eval init)");
      Generate(ctx, node->lhs, VC_RVAL, 1);
      AddLabelAutoL(ctx, label_cond);
      if (!(Generate(ctx, node->cond, VC_RVAL, 1) & GEN_BOOL)) {
        ConvertToBoolean(ctx);
      }
      InsnLabelAutoL(ctx, "jz", label_end);
      Generate(ctx, node->rhs, VC_RVAL, 1);
      AddLabelAutoL(ctx, label_next);
      Generate(ctx, node->lhs->next, VC_RVAL, 1);
      InsnLabelAutoL(ctx, "jmp", label_cond);
      PRINT_NODE_COMMENT(ctx, node, "For (end)");
      AddLabelAutoL(ctx, label_end);

      ctx->jump_labels = old_labels;
    }
    break;
  case kNodeWhile:
    {
      int label_cond = GenLabel(ctx);
      int label_end = GenLabel(ctx);

      struct JumpLabels old_labels = ctx->jump_labels;
      ctx->jump_labels.lbreak = label_end;
      ctx->jump_labels.lcontinue = label_cond;

      PRINT_NODE_COMMENT(ctx, node, "While");
      AddLabelAutoL(ctx, label_cond);
      if (!(Generate(ctx, node->cond, VC_RVAL, 1) & GEN_BOOL)) {
        ConvertToBoolean(ctx);
      }
      InsnLabelAutoL(ctx, "jz", label_end);
      Generate(ctx, node->rhs, VC_RVAL, 1);
      InsnLabelAutoL(ctx, "jmp", label_cond);
      PRINT_NODE_COMMENT(ctx, node, "While (end)");
      AddLabelAutoL(ctx, label_end);

      ctx->jump_labels = old_labels;
    }
    break;
  case kNodeBreak:
    InsnLabelAutoL(ctx, "jmp", ctx->jump_labels.lbreak);
    break;
  case kNodeContinue:
    InsnLabelAutoL(ctx, "jmp", ctx->jump_labels.lcontinue);
    break;
  case kNodeTypeSpec:
  case kNodePList:
  case kNodeIList:
    break;
  case kNodeAsm:
    {
      struct AsmLine *l = GenAsmLine(ctx, kAsmLineRaw);
      l->raw[0] = INDENT[0];
      int len = 1;
      struct Token *t = node->lhs->token;
      while (t->kind == kTokenString) {
        len += DecodeStringLiteral(l->raw + len, sizeof(l->raw) - len, t);
        t = t->next;
      }
    }
    break;
  }

  if (100 <= node->kind && node->kind < 200) { // standard binary expression
    if (gen_result & GEN_INTERP) {
      if (req_onstack) {
        GetLastInsn(ctx)->kind = kInsnInterpResult;
      } else {
        GetLastInsn(ctx)->kind = kInsnInterp;
      }
    }
  }

  return gen_result;
}

struct Symbol *AppendBifSymbol(struct Symbol *head, const char *id, struct Type *ret_type) {
  struct Token *tk = NewToken(kTokenId, strdup(id), strlen(id));
  struct Symbol *sym = AppendSymbol(head, NewSymbol(kSymBif, tk));
  sym->def = NewNode(kNodeDefFunc, tk);
  sym->def->lhs = NewNode(kNodeTypeSpec, tk);
  sym->def->lhs->type = ret_type;
  return sym;
}

struct Symbol *make_builtin_syms() {
  struct Symbol *builtin_syms = NewSymbol(kSymHead, NULL);
  struct Symbol *sym = builtin_syms;

  struct Type *type_uint = NewType(kTypeInt);
  type_uint->attr &= ~TYPE_ATTR_SIGNED;

  sym = AppendBifSymbol(sym, "__builtin_set_gp", NewType(kTypeVoid));
  sym = AppendBifSymbol(sym, "__builtin_write_pmem", type_uint);
  sym = AppendBifSymbol(sym, "__builtin_set_isr", NewType(kTypeVoid));

  return builtin_syms;
}

char *PreProc(FILE *input_file, const char *input_dirname) {
  char *src = malloc(MAX_SRC_LEN);
  size_t src_len = 0, src_free_len = MAX_SRC_LEN;
  long input_prev_pos = 0;

  while (fgets(src + src_len, src_free_len, input_file) != NULL) {
    if (src[src_len] == '#') { // pre-process
      char *p = src + src_len + 1;
      while (*p && isspace(*p)) {
        ++p;
      }
      if (strncmp(p, "include", 7) == 0) {
        p += 7;
        while (*p && isspace(*p)) {
          ++p;
        }
        if (*p == '<') {
          fprintf(stderr, "not implemented #include <...>\n");
          Locate(p);
          exit(1);
        } else if (*p == '"') {
          char *q = p + 1;
          while (*q && *q != '"') {
            ++q;
          }
          if (*q == '\0') {
            fprintf(stderr, "no closing '\"'\n");
            Locate(p);
            exit(1);
          } else { // *q == '"'
            *q = '\0';
            size_t dirname_len = strlen(input_dirname);
            char hpath[MAX_PATHLEN];
            strcpy(hpath, input_dirname);
            strcat(hpath, "/");
            strncat(hpath, p + 1, MAX_PATHLEN - dirname_len - 2);
            FILE *hfile = fopen(hpath, "r");
            if (hfile == NULL) {
              fprintf(stderr, "no such file: '%s'\n", p + 1);
              Locate(p + 1);
              exit(1);
            }
            size_t hlen = fread(src + src_len, 1, src_free_len - 1, hfile);
            src_len += hlen;
          }
        } else {
          fprintf(stderr, "'\"' is expected after #include\n");
          Locate(p);
          exit(1);
        }
      } else {
        fprintf(stderr, "not implemented pre-process directive\n");
        Locate(p);
        exit(1);
      }
    } else { // normal line
      long read_len = ftell(input_file) - input_prev_pos;
      src_len += read_len;
      src_free_len -= read_len;
    }

    input_prev_pos = ftell(input_file);
  }
  src[src_len] = '\0';
  return src;
}

int main(int argc, char **argv) {
  int print_ast = 0;
  const char *input_filename = NULL;
  const char *output_filename = "a.s";
  char input_dirname[MAX_PATHLEN] = ".";

  int ret_from_start = 0; // 真なら start 関数は return する
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--ast") == 0) {
      print_ast = 1;
    } else if (strcmp(argv[i], "-o") == 0) {
      i++;
      if (i >= argc) {
        fprintf(stderr, "output file name is not specified\n");
        exit(1);
      }
      output_filename = argv[i];
    } else if (input_filename) {
      fprintf(stderr, "multiple inputs are not supported: '%s'\n", argv[i]);
      exit(1);
    } else if (strcmp(argv[i], "--ret-from-start") == 0) {
      ret_from_start = 1;
    } else {
      input_filename = argv[i];
    }
  }

  if (input_filename == NULL) {
    fprintf(stderr, "no input file\n");
    exit(1);
  }

  FILE *input_file = stdin;
  FILE *output_file = stdout;
  if (strcmp(input_filename, "-") != 0) {
    input_file = fopen(input_filename, "r");
    if (input_file == NULL) {
      fprintf(stderr, "no such file: '%s'\n", input_filename);
      exit(1);
    }
    strncpy(input_dirname, input_filename, MAX_PATHLEN - 1);
    input_dirname[MAX_PATHLEN - 1] = '\0';
    strcpy(input_dirname, dirname(input_dirname));
  }

  src = PreProc(input_file, input_dirname);
  if (input_file != stdin) {
    fclose(input_file);
  }

  struct Scope *global_scope = NewGlobalScope(make_builtin_syms());
  struct ParseContext parse_ctx = {
    global_scope,
    kPPInit,
  };

  Tokenize(src);
  struct Node *ast = Program(&parse_ctx);
  Expect(kTokenEOF);

  if (print_ast) {
    char ast_filename[256];
    if (strcmp(input_filename, "-") == 0) {
      strcpy(ast_filename, "stdin.ast");
    } else {
      snprintf(ast_filename, sizeof(ast_filename), "%s.ast", input_filename);
    }
    FILE *ast_file = fopen(ast_filename, "w");
    for (struct Node *n = ast; n; n = n->next) {
      PrintNode(ast_file, n, 0, NULL);
    }
    fclose(ast_file);
  }

  struct GenContext gen_ctx = {
    parse_ctx.scope,
    0, 0, 0, {}, {-1, -1}, 0, {}, print_ast, 0
  };
  AddLabelStr(&gen_ctx, "start");

  if (strcmp(output_filename, "-") != 0) {
    output_file = fopen(output_filename, "w");
  }
  fprintf(output_file, "section .data\n");
  int gvar_offset = 0;
  for (struct Symbol *sym = global_scope->syms->next; sym; sym = sym->next) {
    if (sym->kind == kSymGVar && (sym->attr & SYM_ATTR_FIXED_ADDR) == 0) {
      // at 属性で配置アドレスを決めたもの（SYM_ATTR_FIXED_ADDR）は無視

      int mem_size = (SizeofType(sym->type) + 1) & 0xfffe;
      gvar_offset += mem_size;

      fprintf(output_file, "%.*s:", sym->name->len, sym->name->raw);
      if (sym->def->rhs == NULL || sym->def->rhs->kind != kNodeInteger) {
        for (int i = 0; i < mem_size; i++) {
          if ((i & 0xf) == 0) {
            fprintf(output_file, "\n" INDENT "db 0");
          } else {
            fprintf(output_file, ",0");
          }
        }
        GenVarInit(&gen_ctx, sym);
      } else { // kind == kNodeInteger
        int val = sym->def->rhs->token->value.as_int;
        fprintf(output_file, "\n" INDENT "db %d,%d", val & 0xff, (val >> 8) & 0xff);
      }
      fprintf(output_file, "\n");
    }
  }

  if (ret_from_start) {
    InsnLabelStr(&gen_ctx, "call", "main");
    Insn(&gen_ctx, "ret");
  } else {
    InsnLabelStr(&gen_ctx, "call", "main");
    InsnBaseOff(&gen_ctx, "st", NULL, 0x06); // UART へ出力
    AddLabelStr(&gen_ctx, "fin");
    InsnLabelStr(&gen_ctx, "jmp", "fin");
  }

  for (struct Node *n = ast; n; n = n->next) {
    if (n->kind == kNodeDefFunc) {
      Generate(&gen_ctx, n, VC_NO_NEED, 0);
    }
  }
  //insn_add_fp->operands[1].val_int = global_scope->var_offset;

  for (int i = 0; i < gen_ctx.num_strings; i++) {
    struct Token *tk_str = gen_ctx.strings[i];
    char buf[256];
    fprintf(output_file, "STR_%d: \n" INDENT "db ", i);

    while (tk_str->kind == kTokenString) {
      int len = DecodeStringLiteral(buf, sizeof(buf), tk_str);
      for (int i = 0; i < len; i++) {
        fprintf(output_file, "0x%02x,", buf[i] & 0xff);
      }
      tk_str = tk_str->next;
    }
    fprintf(output_file, "0\n");
  }

  fprintf(output_file, "\nsection .text\n");

  for (int i = 0; i < gen_ctx.num_line; i++) {
    struct AsmLine *line = gen_ctx.asm_lines + i;
    int get_interp_result = 0;

    switch (line->kind) {
    case kAsmLineDeleted:
      break; // 何もしない
    case kAsmLineLabel:
      PrintLabel(output_file, &line->label);
      fprintf(output_file, ":\n");
      break;
    case kAsmLineInsn:
      fprintf(output_file, INDENT);
      get_interp_result = line->insn.kind == kInsnInterpResult &&
                          strcmp(line->insn.opcode, "push") != 0;
      if (line->insn.kind == kInsnInterp || get_interp_result) {
        fprintf(output_file, ".");
      }
      fprintf(output_file, "%s", line->insn.opcode);

      for (int opr_i = 0;
           opr_i < MAX_OPERANDS && line->insn.operands[opr_i].kind != kOprNone;
           opr_i++) {
        struct Operand *opr = line->insn.operands + opr_i;
        fputc(" ,"[opr_i > 0], output_file);
        switch (opr->kind) {
        case kOprNone:
          // pass
        case kOprInt:
          fprintf(output_file, "%d", opr->val_int);
          break;
        case kOprLabel:
          PrintLabel(output_file, &opr->val_label);
          break;
        case kOprReg:
          fprintf(output_file, "%s", opr->val_reg);
          break;
        case kOprBaseOff:
          if (opr->val_base_off.base) {
            fprintf(output_file, "%s+", opr->val_base_off.base);
          }
          fprintf(output_file, "%d", opr->val_base_off.offset);
          break;
        case kOprBaseLabel:
          if (opr->val_base_label.base) {
            fprintf(output_file, "%s+", opr->val_base_label.base);
          }
          PrintLabel(output_file, &opr->val_base_label.label);
          break;
        }
      }
      fprintf(output_file, "\n");

      if (get_interp_result) {
        fprintf(output_file, INDENT "push $top\n");
      }
      break;
    case kAsmLineIndentedComment:
      fprintf(output_file, INDENT "; %s\n", line->comment);
      break;
    case kAsmLineRaw:
      fprintf(output_file, "%s\n", line->raw);
      break;
    }
  }

  // built-in functions
  // addr_t __builtin_write_pmem(addr_t pmem_addr, uint16_t insn_hi, uint16_t insn_lo);
  //fprintf(output_file, "__builtin_write_pmem:\n"
  //                     INDENT "siha\n"
  //                     INDENT "sila\n"
  //                     INDENT "ret\n");

  if (output_file != stdout) {
    fclose(output_file);
  }

  return 0;
}
