/* Minimal math renderer: Greek, superscripts/subscripts, \frac, \sqrt. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "mdvic/math.h"

struct mbuf { char *p; size_t n; size_t cap; };

static int mb_grow(struct mbuf *b, size_t add) {
    size_t need = b->n + add + 1;
    if (need <= b->cap) return 0;
    size_t ncap = b->cap ? b->cap * 2 : 128;
    while (ncap < need) ncap *= 2;
    char *np = (char *)realloc(b->p, ncap);
    if (!np) return -1;
    b->p = np; b->cap = ncap; return 0;
}
static int mb_putc(struct mbuf *b, char c) {
    if (mb_grow(b, 1) != 0) return -1;
    b->p[b->n++] = c; b->p[b->n] = '\0'; return 0;
}
static int mb_puts(struct mbuf *b, const char *s) {
    size_t m = strlen(s);
    if (mb_grow(b, m) != 0) return -1;
    memcpy(b->p + b->n, s, m); b->n += m; b->p[b->n] = '\0'; return 0;
}

struct map { const char *key; const char *val; };

static const struct map greek_map[] = {
    {"alpha", "α"}, {"beta", "β"}, {"gamma", "γ"}, {"delta", "δ"}, {"epsilon","ε"},
    {"zeta","ζ"}, {"eta","η"}, {"theta","θ"}, {"iota","ι"}, {"kappa","κ"},
    {"lambda","λ"}, {"mu","μ"}, {"nu","ν"}, {"xi","ξ"}, {"pi","π"},
    {"rho","ρ"}, {"sigma","σ"}, {"tau","τ"}, {"upsilon","υ"}, {"phi","φ"},
    {"chi","χ"}, {"psi","ψ"}, {"omega","ω"},
    {"Gamma","Γ"}, {"Delta","Δ"}, {"Theta","Θ"}, {"Lambda","Λ"}, {"Xi","Ξ"},
    {"Pi","Π"}, {"Sigma","Σ"}, {"Upsilon","Υ"}, {"Phi","Φ"}, {"Psi","Ψ"}, {"Omega","Ω"},
    {NULL,NULL}
};

static const struct map sym_map[] = {
    {"pm","±"}, {"times","×"}, {"cdot","·"}, {"partial","∂"}, {"nabla","∇"},
    {"infty","∞"}, {"le","≤"}, {"ge","≥"}, {"neq","≠"}, {NULL,NULL}
};

static const char *sup_char(int ch) {
    switch (ch) {
    case '0': return "⁰"; case '1': return "¹"; case '2': return "²"; case '3': return "³";
    case '4': return "⁴"; case '5': return "⁵"; case '6': return "⁶"; case '7': return "⁷";
    case '8': return "⁸"; case '9': return "⁹"; case '+': return "⁺"; case '-': return "⁻";
    case '=': return "⁼"; case '(': return "⁽"; case ')': return "⁾"; case 'n': return "ⁿ"; case 'i': return "ⁱ";
    default: return NULL;
    }
}
static const char *sub_char(int ch) {
    switch (ch) {
    case '0': return "₀"; case '1': return "₁"; case '2': return "₂"; case '3': return "₃";
    case '4': return "₄"; case '5': return "₅"; case '6': return "₆"; case '7': return "₇";
    case '8': return "₈"; case '9': return "₉"; case '+': return "₊"; case '-': return "₋";
    case '=': return "₌"; case '(': return "₍"; case ')': return "₎";
    case 'a': return "ₐ"; case 'e': return "ₑ"; case 'o': return "ₒ"; case 'x': return "ₓ";
    case 'h': return "ₕ"; case 'k': return "ₖ"; case 'l': return "ₗ"; case 'm': return "ₘ";
    case 'n': return "ₙ"; case 'p': return "ₚ"; case 's': return "ₛ"; case 't': return "ₜ";
    default: return NULL;
    }
}

static int emit_sup(struct mbuf *b, const char *s, size_t len, int unicode) {
    if (!unicode) {
        if (mb_putc(b, '^') != 0) return -1;
        if (len > 1) { if (mb_putc(b, '(') != 0) return -1; }
        if (mb_grow(b, len) != 0) return -1; memcpy(b->p + b->n, s, len); b->n += len; b->p[b->n] = '\0';
        if (len > 1) { if (mb_putc(b, ')') != 0) return -1; }
        return 0;
    }
    size_t i;
    for (i = 0; i < len; i++) {
        const char *m = sup_char((unsigned char)s[i]);
        if (m) { if (mb_puts(b, m) != 0) return -1; }
        else { if (mb_putc(b, s[i]) != 0) return -1; }
    }
    return 0;
}
static int emit_sub(struct mbuf *b, const char *s, size_t len, int unicode) {
    if (!unicode) {
        if (mb_putc(b, '_') != 0) return -1;
        if (len > 1) { if (mb_putc(b, '(') != 0) return -1; }
        if (mb_grow(b, len) != 0) return -1; memcpy(b->p + b->n, s, len); b->n += len; b->p[b->n] = '\0';
        if (len > 1) { if (mb_putc(b, ')') != 0) return -1; }
        return 0;
    }
    size_t i;
    for (i = 0; i < len; i++) {
        const char *m = sub_char((unsigned char)s[i]);
        if (m) { if (mb_puts(b, m) != 0) return -1; }
        else { if (mb_putc(b, s[i]) != 0) return -1; }
    }
    return 0;
}

static int render_subexpr(const char *s, size_t len, struct mbuf *out, const struct MdvicOptions *opt);

static int render_command(const char *s, size_t n, size_t *pos, struct mbuf *out,
                          const struct MdvicOptions *opt) {
    size_t i = *pos + 1;
    size_t start = i;
    while (i < n && ((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z'))) i++;
    size_t clen = i - start;
    if (clen == 0) { /* solitary backslash */ return mb_putc(out, '\\'); }
    char name[32]; if (clen >= sizeof(name)) clen = sizeof(name) - 1; memcpy(name, s + start, clen); name[clen] = '\0';
    *pos = i;
    /* Greek */
    const struct map *m;
    for (m = greek_map; m->key; m++) {
        if (strcmp(name, m->key) == 0) return mb_puts(out, (opt && opt->math_mode == MDVIC_MATH_ASCII) ? name : m->val);
    }
    /* Symbols */
    for (m = sym_map; m->key; m++) {
        if (strcmp(name, m->key) == 0) return mb_puts(out, (opt && opt->math_mode == MDVIC_MATH_ASCII) ? name : m->val);
    }
    if (strcmp(name, "sqrt") == 0) {
        /* optional [n] */
        int ascii = (opt && opt->math_mode == MDVIC_MATH_ASCII);
        int ascii_root_prefix = 0;
        if (*pos < n && s[*pos] == '[') {
            size_t j = *pos + 1; size_t idx_start = j; while (j < n && s[j] != ']') j++; size_t idx_len = (j < n) ? (j - idx_start) : 0; *pos = (j < n) ? (j + 1) : j;
            if (ascii) {
                mb_puts(out, "root["); if (idx_len) { mb_grow(out, idx_len); memcpy(out->p + out->n, s + idx_start, idx_len); out->n += idx_len; out->p[out->n] = '\0'; } mb_puts(out, "]");
                ascii_root_prefix = 1;
            } else {
                /* superscript index */
                emit_sup(out, s + idx_start, idx_len, 1);
            }
        }
        if (*pos < n && s[*pos] == '{') {
            (*pos)++; size_t depth = 1; size_t startc = *pos;
            while (*pos < n && depth) { if (s[*pos] == '{') depth++; else if (s[*pos] == '}') depth--; (*pos)++; }
            size_t arglen = (*pos - startc) - 1;
            if (ascii) {
                if (ascii_root_prefix) {
                    mb_putc(out, '(');
                    struct mbuf tmp = {0}; render_subexpr(s + startc, arglen, &tmp, opt); mb_puts(out, tmp.p ? tmp.p : ""); free(tmp.p);
                    mb_putc(out, ')');
                } else {
                    mb_puts(out, "sqrt("); struct mbuf tmp = {0}; render_subexpr(s + startc, arglen, &tmp, opt); mb_puts(out, tmp.p ? tmp.p : ""); free(tmp.p); mb_putc(out, ')');
                }
            } else {
                mb_puts(out, "√("); struct mbuf tmp = {0}; render_subexpr(s + startc, arglen, &tmp, opt); mb_puts(out, tmp.p ? tmp.p : ""); free(tmp.p); mb_putc(out, ')');
            }
            return 0;
        }
        return mb_puts(out, "√");
    }
    if (strcmp(name, "frac") == 0) {
        /* {a}{b} */
        if (*pos < n && s[*pos] == '{') {
            (*pos)++; size_t depth = 1; size_t astart = *pos; while (*pos < n && depth) { if (s[*pos] == '{') depth++; else if (s[*pos] == '}') depth--; (*pos)++; }
            size_t alen = (*pos - astart) - 1;
            if (*pos < n && s[*pos] == '{') {
                (*pos)++; depth = 1; size_t bstart = *pos; while (*pos < n && depth) { if (s[*pos] == '{') depth++; else if (s[*pos] == '}') depth--; (*pos)++; }
                size_t blen = (*pos - bstart) - 1;
                struct mbuf num = {0}, den = {0};
                render_subexpr(s + astart, alen, &num, opt);
                render_subexpr(s + bstart, blen, &den, opt);
                if (opt && opt->math_mode == MDVIC_MATH_ASCII) { mb_puts(out, num.p ? num.p : ""); mb_putc(out, '/'); mb_puts(out, den.p ? den.p : ""); }
                else { mb_puts(out, num.p ? num.p : ""); mb_puts(out, "⁄"); mb_puts(out, den.p ? den.p : ""); }
                free(num.p); free(den.p);
                return 0;
            }
        }
        return mb_puts(out, "frac");
    }
    /* Unknown command: emit as-is without backslash */
    return mb_puts(out, name);
}

static int render_subexpr(const char *s, size_t len, struct mbuf *out, const struct MdvicOptions *opt) {
    size_t i = 0; int unicode = (!opt || opt->math_mode == MDVIC_MATH_UNICODE);
    while (i < len) {
        char c = s[i];
        if (c == '\\') { if (render_command(s, len, &i, out, opt) != 0) return -1; continue; }
        else if (c == '^' || c == '_') { int is_sup = (c == '^'); i++; if (i < len && s[i] == '{') { size_t start = ++i; int depth = 1; while (i < len && depth) { if (s[i] == '{') depth++; else if (s[i] == '}') depth--; i++; } size_t clen = (i - start) - 1; if (is_sup) { if (emit_sup(out, s + start, clen, unicode) != 0) return -1; } else { if (emit_sub(out, s + start, clen, unicode) != 0) return -1; } continue; } else if (i < len) { if (is_sup) { if (emit_sup(out, s + i, 1, unicode) != 0) return -1; } else { if (emit_sub(out, s + i, 1, unicode) != 0) return -1; } i++; continue; } }
        if (mb_putc(out, c) != 0) return -1; i++;
    }
    return 0;
}

int mdvic_math_render(const char *s, size_t len, const struct MdvicOptions *opt,
                      char **out_str, size_t *out_len) {
    struct mbuf out; out.p = NULL; out.n = 0; out.cap = 0;
    size_t i = 0;
    int unicode = (!opt || opt->math_mode == MDVIC_MATH_UNICODE);
    while (i < len) {
        char c = s[i];
        if (c == '\\') {
            if (render_command(s, len, &i, &out, opt) != 0) { free(out.p); return -1; }
            continue;
        } else if (c == '^' || c == '_') {
            int is_sup = (c == '^');
            i++;
            if (i < len && s[i] == '{') {
                size_t start = ++i; int depth = 1;
                while (i < len && depth) { if (s[i] == '{') depth++; else if (s[i] == '}') depth--; i++; }
                size_t clen = (i - start) - 1;
                if (is_sup) { if (emit_sup(&out, s + start, clen, unicode) != 0) { free(out.p); return -1; } }
                else { if (emit_sub(&out, s + start, clen, unicode) != 0) { free(out.p); return -1; } }
                continue;
            } else if (i < len) {
                if (is_sup) { if (emit_sup(&out, s + i, 1, unicode) != 0) { free(out.p); return -1; } }
                else { if (emit_sub(&out, s + i, 1, unicode) != 0) { free(out.p); return -1; } }
                i++;
                continue;
            }
        }
        /* default: copy */
        if (mb_putc(&out, c) != 0) { free(out.p); return -1; }
        i++;
    }
    if (!out.p) { out.p = (char *)malloc(1); if (!out.p) return -1; out.p[0] = '\0'; }
    if (out_len) *out_len = out.n;
    *out_str = out.p;
    return 0;
}
