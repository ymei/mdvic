#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "mdvic/wrap.h"
#include "mdvic/wcwidth.h"

static int byte_to_width(unsigned char c) {
    /* ASCII printable range gets width 1; newlines handled by caller */
    if (c >= 0x20 && c <= 0x7E) return 1;
    /* UTF-8 continuation or multibyte lead: treat as width 1 in skeleton */
    return 1;
}

int mdvic_wrap_write(FILE *out, const char *s, size_t len, int width, int *col) {
    int ccol = col ? *col : 0;
    size_t i;
    for (i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == '\n') {
            if (fputc('\n', out) == EOF) return -1;
            ccol = 0;
            continue;
        }
        if (width > 0 && ccol >= width) {
            if (fputc('\n', out) == EOF) return -1;
            ccol = 0;
        }
        if (fputc((int)ch, out) == EOF) return -1;
        ccol += (width > 0) ? byte_to_width(ch) : 0;
    }
    if (col) *col = ccol;
    return 0;
}

