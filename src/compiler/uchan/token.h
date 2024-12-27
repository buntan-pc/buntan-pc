#pragma once

#include <stddef.h>

enum TokenKind {
  // 0 - 127 は 1 文字演算子用に予約
  kTokenInteger = 128,
  kTokenReturn,
  kTokenInt,
  kTokenId,
  kTokenIf,
  kTokenElse,
  kTokenCompAssign,
  kTokenInc = kTokenCompAssign + 128,
  kTokenDec,
  kTokenFor,
  kTokenChar,
  kTokenEq,
  kTokenNEq,
  kTokenAnd,
  kTokenOr,
  kTokenCharacter, // char literal '*'
  kTokenString,    // string literal "foo"
  kTokenWhile,
  kTokenBreak,
  kTokenContinue,
  kTokenRShift,
  kTokenLShift,
  kTokenVoid,
  kTokenAsm,
  kTokenAttr,
  kTokenLE,
  kTokenGE,
  kTokenEOF,
  kTokenSigned,
  kTokenUnsigned,
  kTokenSwitch,
  kTokenCase,
  kTokenDefault,
};

struct Token {
  int kind;            // トークンの種類
  char *raw;           // トークン文字列（ソースコード文字列へのポインタ）
  int len;             // トークン文字列の長さ
  struct Token *next;

  union {
    int as_int;        // kTokenInteger
  } value;             // トークンの値
};

extern struct Token *cur_token;

struct Token *NewToken(int kind, char *raw, size_t len);
void Tokenize(char *src);
struct Token *Consume(int kind);
struct Token *Expect(int kind);
char DecodeEscape(char *s, int *i);
int DecodeStringLiteral(char *buf, int size, struct Token *tok);
