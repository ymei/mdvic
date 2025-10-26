#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/ioctl.h>
#else
#include <windows.h>
#endif

#include "mdvic/mdvic.h"
#include "mdvic/wrap.h"
#include "mdvic/math.h"
#include "mdvic/lint.h"
#ifdef HAVE_LIBCMARK
#include <cmark.h>
#endif
#include "mdvic/wcwidth.h"

/* ---------------- Rendering primitives ---------------- */

struct Style {
    int bold;
    int italic;
    int underline;
    int inverse;
    int dim;
    int fg; /* -1=default, 30..37 basic */
};

struct Out {
    FILE *out;
    int width;
    int col;
    int need_reset;
    int color_enabled;
    int osc8_enabled;
    struct Style style;
    int list_depth;
    int quote_depth;
    char prefix_first[32];
    int prefix_first_len;
    char prefix_next[32];
    int prefix_next_len;
    const char *filename;
    const char *source;
    size_t source_len;
};

static void style_init(struct Style *s) {
    s->bold = 0; s->italic = 0; s->underline = 0; s->inverse = 0; s->dim = 0; s->fg = -1;
}

static void out_init(struct Out *o, FILE *out, int width, const struct MdvicOptions *opt) {
    o->out = out;
    o->width = width;
    o->col = 0;
    o->need_reset = 0;
    o->color_enabled = opt && !opt->no_color;
    o->osc8_enabled = opt && opt->enable_osc8;
    style_init(&o->style);
    o->list_depth = 0;
    o->quote_depth = 0;
    o->prefix_first[0] = '\0';
    o->prefix_first_len = 0;
    o->prefix_next[0] = '\0';
    o->prefix_next_len = 0;
    o->filename = NULL;
    o->source = NULL; o->source_len = 0;
}

static void out_emit_style(struct Out *o) {
    if (!o->color_enabled) return;
    /* Build minimal SGR sequence */
    fputs("\x1b[0m", o->out);
    if (o->style.bold) fputs("\x1b[1m", o->out);
    if (o->style.dim) fputs("\x1b[2m", o->out);
    if (o->style.italic) fputs("\x1b[3m", o->out);
    if (o->style.underline) fputs("\x1b[4m", o->out);
    if (o->style.inverse) fputs("\x1b[7m", o->out);
    if (o->style.fg >= 30 && o->style.fg <= 37) {
        fprintf(o->out, "\x1b[%dm", o->style.fg);
    }
    o->need_reset = 1;
}

static void out_reset(struct Out *o) {
    if (o->color_enabled && o->need_reset) {
        fputs("\x1b[0m", o->out);
        o->need_reset = 0;
    }
}

static void out_write(struct Out *o, const char *s, size_t len) {
    (void)mdvic_wrap_write_pref2(o->out, s, len, o->width, &o->col,
                                 o->prefix_first, o->prefix_first_len,
                                 o->prefix_next, o->prefix_next_len);
}

static void out_text(struct Out *o, const char *s) {
    size_t len = strlen(s);
    out_write(o, s, len);
}

static void out_puts(struct Out *o, const char *s) {
    out_text(o, s);
    if (o->col != 0) { fputc('\n', o->out); o->col = 0; }
}

static void out_newline(struct Out *o) {
    fputc('\n', o->out);
    o->col = 0;
}

static void out_set_prefix(struct Out *o, const char *first, const char *next) {
    size_t n1 = strlen(first);
    if (n1 > sizeof(o->prefix_first) - 1) n1 = sizeof(o->prefix_first) - 1;
    memcpy(o->prefix_first, first, n1);
    o->prefix_first[n1] = '\0';
    o->prefix_first_len = (int)n1;
    size_t n2 = strlen(next);
    if (n2 > sizeof(o->prefix_next) - 1) n2 = sizeof(o->prefix_next) - 1;
    memcpy(o->prefix_next, next, n2);
    o->prefix_next[n2] = '\0';
    o->prefix_next_len = (int)n2;
}

static void out_clear_prefix(struct Out *o) {
    o->prefix_first[0] = '\0';
    o->prefix_first_len = 0;
    o->prefix_next[0] = '\0';
    o->prefix_next_len = 0;
}

static void osc8_begin(struct Out *o, const char *url) {
    if (o->color_enabled && o->osc8_enabled) {
        fputs("\x1b]8;;", o->out);
        fputs(url ? url : "", o->out);
        fputc('\a', o->out);
    }
}

static void osc8_end(struct Out *o) {
    if (o->color_enabled && o->osc8_enabled) {
        fputs("\x1b]8;;\a", o->out);
    }
}

/* Math-aware text: find $...$ or $$...$$ and render math via mdvic_math_render. */
static void render_text_with_math(struct Out *o, const char *lit, const struct MdvicOptions *opt) {
    const char *p = lit;
    while (*p) {
        const char *d = strchr(p, '$');
        if (!d) {
            out_write(o, p, strlen(p));
            break;
        }
        /* write before $ */
        if (d > p) {
            out_write(o, p, (size_t)(d - p));
        }
        /* count consecutive $ */
        const char *q = d;
        int count = 0;
        while (*q == '$') { count++; q++; }
        /* find closing */
        const char *start = q;
        const char *end = NULL;
        const char *scan = start;
        while ((scan = strchr(scan, '$')) != NULL) {
            int c2 = 0; const char *t = scan;
            while (*t == '$') { c2++; t++; }
            if (c2 == count) { end = scan; break; }
            scan = t;
        }
        if (!end) {
            /* no closing, emit as literal */
            out_write(o, d, strlen(d));
            break;
        }
        char *mout = NULL; size_t mlen = 0;
        if (mdvic_math_render(start, (size_t)(end - start), opt, &mout, &mlen) == 0 && mout) {
            if (count >= 2) { if (o->col != 0) out_newline(o); }
            out_write(o, mout, mlen);
            if (count >= 2) { out_newline(o); }
            free(mout);
        } else {
            out_write(o, start, (size_t)(end - start));
        }
        p = end + count;
    }
}

#ifdef HAVE_LIBCMARK
static void render_node(struct Out *o, cmark_node *node, const struct MdvicOptions *opt);

static void render_inlines(struct Out *o, cmark_node *node, const struct MdvicOptions *opt) {
    for (cmark_node *n = cmark_node_first_child(node); n; n = cmark_node_next(n)) {
        render_node(o, n, opt);
    }
}

/* ---------------- GFM table detection and rendering ---------------- */

struct Buf { char *p; size_t n; size_t cap; };
static void buf_init(struct Buf *b) { b->p = NULL; b->n = 0; b->cap = 0; }
static int buf_grow(struct Buf *b, size_t add) {
    size_t need = b->n + add + 1;
    if (need <= b->cap) return 0;
    size_t ncap = b->cap ? b->cap * 2 : 256;
    while (ncap < need) ncap *= 2;
    char *np = (char *)realloc(b->p, ncap);
    if (!np) return -1; b->p = np; b->cap = ncap; return 0;
}
static int buf_puts(struct Buf *b, const char *s) {
    size_t m = strlen(s);
    if (buf_grow(b, m) != 0) return -1;
    memcpy(b->p + b->n, s, m); b->n += m; b->p[b->n] = '\0'; return 0;
}
static int buf_putc(struct Buf *b, char c) {
    if (buf_grow(b, 1) != 0) return -1; b->p[b->n++] = c; b->p[b->n] = '\0'; return 0;
}
static void buf_free(struct Buf *b) { free(b->p); b->p = NULL; b->n = b->cap = 0; }

static void collect_plain_text_inline(struct Buf *b, cmark_node *node) {
    switch (cmark_node_get_type(node)) {
    case CMARK_NODE_TEXT:
    case CMARK_NODE_CODE: {
        const char *lit = cmark_node_get_literal(node);
        if (lit) buf_puts(b, lit);
        break;
    }
    case CMARK_NODE_SOFTBREAK:
        buf_putc(b, '\n'); break;
    case CMARK_NODE_LINEBREAK:
        buf_putc(b, '\n'); break;
    case CMARK_NODE_EMPH:
    case CMARK_NODE_STRONG:
    case CMARK_NODE_LINK:
    case CMARK_NODE_IMAGE: {
        for (cmark_node *n = cmark_node_first_child(node); n; n = cmark_node_next(n)) {
            collect_plain_text_inline(b, n);
        }
        break;
    }
    default:
        /* ignore */
        break;
    }
}

static char *str_trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t')) { *--e = '\0'; }
    return s;
}

static int split_pipe(char *line, char ***out_fields, int *out_count) {
    int cap = 8, n = 0; char **arr = (char **)malloc((size_t)cap * sizeof(char*)); if (!arr) return -1;
    char *p = line;
    /* trim */
    p = str_trim(p);
    if (*p == '|') { p++; }
    char *start = p;
    while (*p) {
        if (*p == '|') {
            *p = '\0';
            char *cell = str_trim(start);
            if (n == cap) { cap *= 2; char **na = (char **)realloc(arr, (size_t)cap * sizeof(char*)); if (!na) { free(arr); return -1; } arr = na; }
            arr[n++] = cell;
            start = p + 1;
        }
        p++;
    }
    if (p != start) {
        char *cell = str_trim(start);
        if (n == cap) { cap *= 2; char **na = (char **)realloc(arr, (size_t)cap * sizeof(char*)); if (!na) { free(arr); return -1; } arr = na; }
        arr[n++] = cell;
    }
    *out_fields = arr; *out_count = n; return 0;
}

static int parse_sep_fields(char **fields, int n, int *align) {
    for (int i = 0; i < n; i++) {
        char *f = fields[i];
        int left = 0, right = 0, dashes = 0;
        while (*f == ' ' || *f == '\t') f++;
        if (*f == ':') { left = 1; f++; }
        while (*f == '-') { dashes++; f++; }
        if (*f == ':') { right = 1; f++; }
        while (*f == ' ' || *f == '\t') f++;
        if (*f != '\0' || dashes < 3) return -1;
        align[i] = left && right ? 1 : right ? 2 : 0; /* 0=left,1=center,2=right */
    }
    return 0;
}

static int cell_width(const char *s) {
    /* measure display width using wcwidth */
    int w = 0; const char *p = s;
    while (*p) {
        unsigned char c0 = (unsigned char)*p;
        size_t consumed = 1; uint32_t cp = 0;
        if ((c0 & 0x80) == 0) { cp = c0; }
        else if ((c0 & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) { cp = ((uint32_t)(c0 & 0x1F) << 6) | (uint32_t)(p[1] & 0x3F); consumed = 2; }
        else if ((c0 & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) { cp = ((uint32_t)(c0 & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (uint32_t)(p[2] & 0x3F); consumed = 3; }
        else if ((c0 & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) { cp = ((uint32_t)(c0 & 0x07) << 18) | ((uint32_t)(p[1] & 0x3F) << 12) | ((uint32_t)(p[2] & 0x3F) << 6) | (uint32_t)(p[3] & 0x3F); consumed = 4; }
        else { cp = c0; consumed = 1; }
        w += mdvic_wcwidth(cp);
        p += consumed;
    }
    return w < 0 ? 0 : w;
}

/* Forward decls for style helpers used in cell rendering */
static void style_push(struct Out *o, struct Style *saved, const struct Style *delta);
static void style_pop(struct Out *o, const struct Style *saved);

static int line_span_from_source(const char *src, size_t len, int start_line, int end_line, size_t *out_s, size_t *out_e) {
    if (start_line < 1 || end_line < start_line) return -1;
    int line = 1; size_t i = 0; size_t s = 0, e = len;
    while (i < len && line < start_line) { if (src[i++] == '\n') line++; }
    if (line != start_line) return -1; s = i;
    while (i < len && line <= end_line) { if (src[i++] == '\n') line++; }
    e = (line > end_line) ? (i - 1) : i;
    *out_s = s; *out_e = e; return 0;
}

static int rendered_text_width(const char *s) {
#ifdef HAVE_LIBCMARK
    cmark_node *doc = cmark_parse_document(s, strlen(s), CMARK_OPT_DEFAULT);
    if (!doc) return cell_width(s);
    struct Buf b; buf_init(&b);
    for (cmark_node *blk = cmark_node_first_child(doc); blk; blk = cmark_node_next(blk)) {
        if (cmark_node_get_type(blk) == CMARK_NODE_PARAGRAPH) {
            for (cmark_node *n = cmark_node_first_child(blk); n; n = cmark_node_next(n)) {
                collect_plain_text_inline(&b, n);
            }
        }
    }
    int w = cell_width(b.p ? b.p : "");
    buf_free(&b);
    cmark_node_free(doc);
    return w;
#else
    return cell_width(s);
#endif
}

static void render_cell_content(struct Out *o, const char *s, int is_header, const struct MdvicOptions *opt) {
#ifdef HAVE_LIBCMARK
    cmark_node *doc = cmark_parse_document(s, strlen(s), CMARK_OPT_DEFAULT);
    if (doc) {
        struct Style saved, delta; style_init(&delta); if (is_header) delta.bold = 1;
        style_push(o, &saved, &delta);
        int saved_w = o->width; o->width = 0;
        for (cmark_node *blk = cmark_node_first_child(doc); blk; blk = cmark_node_next(blk)) {
            if (cmark_node_get_type(blk) == CMARK_NODE_PARAGRAPH) {
                render_inlines(o, blk, opt);
            }
        }
        o->width = saved_w;
        style_pop(o, &saved);
        cmark_node_free(doc);
        return;
    }
#endif
    /* Fallback: plain */
    int saved_w = o->width; o->width = 0;
    if (is_header) { struct Style saved, delta; style_init(&delta); delta.bold = 1; style_push(o, &saved, &delta); out_text(o, s); style_pop(o, &saved); }
    else { out_text(o, s); }
    o->width = saved_w;
}

static int mdvic_render_gfm_table_if_any(struct Out *o, cmark_node *node, const struct MdvicOptions *opt) {
    (void)opt;
    if (!o->source) return 0;
    size_t sidx = 0, eidx = 0;
    if (line_span_from_source(o->source, o->source_len, cmark_node_get_start_line(node), cmark_node_get_end_line(node), &sidx, &eidx) != 0) return 0;
    struct Buf b; buf_init(&b);
    if (buf_puts(&b, "") != 0) { buf_free(&b); return 0; }
    if (buf_grow(&b, eidx - sidx) != 0) { buf_free(&b); return 0; }
    memcpy(&b.p[b.n], o->source + sidx, eidx - sidx);
    b.n += (eidx - sidx);
    b.p[b.n] = '\0';
    /* Split into lines (raw from source, with markup) */
    int lines_cap = 8, lines_n = 0; char **lines = (char **)malloc((size_t)lines_cap * sizeof(char*)); if (!lines) { buf_free(&b); return 0; }
    /* tokenize by replacing \n with \0 */
    for (size_t i = 0, start = 0; i <= b.n; i++) {
        if (i == b.n || b.p[i] == '\n') {
            b.p[i] = '\0';
            char *ln = str_trim(b.p + start);
            if (*ln) {
                if (lines_n == lines_cap) { lines_cap *= 2; char **nl = (char **)realloc(lines, (size_t)lines_cap * sizeof(char*)); if (!nl) { free(lines); buf_free(&b); return 0; } lines = nl; }
                lines[lines_n++] = ln;
            }
            start = (unsigned)i + 1;
        }
    }
    if (lines_n < 2) { free(lines); buf_free(&b); return 0; }
    /* Quick heuristic: must look like a table header + separator */
    if (!strchr(lines[0], '|') || !strchr(lines[1], '|') || strchr(lines[1], '-') == NULL) {
        free(lines); buf_free(&b); return 0;
    }
    /* Parse separator line */
    char *sep_line = strdup(lines[1]); if (!sep_line) { free(lines); buf_free(&b); return 0; }
    char **sep_fields = NULL; int ncols = 0; if (split_pipe(sep_line, &sep_fields, &ncols) != 0 || ncols <= 0) { free(sep_line); free(lines); buf_free(&b); return 0; }
    int *align = (int *)calloc((size_t)ncols, sizeof(int)); if (!align) { free(sep_fields); free(sep_line); free(lines); buf_free(&b); return 0; }
    if (parse_sep_fields(sep_fields, ncols, align) != 0) {
        if (!opt || opt->enable_lint) fprintf(stderr, "%s:%d: malformed table separator row\n", o->filename ? o->filename : "-", cmark_node_get_start_line(node) + 1);
        free(align); free(sep_fields); free(sep_line); free(lines); buf_free(&b); return 0;
    }
    free(sep_fields); free(sep_line);

    /* Parse header */
    char *hdr_line = strdup(lines[0]); if (!hdr_line) { free(align); free(lines); buf_free(&b); return 0; }
    char **hdr_cells = NULL; int hdr_n = 0; if (split_pipe(hdr_line, &hdr_cells, &hdr_n) != 0) { free(hdr_line); free(align); free(lines); buf_free(&b); return 0; }

    /* Parse rows */
    int rows_cap = 8, rows_n = 0; char ***rows = (char ***)malloc((size_t)rows_cap * sizeof(char**)); if (!rows) { free(hdr_cells); free(hdr_line); free(align); free(lines); buf_free(&b); return 0; }
    int *row_counts = (int *)malloc((size_t)rows_cap * sizeof(int)); if (!row_counts) { free(rows); free(hdr_cells); free(hdr_line); free(align); free(lines); buf_free(&b); return 0; }
    for (int li = 2; li < lines_n; li++) {
        if (!strchr(lines[li], '|')) break;
        if (rows_n == rows_cap) {
            int ncap = rows_cap * 2;
            char ***nr = (char ***)realloc(rows, (size_t)ncap * sizeof(char**));
            int *nc = (int *)realloc(row_counts, (size_t)ncap * sizeof(int));
            if (!nr || !nc) { /* out of mem */ break; }
            rows = nr; row_counts = nc; rows_cap = ncap;
        }
        char *dup = strdup(lines[li]); if (!dup) break;
        char **cells = NULL; int cn = 0; if (split_pipe(dup, &cells, &cn) != 0) { free(dup); break; }
        rows[rows_n] = cells; row_counts[rows_n] = cn; rows_n++;
    }

    /* If no body rows and hdr_n==0, bail */
    if (hdr_n == 0) { for (int r=0;r<rows_n;r++) free(rows[r]); free(row_counts); free(rows); free(hdr_cells); free(hdr_line); free(align); free(lines); buf_free(&b); return 0; }

    /* Compute col widths based on rendered text widths */
    int *colw = (int *)calloc((size_t)ncols, sizeof(int)); if (!colw) { for (int r=0;r<rows_n;r++) free(rows[r]); free(row_counts); free(rows); free(hdr_cells); free(hdr_line); free(align); free(lines); buf_free(&b); return 0; }
    for (int i = 0; i < ncols; i++) {
        const char *hc = (i < hdr_n) ? hdr_cells[i] : "";
        int w = rendered_text_width(hc); if (w > colw[i]) colw[i] = w;
    }
    for (int r = 0; r < rows_n; r++) {
        if (row_counts[r] != ncols) {
            if (!opt || opt->enable_lint) fprintf(stderr, "%s:%d: table row has %d columns; expected %d\n", o->filename ? o->filename : "-", cmark_node_get_start_line(node) + 1 + r + 2, row_counts[r], ncols);
        }
        for (int i = 0; i < ncols; i++) {
            const char *c = (i < row_counts[r]) ? rows[r][i] : "";
            int w = rendered_text_width(c); if (w > colw[i]) colw[i] = w;
        }
    }

    /* Print table */
    int saved_w = o->width; o->width = 0;
    /* header */
    fputc('|', o->out);
    for (int i = 0; i < ncols; i++) {
        fputc(' ', o->out);
        const char *hc = (i < hdr_n) ? hdr_cells[i] : "";
        /* left pad according to alignment */
        int w = rendered_text_width(hc);
        int pad = (w < colw[i]) ? (colw[i] - w) : 0;
        int lp=0, rp=0; if (align[i] == 2) { lp = pad; } else if (align[i] == 1) { lp = pad/2; rp = pad - lp; } else { rp = pad; }
        for (int k=0;k<lp;k++) fputc(' ', o->out);
        render_cell_content(o, hc, 1, opt);
        for (int k=0;k<rp;k++) fputc(' ', o->out);
        fputc(' ', o->out);
        fputc('|', o->out);
    }
    fputc('\n', o->out);
    /* separator (render according to width) */
    fputc('|', o->out);
    for (int i = 0; i < ncols; i++) {
        fputc(' ', o->out);
        int left = (align[i] == 1 || align[i] == 0) ? 1 : 0;
        int right = (align[i] == 1 || align[i] == 2) ? 1 : 0;
        if (left) fputc(':', o->out);
        for (int k = 0; k < colw[i]; k++) fputc('-', o->out);
        if (right) fputc(':', o->out);
        fputc(' ', o->out);
        fputc('|', o->out);
    }
    fputc('\n', o->out);
    /* rows */
    for (int r = 0; r < rows_n; r++) {
        fputc('|', o->out);
        for (int i = 0; i < ncols; i++) {
            fputc(' ', o->out);
            const char *c = (i < row_counts[r]) ? rows[r][i] : "";
            int w = rendered_text_width(c);
            int pad = (w < colw[i]) ? (colw[i] - w) : 0;
            int lp=0, rp=0; if (align[i] == 2) { lp = pad; } else if (align[i] == 1) { lp = pad/2; rp = pad - lp; } else { rp = pad; }
            for (int k=0;k<lp;k++) fputc(' ', o->out);
            render_cell_content(o, c, 0, opt);
            for (int k=0;k<rp;k++) fputc(' ', o->out);
            fputc(' ', o->out);
            fputc('|', o->out);
        }
        fputc('\n', o->out);
    }
    fputc('\n', o->out);
    o->width = saved_w;

    /* cleanup */
    for (int r = 0; r < rows_n; r++) free(rows[r]);
    free(row_counts); free(rows);
    free(hdr_cells); free(hdr_line);
    free(align);
    free(lines); buf_free(&b);
    return 1;
}

static void style_push(struct Out *o, struct Style *saved, const struct Style *delta) {
    *saved = o->style;
    if (delta->bold) o->style.bold = 1;
    if (delta->italic) o->style.italic = 1;
    if (delta->underline) o->style.underline = 1;
    if (delta->inverse) o->style.inverse = 1;
    if (delta->dim) o->style.dim = 1;
    if (delta->fg >= 0) o->style.fg = delta->fg;
    out_emit_style(o);
}

static void style_pop(struct Out *o, const struct Style *saved) {
    o->style = *saved;
    out_emit_style(o);
}

static void render_node(struct Out *o, cmark_node *node, const struct MdvicOptions *opt) {
    switch (cmark_node_get_type(node)) {
    case CMARK_NODE_DOCUMENT: {
        for (cmark_node *n = cmark_node_first_child(node); n; n = cmark_node_next(n)) {
            render_node(o, n, opt);
        }
        break;
    }
    case CMARK_NODE_PARAGRAPH: {
        if (!mdvic_render_gfm_table_if_any(o, node, opt)) {
            render_inlines(o, node, opt);
            out_newline(o);
            out_newline(o);
        }
        break;
    }
    case CMARK_NODE_HEADING: {
        int level = cmark_node_get_heading_level(node);
        struct Style saved, delta; style_init(&delta);
        delta.bold = 1;
        /* map level to a color */
        delta.fg = (level == 1) ? 36 : (level == 2) ? 33 : 32; /* cyan, yellow, green */
        style_push(o, &saved, &delta);
        render_inlines(o, node, opt);
        style_pop(o, &saved);
        out_newline(o);
        out_newline(o);
        break;
    }
    case CMARK_NODE_TEXT: {
        const char *lit = cmark_node_get_literal(node);
        if (lit) {
            /* math-aware write */
            render_text_with_math(o, lit, opt);
        }
        break;
    }
    case CMARK_NODE_SOFTBREAK: {
        out_write(o, " ", 1);
        break;
    }
    case CMARK_NODE_LINEBREAK: {
        out_newline(o);
        break;
    }
    case CMARK_NODE_STRONG: {
        struct Style saved, delta; style_init(&delta); delta.bold = 1;
        style_push(o, &saved, &delta);
        render_inlines(o, node, opt);
        style_pop(o, &saved);
        break;
    }
    case CMARK_NODE_EMPH: {
        struct Style saved, delta; style_init(&delta); delta.italic = 1;
        style_push(o, &saved, &delta);
        render_inlines(o, node, opt);
        style_pop(o, &saved);
        break;
    }
    case CMARK_NODE_CODE: {
        const char *lit = cmark_node_get_literal(node);
        struct Style saved, delta; style_init(&delta); delta.inverse = 1;
        style_push(o, &saved, &delta);
        if (lit) out_write(o, lit, strlen(lit));
        style_pop(o, &saved);
        break;
    }
    case CMARK_NODE_CODE_BLOCK: {
        const char *lit = cmark_node_get_literal(node);
        (void)cmark_node_get_fence_info(node);
        /* Print as indented block without wrapping */
        if (lit) {
            const char *p = lit;
            while (*p) {
                const char *nl = strchr(p, '\n');
                size_t seg = nl ? (size_t)(nl - p) : strlen(p);
                out_write(o, "    ", 4);
                /* Temporarily bypass soft wrap for code by using width=0 */
                int saved_w = o->width; o->width = 0;
                out_write(o, p, seg);
                o->width = saved_w;
                out_newline(o);
                if (!nl) break;
                p = nl + 1;
            }
            out_newline(o);
        }
        break;
    }
    case CMARK_NODE_LIST: {
        int tight = cmark_node_get_list_tight(node);
        cmark_list_type lt = cmark_node_get_list_type(node);
        int start = cmark_node_get_list_start(node);
        int index = start ? start : 1;
        o->list_depth++;
        for (cmark_node *it = cmark_node_first_child(node); it; it = cmark_node_next(it)) {
            char bullet[32], spaces[32];
            if (lt == CMARK_ORDERED_LIST) { snprintf(bullet, sizeof(bullet), "%d. ", index++); }
            else { snprintf(bullet, sizeof(bullet), "- "); }
            size_t blen = strlen(bullet);
            if (blen >= sizeof(spaces)) blen = sizeof(spaces)-1;
            for (size_t k = 0; k < blen; k++) spaces[k] = ' ';
            spaces[blen] = '\0';
            out_set_prefix(o, bullet, spaces);
            /* Render item children (blocks) */
            for (cmark_node *blk = cmark_node_first_child(it); blk; blk = cmark_node_next(blk)) {
                render_node(o, blk, opt);
            }
            out_clear_prefix(o);
            if (!tight) out_newline(o);
        }
        o->list_depth--;
        break;
    }
    case CMARK_NODE_BLOCK_QUOTE: {
        o->quote_depth++;
        char qpref1[32]; char qpref2[32];
        int n = 0; for (int i = 0; i < o->quote_depth; i++) { if (n + 2 < (int)sizeof(qpref1)) { qpref1[n++] = '>'; qpref1[n++] = ' '; } }
        qpref1[n] = '\0';
        /* next lines align with 2 spaces after the '>'s */
        for (int i = 0; i < n; i++) qpref2[i] = (qpref1[i] == '>') ? '>' : ' ';
        qpref2[n] = '\0';
        out_set_prefix(o, qpref1, qpref2);
        for (cmark_node *n = cmark_node_first_child(node); n; n = cmark_node_next(n)) {
            render_node(o, n, opt);
        }
        out_clear_prefix(o);
        o->quote_depth--;
        break;
    }
    case CMARK_NODE_THEMATIC_BREAK: {
        out_puts(o, "-----");
        out_newline(o);
        break;
    }
    case CMARK_NODE_LINK: {
        const char *url = cmark_node_get_url(node);
        int have_osc8 = o->color_enabled && o->osc8_enabled;
        if (have_osc8 && url) osc8_begin(o, url);
        render_inlines(o, node, opt);
        if (have_osc8 && url) osc8_end(o);
        if (!have_osc8 && url && url[0] != '\0') {
            out_text(o, " (" ); out_text(o, url); out_text(o, ")");
        }
        break;
    }
    case CMARK_NODE_IMAGE: {
        const char *url = cmark_node_get_url(node);
        out_text(o, "[image: ");
        render_inlines(o, node, opt);
        out_text(o, "]");
        if (url) { out_text(o, " (" ); out_text(o, url); out_text(o, ")"); }
        break;
    }
    default: {
        /* Fallback: render children */
        for (cmark_node *n = cmark_node_first_child(node); n; n = cmark_node_next(n)) {
            render_node(o, n, opt);
        }
        break;
    }
    }
}
#endif /* HAVE_LIBCMARK */

int mdvic_render_stream(FILE *in, FILE *out, const struct MdvicOptions *opt, const char *filename) {
    (void)filename;
    int width = opt ? opt->width : 0;

#ifdef HAVE_LIBCMARK
    /* Slurp input */
    size_t cap = 8192, len = 0; char *data = (char *)malloc(cap);
    if (!data) return -1;
    for (;;) {
        if (cap - len < 4096) { size_t ncap = cap * 2; char *nd = (char *)realloc(data, ncap); if (!nd) { free(data); return -1; } data = nd; cap = ncap; }
        size_t n = fread(data + len, 1, 4096, in); len += n; if (n < 4096) { if (feof(in)) break; if (ferror(in)) { free(data); return -1; } }
    }
    /* Lint before parsing */
    if (!opt || opt->enable_lint) {
        (void)mdvic_lint_buffer(data, len, stderr, filename);
    }
    cmark_node *doc = cmark_parse_document(data, len, CMARK_OPT_DEFAULT);
    if (!doc) return -1;
    struct Out o; out_init(&o, out, width, opt); o.filename = filename; o.source = data; o.source_len = len;
    render_node(&o, doc, opt);
    cmark_node_free(doc);
    free(data);
    out_reset(&o);
    if (o.col != 0) fputc('\n', out);
    return 0;
#else
    /* Passthrough */
    struct Out o; out_init(&o, out, width, opt); o.filename = filename;
    char buf[4096];
    while (fgets(buf, (int)sizeof(buf), in) != NULL) {
        size_t n = strlen(buf);
        out_write(&o, buf, n);
    }
    out_reset(&o);
    if (o.col != 0) fputc('\n', out);
    return 0;
#endif
}

void mdvic_apply_env_overrides(struct MdvicOptions *opt) {
    if (!opt) return;
    const char *no_color = getenv("MDVIC_NO_COLOR");
    if (no_color && no_color[0] != '\0') {
        opt->no_color = true;
    }
    const char *no_lint = getenv("MDVIC_NO_LINT");
    if (no_lint && no_lint[0] != '\0') {
        opt->enable_lint = false;
    }
    const char *no_wrap = getenv("MDVIC_NO_WRAP");
    const char *do_wrap = getenv("MDVIC_WRAP");
    if (no_wrap && no_wrap[0] != '\0') {
        opt->enable_wrap = false;
    } else if (do_wrap && do_wrap[0] != '\0') {
        opt->enable_wrap = true;
    }
    const char *no_osc8 = getenv("MDVIC_NO_OSC8");
    if (no_osc8 && no_osc8[0] != '\0') {
        opt->enable_osc8 = false;
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
#ifndef _WIN32
    /* Prefer TIOCGWINSZ on stdout if a TTY */
    if (isatty(STDOUT_FILENO)) {
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
            if (ws.ws_col > 0) return (int)ws.ws_col;
        }
    }
#else
    /* Windows console: use window width */
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (h != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(h, &info)) {
        SHORT cols = (SHORT)(info.srWindow.Right - info.srWindow.Left + 1);
        if (cols > 0) return (int)cols;
    }
#endif
    /* Fallback: COLUMNS env */
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
