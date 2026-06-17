// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2024 Kota UCHIDA
 */
#pragma once

#include <stddef.h>
#include <stdio.h>

#include "token.h"

enum TypeKind {
  kTypeChar,
  kTypeInt,
  kTypePtr,
  kTypeArray,
  kTypeVoid,
  kTypeEllipsis,
  kTypeFunc,
};

// 整数型（char, int）はデフォルトで unsigned とみなす
#define TYPE_ATTR_SIGNED 0x0001u

#define IS_UNSIGNED_INT(t) \
  ((t)->kind == kTypeInt && ((t)->attr & TYPE_ATTR_SIGNED) == 0)

struct Type {
  enum TypeKind kind;
  struct Type *base;
  struct Type *next; // func params
  union {
    int len; // for kTypeArray
    struct Token *id; // for kTypeFunc
  };
  unsigned int attr;
};

struct Type *NewType(enum TypeKind kind);
struct Type *NewFuncType(struct Type *ret_type, struct Token *id);
size_t SizeofType(struct Type *type);
void PrintType(FILE *out, struct Type *type);
