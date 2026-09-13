#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PROG_NAME "mgcc"
#define PROG_VERSION "0.1.0"

static void print_version(void)
{
    printf("%s %s\n", PROG_NAME, PROG_VERSION);
    printf("Self-compiling C compiler for testing Mistral Vibe\n");
}

static void print_help(void)
{
    printf("Usage: %s [options] file...\n", PROG_NAME);
    printf("Options:\n");
    printf("  --help            Display this information\n");
    printf("  --version         Display compiler version information\n");
    printf("  -c                Compile and assemble, do not link\n");
    printf("  -S                Compile only, do not assemble or link\n");
    printf("  -o <file>         Place output into <file>\n");
    printf("  -I<dir>           Add <dir> to the end of the main include path\n");
    printf("  -L<dir>           Add <dir> to the end of the library search path\n");
    printf("  -l<lib>           Link against library <lib>\n");
}

static const char *not_impl = "mgcc: not yet implemented";

static void handle_compile_only(void)
{
    printf("%s: -S (compile to assembly)\n", not_impl);
}

static void handle_compile_assemble(void)
{
    printf("%s: -c (compile to object file)\n", not_impl);
}

static void handle_link(void)
{
    printf("%s: full link step\n", not_impl);
}

static void handle_include_dir(const char *dir)
{
    printf("%s: add include directory '%s'\n", not_impl, dir);
}

static void handle_lib_dir(const char *dir)
{
    printf("%s: add library directory '%s'\n", not_impl, dir);
}

static void handle_library(const char *lib)
{
    printf("%s: link library '%s'\n", not_impl, lib);
}

static void handle_output(const char *file)
{
    printf("%s: output file '%s'\n", not_impl, file);
}

static int parse_option(const char *arg, int *idx, int argc, char **argv)
{
    if (strcmp(arg, "--help") == 0) {
        print_help();
        exit(0);
    }
    if (strcmp(arg, "--version") == 0) {
        print_version();
        exit(0);
    }
    if (arg[0] != '-' || arg[1] == '\0')
        return 0;

    if (arg[1] == 'I') {
        handle_include_dir(arg + 2);
        return 1;
    }
    if (arg[1] == 'L') {
        handle_lib_dir(arg + 2);
        return 1;
    }
    if (arg[1] == 'l') {
        handle_library(arg + 2);
        return 1;
    }
    if (arg[1] == 'o' && arg[2] == '\0') {
        if (*idx + 1 >= argc) {
            fprintf(stderr, "%s: option '-o' requires an argument\n", PROG_NAME);
            exit(2);
        }
        handle_output(argv[++(*idx)]);
        return 1;
    }
    if (arg[1] == 'S' && arg[2] == '\0') {
        handle_compile_only();
        return 1;
    }
    if (arg[1] == 'c' && arg[2] == '\0') {
        handle_compile_assemble();
        return 1;
    }

    fprintf(stderr, "%s: unrecognized option '%s'\n", PROG_NAME, arg);
    return -1;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        print_help();
        return 0;
    }

    int saw_action = 0;
    int saw_source = 0;
    int status = 0;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        int r = parse_option(arg, &i, argc, argv);
        if (r < 0) {
            status = 1;
        } else if (r == 1) {
            if (strcmp(arg, "-S") == 0 || strcmp(arg, "-c") == 0)
                saw_action = 1;
        } else {
            saw_source = 1;
            printf("%s: compile source '%s'\n", not_impl, arg);
        }
    }

    if (status == 0 && saw_source && !saw_action)
        handle_link();

    return status;
}
