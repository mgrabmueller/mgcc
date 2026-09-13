#ifndef MGCC_TOKEN_H
#define MGCC_TOKEN_H

#include <stddef.h>
#include "arena.h"
#include "source.h"

enum token_kind {
    TOK_EOF,
    TOK_IDENT,
    TOK_NUMBER,
    TOK_STRING,
    TOK_CHAR,
    TOK_PUNCT,
    TOK_OTHER,
};

struct token {
    struct token *next;
    enum token_kind kind;
    const char *text;
    size_t len;
    struct source_loc loc;
};

struct token_list {
    struct token *head;
    struct token *tail;
};

struct token_list *tokenize(struct arena *a, const char *data, size_t len);

#endif
