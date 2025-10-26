/* mdvic: Minimal terminal Markdown + math renderer (skeleton) */
#ifndef MDVIC_MDVIC_H
#define MDVIC_MDVIC_H

#include <stdio.h>
#include <stdbool.h>

#define MDVIC_VERSION "0.0.1-dev"

enum mdvic_math_mode {
    MDVIC_MATH_UNICODE = 0,
    MDVIC_MATH_ASCII = 1
};

struct MdvicOptions {
    bool no_color;
    int width; /* 0 = auto-detect */
    enum mdvic_math_mode math_mode;
};

/* Apply environment overrides (MDVIC_NO_COLOR, MDVIC_WIDTH, MDVIC_MATH). */
void mdvic_apply_env_overrides(struct MdvicOptions *opt);

/* Best-effort detection of terminal width; returns 0 if unknown. */
int mdvic_detect_width(void);

/* Render a single input stream to out, honoring options. */
int mdvic_render_stream(FILE *in, FILE *out, const struct MdvicOptions *opt, const char *filename);

#endif /* MDVIC_MDVIC_H */

