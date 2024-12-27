#pragma once

#include <stdint.h>

#include "token.h"

enum SymbolKind {
  kSymHead, // 線形リストのヘッドを表す特殊な種別
  kSymGVar, // グローバル変数
  kSymLVar, // ローカル変数
  kSymFunc, // 関数
  kSymBif,  // ビルトイン関数（built-in function）
};

struct Type;
struct Node;

#define SYM_ATTR_FIXED_ADDR 0x0001u

struct Symbol {
  enum SymbolKind kind;
  struct Token *name;
  struct Symbol *next;
  struct Node *def;

  uint16_t offset;
  struct Type *type;
  unsigned int attr;
};

struct Symbol *NewSymbol(enum SymbolKind kind, struct Token *name);
struct Symbol *AppendSymbol(struct Symbol *head, struct Symbol *sym);
struct Symbol *FindSymbolLocal(struct Symbol *head, struct Token *name);

struct Scope {
  struct Scope *parent;
  struct Symbol *syms;
  int var_offset; // 変数のオフセットアドレス
};

// 新しくスコープを生成し、生成したスコープを返す
struct Scope *EnterScope(struct Scope *current);

// 現在のスコープを抜け、1 段上のスコープを返す
struct Scope *LeaveScope(struct Scope *current);

// 現在のスコープにおいてシンボルを検索する
struct Symbol *FindSymbol(struct Scope *scope, struct Token *name);

struct Scope *NewGlobalScope(struct Symbol *global_syms);
