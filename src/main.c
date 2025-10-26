#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include "mdvic/mdvic.h"

static void print_usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s [--no-color] [--no-lint] [--wrap|--no-wrap] [--width N] [--math {unicode|ascii}] [FILE...]\n",
            prog);
}

static void print_version(void) {
    printf("mdvic %s\n", MDVIC_VERSION);
}

static int parse_int(const char *s, int *out) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || (end && *end != '\0')) return -1;
    if (v < 0 || v > 1000000) return -1;
    *out = (int)v;
    return 0;
}

int main(int argc, char **argv) {
    struct MdvicOptions opt;
    opt.no_color = false;
    opt.width = 0;
    opt.math_mode = MDVIC_MATH_UNICODE;
    opt.enable_lint = true;
    opt.enable_wrap = false; /* default no-wrap */
    opt.enable_osc8 = true;
    opt.accent_group = false; /* default: accent last char */

    mdvic_apply_env_overrides(&opt);

    const char *prog = (argc > 0 && argv[0]) ? argv[0] : "mdvic";

    int i = 1;
    while (i < argc) {
        const char *arg = argv[i];
        if (strcmp(arg, "--") == 0) { i++; break; }
        if (strcmp(arg, "-") == 0) { break; }
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_usage(prog);
            return 0;
        } else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0) {
            print_version();
            return 0;
        } else if (strcmp(arg, "--no-color") == 0) {
            opt.no_color = true;
            i++;
        } else if (strncmp(arg, "--width=", 8) == 0) {
            int w = 0; if (parse_int(arg + 8, &w) != 0) { fprintf(stderr, "Invalid width: %s\n", arg+8); return 2; }
            opt.width = w; i++;
        } else if (strcmp(arg, "--width") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "--width requires a value\n"); return 2; }
            int w = 0; if (parse_int(argv[i+1], &w) != 0) { fprintf(stderr, "Invalid width: %s\n", argv[i+1]); return 2; }
            opt.width = w; i += 2;
        } else if (strncmp(arg, "--math=", 8) == 0) {
            const char *m = arg + 8;
            if (strcmp(m, "unicode") == 0) opt.math_mode = MDVIC_MATH_UNICODE;
            else if (strcmp(m, "ascii") == 0) opt.math_mode = MDVIC_MATH_ASCII;
            else { fprintf(stderr, "Invalid math mode: %s\n", m); return 2; }
            i++;
        } else if (strcmp(arg, "--math") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "--math requires a value\n"); return 2; }
            const char *m = argv[i+1];
            if (strcmp(m, "unicode") == 0) opt.math_mode = MDVIC_MATH_UNICODE;
            else if (strcmp(m, "ascii") == 0) opt.math_mode = MDVIC_MATH_ASCII;
            else { fprintf(stderr, "Invalid math mode: %s\n", m); return 2; }
            i += 2;
        } else if (strcmp(arg, "--no-lint") == 0) {
            opt.enable_lint = false; i++;
        } else if (strcmp(arg, "--lint") == 0) {
            opt.enable_lint = true; i++;
        } else if (strcmp(arg, "--wrap") == 0) {
            opt.enable_wrap = true; i++;
        } else if (strcmp(arg, "--no-wrap") == 0) {
            opt.enable_wrap = false; i++;
        } else if (strcmp(arg, "--no-osc8") == 0) {
            opt.enable_osc8 = false; i++;
        } else if (strcmp(arg, "--osc8") == 0) {
            opt.enable_osc8 = true; i++;
        } else if (strncmp(arg, "--accent=", 9) == 0) {
            const char *am = arg + 9;
            if (strcmp(am, "last") == 0) opt.accent_group = false;
            else if (strcmp(am, "group") == 0) opt.accent_group = true;
            else { fprintf(stderr, "Invalid accent mode: %s\n", am); return 2; }
            i++;
        } else if (arg[0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", arg);
            print_usage(prog);
            return 2;
        } else {
            break;
        }
    }

    if (opt.enable_wrap && opt.width == 0) {
        int detected = mdvic_detect_width();
        if (detected > 0) opt.width = detected;
    }

    int exit_code = 0;
    if (i >= argc) {
        if (mdvic_render_stream(stdin, stdout, &opt, "-") != 0) {
            exit_code = 1;
        }
    } else {
        int first = 1;
        for (; i < argc; i++) {
            const char *path = argv[i];
            FILE *fp = NULL;
            if (strcmp(path, "-") == 0) {
                fp = stdin;
            } else {
                fp = fopen(path, "rb");
                if (!fp) {
                    fprintf(stderr, "mdvic: cannot open '%s': %s\n", path, strerror(errno));
                    exit_code = 1;
                    continue;
                }
            }
            if (!first) {
                /* Separator line between files */
                if (opt.no_color) fputs("\n---\n", stdout);
                else fputs("\n\x1b[2m---\x1b[0m\n", stdout);
            }
            if (mdvic_render_stream(fp, stdout, &opt, path) != 0) {
                exit_code = 1;
            }
            if (fp != stdin) fclose(fp);
            first = 0;
        }
    }

    return exit_code;
}
