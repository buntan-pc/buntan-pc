// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 Kota UCHIDA
 *
 * 最適化に関する機能を集めたファイル
 */

#include <stdlib.h>
#include <string.h>
#include "opt.h"

#define JUMPTABLE_MAX 1024

/*
 * L_here:
 *     jmp/jz L_to
 *
 * の情報を表す。
 */
struct JumpTableItem {
  int here_label_no;        // L_here の here を表す
  struct Instruction *insn; // L_to の情報も含む
};

static int IsJmp(struct Instruction *insn) {
  return strcmp(insn->opcode, "jmp") == 0;
}

static int IsJz(struct Instruction *insn) {
  return strcmp(insn->opcode, "jz") == 0;
}

static int IsRet(struct Instruction *insn) {
  return strcmp(insn->opcode, "ret") == 0;
}

static int GetOprLabelNo(struct Instruction *insn) {
  struct Operand *opr = insn->operands + 0;
  if (opr && opr->kind == kOprLabel && opr->val_label.kind == kLabelAutoL) {
    return opr->val_label.label_no;
  }
  return 0;
}

static int IsLabelAutoL(struct AsmLine *line) {
  return
    line &&
    line->kind == kAsmLineLabel &&
    line->label.kind == kLabelAutoL;
}

static void ResolveJumpTable(struct JumpTableItem *jump_table, int n) {
  for (int i = 0; i < n; i++) {
    struct JumpTableItem *item = jump_table + i;
    if (!IsJmp(item->insn)) {
      continue;
    }
    // jmp だけを対象にする
    int to = GetOprLabelNo(item->insn);
    if (to == 0) {
      continue;
    }

    for (int j = 0; j < n; j++) {
      if (i == j) {
        continue;
      }
      struct JumpTableItem *item_to = jump_table + j;
      if (to != item_to->here_label_no) {
        continue;
      }
      item->insn = item_to->insn;
    }
  }
}

static void PrintJumpTable(struct JumpTableItem *jump_table, int n) {
  for (int i = 0; i < n; i++) {
    struct Operand *opr = jump_table[i].insn->operands + 0;
    if (opr->kind == kOprLabel) {
      if (opr->val_label.kind == kLabelAutoL) {
      fprintf(stderr, "jump_table[%3d]: here=%d opcode=%s to=%d\n",
          i,
          jump_table[i].here_label_no,
          jump_table[i].insn->opcode,
          opr->val_label.label_no
      );
      } else {
        fprintf(stderr, "jump_table[%3d]: here=%d opcode=%s to=(unexpedted label kind %d)\n",
            i,
            jump_table[i].here_label_no,
            jump_table[i].insn->opcode,
            opr->val_label.kind
        );
      }
    } else {
      fprintf(stderr, "jump_table[%3d]: here=%d opcode=%s to=(unexpected operand kind %d)\n",
          i,
          jump_table[i].here_label_no,
          jump_table[i].insn->opcode,
          opr->kind
      );
    }
  }
}

// ラベルの参照回数
struct LabelRef {
  int label_no;
  int num_ref;
};

void ChangeLabelRef(struct LabelRef *label_refs, int n, int orig_label_no, int new_label_no) {
  for (int i = 0; i < n; i++) {
    struct LabelRef *lr = label_refs + i;
    if (lr->label_no == orig_label_no) {
      lr->num_ref--;
    }
    if (lr->label_no == new_label_no) {
      lr->num_ref++;
    }
  }
}

void OptimizeAsmJumps(struct AsmLine *asm_lines, int num_line) {
  struct JumpTableItem jump_table[JUMPTABLE_MAX];
  int jump_table_i = 0;
  struct LabelRef label_refs[JUMPTABLE_MAX];
  int label_ref_i = 0;

  for (int asm_i = 0; asm_i < num_line; asm_i++) {
    struct AsmLine *line = asm_lines + asm_i;
    switch (line->kind) {
    case kAsmLineInsn:
      if (IsJmp(&line->insn)) {
        // 行を前方にたどり、ラベルが連続する区間を処理する
        for (int j = asm_i - 1; j >= 0; j--) {
          struct AsmLine *label_line = asm_lines + j;
          if (!IsLabelAutoL(label_line)) {
            break;
          }

          if (jump_table_i >= JUMPTABLE_MAX) {
            fprintf(stderr, "OptimizeAsmJumps: too many jumps\n");
            goto do_optimize;
          }
          jump_table[jump_table_i].here_label_no = label_line->label.label_no;
          jump_table[jump_table_i].insn = &line->insn;
          jump_table_i++;
        }
      }
      if (IsJmp(&line->insn) || IsJz(&line->insn)) {
        // ラベルの参照回数を記録しておく
        int to = GetOprLabelNo(&line->insn);
        if (to == 0) {
          // jmp fin みたいなケース
          continue;
        }
        int found = 0;
        for (int i = 0; i < label_ref_i; i++) {
          if (label_refs[i].label_no == to) {
            label_refs[i].num_ref++;
            found = 1;
            break;
          }
        }
        if (!found) {
          label_refs[label_ref_i].label_no = to;
          label_refs[label_ref_i].num_ref = 1;
          label_ref_i++;
        }
      }
      break;
    default:
      break;
    }
  }

  int jump_table_len = jump_table_i;
  int label_refs_len = label_ref_i;

do_optimize:

  //fprintf(stderr, "OptimizeAsmJumps: optimizing\n");
  //PrintJumpTable(jump_table, jump_table_len);
  ResolveJumpTable(jump_table, jump_table_len);
  //PrintJumpTable(jump_table, jump_table_len);

  // ジャンプ表を使ってジャンプ系命令を書き換える
  for (int asm_i = 0; asm_i < num_line; asm_i++) {
    struct AsmLine *line = asm_lines + asm_i;
    if (line->kind != kAsmLineInsn ||
        !(IsJmp(&line->insn) || IsJz(&line->insn))) {
      continue;
    }
    int orig_to = GetOprLabelNo(&line->insn);
    if (orig_to == 0) {
      continue;
    }
    for (int i = 0; i < jump_table_len; i++) {
      if (orig_to != jump_table[i].here_label_no) {
        continue;
      }
      int new_to = GetOprLabelNo(jump_table[i].insn);
      // ジャンプ先ラベル番号を書き換える
      line->insn.operands[0] = jump_table[i].insn->operands[0];
      ChangeLabelRef(label_refs, label_refs_len, orig_to, new_to);
    }
  }

  // 直後のラベルにジャンプする命令を除去
  for (int asm_i = 0; asm_i < num_line - 1; asm_i++) {
    struct AsmLine *line = asm_lines + asm_i;
    if (line->kind != kAsmLineInsn ||
        !(IsJmp(&line->insn) || IsJz(&line->insn))) {
      continue;
    }
    int to = GetOprLabelNo(&line->insn);
    if (to == 0) {
      continue;
    }
    struct AsmLine *next_line = line + 1;
    if (!IsLabelAutoL(next_line) || to != next_line->label.label_no) {
      continue;
    }

    // jmp/jz L_x の次の行が L_x なので、この jmp/jz を除去できる
    line->kind = kAsmLineDeleted;
    ChangeLabelRef(label_refs, label_refs_len, to, 0);
  }

  // 一度も参照されないラベル定義行を削除
  for (int asm_i = 0; asm_i < num_line; asm_i++) {
    struct AsmLine *line = asm_lines + asm_i;
    if (!IsLabelAutoL(line)) {
      continue;
    }
    for (int i = 0; i < label_refs_len; i++) {
      struct LabelRef *lr = label_refs + i;
      if (lr->label_no != line->label.label_no) {
        continue;
      }
      if (lr->num_ref == 0) {
        line->kind = kAsmLineDeleted;
      }
      break;
    }
  }

  // 到達しないコードを削除
  int dead_area = 0;
  for (int asm_i = 0; asm_i < num_line; asm_i++) {
    struct AsmLine *line = asm_lines + asm_i;
    if (dead_area) {
      switch (line->kind) {
      case kAsmLineLabel:
        dead_area = 0;
        break;
      case kAsmLineInsn:
        line->kind = kAsmLineDeleted;
        break;
      default:
        break;
      }
    } else {
      if (line->kind == kAsmLineInsn &&
          (IsJmp(&line->insn) || IsRet(&line->insn))) {
        dead_area = 1;
      }
    }
  }
}
