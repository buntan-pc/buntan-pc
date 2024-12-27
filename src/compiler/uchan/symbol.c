#include "symbol.h"

#include <stdlib.h>
#include <string.h>

void Locate(char *p);

struct Symbol *NewSymbol(enum SymbolKind kind, struct Token *name) {
  struct Symbol *s = malloc(sizeof(struct Symbol));
  s->kind = kind;
  s->name = name;
  s->next = NULL;
  s->def = NULL;

  s->offset = 0;
  s->type = NULL;
  s->attr = 0;

  return s;
}

struct Symbol *AppendSymbol(struct Symbol *head, struct Symbol *sym) {
  for (; head->next; head = head->next);
  head->next = sym;
  return sym;
}

struct Symbol *FindSymbolLocal(struct Symbol *head, struct Token *name) {
  for (; head; head = head->next) {
    if (head->kind != kSymHead &&
        head->name->len == name->len &&
        strncmp(head->name->raw, name->raw, name->len) == 0) {
      return head;
    }
  }
  return NULL;
}

struct Scope *EnterScope(struct Scope *current) {
  struct Scope *s = NewGlobalScope(NewSymbol(kSymHead, NULL));
  s->parent = current;
  return s;
}

struct Symbol *FindSymbol(struct Scope *scope, struct Token *name) {
  struct Symbol *sym = NULL;
  while (scope && !sym) {
    sym = FindSymbolLocal(scope->syms, name);
    scope = scope->parent;
  }
  return sym;
}

struct Scope *NewGlobalScope(struct Symbol *global_syms) {
  struct Scope *s = malloc(sizeof(struct Scope));
  s->parent = NULL;
  s->syms = global_syms;
  s->var_offset = 0;
  return s;
}
