#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>

#include "mdvic/lint.h"

static const char *skip_spaces(const char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

static int fence_len_at(const char *s, char mark) {
    int n = 0; while (*s == mark) { n++; s++; }
    return n;
}

int mdvic_lint_buffer(const char *data, size_t len, FILE *err, const char *filename) {
    size_t i = 0; int line = 1; int issues = 0;
    int in_fence = 0; char fence_mark = 0; int fence_len = 0; int fence_line = 0;
    int inline_tick_line = 0;

    while (i < len) {
        size_t line_start = i; size_t line_end = i;
        while (line_end < len && data[line_end] != '\n') line_end++;
        const char *s = data + line_start; size_t slen = line_end - line_start;

        /* Detect fenced code blocks */
        const char *t = skip_spaces(s);
        int prev_in_fence = in_fence;
        int fence_event = 0; /* 1=open, 2=close */
        if (!in_fence && slen >= 3) {
            if (*t == '`' || *t == '~') {
                int n = fence_len_at(t, *t);
                if (n >= 3) { in_fence = 1; fence_mark = *t; fence_len = n; fence_line = line; fence_event = 1; }
            }
        } else if (in_fence) {
            /* Closing fence: same mark and at least fence_len */
            if (*t == fence_mark) {
                int n = fence_len_at(t, *t);
                if (n >= fence_len) { in_fence = 0; fence_mark = 0; fence_len = 0; fence_line = 0; fence_event = 2; }
            }
        }

        /* Unmatched inline code span per line (ignore inside fences and fence marker lines) */
        if (!prev_in_fence && fence_event == 0) {
            int open = 0; int run = 0; const char *p = s; int col = 1;
            while ((size_t)(p - s) < slen) {
                char c = *p++;
                if (c == '`') {
                    run++;
                    continue;
                }
                if (run > 0) {
                    if (run == 1) {
                        if (!open) { open = 1; inline_tick_line = line; }
                        else { open = 0; }
                    }
                    run = 0;
                }
                (void)col; col++;
            }
            if (run > 0) {
                if (run == 1) {
                    if (!open) { open = 1; inline_tick_line = line; }
                    else { open = 0; }
                }
            }
            if (open) {
                fprintf(err, "%s:%d: unmatched inline code backticks\n", filename ? filename : "-", inline_tick_line);
                issues++;
            }
        }

        i = (line_end < len) ? (line_end + 1) : line_end;
        line++;
    }

    if (in_fence) {
        fprintf(err, "%s:%d: unclosed code fence\n", filename ? filename : "-", fence_line);
        issues++;
    }

    return issues;
}
