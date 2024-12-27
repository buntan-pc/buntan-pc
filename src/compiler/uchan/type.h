#pragma once

#include <stddef.h>
#include <stdio.h>

enum TypeKind {
  kTypeChar,
  kTypeInt,
  kTypePtr,
  kTypeArray,
  kTypeVoid,
};

// 整数型（char, int）はデフォルトで unsigned とみなす
#define TYPE_ATTR_SIGNED 0x0001u

#define IS_UNSIGNED_INT(t) \
  ((t)->kind == kTypeInt && ((t)->attr & TYPE_ATTR_SIGNED) == 0)

struct Type {
  enum TypeKind kind;
  struct Type *base;
  union {
    int len; // for kTypeArray
    struct Token *id; // for function pointer
  };
  unsigned int attr;
};

struct Type *NewType(enum TypeKind kind);
struct Type *NewFuncType(struct Type *ret_type, struct Token *id);
size_t SizeofType(struct Type *type);
void PrintType(FILE *out, struct Type *type);
