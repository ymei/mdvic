#include "mdvic/wcwidth.h"

/* Prefer generated table if available */
#if __has_include("mdvic/wcwidth_table.h")
#include "mdvic/wcwidth_table.h"
#define MDVIC_HAVE_WCWIDTH_TABLE 1
#endif

struct interval { uint32_t first; uint32_t last; };

static int in_intervals(uint32_t ucs, const struct interval *table, int n) {
    /* table is assumed sorted and non-overlapping */
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        uint32_t a = table[mid].first, b = table[mid].last;
        if (ucs < a) hi = mid - 1;
        else if (ucs > b) lo = mid + 1;
        else return 1;
    }
    return 0;
}

#ifdef MDVIC_HAVE_WCWIDTH_TABLE
int mdvic_wcwidth(uint32_t ucs) {
    if (ucs == 0) return 0;
    if (ucs < 0x20u) return 0; /* C0 */
    if (ucs >= 0x7F && ucs < 0xA0) return 0; /* C1 */
    if (ucs >= 0x20u && ucs <= 0x7Eu) return 1; /* ASCII */
    if (in_intervals(ucs, mdvic_combining, (int)MDVIC_COMBINING_COUNT)) return 0;
    if (in_intervals(ucs, mdvic_wide, (int)MDVIC_WIDE_COUNT)) return 2;
    return 1;
}
#else
/* Fallback heuristic when no table is generated */
static const struct interval combining_fallback[] = {
    {0x0300,0x036F}, {0x1AB0,0x1AFF}, {0x1DC0,0x1DFF}, {0x20D0,0x20FF}, {0xFE20,0xFE2F}
};
static const struct interval wide_fallback[] = {
    {0x1100,0x115F}, {0x2329,0x2329}, {0x232A,0x232A},
    {0x2E80,0xA4CF}, {0xAC00,0xD7A3}, {0xF900,0xFAFF}, {0xFE10,0xFE19},
    {0xFE30,0xFE6F}, {0xFF00,0xFF60}, {0xFFE0,0xFFE6},
    {0x1F300,0x1F64F}, {0x1F680,0x1F6FF}, {0x1F900,0x1F9FF}, {0x1FA70,0x1FAFF},
    {0x1F1E6,0x1F1FF}
};
int mdvic_wcwidth(uint32_t ucs) {
    if (ucs == 0) return 0;
    if (ucs < 0x20u) return 0; /* C0 */
    if (ucs >= 0x7F && ucs < 0xA0) return 0; /* C1 */
    if (ucs >= 0x20u && ucs <= 0x7Eu) return 1; /* ASCII */
    if (in_intervals(ucs, combining_fallback, (int)(sizeof(combining_fallback)/sizeof(combining_fallback[0])))) return 0;
    if (in_intervals(ucs, wide_fallback, (int)(sizeof(wide_fallback)/sizeof(wide_fallback[0])))) return 2;
    return 1;
}
#endif
