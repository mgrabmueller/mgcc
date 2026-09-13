#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "arena.h"
#include "token.h"

#define PROG_NAME "mgcc"
#define PROG_VERSION "0.3.0"

#define MAX_ARGS 256

enum stop_phase {
    STOP_LINK = 0,
    STOP_ASSEMBLE = 1,
    STOP_COMPILE = 2,
};

struct options {
    enum stop_phase stop;
    const char *output;
    const char *input;
    int dump_tokens;
    char include_dirs[MAX_ARGS][256];
    int n_include_dirs;
    char lib_dirs[MAX_ARGS][256];
    int n_lib_dirs;
    char libs[MAX_ARGS][256];
    int n_libs;
};

static struct arena g_arena;

static void print_version(void)
{
    printf("%s %s\n", PROG_NAME, PROG_VERSION);
    printf("Self-compiling C compiler for testing Mistral Vibe\n");
}

static void print_help(void)
{
    printf("Usage: %s [options] file\n", PROG_NAME);
    printf("Options:\n");
    printf("  --help            Display this information\n");
    printf("  --version         Display compiler version information\n");
    printf("  -c                Compile and assemble, do not link\n");
    printf("  -S                Compile only, do not assemble or link\n");
    printf("  -o <file>         Place output into <file>\n");
    printf("  -I<dir>           Add <dir> to the end of the main include path\n");
    printf("  -L<dir>           Add <dir> to the end of the library search path\n");
    printf("  -l<lib>           Link against library <lib>\n");
    printf("  --dump-tokens     Tokenize preprocessed input and print tokens\n");
}

static char *g_tmpfiles[MAX_ARGS];
static int g_n_tmpfiles = 0;

static void cleanup_tmpfiles(void)
{
    for (int i = 0; i < g_n_tmpfiles; i++) {
        if (g_tmpfiles[i]) {
            unlink(g_tmpfiles[i]);
            free(g_tmpfiles[i]);
            g_tmpfiles[i] = NULL;
        }
    }
    arena_free(&g_arena);
}

static void die(const char *fmt, ...);

static void track_tmp(char *path)
{
    if (g_n_tmpfiles >= MAX_ARGS)
        die("too many temp files");
    g_tmpfiles[g_n_tmpfiles++] = path;
}

static void die(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "%s: ", PROG_NAME);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

static void add_string(char arr[MAX_ARGS][256], int *n, const char *s)
{
    if (*n >= MAX_ARGS)
        die("too many arguments");
    strncpy(arr[*n], s, 255);
    arr[*n][255] = '\0';
    (*n)++;
}

static void parse_args(int argc, char **argv, struct options *opt)
{
    opt->stop = STOP_LINK;
    opt->output = NULL;
    opt->input = NULL;
    opt->dump_tokens = 0;
    opt->n_include_dirs = 0;
    opt->n_lib_dirs = 0;
    opt->n_libs = 0;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--help") == 0) {
            print_help();
            exit(0);
        }
        if (strcmp(arg, "--version") == 0) {
            print_version();
            exit(0);
        }
        if (strcmp(arg, "--dump-tokens") == 0) {
            opt->dump_tokens = 1;
            continue;
        }
        if (strcmp(arg, "-S") == 0) {
            opt->stop = STOP_COMPILE;
            continue;
        }
        if (strcmp(arg, "-c") == 0) {
            opt->stop = STOP_ASSEMBLE;
            continue;
        }
        if (strcmp(arg, "-o") == 0) {
            if (i + 1 >= argc)
                die("option '-o' requires an argument");
            opt->output = argv[++i];
            continue;
        }
        if (arg[0] == '-' && arg[1] == 'I') {
            add_string(opt->include_dirs, &opt->n_include_dirs, arg + 2);
            continue;
        }
        if (arg[0] == '-' && arg[1] == 'L') {
            add_string(opt->lib_dirs, &opt->n_lib_dirs, arg + 2);
            continue;
        }
        if (arg[0] == '-' && arg[1] == 'l') {
            add_string(opt->libs, &opt->n_libs, arg + 2);
            continue;
        }
        if (arg[0] == '-' && arg[1] != '\0')
            die("unrecognized option '%s'", arg);
        if (opt->input != NULL)
            die("only one input file is supported (got '%s' and '%s')",
                opt->input, arg);
        opt->input = arg;
    }

    if (opt->input == NULL)
        die("no input file");
}

static void add_include_args(char *argv[MAX_ARGS], int *n,
                             struct options *opt)
{
    for (int i = 0; i < opt->n_include_dirs; i++) {
        char buf[264];
        snprintf(buf, sizeof(buf), "-I%s", opt->include_dirs[i]);
        argv[(*n)++] = arena_strdup(&g_arena, buf);
    }
}

static void add_lib_args(char *argv[MAX_ARGS], int *n, struct options *opt)
{
    for (int i = 0; i < opt->n_lib_dirs; i++) {
        char buf[264];
        snprintf(buf, sizeof(buf), "-L%s", opt->lib_dirs[i]);
        argv[(*n)++] = arena_strdup(&g_arena, buf);
    }
    for (int i = 0; i < opt->n_libs; i++) {
        char buf[264];
        snprintf(buf, sizeof(buf), "-l%s", opt->libs[i]);
        argv[(*n)++] = arena_strdup(&g_arena, buf);
    }
}

static void run_gcc(char *argv[MAX_ARGS])
{
    fflush(NULL);
    pid_t pid = fork();
    if (pid < 0)
        die("fork failed");
    if (pid == 0) {
        execvp("gcc", argv);
        _exit(127);
    }
    int status;
    if (waitpid(pid, &status, 0) < 0)
        die("waitpid failed");
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        exit(WIFEXITED(status) ? WEXITSTATUS(status) : 1);
}

static char *replace_ext(const char *path, const char *newext)
{
    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');
    size_t baselen = dot && (!slash || dot > slash) ? (size_t)(dot - path) : strlen(path);
    char *out = malloc(baselen + strlen(newext) + 1);
    if (!out)
        die("out of memory");
    memcpy(out, path, baselen);
    strcpy(out + baselen, newext);
    return out;
}

static char *default_output(const char *input, enum stop_phase stop)
{
    switch (stop) {
    case STOP_COMPILE:  return replace_ext(input, ".s");
    case STOP_ASSEMBLE: return replace_ext(input, ".o");
    case STOP_LINK:     return strdup("a.out");
    }
    return NULL;
}

static char *mktmp(const char *suffix)
{
    char tmpl[] = "/tmp/mgcc_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0)
        die("mkstemp failed");
    close(fd);
    char *out = malloc(strlen(tmpl) + strlen(suffix) + 1);
    if (!out)
        die("out of memory");
    sprintf(out, "%s%s", tmpl, suffix);
    unlink(tmpl);
    track_tmp(out);
    return out;
}

static void preprocess(const char *input, const char *output,
                       struct options *opt)
{
    char *argv[MAX_ARGS];
    int n = 0;
    argv[n++] = "gcc";
    argv[n++] = "-E";
    argv[n++] = "-o";
    argv[n++] = (char *)output;
    add_include_args(argv, &n, opt);
    argv[n++] = (char *)input;
    argv[n] = NULL;
    run_gcc(argv);
}

static void compile(const char *input, const char *output,
                    struct options *opt)
{
    (void)opt;
    char *argv[MAX_ARGS];
    int n = 0;
    argv[n++] = "gcc";
    argv[n++] = "-S";
    argv[n++] = "-x";
    argv[n++] = "cpp-output";
    argv[n++] = "-o";
    argv[n++] = (char *)output;
    argv[n++] = (char *)input;
    argv[n] = NULL;
    run_gcc(argv);
}

static void assemble(const char *input, const char *output,
                     struct options *opt)
{
    (void)opt;
    char *argv[MAX_ARGS];
    int n = 0;
    argv[n++] = "gcc";
    argv[n++] = "-c";
    argv[n++] = "-x";
    argv[n++] = "assembler";
    argv[n++] = "-o";
    argv[n++] = (char *)output;
    argv[n++] = (char *)input;
    argv[n] = NULL;
    run_gcc(argv);
}

static void link_objs(const char *input, const char *output,
                      struct options *opt)
{
    char *argv[MAX_ARGS];
    int n = 0;
    argv[n++] = "gcc";
    argv[n++] = "-o";
    argv[n++] = (char *)output;
    argv[n++] = (char *)input;
    add_lib_args(argv, &n, opt);
    argv[n] = NULL;
    run_gcc(argv);
}

static char *read_file_into_arena(struct arena *a, const char *path,
                                     size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        die("cannot open '%s'", path);
    if (fseek(f, 0, SEEK_END) != 0)
        die("seek failed on '%s'", path);
    long sz = ftell(f);
    if (sz < 0)
        die("tell failed on '%s'", path);
    rewind(f);
    char *buf = arena_alloc(a, (size_t)sz + 1);
    if (sz > 0 && fread(buf, 1, (size_t)sz, f) != (size_t)sz)
        die("short read on '%s'", path);
    buf[sz] = '\0';
    fclose(f);
    *out_len = (size_t)sz;
    return buf;
}

static const char *tok_kind_name(enum token_kind k)
{
    switch (k) {
    case TOK_EOF:    return "EOF";
    case TOK_IDENT:  return "ident";
    case TOK_NUMBER: return "number";
    case TOK_STRING: return "string";
    case TOK_CHAR:   return "char";
    case TOK_PUNCT:  return "punct";
    case TOK_OTHER:  return "other";
    }
    return "?";
}

static void dump_tokens(struct arena *a, const char *pp_file)
{
    size_t len;
    char *data = read_file_into_arena(a, pp_file, &len);
    struct token_list *tl = tokenize(a, data, len);
    for (struct token *t = tl->head; t; t = t->next) {
        printf("%s:%d:%d  %-7s  off=%zu  len=%zu  '",
               t->loc.filename ? t->loc.filename : "?",
               t->loc.line, t->loc.col,
               tok_kind_name(t->kind), t->offset, t->len);
        for (size_t i = 0; i < t->len; i++) {
            char ch = t->text[i];
            if (ch == '\n')
                fputs("\\n", stdout);
            else if (ch == '\t')
                fputs("\\t", stdout);
            else
                putchar(ch);
        }
        printf("'\n");
    }
}

int main(int argc, char **argv)
{
    atexit(cleanup_tmpfiles);
    arena_init(&g_arena, ARENA_DEFAULT_BLOCK);
    struct options opt;
    parse_args(argc, argv, &opt);

    char *pp_tmp = mktmp(".pp");
    preprocess(opt.input, pp_tmp, &opt);

    if (opt.dump_tokens) {
        dump_tokens(&g_arena, pp_tmp);
        return 0;
    }
    arena_reset(&g_arena);

    if (opt.stop == STOP_COMPILE) {
        char *out = opt.output ? strdup(opt.output)
                               : default_output(opt.input, STOP_COMPILE);
        compile(pp_tmp, out, &opt);
        arena_reset(&g_arena);
        free(out);
        return 0;
    }

    char *s_tmp = mktmp(".s");
    compile(pp_tmp, s_tmp, &opt);
    arena_reset(&g_arena);

    if (opt.stop == STOP_ASSEMBLE) {
        char *out = opt.output ? strdup(opt.output)
                               : default_output(opt.input, STOP_ASSEMBLE);
        assemble(s_tmp, out, &opt);
        arena_reset(&g_arena);
        free(out);
        return 0;
    }

    char *o_tmp = mktmp(".o");
    assemble(s_tmp, o_tmp, &opt);
    arena_reset(&g_arena);

    char *out = opt.output ? strdup(opt.output)
                           : default_output(opt.input, STOP_LINK);
    link_objs(o_tmp, out, &opt);
    arena_reset(&g_arena);
    free(out);
    return 0;
}
