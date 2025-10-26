#ifndef MDVIC_WRAP_H
#define MDVIC_WRAP_H

#include <stdio.h>
#include <stddef.h>

/*
 * Write string with soft wrap at given width. Maintains column via *col.
 * - Ignores ANSI CSI sequences (ESC[...final) for width counting.
 * - Ignores OSC-8 hyperlinks (ESC]8;;...BEL or ST) for width counting.
 * - Treats newline as hard break and resets column.
 * - Simple greedy wrapping may break in the middle of words.
 */
int mdvic_wrap_write_pref2(FILE *out, const char *s, size_t len, int width, int *col,
                           const char *prefix_first, int prefix_first_len,
                           const char *prefix_next, int prefix_next_len);
int mdvic_wrap_write_pref(FILE *out, const char *s, size_t len, int width, int *col,
                          const char *prefix, int prefix_len);
int mdvic_wrap_write(FILE *out, const char *s, size_t len, int width, int *col);

#endif /* MDVIC_WRAP_H */
