#include <stdio.h>
#include <stddef.h>
#include "mdvic/math.h"

int mdvic_render_math_segment(const char *s, size_t len, FILE *out, const struct MdvicOptions *opt) {
    (void)opt;
    /* Skeleton: passthrough without transformation */
    size_t i;
    for (i = 0; i < len; i++) {
        if (fputc((unsigned char)s[i], out) == EOF) return -1;
    }
    return 0;
}

