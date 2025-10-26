/* Minimal math renderer: Greek, superscripts/subscripts, \frac, \sqrt. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "mdvic/math.h"

struct mbuf { char *p; size_t n; size_t cap; };
static int g_display_hint = 0;
static size_t disp_len(const char *p) { return p ? strlen(p) : 0; }

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

static void ensure_space_before(struct mbuf *out) {
    if (out->n > 0) {
        char prev = out->p[out->n - 1];
        if (prev != ' ' && prev != '\n') {
            mb_putc(out, ' ');
        }
    }
}
static void ensure_space_after(struct mbuf *out, const char *s, size_t pos, size_t n) {
    if (pos >= n) return;
    char nx = s[pos];
    if (nx == ' ' || nx == '\n' || nx == '\t') return;
    if (nx == ',' || nx == ';' || nx == ':' || nx == ')' || nx == ']' || nx == '}' ) return;
    mb_putc(out, ' ');
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
    {"varepsilon","ϵ"}, {"varphi","ϕ"}, {"vartheta","ϑ"}, {"varrho","ϱ"}, {"varsigma","ς"},
    {NULL,NULL}
};

static const struct map sym_map[] = {
    {"pm","±"}, {"times","×"}, {"cdot","·"}, {"partial","∂"}, {"nabla","∇"},
    {"infty","∞"}, {"le","≤"}, {"ge","≥"}, {"neq","≠"},
    {"leq","≤"}, {"geq","≥"}, {"approx","≈"}, {"sim","∼"},
    {"in","∈"}, {"notin","∉"}, {"subset","⊂"}, {"supset","⊃"},
    {"subseteq","⊆"}, {"supseteq","⊇"}, {"to","→"}, {"rightarrow","→"},
    {"leftarrow","←"}, {"leftrightarrow","↔"}, {"Rightarrow","⇒"},
    {"Leftarrow","⇐"}, {"Leftrightarrow","⇔"}, {"cdots","⋯"}, {"ldots","…"},
    {"uparrow","↑"}, {"downarrow","↓"}, {"updownarrow","↕"},
    {"longrightarrow","⟶"}, {"longleftarrow","⟵"}, {"mapsto","↦"},
    {"hookrightarrow","↪"}, {"rightharpoonup","⇀"}, {"rightharpoondown","⇁"},
    {"implies","⇒"}, {"iff","⇔"},
    {"circ","∘"}, {"bullet","•"}, {"cdotp","·"},
    {NULL,NULL}
};

static const char *func_map(const char *name) {
    /* Render common functions in roman letters */
    if (strcmp(name, "sin") == 0) return "sin";
    if (strcmp(name, "cos") == 0) return "cos";
    if (strcmp(name, "tan") == 0) return "tan";
    if (strcmp(name, "log") == 0) return "log";
    if (strcmp(name, "ln") == 0) return "ln";
    if (strcmp(name, "exp") == 0) return "exp";
    if (strcmp(name, "lim") == 0) return "lim";
    return NULL;
}

static int mb_puts_combining_last(struct mbuf *out, const char *s, size_t len, const char *comb) {
    if (len == 0) return 0;
    /* copy all bytes */
    if (mb_grow(out, len + strlen(comb)) != 0) return -1;
    memcpy(out->p + out->n, s, len); out->n += len; out->p[out->n] = '\0';
    /* append combining mark to affect last codepoint */
    if (mb_puts(out, comb) != 0) return -1;
    return 0;
}

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
    /* Handle \left ... \right early to avoid prefix collisions */
    if (strcmp(name, "left") == 0) {
        char od = 0, cd = 0; const char *ol = NULL; const char *cl_from_left = NULL;
        if (*pos < n) {
            if (s[*pos] == '\\') {
                size_t i2 = *pos + 1, start2 = i2; while (i2 < n && ((s[i2]>='a'&&s[i2]<='z')||(s[i2]>='A'&&s[i2]<='Z'))) i2++;
                size_t klen = i2 - start2;
                if (klen > 0) { char kw[16]; if (klen>=sizeof(kw)) klen=sizeof(kw)-1; memcpy(kw,s+start2,klen); kw[klen]='\0';
                    if (strcmp(kw, "langle") == 0) { ol = (opt && opt->math_mode==MDVIC_MATH_ASCII)?"<":"⟨"; cl_from_left = (opt && opt->math_mode==MDVIC_MATH_ASCII)?">":"⟩"; *pos = i2; }
                    else if (strcmp(kw, "lceil") == 0) { ol = (opt && opt->math_mode==MDVIC_MATH_ASCII)?"[":"⌈"; cl_from_left = (opt && opt->math_mode==MDVIC_MATH_ASCII)?"]":"⌉"; *pos = i2; }
                    else if (strcmp(kw, "lfloor") == 0) { ol = (opt && opt->math_mode==MDVIC_MATH_ASCII)?"[":"⌊"; cl_from_left = (opt && opt->math_mode==MDVIC_MATH_ASCII)?"]":"⌋"; *pos = i2; }
                }
            } else { od = s[*pos]; (*pos)++; switch (od){case '(':cd=')';break;case '[':cd=']';break;case '{':cd='}';break;case '|':cd='|';break;default:cd=od;break;}}
        }
        const char *needle = "\\right"; const char *pend = NULL; size_t nn = strlen(needle);
        for (size_t k=*pos; k+nn <= n; k++) { if (strncmp(s+k, needle, nn)==0) { pend = s + k; break; } }
        size_t content_len = pend ? (size_t)(pend - (s + *pos)) : (n - *pos); if (content_len > 65536) content_len = 65536;
        struct mbuf inner = {0}; render_subexpr(s + *pos, content_len, &inner, opt);
        if (ol) { mb_puts(out, ol); } else { mb_putc(out, od?od:'('); }
        mb_puts(out, inner.p?inner.p:"");
        const char *cr = NULL; char rchar = 0;
        if (pend) { size_t p = (size_t)(pend - s) + nn; if (p < n) { if (s[p]=='\\') { size_t j2=p+1,st2=j2; while (j2<n && ((s[j2]>='a'&&s[j2]<='z')||(s[j2]>='A'&&s[j2]<='Z'))) j2++; size_t lk=j2-st2; if (lk>0){ char kw[16]; if (lk>=sizeof(kw)) lk=sizeof(kw)-1; memcpy(kw,s+st2,lk); kw[lk]='\0'; if (strcmp(kw,"rangle")==0) cr=(opt&&opt->math_mode==MDVIC_MATH_ASCII)?">":"⟩"; else if (strcmp(kw,"rceil")==0) cr=(opt&&opt->math_mode==MDVIC_MATH_ASCII)?"]":"⌉"; else if (strcmp(kw,"rfloor")==0) cr=(opt&&opt->math_mode==MDVIC_MATH_ASCII)?"]":"⌋"; } } else { rchar=s[p]; } } }
        if (cr) { mb_puts(out, cr); } else if (rchar) { mb_putc(out, rchar); } else if (cl_from_left) { mb_puts(out, cl_from_left); } else { mb_putc(out, cd?cd:')'); }
        free(inner.p);
        if (pend) { *pos = (size_t)(pend - s) + nn; if (*pos < n) { if (s[*pos] == '\\') { size_t i3=*pos+1; while (i3<n && ((s[i3]>='a'&&s[i3]<='z')||(s[i3]>='A'&&s[i3]<='Z'))) i3++; *pos=i3; } else { (*pos)++; } } }
        else { *pos = n; }
        return 0;
    }
    /* Greek */
    const struct map *m;
    for (m = greek_map; m->key; m++) {
        size_t klen = strlen(m->key);
        if (strncmp(name, m->key, klen) == 0) {
            if (clen > klen) { *pos -= (clen - klen); }
            return mb_puts(out, (opt && opt->math_mode == MDVIC_MATH_ASCII) ? name : m->val);
        }
    }
    /* Symbols */
    {
        const struct map *best = NULL; size_t best_len = 0;
        for (m = sym_map; m->key; m++) {
            size_t klen = strlen(m->key);
            if (klen > best_len && strncmp(name, m->key, klen) == 0) {
                best = m; best_len = klen;
            }
        }
        if (best) {
            if (clen > best_len) { *pos -= (clen - best_len); }
            const char *matched_key = best->key;
            const char *val = (opt && opt->math_mode == MDVIC_MATH_ASCII) ? matched_key : best->val;
            if (strcmp(matched_key, "cdot")==0 || strcmp(matched_key, "cdotp")==0 || strcmp(matched_key, "circ")==0 || strcmp(matched_key, "bullet")==0 ||
                strcmp(matched_key, "times")==0 || strcmp(matched_key, "in")==0 || strcmp(matched_key, "notin")==0 || strcmp(matched_key, "subset")==0 || strcmp(matched_key, "supset")==0 ||
                strcmp(matched_key, "subseteq")==0 || strcmp(matched_key, "supseteq")==0 ||
                strcmp(matched_key, "to")==0 || strcmp(matched_key, "rightarrow")==0 || strcmp(matched_key, "leftarrow")==0 || strcmp(matched_key, "leftrightarrow")==0 ||
                strcmp(matched_key, "Rightarrow")==0 || strcmp(matched_key, "Leftarrow")==0 || strcmp(matched_key, "Leftrightarrow")==0 || strcmp(matched_key, "implies")==0 || strcmp(matched_key, "iff")==0 ||
                strcmp(matched_key, "longrightarrow")==0 || strcmp(matched_key, "longleftarrow")==0 || strcmp(matched_key, "mapsto")==0 || strcmp(matched_key, "hookrightarrow")==0 ||
                strcmp(matched_key, "rightharpoonup")==0 || strcmp(matched_key, "rightharpoondown")==0) {
                ensure_space_before(out);
                mb_puts(out, val);
                ensure_space_after(out, s, *pos, n);
                return 0;
            }
            return mb_puts(out, val);
        }
    }
    /* Functions */
    const char *f = NULL; {
        /* support longest prefix for functions */
        const char *cand = func_map(name);
        if (!cand) {
            /* try shorter by trimming trailing letters */
            for (size_t L = clen; L > 0; L--) {
                char tmp[32]; if (L >= sizeof(tmp)) continue; memcpy(tmp, name, L); tmp[L]='\0'; const char *c2 = func_map(tmp); if (c2) { cand = c2; if (clen > L) *pos -= (clen - L); break; }
            }
        }
        f = cand;
    }
    if (f) {
        /* Add a space if next token is a variable/letter and not '(' or '{' */
        mb_puts(out, f);
        if (*pos < n) {
            char nx = s[*pos];
            if (nx != ' ' && nx != '\n' && nx != '\t' && nx != '(' && nx != '{' && nx != '[') {
                mb_putc(out, ' ');
            }
        }
        return 0;
    }

    /* Accents: \hat, \bar, \tilde, \vec, \overline */
    if (strcmp(name, "hat") == 0 || strcmp(name, "bar") == 0 || strcmp(name, "tilde") == 0 || strcmp(name, "vec") == 0 || strcmp(name, "overline") == 0 ||
        strcmp(name, "breve") == 0 || strcmp(name, "check") == 0 || strcmp(name, "acute") == 0 || strcmp(name, "grave") == 0 ||
        strcmp(name, "dot") == 0 || strcmp(name, "ddot") == 0 || strcmp(name, "underline") == 0) {
        int unicode = (!opt || opt->math_mode == MDVIC_MATH_UNICODE);
        int apply_group = (opt && opt->accent_group) ? 1 : 0;
        const char *comb = NULL;
        if (unicode) {
            if (strcmp(name, "hat") == 0) comb = "\xCC\x82"; /* U+0302 */
            else if (strcmp(name, "bar") == 0 || strcmp(name, "overline") == 0) comb = "\xCC\x84"; /* U+0304 */
            else if (strcmp(name, "tilde") == 0) comb = "\xCC\x83"; /* U+0303 */
            else if (strcmp(name, "vec") == 0) comb = "\xE2\x83\x97"; /* U+20D7 combining right arrow above */
            else if (strcmp(name, "breve") == 0) comb = "\xCC\x86"; /* U+0306 */
            else if (strcmp(name, "check") == 0) comb = "\xCC\x8C"; /* U+030C caron */
            else if (strcmp(name, "acute") == 0) comb = "\xCC\x81"; /* U+0301 */
            else if (strcmp(name, "grave") == 0) comb = "\xCC\x80"; /* U+0300 */
            else if (strcmp(name, "dot") == 0) comb = "\xCC\x87"; /* U+0307 */
            else if (strcmp(name, "ddot") == 0) comb = "\xCC\x88"; /* U+0308 */
            else if (strcmp(name, "underline") == 0) comb = "\xCC\xB2"; /* U+0332 combining low line */
        }
        if (*pos < n) {
            if (s[*pos] == '{') {
                size_t start = ++(*pos); int depth = 1; while (*pos < n && depth) { if (s[*pos] == '{') depth++; else if (s[*pos] == '}') depth--; (*pos)++; }
                size_t alen = (*pos - start) - 1;
                if (unicode && comb && !apply_group) {
                    /* apply to last character of group */
                    return mb_puts_combining_last(out, s + start, alen, comb);
                } else {
                    /* ASCII fallback */
                    mb_puts(out, name); mb_putc(out, '(');
                    if (mb_grow(out, alen) != 0) return -1; memcpy(out->p + out->n, s + start, alen); out->n += alen; out->p[out->n] = '\0';
                    mb_putc(out, ')');
                    return 0;
                }
            } else {
                /* next single char */
                if (unicode && comb) {
                    return mb_puts_combining_last(out, s + *pos, 1, comb), (*pos)++, 0;
                } else {
                    mb_puts(out, name); mb_putc(out, '('); mb_putc(out, s[*pos]); mb_putc(out, ')'); (*pos)++; return 0;
                }
            }
        }
        return 0;
    }
    if (strcmp(name, "text") == 0) {
        /* literal text inside braces */
        if (*pos < n && s[*pos] == '{') {
            size_t j = *pos + 1; int depth = 1; while (j < n && depth) { if (s[j] == '{') depth++; else if (s[j] == '}') depth--; j++; }
            size_t tlen = (j <= n) ? (j - (*pos + 1) - 0) : 0; /* exclude braces */
            if (tlen > 0) { if (mb_grow(out, tlen) != 0) return -1; memcpy(out->p + out->n, s + *pos + 1, tlen); out->n += tlen; out->p[out->n]='\0'; }
            *pos = (j <= n) ? j : n;
            return 0;
        }
        return 0;
    }
    if (strcmp(name, "sum") == 0 || strcmp(name, "prod") == 0) {
        int ascii = (opt && opt->math_mode == MDVIC_MATH_ASCII);
        const char *sym = (!ascii) ? ((name[0]=='s')?"∑":"∏") : ((name[0]=='s')?"sum":"prod");
        /* capture following _{...} and ^{...} if present */
        struct mbuf sub = {0}, sup = {0};
        for (int iter=0; iter<2; iter++) {
            if (*pos < n && (s[*pos] == '_' || s[*pos] == '^')) {
                int is_sub = (s[*pos] == '_');
                (*pos)++;
                if (*pos < n && s[*pos] == '{') {
                    size_t start = ++(*pos); int depth = 1; while (*pos < n && depth) { if (s[*pos]=='{') depth++; else if (s[*pos]=='}') depth--; (*pos)++; }
                    size_t glen = (*pos - start) - 1;
                    struct mbuf tmp = {0}; render_subexpr(s + start, glen, &tmp, opt);
                    if (is_sub) { mb_puts(&sub, tmp.p?tmp.p:""); }
                    else { mb_puts(&sup, tmp.p?tmp.p:""); }
                    free(tmp.p);
                } else if (*pos < n) {
                    if (is_sub) { mb_putc(&sub, s[*pos]); }
                    else { mb_putc(&sup, s[*pos]); }
                    (*pos)++;
                }
            }
        }
        if (g_display_hint) {
            if (sup.p && sup.n) { mb_puts(out, sup.p); mb_putc(out, '\n'); }
            mb_puts(out, sym); mb_putc(out, '\n');
            if (sub.p && sub.n) { mb_puts(out, sub.p); mb_putc(out, '\n'); }
            /* consume a single following space to avoid blank spacer line */
            while (*pos < n && s[*pos] == ' ') (*pos)++;
        } else {
            mb_puts(out, sym);
            if (sub.p && sub.n) { if (emit_sub(out, sub.p, sub.n, !ascii) != 0) { free(sub.p); free(sup.p); return -1; } }
            if (sup.p && sup.n) { if (emit_sup(out, sup.p, sup.n, !ascii) != 0) { free(sub.p); free(sup.p); return -1; } }
        }
        free(sub.p); free(sup.p);
        return 0;
    }
    if (strcmp(name, "left") == 0) {
        /* parse delimiter and content until \right */
        /* read delimiter */
        char od = 0, cd = 0; const char *ol = NULL; const char *cl_from_left = NULL;
        if (*pos < n) {
            if (s[*pos] == '\\') {
                size_t i2 = *pos + 1, start2 = i2;
                while (i2 < n && ((s[i2] >= 'a' && s[i2] <= 'z') || (s[i2] >= 'A' && s[i2] <= 'Z'))) i2++;
                size_t klen = i2 - start2;
                if (klen > 0) {
                    char kw[16]; if (klen >= sizeof(kw)) klen = sizeof(kw)-1; memcpy(kw, s+start2, klen); kw[klen] = '\0';
                    if (strcmp(kw, "langle") == 0) { ol = (opt && opt->math_mode==MDVIC_MATH_ASCII)?"<":"⟨"; cl_from_left = (opt && opt->math_mode==MDVIC_MATH_ASCII)?">":"⟩"; *pos = i2; }
                    else if (strcmp(kw, "lceil") == 0) { ol = (opt && opt->math_mode==MDVIC_MATH_ASCII)?"[":"⌈"; cl_from_left = (opt && opt->math_mode==MDVIC_MATH_ASCII)?"]":"⌉"; *pos = i2; }
                    else if (strcmp(kw, "lfloor") == 0) { ol = (opt && opt->math_mode==MDVIC_MATH_ASCII)?"[":"⌊"; cl_from_left = (opt && opt->math_mode==MDVIC_MATH_ASCII)?"]":"⌋"; *pos = i2; }
                }
            } else {
                od = s[*pos]; *pos += 1;
                switch (od) {
                    case '(': cd = ')'; break; case '[': cd = ']'; break; case '{': cd = '}'; break; case '|': cd = '|'; break;
                    default: cd = od; break;
                }
            }
        }
        /* find \right */
        const char *needle = "\\right"; const char *pend = NULL; size_t nn = strlen(needle);
        for (size_t k=*pos; k+nn <= n; k++) { if (strncmp(s+k, needle, nn)==0) { pend = s + k; break; } }
        size_t content_len = pend ? (size_t)(pend - (s + *pos)) : (n - *pos); if (content_len > 65536) content_len = 65536;
        struct mbuf inner = {0}; render_subexpr(s + *pos, content_len, &inner, opt);
        if (ol) { mb_puts(out, ol); } else { mb_putc(out, od?od:'('); }
        mb_puts(out, inner.p?inner.p:"");
        const char *cr = NULL; char rchar = 0;
        if (pend) {
            size_t p = (size_t)(pend - s) + nn;
            if (p < n) {
                if (s[p] == '\\') {
                    size_t j2 = p + 1, st2 = j2; while (j2 < n && ((s[j2]>='a'&&s[j2]<='z')||(s[j2]>='A'&&s[j2]<='Z'))) j2++;
                    size_t lk = j2 - st2; if (lk > 0) {
                        char kw[16]; if (lk>=sizeof(kw)) lk=sizeof(kw)-1; memcpy(kw,s+st2,lk); kw[lk]='\0';
                        if (strcmp(kw, "rangle") == 0) cr = (opt && opt->math_mode==MDVIC_MATH_ASCII)?">":"⟩";
                        else if (strcmp(kw, "rceil") == 0) cr = (opt && opt->math_mode==MDVIC_MATH_ASCII)?"]":"⌉";
                        else if (strcmp(kw, "rfloor") == 0) cr = (opt && opt->math_mode==MDVIC_MATH_ASCII)?"]":"⌋";
                    }
                } else {
                    rchar = s[p];
                }
            }
        }
        if (cr) { mb_puts(out, cr); }
        else if (rchar) { mb_putc(out, rchar); }
        else if (cl_from_left) { mb_puts(out, cl_from_left); }
        else { mb_putc(out, cd?cd:')'); }
        free(inner.p);
        if (pend) {
            *pos = (size_t)(pend - s) + nn;
            /* skip delimiter after \right */
            if (*pos < n) {
                if (s[*pos] == '\\') {
                    size_t i3 = *pos + 1; while (i3 < n && ((s[i3] >= 'a' && s[i3] <= 'z') || (s[i3] >= 'A' && s[i3] <= 'Z'))) i3++;
                    *pos = i3;
                } else {
                    *pos += 1;
                }
            }
        } else {
            *pos = n;
        }
        return 0;
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
                size_t nlen = disp_len(num.p), dlen = disp_len(den.p);
                int ascii = (opt && opt->math_mode == MDVIC_MATH_ASCII);
                int stacked = g_display_hint ? 1 : 0;
                if (stacked) {
                    size_t w = nlen > dlen ? nlen : dlen;
                    if (out->n && out->p[out->n-1] != '\n') mb_putc(out, '\n');
                    mb_puts(out, num.p ? num.p : ""); mb_putc(out, '\n');
                    for (size_t k=0;k<w;k++) mb_putc(out, '-');
                    mb_putc(out, '\n');
                    mb_puts(out, den.p ? den.p : "");
                } else {
                    if (ascii) { mb_puts(out, num.p ? num.p : ""); mb_putc(out, '/'); mb_puts(out, den.p ? den.p : ""); }
                    else { mb_puts(out, num.p ? num.p : ""); mb_puts(out, "⁄"); mb_puts(out, den.p ? den.p : ""); }
                }
                free(num.p); free(den.p);
                return 0;
            }
        }
        return mb_puts(out, "frac");
    }
    if (strcmp(name, "begin") == 0) {
        /* expect {pmatrix}...\end{pmatrix} */
        if (*pos < n && s[*pos] == '{') {
            size_t type_start = *pos + 1; size_t j = type_start; while (j < n && s[j] != '}') j++;
            size_t tlen = (j < n) ? (j - type_start) : 0; *pos = (j < n) ? (j + 1) : j;
            int is_p = (tlen == 7 && strncmp(s + type_start, "pmatrix", 7) == 0);
            int is_b = (tlen == 7 && strncmp(s + type_start, "bmatrix", 7) == 0);
            int is_v = (tlen == 7 && strncmp(s + type_start, "vmatrix", 7) == 0);
            if (is_p || is_b || is_v) {
                /* find \end{pmatrix} from *pos */
                const char *needle = is_p?"\\end{pmatrix}": (is_b?"\\end{bmatrix}":"\\end{vmatrix}"); const char *pend = NULL; size_t nn = strlen(needle);
                for (size_t k=*pos; k+nn <= n; k++) { if (strncmp(s+k, needle, nn)==0) { pend = s + k; break; } }
                size_t content_len = pend ? (size_t)(pend - (s + *pos)) : (n - *pos);
                if (content_len > 32768) content_len = 32768; /* safety cap */
                int ascii = (opt && opt->math_mode == MDVIC_MATH_ASCII);
                if (!g_display_hint) {
                    /* Inline simple: ( a b ; c d ) */
                    char l = is_p?'(' : (is_b?'[' : '|'); char r = is_p?')' : (is_b?']' : '|');
                    mb_putc(out, l); mb_putc(out, ' ');
                    for (size_t pcur=0; pcur < content_len; pcur++) {
                        char ch = s[*pos + pcur];
                        if (ch == '\\') { mb_puts(out, "; "); if (pcur+1<content_len && s[*pos+pcur+1]=='\\') pcur++; }
                        else if (ch == '&') { mb_putc(out, ' '); }
                        else if (ch == '{' || ch == '}') { /* skip */ }
                        else { mb_putc(out, ch); }
                    }
                    mb_putc(out, ' '); mb_putc(out, r);
                    if (pend) { *pos = (size_t)(pend - s) + nn; }
                    return 0;
                } else {
                    /* Display: multi-line with padding */
                    /* Parse into cells */
                    size_t idx = *pos; const size_t end = *pos + content_len;
                    size_t rows_cap=8, rows_n=0; int cols_max=0;
                    struct mbuf **cells = (struct mbuf**)calloc(rows_cap,sizeof(*cells));
                    int *col_counts = (int*)calloc(rows_cap,sizeof(int));
                    while (idx < end && rows_n < 64) {
                        size_t row_end = idx;
                        while (row_end < end) { if (s[row_end] == '\\') { if (row_end+1<end && s[row_end+1]=='\\') break; else break; } row_end++; }
                        int row_cols_cap=8, row_cols=0; struct mbuf *row = (struct mbuf*)calloc((size_t)row_cols_cap, sizeof(*row));
                        size_t seg = idx;
                        for (size_t pcur = idx; pcur <= row_end && row_cols < 32; pcur++) {
                            if (pcur == row_end || s[pcur] == '&') {
                                struct mbuf cell = {0}; render_subexpr(s + seg, pcur - seg, &cell, opt);
                                if (row_cols == row_cols_cap) { row_cols_cap*=2; row = (struct mbuf*)realloc(row, (size_t)row_cols_cap*sizeof(*row)); }
                                row[row_cols++] = cell; seg = pcur + 1;
                            }
                        }
                        if (rows_n == rows_cap) { rows_cap*=2; cells = (struct mbuf**)realloc(cells, (size_t)rows_cap*sizeof(*cells)); col_counts = (int*)realloc(col_counts,(size_t)rows_cap*sizeof(int)); }
                        cells[rows_n] = row; col_counts[rows_n] = row_cols; if (row_cols > cols_max) cols_max=row_cols; rows_n++;
                        idx = (row_end < end) ? ( (row_end+1<end && s[row_end+1]=='\\') ? row_end+2 : row_end+1 ) : row_end;
                    }
                    int *colw = (int*)calloc((size_t)cols_max, sizeof(int));
                    for (size_t r=0; r<rows_n; r++) for (int c=0; c<col_counts[r]; c++) { int w=(int)disp_len(cells[r][c].p); if (w>colw[c]) colw[c]=w; }
                    const char *ltop = is_p?"(": (is_b?"[":"|"); const char *lmid = ltop; const char *lbot = ltop;
                    const char *rtop = is_p?")": (is_b?"]":"|"); const char *rmid = rtop; const char *rbot = rtop;
                    for (size_t r=0; r<rows_n; r++) {
                        const char *L = (r==0)? ltop : (r==rows_n-1? lbot : lmid);
                        const char *R = (r==0)? rtop : (r==rows_n-1? rbot : rmid);
                        mb_puts(out, L); mb_putc(out, ' ');
                        for (int c=0; c<cols_max; c++) {
                            const char *txt = (c<col_counts[r] && cells[r][c].p)?cells[r][c].p:"";
                            mb_puts(out, txt);
                            int pad = colw[c] - (int)disp_len(txt);
                            for (int k=0;k<pad;k++) mb_putc(out, ' ');
                            if (c!=cols_max-1) mb_putc(out, ' ');
                        }
                        mb_putc(out, ' '); mb_puts(out, R); mb_putc(out,'\n');
                    }
                    for (size_t r=0;r<rows_n;r++) { for (int c=0;c<col_counts[r];c++) free(cells[r][c].p); free(cells[r]); }
                    free(cells); free(col_counts); free(colw);
                    if (pend) { *pos = (size_t)(pend - s) + nn; }
                    return 0;
                }
            }
        }
        return mb_puts(out, "begin");
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
                      int display, char **out_str, size_t *out_len) {
    struct mbuf out; out.p = NULL; out.n = 0; out.cap = 0;
    g_display_hint = display ? 1 : 0;
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
