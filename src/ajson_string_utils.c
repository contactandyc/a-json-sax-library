// SPDX-FileCopyrightText: 2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include "a-json-sax-library/ajson_string_utils.h"
#include <string.h>

void ajson_file_write_valid_utf8(FILE *out, const char *src, size_t len)
{
    size_t i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)src[i];
        if (c < 0x80) {
            fputc(c, out);
            i++;
        }
        else if ((c & 0xE0) == 0xC0 && (i+1) < len) {
            unsigned char c2 = (unsigned char)src[i+1];
            if ((c2 & 0xC0) == 0x80) {
                fputc(c, out);
                fputc(c2, out);
                i += 2;
            } else i++;
        } else if ((c & 0xF0) == 0xE0 && (i+2) < len) {
            unsigned char c2 = (unsigned char)src[i+1];
            unsigned char c3 = (unsigned char)src[i+2];
            if (((c2 & 0xC0) == 0x80) && ((c3 & 0xC0) == 0x80)) {
                fputc(c, out);
                fputc(c2, out);
                fputc(c3, out);
                i += 3;
            } else i++;
        } else if ((c & 0xF8) == 0xF0 && (i+3) < len) {
            unsigned char c2 = (unsigned char)src[i+1];
            unsigned char c3 = (unsigned char)src[i+2];
            unsigned char c4 = (unsigned char)src[i+3];
            if (((c2 & 0xC0) == 0x80) && ((c3 & 0xC0) == 0x80) && ((c4 & 0xC0) == 0x80)) {
                fputc(c, out);
                fputc(c2, out);
                fputc(c3, out);
                fputc(c4, out);
                i += 4;
            } else i++;
        } else i++;
    }
}

char *ajson_copy_valid_utf8(char *dest, const char *src, size_t len)
{
    size_t i = 0;
    char *d = dest;
    while (i < len) {
        unsigned char c = (unsigned char)src[i];
        if (c < 0x80) {
            *d++ = c;
            i++;
        } else if ((c & 0xE0) == 0xC0 && (i + 1) < len) {
            unsigned char c2 = (unsigned char)src[i + 1];
            if ((c2 & 0xC0) == 0x80) {
                *d++ = c; *d++ = c2; i += 2;
            } else i++;
        } else if ((c & 0xF0) == 0xE0 && (i + 2) < len) {
            unsigned char c2 = (unsigned char)src[i + 1];
            unsigned char c3 = (unsigned char)src[i + 2];
            if (((c2 & 0xC0) == 0x80) && ((c3 & 0xC0) == 0x80)) {
                *d++ = c; *d++ = c2; *d++ = c3; i += 3;
            } else i++;
        } else if ((c & 0xF8) == 0xF0 && (i + 3) < len) {
            unsigned char c2 = (unsigned char)src[i + 1];
            unsigned char c3 = (unsigned char)src[i + 2];
            unsigned char c4 = (unsigned char)src[i + 3];
            if (((c2 & 0xC0) == 0x80) && ((c3 & 0xC0) == 0x80) && ((c4 & 0xC0) == 0x80)) {
                *d++ = c; *d++ = c2; *d++ = c3; *d++ = c4; i += 4;
            } else i++;
        } else i++;
    }
    return d;
}

void ajson_buffer_append_valid_utf8(aml_buffer_t *bh, const char *src, size_t len)
{
    size_t i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)src[i];
        if (c < 0x80) {
            aml_buffer_appendc(bh, c);
            i++;
        } else if ((c & 0xE0) == 0xC0 && (i + 1) < len) {
            unsigned char c2 = (unsigned char)src[i+1];
            if ((c2 & 0xC0) == 0x80) {
                aml_buffer_appendc(bh, c); aml_buffer_appendc(bh, c2); i += 2;
            } else i++;
        } else if ((c & 0xF0) == 0xE0 && (i + 2) < len) {
            unsigned char c2 = (unsigned char)src[i+1];
            unsigned char c3 = (unsigned char)src[i+2];
            if (((c2 & 0xC0) == 0x80) && ((c3 & 0xC0) == 0x80)) {
                aml_buffer_appendc(bh, c); aml_buffer_appendc(bh, c2); aml_buffer_appendc(bh, c3); i += 3;
            } else i++;
        } else if ((c & 0xF8) == 0xF0 && (i + 3) < len) {
            unsigned char c2 = (unsigned char)src[i+1];
            unsigned char c3 = (unsigned char)src[i+2];
            unsigned char c4 = (unsigned char)src[i+3];
            if (((c2 & 0xC0) == 0x80) && ((c3 & 0xC0) == 0x80) && ((c4 & 0xC0) == 0x80)) {
                aml_buffer_appendc(bh, c); aml_buffer_appendc(bh, c2); aml_buffer_appendc(bh, c3); aml_buffer_appendc(bh, c4); i += 4;
            } else i++;
        } else i++;
    }
}

size_t ajson_strip_invalid_utf8_inplace(char *str, size_t len)
{
    size_t in_i = 0;
    size_t out_i = 0;
    while (in_i < len) {
        unsigned char c = (unsigned char)str[in_i];
        if (c < 0x80) {
            str[out_i++] = c;
            in_i++;
        } else if ((c & 0xE0) == 0xC0 && (in_i + 1) < len) {
            unsigned char c2 = (unsigned char)str[in_i + 1];
            if ((c2 & 0xC0) == 0x80) {
                str[out_i++] = c; str[out_i++] = c2; in_i += 2;
            } else in_i++;
        } else if ((c & 0xF0) == 0xE0 && (in_i + 2) < len) {
            unsigned char c2 = (unsigned char)str[in_i + 1];
            unsigned char c3 = (unsigned char)str[in_i + 2];
            if (((c2 & 0xC0) == 0x80) && ((c3 & 0xC0) == 0x80)) {
                str[out_i++] = c; str[out_i++] = c2; str[out_i++] = c3; in_i += 3;
            } else in_i++;
        } else if ((c & 0xF8) == 0xF0 && (in_i + 3) < len) {
            unsigned char c2 = (unsigned char)str[in_i + 1];
            unsigned char c3 = (unsigned char)str[in_i + 2];
            unsigned char c4 = (unsigned char)str[in_i + 3];
            if (((c2 & 0xC0) == 0x80) && ((c3 & 0xC0) == 0x80) && ((c4 & 0xC0) == 0x80)) {
                str[out_i++] = c; str[out_i++] = c2; str[out_i++] = c3; str[out_i++] = c4; in_i += 4;
            } else in_i++;
        } else in_i++;
    }
    return out_i;
}

static int unicode_to_utf8(char *dest, char **src) {
    char *s = *src;
    unsigned int ch;
    int c = *s++;
    if (c >= '0' && c <= '9') ch = c - '0';
    else if (c >= 'A' && c <= 'F') ch = c - 'A' + 10;
    else if (c >= 'a' && c <= 'f') ch = c - 'a' + 10;
    else return -1;

    c = *s++;
    if (c >= '0' && c <= '9') ch = (ch << 4) + c - '0';
    else if (c >= 'A' && c <= 'F') ch = (ch << 4) + c - 'A' + 10;
    else if (c >= 'a' && c <= 'f') ch = (ch << 4) + c - 'a' + 10;
    else return -1;

    c = *s++;
    if (c >= '0' && c <= '9') ch = (ch << 4) + c - '0';
    else if (c >= 'A' && c <= 'F') ch = (ch << 4) + c - 'A' + 10;
    else if (c >= 'a' && c <= 'f') ch = (ch << 4) + c - 'a' + 10;
    else return -1;

    c = *s++;
    if (c >= '0' && c <= '9') ch = (ch << 4) + c - '0';
    else if (c >= 'A' && c <= 'F') ch = (ch << 4) + c - 'A' + 10;
    else if (c >= 'a' && c <= 'f') ch = (ch << 4) + c - 'a' + 10;
    else return -1;

    if (ch >= 0xD800 && ch <= 0xDBFF) {
        if (*s != '\\' || s[1] != 'u') return -1;
        s += 2;
        unsigned int ch2;
        c = *s++;
        if (c >= '0' && c <= '9') ch2 = c - '0';
        else if (c >= 'A' && c <= 'F') ch2 = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') ch2 = c - 'a' + 10;
        else return -1;

        c = *s++;
        if (c >= '0' && c <= '9') ch2 = (ch2 << 4) + c - '0';
        else if (c >= 'A' && c <= 'F') ch2 = (ch2 << 4) + c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') ch2 = (ch2 << 4) + c - 'a' + 10;
        else return -1;

        c = *s++;
        if (c >= '0' && c <= '9') ch2 = (ch2 << 4) + c - '0';
        else if (c >= 'A' && c <= 'F') ch2 = (ch2 << 4) + c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') ch2 = (ch2 << 4) + c - 'a' + 10;
        else return -1;

        c = *s++;
        if (c >= '0' && c <= '9') ch2 = (ch2 << 4) + c - '0';
        else if (c >= 'A' && c <= 'F') ch2 = (ch2 << 4) + c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') ch2 = (ch2 << 4) + c - 'a' + 10;
        else return -1;

        if (ch2 < 0xDC00 || ch2 > 0xDFFF) return -1;
        ch = ((ch - 0xD800) << 10) + (ch2 - 0xDC00) + 0x10000;
    }

    *src = s;

    if (ch < 0x80) { dest[0] = (char)ch; return 1; }
    if (ch < 0x800) {
        dest[0] = (ch >> 6) | 0xC0;
        dest[1] = (ch & 0x3F) | 0x80;
        return 2;
    }
    if (ch < 0x10000) {
        dest[0] = (ch >> 12) | 0xE0;
        dest[1] = ((ch >> 6) & 0x3F) | 0x80;
        dest[2] = (ch & 0x3F) | 0x80;
        return 3;
    }
    if (ch < 0x110000) {
        dest[0] = (ch >> 18) | 0xF0;
        dest[1] = ((ch >> 12) & 0x3F) | 0x80;
        dest[2] = ((ch >> 6) & 0x3F) | 0x80;
        dest[3] = (ch & 0x3F) | 0x80;
        return 4;
    }
    return 0;
}

static inline char *_ajson_decode(aml_pool_t *pool, char **eptr, char *s, char *p, size_t length) {
    char *sp;
    char *res = (char *)aml_pool_alloc(pool, length + 1);
    size_t pos = p - s;
    memcpy(res, s, pos);
    char *rp = res + pos;
    char *ep = s + length;
    while (p < ep) {
        int ch = *p++;
        if (ch != '\\') *rp++ = ch;
        else {
            ch = *p++;
            switch (ch) {
            case '\"': *rp++ = '\"'; break;
            case '\\': *rp++ = '\\'; break;
            case '/':  *rp++ = '/'; break;
            case 'b':  *rp++ = 8; break;
            case 'f':  *rp++ = 12; break;
            case 'n':  *rp++ = 10; break;
            case 'r':  *rp++ = 13; break;
            case 't':  *rp++ = 9; break;
            case 'u':
                sp = p - 2;
                ch = unicode_to_utf8(rp, &p);
                if (ch == -1) {
                    p = sp;
                    *rp++ = *p++; *rp++ = *p++; *rp++ = *p++;
                    *rp++ = *p++; *rp++ = *p++; *rp++ = *p++;
                } else rp += ch;
                break;
            }
        }
    }
    *rp = 0;
    *eptr = rp;
    return res;
}

char *_ajson_encode(aml_pool_t *pool, char *s, char *p, size_t length) {
    char *res = (char *)aml_pool_alloc(pool, (length * 6) + 3);
    char *wp = res;
    memcpy(res, s, p - s);
    wp += (p - s);
    char *ep = s + length;
    while (p < ep) {
        switch (*p) {
        case '\"': *wp++ = '\\'; *wp++ = '\"'; p++; break;
        case '\\': *wp++ = '\\'; *wp++ = '\\'; p++; break;
        case '/':  *wp++ = '\\'; *wp++ = '/';  p++; break;
        case '\b': *wp++ = '\\'; *wp++ = 'b';  p++; break;
        case '\f': *wp++ = '\\'; *wp++ = 'f';  p++; break;
        case '\n': *wp++ = '\\'; *wp++ = 'n';  p++; break;
        case '\r': *wp++ = '\\'; *wp++ = 'r';  p++; break;
        case '\t': *wp++ = '\\'; *wp++ = 't';  p++; break;
        default:
            if ((unsigned char)(*p) < 0x20) {
                sprintf(wp, "\\u%04X", (unsigned char)(*p));
                wp += 6;
                p++;
            } else {
                *wp++ = *p++;
            }
            break;
        }
    }
    *wp = 0;
    return res;
}

char *ajson_encode(aml_pool_t *pool, char *s, size_t length) {
    char *p = s;
    char *ep = s + length;
    while (p < ep) {
        unsigned char ch = (unsigned char)*p;
        if (ch < 0x20 || ch == '\"' || ch == '\\' || ch == '/' || ch == 0)
           return _ajson_encode(pool, s, p, length);
        p++;
    }
    return s;
}

char *ajson_decode(aml_pool_t *pool, char *s, size_t length) {
    char *p = s;
    char *ep = p + length;
    for (;;) {
        if (p == ep) return s;
        else if (*p == '\\') break;
        p++;
    }
    char *eptr = NULL;
    return _ajson_decode(pool, &eptr, s, p, length);
}

char *ajson_decode2(size_t *rlen, aml_pool_t *pool, char *s, size_t length) {
    char *p = s;
    char *ep = p + length;
    for (;;) {
        if (p == ep) {
            *rlen = length;
            return s;
        }
        else if (*p == '\\') break;
        p++;
    }
    char *eptr = NULL;
    char *r = _ajson_decode(pool, &eptr, s, p, length);
    *rlen = eptr - r;
    return r;
}
