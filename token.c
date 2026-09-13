#define _POSIX_C_SOURCE 200809L
#include "token.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void tl_append(struct arena *a, struct token_list *tl,
                      enum token_kind kind, const char *text, size_t len,
                      struct source_loc loc)
{
    struct token *t = arena_alloc(a, sizeof(*t));
    t->next = NULL;
    t->kind = kind;
    t->text = text;
    t->len = len;
    t->loc = loc;
    if (tl->tail)
        tl->tail->next = t;
    else
        tl->head = t;
    tl->tail = t;
}

static int is_ident_start(int c)
{
    return isalpha(c) || c == '_' || c == '$';
}

static int is_ident_cont(int c)
{
    return isalnum(c) || c == '_' || c == '$';
}

static void skip_line(const char **p, int *col, const char *end)
{
    while (*p < end && **p != '\n') {
        (*p)++;
        (*col)++;
    }
}

struct token_list *tokenize(struct arena *a, const char *data, size_t len)
{
    struct token_list *tl = arena_alloc(a, sizeof(*tl));
    tl->head = NULL;
    tl->tail = NULL;

    struct source_loc cur = { "<unknown>", 1, 1 };
    int col = 1;
    const char *p = data;
    const char *end = data + len;

    while (p < end) {
        unsigned char c = (unsigned char)*p;

        if (c == '\n') {
            cur.line++;
            col = 1;
            p++;
            continue;
        }

        if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') {
            col++;
            p++;
            continue;
        }

        if (c == '/' && p + 1 < end && p[1] == '/') {
            skip_line(&p, &col, end);
            continue;
        }

        if (c == '/' && p + 1 < end && p[1] == '*') {
            p += 2;
            col += 2;
            while (p + 1 < end && !(p[0] == '*' && p[1] == '/')) {
                if (*p == '\n') {
                    cur.line++;
                    col = 1;
                } else {
                    col++;
                }
                p++;
            }
            if (p + 1 < end) {
                p += 2;
                col += 2;
            }
            continue;
        }

        if (c == '#' && (p == data || p[-1] == '\n')) {
            p++;
            col++;
            while (p < end && (*p == ' ' || *p == '\t')) {
                p++; col++;
            }
            if (p < end && isdigit((unsigned char)*p)) {
                int lnum = 0;
                while (p < end && isdigit((unsigned char)*p)) {
                    lnum = lnum * 10 + (*p - '0');
                    p++; col++;
                }
                while (p < end && (*p == ' ' || *p == '\t')) {
                    p++; col++;
                }
                if (p < end && *p == '"') {
                    char fname[1024];
                    size_t flen = 0;
                    p++; col++;
                    while (p < end && *p != '"' && flen < sizeof(fname) - 1) {
                        fname[flen++] = *p;
                        p++; col++;
                    }
                    fname[flen] = '\0';
                    if (p < end && *p == '"') {
                        p++; col++;
                    }
                    cur.filename = arena_strdup(a, fname);
                    cur.line = lnum;
                    col = 1;
                }
                skip_line(&p, &col, end);
                continue;
            }
            skip_line(&p, &col, end);
            continue;
        }

        if (c == '"') {
            const char *start = p;
            int sc = col;
            p++; col++;
            while (p < end && *p != '"') {
                if (*p == '\\' && p + 1 < end) {
                    p++; col++;
                    if (p < end) { p++; col++; }
                } else {
                    p++; col++;
                }
            }
            if (p < end) { p++; col++; }
            struct source_loc loc = cur;
            loc.col = sc;
            tl_append(a, tl, TOK_STRING, start, (size_t)(p - start), loc);
            continue;
        }

        if (c == '\'') {
            const char *start = p;
            int sc = col;
            p++; col++;
            while (p < end && *p != '\'') {
                if (*p == '\\' && p + 1 < end) {
                    p++; col++;
                    if (p < end) { p++; col++; }
                } else {
                    p++; col++;
                }
            }
            if (p < end) { p++; col++; }
            struct source_loc loc = cur;
            loc.col = sc;
            tl_append(a, tl, TOK_CHAR, start, (size_t)(p - start), loc);
            continue;
        }

        if (is_ident_start(c)) {
            const char *start = p;
            int sc = col;
            while (p < end && is_ident_cont((unsigned char)*p)) {
                p++; col++;
            }
            struct source_loc loc = cur;
            loc.col = sc;
            tl_append(a, tl, TOK_IDENT, start, (size_t)(p - start), loc);
            continue;
        }

        if (isdigit(c) || (c == '.' && p + 1 < end && isdigit((unsigned char)p[1]))) {
            const char *start = p;
            int sc = col;
            int saw_e = 0;
            while (p < end) {
                unsigned char ch = (unsigned char)*p;
                if (isalnum(ch) || ch == '.') {
                    if (ch == 'e' || ch == 'E') saw_e = 1;
                    p++; col++;
                } else if (saw_e && (ch == '+' || ch == '-')) {
                    saw_e = 0;
                    p++; col++;
                } else if (ch == '\'' && p > start && isalnum((unsigned char)p[-1])) {
                    p++; col++;
                } else {
                    break;
                }
            }
            struct source_loc loc = cur;
            loc.col = sc;
            tl_append(a, tl, TOK_NUMBER, start, (size_t)(p - start), loc);
            continue;
        }

        if (ispunct(c)) {
            const char *start = p;
            int sc = col;
            if (p + 2 < end && p[0] == '.' && p[1] == '.' && p[2] == '.') {
                p += 3; col += 3;
                struct source_loc loc = cur; loc.col = sc;
                tl_append(a, tl, TOK_PUNCT, start, 3, loc);
                continue;
            }
            if (p + 2 < end &&
                ((p[0] == '<' && p[1] == '<' && p[2] == '=') ||
                 (p[0] == '>' && p[1] == '>' && p[2] == '='))) {
                p += 3; col += 3;
                struct source_loc loc = cur; loc.col = sc;
                tl_append(a, tl, TOK_PUNCT, start, 3, loc);
                continue;
            }
            if (p + 1 < end) {
                char t2 = p[1];
                int is_two = 0;
                const char *pairs[][2] = {
                    {"<", "<"}, {">", ">"}, {"<", "="}, {">", "="},
                    {"=", "="}, {"!", "="}, {"&", "&"}, {"|", "|"},
                    {"+", "+"}, {"-", "-"}, {"-", ">"}, {"+", "="},
                    {"-", "="}, {"*", "="}, {"/", "="}, {"%", "="},
                    {"&", "="}, {"|", "="}, {"^", "="}, {":", ":"},
                    {"#", "#"},
                };
                for (size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++) {
                    if (p[0] == pairs[i][0][0] && t2 == pairs[i][1][0]) {
                        is_two = 1;
                        break;
                    }
                }
                if (is_two) {
                    p += 2; col += 2;
                    struct source_loc loc = cur; loc.col = sc;
                    tl_append(a, tl, TOK_PUNCT, start, 2, loc);
                    continue;
                }
            }
            p++; col++;
            {
                struct source_loc loc = cur; loc.col = sc;
                tl_append(a, tl, TOK_PUNCT, start, 1, loc);
            }
            continue;
        }

        {
            const char *start = p;
            int sc = col;
            p++; col++;
            struct source_loc loc = cur; loc.col = sc;
            tl_append(a, tl, TOK_OTHER, start, 1, loc);
        }
    }

    return tl;
}
