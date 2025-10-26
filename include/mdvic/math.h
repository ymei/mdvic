#ifndef MDVIC_MATH_H
#define MDVIC_MATH_H

#include <stddef.h>
#include "mdvic.h"

/* Render a math segment (content inside $...$ or $$...$$) to a UTF-8 string.
 * Caller must free(*out_str). Returns 0 on success.
 * Honors opt->math_mode (unicode|ascii).
 */
int mdvic_math_render(const char *s, size_t len, const struct MdvicOptions *opt,
                      char **out_str, size_t *out_len);

#endif /* MDVIC_MATH_H */
