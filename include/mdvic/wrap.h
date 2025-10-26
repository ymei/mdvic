#ifndef MDVIC_WRAP_H
#define MDVIC_WRAP_H

#include <stdio.h>
#include <stddef.h>

/* Write string with soft wrap at given width. Maintains column via *col. */
int mdvic_wrap_write(FILE *out, const char *s, size_t len, int width, int *col);

#endif /* MDVIC_WRAP_H */

