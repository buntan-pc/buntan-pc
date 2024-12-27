#pragma once

#include <stdint.h>
#include <stdio.h>

#include "token.h"
#include "type.h"

enum NodeKind {
  // 式（値をスタックに積むもの）
  kNodeInteger,
  kNodeId,
  kNodeString,// string literal
  kNodeInc,
  kNodeDec,
  kNodeRef,   // & exp
  kNodeDeref, // * exp
  kNodeNot,   // ~
  kNodeCall,
  kNodeCast,
  kNodeVoid,  // 空の式（単なる ;）
  kNodeLAnd,  // &&
  kNodeLOr,   // ||
  kNodeAssign,
  kNodeAdd = 100, // standard binary expression
  kNodeSub,
  kNodeMul,
  kNodeLT,    // <
  kNodeLE,    // <=
  kNodeEq,
  kNodeNEq,
  kNodeAnd,   // &
  kNodeOr,    // |
  kNodeXor,   // ^
  kNodeRShift,// >>
  kNodeLShift,// <<
  // 式以外（値をスタックに積まないもの）
  kNodeDefFunc = 200,
  kNodeBlock,
  kNodeReturn,
  kNodeDefVar,
  kNodeIf,
  kNodeFor,
  kNodeWhile,
  kNodeBreak,
  kNodeContinue,
  kNodeTypeSpec,
  kNodePList,
  kNodeIList, // int a[n] = {...}; の右辺
  kNodeAsm, // インラインアセンブラ
  kNodeSwitch,
  kNodeCase,
};

struct Node {
  enum NodeKind kind;  // ノードの種類
  int index;           // ノードのインデックス
  struct Token *token; // ノードを代表するトークン
  struct Node *next;   // 線形リスト
  struct Type *type;   // ノードの型
  struct Scope *scope; // シンボルテーブル (kNodeDefFunc, kNodeBlock で有効)

  // 用途         lhs       rhs       cond      備考
  // ------------------------------------------------------
  // 2 項演算     左辺      右辺      NULL
  // kNodeReturn  返却値    NULL      NULL
  // kNodeDefVar  変数名    初期値    NULL
  // kNodeIf      then      else      条件式
  // kNodeFor     初期化式  ブロック  条件式    lhs->next は更新式
  // kNodeWhile   NULL      ブロック  条件式
  // kNodeDefFunc 戻り値型  ブロック  仮引数
  // kNodeBlock   NULL      ブロック  NULL
  // kNodeCall    関数名    引数      NULL
  // kNodePList   変数名    NULL      NULL
  // kNodeAsm     文字列    NULL      NULL
  // kNodeCast    型指定子  式        NULL
  // kNodeIList   NULL      式        NULL
  // kNodeCase    NULL      文        値        値は kNodeInteger
  struct Node *lhs, *rhs, *cond;
};

struct Scope;

enum ParsePass {
  kPPInit,
  kPPMain,
};

struct ParseContext {
  struct Scope *scope;
  int pass;
};

struct Node *NewNode(enum NodeKind kind, struct Token *token);
struct Node *NewNodeBinOp(enum NodeKind kind, struct Token *op,
                          struct Node *lhs, struct Node *rhs);
struct Node *Program(struct ParseContext *ctx);
struct Node *ExternalDeclaration(struct ParseContext *ctx);
struct Node *FunctionDefinition(struct ParseContext *ctx,
                                struct Node *tspec, struct Token *id);
struct Node *VariableDefinition(struct ParseContext *ctx,
                                struct Node *tspec, struct Token *id);
struct Node *Block(struct ParseContext *ctx);
struct Node *InnerDeclaration(struct ParseContext *ctx);
struct Node *Statement(struct ParseContext *ctx);
struct Node *InitStatement(struct ParseContext *ctx);
struct Node *Expression(struct ParseContext *ctx);
struct Node *Assignment(struct ParseContext *ctx);
struct Node *LogicalOr(struct ParseContext *ctx);
struct Node *LogicalAnd(struct ParseContext *ctx);
struct Node *BitwiseOr(struct ParseContext *ctx);
struct Node *BitwiseXor(struct ParseContext *ctx);
struct Node *BitwiseAnd(struct ParseContext *ctx);
struct Node *Equality(struct ParseContext *ctx);
struct Node *Relational(struct ParseContext *ctx);
struct Node *BitwiseShift(struct ParseContext *ctx);
struct Node *Additive(struct ParseContext *ctx);
struct Node *Multiplicative(struct ParseContext *ctx);
struct Node *Cast(struct ParseContext *ctx);
struct Node *Unary(struct ParseContext *ctx);
struct Node *Postfix(struct ParseContext *ctx);
struct Node *Primary(struct ParseContext *ctx);

struct Node *TypeSpec();
struct Node *ParameterList();

void PrintNode(FILE *out, struct Node *n, int indent, const char *key);
