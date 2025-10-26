#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>

#include "mdvic/mdvic.h"
#include "mdvic/wrap.h"

int mdvic_render_stream(FILE *in, FILE *out, const struct MdvicOptions *opt, const char *filename) {
    (void)filename; /* reserved for diagnostics */
    int width = opt ? opt->width : 0;
    int col = 0;

    char buf[4096];

#ifdef HAVE_LIBCMARK
    /* Placeholder: future integration with cmark AST renderer.
       For the skeleton, we passthrough input and wrap. */
#endif

    while (fgets(buf, (int)sizeof(buf), in) != NULL) {
        size_t len = strlen(buf);
        if (mdvic_wrap_write(out, buf, len, width, &col) != 0) return -1;
    }
    /* Ensure trailing newline ends the line for a neat prompt return */
    if (col != 0) {
        fputc('\n', out);
    }
    return 0;
}

void mdvic_apply_env_overrides(struct MdvicOptions *opt) {
    if (!opt) return;
    const char *no_color = getenv("MDVIC_NO_COLOR");
    if (no_color && no_color[0] != '\0') {
        opt->no_color = true;
    }
    const char *width = getenv("MDVIC_WIDTH");
    if (width && width[0] != '\0') {
        char *end = NULL;
        long v = strtol(width, &end, 10);
        if (end != width && (*end == '\0') && v > 0 && v < 1000000) {
            opt->width = (int)v;
        }
    }
    const char *math = getenv("MDVIC_MATH");
    if (math && math[0] != '\0') {
        if (strcmp(math, "ascii") == 0) opt->math_mode = MDVIC_MATH_ASCII;
        else if (strcmp(math, "unicode") == 0) opt->math_mode = MDVIC_MATH_UNICODE;
    }
}

int mdvic_detect_width(void) {
    /* Minimal detection: honor COLUMNS env; otherwise unknown. */
    const char *cols = getenv("COLUMNS");
    if (cols && cols[0] != '\0') {
        char *end = NULL;
        long v = strtol(cols, &end, 10);
        if (end != cols && (*end == '\0') && v > 0 && v < 1000000) {
            return (int)v;
        }
    }
    return 0;
}
