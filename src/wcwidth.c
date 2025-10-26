#include "mdvic/wcwidth.h"

/*
 * Minimal placeholder implementation. For now:
 *  - ASCII printable: width 1
 *  - Control chars: width 0
 *  - Everything else: width 1
 * This will be replaced by a compiled Unicode table for correctness.
 */
int mdvic_wcwidth(uint32_t ucs) {
    if (ucs < 0x20u) return 0;
    if (ucs >= 0x20u && ucs <= 0x7Eu) return 1;
    return 1;
}

