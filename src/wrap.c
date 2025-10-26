/* ANSI-aware, UTF-8 aware minimal wrapping */

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "mdvic/wrap.h"
#include "mdvic/wcwidth.h"

static int is_csi_final(unsigned char c) { return (c >= 0x40 && c <= 0x7E); }

static size_t skip_ansi(const char *s, size_t i, size_t len, FILE *out) {
    unsigned char esc = (unsigned char)s[i];
    if (esc != 0x1B) return i;
    if (i + 1 >= len) return i; /* not enough bytes */
    unsigned char next = (unsigned char)s[i+1];
    if (next == '[') {
        /* CSI: ESC [ ... final */
        size_t j = i + 2;
        while (j < len) {
            unsigned char c = (unsigned char)s[j++];
            if (is_csi_final(c)) break;
        }
        fwrite(s + i, 1, j - i, out);
        return j;
    } else if (next == ']') {
        /* OSC: ESC ] ... BEL or ST */
        size_t j = i + 2;
        for (; j < len; j++) {
            unsigned char c = (unsigned char)s[j];
            if (c == 0x07) { j++; break; }
            if (c == 0x1B && j + 1 < len && (unsigned char)s[j+1] == '\\') { j += 2; break; }
        }
        fwrite(s + i, 1, j - i, out);
        return j;
    } else {
        /* Other escape: copy ESC and next byte if any */
        size_t j = i + ((i + 2 <= len) ? 2 : 1);
        fwrite(s + i, 1, j - i, out);
        return j;
    }
}

static int utf8_decode(const char *s, size_t len, size_t *consumed, uint32_t *outcp) {
    if (len == 0) return -1;
    unsigned char c0 = (unsigned char)s[0];
    if (c0 < 0x80) { *consumed = 1; *outcp = c0; return 0; }
    if ((c0 & 0xE0) == 0xC0) {
        if (len < 2) return -1;
        unsigned char c1 = (unsigned char)s[1];
        if ((c1 & 0xC0) != 0x80) return -1;
        *consumed = 2; *outcp = ((uint32_t)(c0 & 0x1F) << 6) | (uint32_t)(c1 & 0x3F); return 0;
    }
    if ((c0 & 0xF0) == 0xE0) {
        if (len < 3) return -1;
        unsigned char c1 = (unsigned char)s[1], c2 = (unsigned char)s[2];
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80) return -1;
        *consumed = 3; *outcp = ((uint32_t)(c0 & 0x0F) << 12) | ((uint32_t)(c1 & 0x3F) << 6) | (uint32_t)(c2 & 0x3F); return 0;
    }
    if ((c0 & 0xF8) == 0xF0) {
        if (len < 4) return -1;
        unsigned char c1 = (unsigned char)s[1], c2 = (unsigned char)s[2], c3 = (unsigned char)s[3];
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) return -1;
        *consumed = 4; *outcp = ((uint32_t)(c0 & 0x07) << 18) | ((uint32_t)(c1 & 0x3F) << 12) | ((uint32_t)(c2 & 0x3F) << 6) | (uint32_t)(c3 & 0x3F); return 0;
    }
    return -1;
}

static int write_prefix(FILE *out, const char *prefix, int prefix_len) {
    if (prefix && prefix_len > 0) {
        if (fwrite(prefix, 1, (size_t)prefix_len, out) != (size_t)prefix_len) return -1;
    }
    return 0;
}

int mdvic_wrap_write_pref2(FILE *out, const char *s, size_t len, int width, int *col,
                           const char *prefix_first, int prefix_first_len,
                           const char *prefix_next, int prefix_next_len) {
    int ccol = col ? *col : 0;
    size_t i = 0;
    if (ccol == 0) { if (write_prefix(out, prefix_first, prefix_first_len) != 0) return -1; ccol += (prefix_first ? prefix_first_len : 0); }
    while (i < len) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == '\n') {
            if (fputc('\n', out) == EOF) return -1;
            ccol = 0; i++;
            if (write_prefix(out, prefix_next, prefix_next_len) != 0) return -1;
            ccol += (prefix_next ? prefix_next_len : 0);
            continue;
        }
        if (ch == 0x1B) {
            size_t j = skip_ansi(s, i, len, out);
            if (j == i) {
                /* unknown escape, copy as-is */
                if (fputc(ch, out) == EOF) return -1;
                i++;
            } else {
                i = j;
            }
            continue;
        }
        if (width > 0 && ccol >= width) {
            if (fputc('\n', out) == EOF) return -1;
            ccol = 0;
            if (write_prefix(out, prefix_next, prefix_next_len) != 0) return -1;
            ccol += (prefix_next ? prefix_next_len : 0);
        }
        /* Decode utf-8 for width; if invalid, treat as single byte */
        size_t consumed = 1; uint32_t cp = 0; int w = 1;
        if (utf8_decode(s + i, len - i, &consumed, &cp) == 0) {
            w = mdvic_wcwidth(cp);
        }
        if (width > 0 && ccol + w > width && w <= width) {
            if (fputc('\n', out) == EOF) return -1; ccol = 0;
            if (write_prefix(out, prefix_next, prefix_next_len) != 0) return -1;
            ccol += (prefix_next ? prefix_next_len : 0);
        }
        if (fwrite(s + i, 1, consumed, out) != consumed) return -1;
        ccol += w;
        i += consumed;
    }
    if (col) *col = ccol;
    return 0;
}

int mdvic_wrap_write_pref(FILE *out, const char *s, size_t len, int width, int *col,
                          const char *prefix, int prefix_len) {
    return mdvic_wrap_write_pref2(out, s, len, width, col, prefix, prefix_len, prefix, prefix_len);
}

int mdvic_wrap_write(FILE *out, const char *s, size_t len, int width, int *col) {
    return mdvic_wrap_write_pref(out, s, len, width, col, NULL, 0);
}
