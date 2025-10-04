/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#if defined(__clang_analyzer__)
#define SDL_DISABLE_ANALYZE_MACROS 1
#endif

#include "../SDL_internal.h"

/* This file contains portable string manipulation functions for SDL */

#include "SDL_stdinc.h"
#include "SDL_vacopy.h"

#if defined(__vita__)
#include <psp2/kernel/clib.h>
#endif

#if !defined(HAVE_VSSCANF) || !defined(HAVE_STRTOL) || !defined(HAVE_STRTOUL) || !defined(HAVE_STRTOD) || !defined(HAVE_STRTOLL) || !defined(HAVE_STRTOULL)
#define SDL_isupperhex(X) (((X) >= 'A') && ((X) <= 'F'))
#define SDL_islowerhex(X) (((X) >= 'a') && ((X) <= 'f'))
#endif

#define UTF8_IsLeadByte(c)     ((c) >= 0xC0 && (c) <= 0xF4)
#define UTF8_IsTrailingByte(c) ((c) >= 0x80 && (c) <= 0xBF)

static size_t UTF8_TrailingBytes(unsigned char c)
{
    if (c >= 0xC0 && c <= 0xDF) {
        return 1;
    } else if (c >= 0xE0 && c <= 0xEF) {
        return 2;
    } else if (c >= 0xF0 && c <= 0xF4) {
        return 3;
    }

    return 0;
}

#if !defined(HAVE_VSSCANF) || !defined(HAVE_STRTOL) || !defined(HAVE_STRTOUL) || !defined(HAVE_STRTOD)
/**
 * Parses an unsigned long long and returns the unsigned value and sign bit.
 *
 * Positive values are clamped to ULLONG_MAX.
 * The result `value == 0 && negative` indicates negative overflow
 * and might need to be handled differently depending on whether a
 * signed or unsigned integer is being parsed.
 */
static size_t SDL_ScanUnsignedLongLongInternal(const char *text, int count, int radix, unsigned long long *valuep, SDL_bool *negativep)
{
    const unsigned long long ullong_max = ~0ULL;

    const char *text_start = text;
    const char *number_start = text_start;
    unsigned long long value = 0;
    SDL_bool negative = SDL_FALSE;
    SDL_bool overflow = SDL_FALSE;

    if (radix == 0 || (radix >= 2 && radix <= 36)) {
        while (SDL_isspace(*text)) {
            ++text;
        }
        if (*text == '-' || *text == '+') {
            negative = *text == '-';
            ++text;
        }
        if ((radix == 0 || radix == 16) && *text == '0' && (text[1] == 'x' || text[1] == 'X')) {
            text += 2;
            radix = 16;
        } else if (radix == 0 && *text == '0' && (text[1] >= '0' && text[1] <= '9')) {
            ++text;
            radix = 8;
        } else if (radix == 0) {
            radix = 10;
        }
        number_start = text;
        do {
            unsigned long long digit;
            if (*text >= '0' && *text <= '9') {
                digit = *text - '0';
            } else if (radix > 10) {
                if (*text >= 'A' && *text < 'A' + (radix - 10)) {
                    digit = 10 + (*text - 'A');
                } else if (*text >= 'a' && *text < 'a' + (radix - 10)) {
                    digit = 10 + (*text - 'a');
                } else {
                    break;
                }
            } else {
                break;
            }
            if (value != 0 && radix > ullong_max / value) {
                overflow = SDL_TRUE;
            } else {
                value *= radix;
                if (digit > ullong_max - value) {
                    overflow = SDL_TRUE;
                } else {
                    value += digit;
                }
            }
            ++text;
        } while (count == 0 || (text - text_start) != count);
    }
    if (text == number_start) {
        if (radix == 16 && text > text_start && (*(text - 1) == 'x' || *(text - 1) == 'X')) {
            // the string was "0x"; consume the '0' but not the 'x'
            --text;
        } else {
            // no number was parsed, and thus no characters were consumed
            text = text_start;
        }
    }
    if (overflow) {
        if (negative) {
            value = 0;
        } else {
            value = ullong_max;
        }
    } else if (value == 0) {
        negative = SDL_FALSE;
    }
    *valuep = value;
    *negativep = negative;
    return text - text_start;
}

static size_t SDL_ScanLong(const char *text, int count, int radix, long *valuep)
{
    const unsigned long long_max = (~0UL) >> 1;
    unsigned long long value;
    SDL_bool negative;
    size_t len = SDL_ScanUnsignedLongLongInternal(text, count, radix, &value, &negative);
    if (negative) {
        const unsigned long abs_long_min = long_max + 1;
        if (value == 0 || value > abs_long_min) {
            value = 0ULL - abs_long_min;
        } else {
            value = 0ULL - value;
        }
    } else if (value > long_max) {
        value = long_max;
    }
    *valuep = (long)value;
    return len;
}
#endif

#if !defined(HAVE_VSSCANF) || !defined(HAVE_STRTOUL) || !defined(HAVE_STRTOD)
static size_t SDL_ScanUnsignedLong(const char *text, int count, int radix, unsigned long *valuep)
{
    const unsigned long ulong_max = ~0UL;
    unsigned long long value;
    SDL_bool negative;
    size_t len = SDL_ScanUnsignedLongLongInternal(text, count, radix, &value, &negative);
    if (negative) {
        if (value == 0 || value > ulong_max) {
            value = ulong_max;
        } else if (value == ulong_max) {
            value = 1;
        } else {
            value = 0ULL - value;
        }
    } else if (value > ulong_max) {
        value = ulong_max;
    }
    *valuep = (unsigned long)value;
    return len;
}
#endif

#ifndef HAVE_VSSCANF
static size_t SDL_ScanUintPtrT(const char *text, int radix, uintptr_t *valuep)
{
    const uintptr_t uintptr_max = ~(uintptr_t)0;
    unsigned long long value;
    SDL_bool negative;
    size_t len = SDL_ScanUnsignedLongLongInternal(text, 0, 16, &value, &negative);
    if (negative) {
        if (value == 0 || value > uintptr_max) {
            value = uintptr_max;
        } else if (value == uintptr_max) {
            value = 1;
        } else {
            value = 0ULL - value;
        }
    } else if (value > uintptr_max) {
        value = uintptr_max;
    }
    *valuep = (uintptr_t)value;
    return len;
}
#endif

#if !defined(HAVE_VSSCANF) || !defined(HAVE_STRTOLL) || !defined(HAVE_STRTOULL)
static size_t SDL_ScanLongLong(const char *text, int count, int radix, Sint64 *valuep)
{
    const unsigned long long llong_max = (~0ULL) >> 1;
    unsigned long long value;
    SDL_bool negative;
    size_t len = SDL_ScanUnsignedLongLongInternal(text, count, radix, &value, &negative);
    if (negative) {
        const unsigned long long abs_llong_min = llong_max + 1;
        if (value == 0 || value > abs_llong_min) {
            value = 0ULL - abs_llong_min;
        } else {
            value = 0ULL - value;
        }
    } else if (value > llong_max) {
        value = llong_max;
    }
    *valuep = value;
    return len;
}
#endif

#if !defined(HAVE_VSSCANF) || !defined(HAVE_STRTOULL)
static size_t SDL_ScanUnsignedLongLong(const char *text, int count, int radix, Uint64 *valuep)
{
    const unsigned long long ullong_max = ~0ULL;
    SDL_bool negative;
    size_t len = SDL_ScanUnsignedLongLongInternal(text, count, radix, valuep, &negative);
    if (negative) {
        if (*valuep == 0) {
            *valuep = ullong_max;
        } else {
            *valuep = 0ULL - *valuep;
        }
    }
    return len;
}
#endif

#if !defined(HAVE_VSSCANF) || !defined(HAVE_STRTOD)
static size_t SDL_ScanFloat(const char *text, double *valuep)
{
    const char *text_start = text;
    const char *number_start = text_start;
    double value = 0.0;
    SDL_bool negative = SDL_FALSE;

    while (SDL_isspace(*text)) {
        ++text;
    }
    if (*text == '-' || *text == '+') {
        negative = *text == '-';
        ++text;
    }
    number_start = text;
    if (SDL_isdigit(*text)) {
        value += SDL_strtoull(text, (char **)(&text), 10);
        if (*text == '.') {
            double denom = 10;
            ++text;
            while (SDL_isdigit(*text)) {
                value += (double)(*text - '0') / denom;
                denom *= 10;
                ++text;
            }
        }
    }
    if (text == number_start) {
        // no number was parsed, and thus no characters were consumed
        text = text_start;
    } else if (negative) {
        value = -value;
    }
    *valuep = value;
    return text - text_start;
}
#endif

void *SDL_memmove(SDL_OUT_BYTECAP(len) void *dst, SDL_IN_BYTECAP(len) const void *src, size_t len)
{
#if defined(HAVE_MEMMOVE)
    return memmove(dst, src, len);
#else
    char *srcp = (char *)src;
    char *dstp = (char *)dst;

    if (src < dst) {
        srcp += len - 1;
        dstp += len - 1;
        while (len--) {
            *dstp-- = *srcp--;
        }
    } else {
        while (len--) {
            *dstp++ = *srcp++;
        }
    }
    return dst;
#endif /* HAVE_MEMMOVE */
}

int SDL_memcmp(const void *s1, const void *s2, size_t len)
{
#if defined(__vita__)
    /*
      Using memcmp on NULL is UB per POSIX / C99 7.21.1/2.
      But, both linux and bsd allow that, with an exception:
      zero length strings are always identical, so NULLs are never dereferenced.
      sceClibMemcmp on PSVita doesn't allow that, so we check ourselves.
    */
    if (len == 0) {
        return 0;
    }
    return sceClibMemcmp(s1, s2, len);
#elif defined(HAVE_MEMCMP)
    return memcmp(s1, s2, len);
#else
    char *s1p = (char *)s1;
    char *s2p = (char *)s2;
    while (len--) {
        if (*s1p != *s2p) {
            return *s1p - *s2p;
        }
        ++s1p;
        ++s2p;
    }
    return 0;
#endif /* HAVE_MEMCMP */
}

size_t
SDL_strlen(const char *string)
{
#if defined(HAVE_STRLEN)
    return strlen(string);
#else
    size_t len = 0;
    while (*string++) {
        ++len;
    }
    return len;
#endif /* HAVE_STRLEN */
}

size_t
SDL_wcslen(const wchar_t *string)
{
#if defined(HAVE_WCSLEN)
    return wcslen(string);
#else
    size_t len = 0;
    while (*string++) {
        ++len;
    }
    return len;
#endif /* HAVE_WCSLEN */
}

size_t
SDL_wcslcpy(SDL_OUT_Z_CAP(maxlen) wchar_t *dst, const wchar_t *src, size_t maxlen)
{
#if defined(HAVE_WCSLCPY)
    return wcslcpy(dst, src, maxlen);
#else
    size_t srclen = SDL_wcslen(src);
    if (maxlen > 0) {
        size_t len = SDL_min(srclen, maxlen - 1);
        SDL_memcpy(dst, src, len * sizeof(wchar_t));
        dst[len] = '\0';
    }
    return srclen;
#endif /* HAVE_WCSLCPY */
}

size_t
SDL_wcslcat(SDL_INOUT_Z_CAP(maxlen) wchar_t *dst, const wchar_t *src, size_t maxlen)
{
#if defined(HAVE_WCSLCAT)
    return wcslcat(dst, src, maxlen);
#else
    size_t dstlen = SDL_wcslen(dst);
    size_t srclen = SDL_wcslen(src);
    if (dstlen < maxlen) {
        SDL_wcslcpy(dst + dstlen, src, maxlen - dstlen);
    }
    return dstlen + srclen;
#endif /* HAVE_WCSLCAT */
}

wchar_t *SDL_wcsdup(const wchar_t *string)
{
    size_t len = ((SDL_wcslen(string) + 1) * sizeof(wchar_t));
    wchar_t *newstr = (wchar_t *)SDL_malloc(len);
    if (newstr) {
        SDL_memcpy(newstr, string, len);
    }
    return newstr;
}

wchar_t *SDL_wcsstr(const wchar_t *haystack, const wchar_t *needle)
{
#if defined(HAVE_WCSSTR)
    return SDL_const_cast(wchar_t *, wcsstr(haystack, needle));
#else
    size_t length = SDL_wcslen(needle);
    while (*haystack) {
        if (SDL_wcsncmp(haystack, needle, length) == 0) {
            return (wchar_t *)haystack;
        }
        ++haystack;
    }
    return NULL;
#endif /* HAVE_WCSSTR */
}

int SDL_wcscmp(const wchar_t *str1, const wchar_t *str2)
{
#if defined(HAVE_WCSCMP)
    return wcscmp(str1, str2);
#else
    while (*str1 && *str2) {
        if (*str1 != *str2) {
            break;
        }
        ++str1;
        ++str2;
    }
    return *str1 - *str2;
#endif /* HAVE_WCSCMP */
}

int SDL_wcsncmp(const wchar_t *str1, const wchar_t *str2, size_t maxlen)
{
#if defined(HAVE_WCSNCMP)
    return wcsncmp(str1, str2, maxlen);
#else
    while (*str1 && *str2 && maxlen) {
        if (*str1 != *str2) {
            break;
        }
        ++str1;
        ++str2;
        --maxlen;
    }
    if (!maxlen) {
        return 0;
    }
    return *str1 - *str2;

#endif /* HAVE_WCSNCMP */
}

int SDL_wcscasecmp(const wchar_t *str1, const wchar_t *str2)
{
#if defined(HAVE_WCSCASECMP)
    return wcscasecmp(str1, str2);
#elif defined(HAVE__WCSICMP)
    return _wcsicmp(str1, str2);
#else
    wchar_t a = 0;
    wchar_t b = 0;
    while (*str1 && *str2) {
        /* FIXME: This doesn't actually support wide characters */
        if (*str1 >= 0x80 || *str2 >= 0x80) {
            a = *str1;
            b = *str2;
        } else {
            a = SDL_toupper((unsigned char)*str1);
            b = SDL_toupper((unsigned char)*str2);
        }
        if (a != b) {
            break;
        }
        ++str1;
        ++str2;
    }

    /* FIXME: This doesn't actually support wide characters */
    if (*str1 >= 0x80 || *str2 >= 0x80) {
        a = *str1;
        b = *str2;
    } else {
        a = SDL_toupper((unsigned char)*str1);
        b = SDL_toupper((unsigned char)*str2);
    }
    return (int)((unsigned int)a - (unsigned int)b);
#endif /* HAVE__WCSICMP */
}

int SDL_wcsncasecmp(const wchar_t *str1, const wchar_t *str2, size_t maxlen)
{
#if defined(HAVE_WCSNCASECMP)
    return wcsncasecmp(str1, str2, maxlen);
#elif defined(HAVE__WCSNICMP)
    return _wcsnicmp(str1, str2, maxlen);
#else
    wchar_t a = 0;
    wchar_t b = 0;
    while (*str1 && *str2 && maxlen) {
        /* FIXME: This doesn't actually support wide characters */
        if (*str1 >= 0x80 || *str2 >= 0x80) {
            a = *str1;
            b = *str2;
        } else {
            a = SDL_toupper((unsigned char)*str1);
            b = SDL_toupper((unsigned char)*str2);
        }
        if (a != b) {
            break;
        }
        ++str1;
        ++str2;
        --maxlen;
    }

    if (maxlen == 0) {
        return 0;
    } else {
        /* FIXME: This doesn't actually support wide characters */
        if (*str1 >= 0x80 || *str2 >= 0x80) {
            a = *str1;
            b = *str2;
        } else {
            a = SDL_toupper((unsigned char)*str1);
            b = SDL_toupper((unsigned char)*str2);
        }
        return (int)((unsigned int)a - (unsigned int)b);
    }
#endif /* HAVE__WCSNICMP */
}

size_t
SDL_strlcpy(SDL_OUT_Z_CAP(maxlen) char *dst, const char *src, size_t maxlen)
{
#if defined(HAVE_STRLCPY)
    return strlcpy(dst, src, maxlen);
#else
    size_t srclen = SDL_strlen(src);
    if (maxlen > 0) {
        size_t len = SDL_min(srclen, maxlen - 1);
        SDL_memcpy(dst, src, len);
        dst[len] = '\0';
    }
    return srclen;
#endif /* HAVE_STRLCPY */
}

size_t
SDL_utf8strlcpy(SDL_OUT_Z_CAP(dst_bytes) char *dst, const char *src, size_t dst_bytes)
{
    size_t src_bytes = SDL_strlen(src);
    size_t bytes = SDL_min(src_bytes, dst_bytes - 1);
    size_t i = 0;
    size_t trailing_bytes = 0;

    if (bytes) {
        unsigned char c = (unsigned char)src[bytes - 1];
        if (UTF8_IsLeadByte(c)) {
            --bytes;
        } else if (UTF8_IsTrailingByte(c)) {
            for (i = bytes - 1; i != 0; --i) {
                c = (unsigned char)src[i];
                trailing_bytes = UTF8_TrailingBytes(c);
                if (trailing_bytes) {
                    if (bytes - i != trailing_bytes + 1) {
                        bytes = i;
                    }

                    break;
                }
            }
        }
        SDL_memcpy(dst, src, bytes);
    }
    dst[bytes] = '\0';

    return bytes;
}

size_t
SDL_utf8strlen(const char *str)
{
    size_t retval = 0;
    const char *p = str;
    unsigned char ch;

    while ((ch = *(p++)) != 0) {
        /* if top two bits are 1 and 0, it's a continuation byte. */
        if ((ch & 0xc0) != 0x80) {
            retval++;
        }
    }

    return retval;
}

size_t
SDL_utf8strnlen(const char *str, size_t bytes)
{
    size_t retval = 0;
    const char *p = str;
    unsigned char ch;

    while ((ch = *(p++)) != 0 && bytes-- > 0) {
        /* if top two bits are 1 and 0, it's a continuation byte. */
        if ((ch & 0xc0) != 0x80) {
            retval++;
        }
    }

    return retval;
}

size_t
SDL_strlcat(SDL_INOUT_Z_CAP(maxlen) char *dst, const char *src, size_t maxlen)
{
#if defined(HAVE_STRLCAT)
    return strlcat(dst, src, maxlen);
#else
    size_t dstlen = SDL_strlen(dst);
    size_t srclen = SDL_strlen(src);
    if (dstlen < maxlen) {
        SDL_strlcpy(dst + dstlen, src, maxlen - dstlen);
    }
    return dstlen + srclen;
#endif /* HAVE_STRLCAT */
}

char *SDL_strdup(const char *string)
{
    size_t len = SDL_strlen(string) + 1;
    char *newstr = (char *)SDL_malloc(len);
    if (newstr) {
        SDL_memcpy(newstr, string, len);
    }
    return newstr;
}

char *SDL_strrev(char *string)
{
#if defined(HAVE__STRREV)
    return _strrev(string);
#else
    size_t len = SDL_strlen(string);
    char *a = &string[0];
    char *b = &string[len - 1];
    len /= 2;
    while (len--) {
        char c = *a; /* NOLINT(clang-analyzer-core.uninitialized.Assign) */
        *a++ = *b;
        *b-- = c;
    }
    return string;
#endif /* HAVE__STRREV */
}

char *SDL_strupr(char *string)
{
#if defined(HAVE__STRUPR)
    return _strupr(string);
#else
    char *bufp = string;
    while (*bufp) {
        *bufp = SDL_toupper((unsigned char)*bufp);
        ++bufp;
    }
    return string;
#endif /* HAVE__STRUPR */
}

char *SDL_strlwr(char *string)
{
#if defined(HAVE__STRLWR)
    return _strlwr(string);
#else
    char *bufp = string;
    while (*bufp) {
        *bufp = SDL_tolower((unsigned char)*bufp);
        ++bufp;
    }
    return string;
#endif /* HAVE__STRLWR */
}

char *SDL_strchr(const char *string, int c)
{
#ifdef HAVE_STRCHR
    return SDL_const_cast(char *, strchr(string, c));
#elif defined(HAVE_INDEX)
    return SDL_const_cast(char *, index(string, c));
#else
    while (*string) {
        if (*string == c) {
            return (char *)string;
        }
        ++string;
    }
    if (c == '\0') {
        return (char *)string;
    }
    return NULL;
#endif /* HAVE_STRCHR */
}

char *SDL_strrchr(const char *string, int c)
{
#ifdef HAVE_STRRCHR
    return SDL_const_cast(char *, strrchr(string, c));
#elif defined(HAVE_RINDEX)
    return SDL_const_cast(char *, rindex(string, c));
#else
    const char *bufp = string + SDL_strlen(string);
    while (bufp >= string) {
        if (*bufp == c) {
            return (char *)bufp;
        }
        --bufp;
    }
    return NULL;
#endif /* HAVE_STRRCHR */
}

char *SDL_strstr(const char *haystack, const char *needle)
{
#if defined(HAVE_STRSTR)
    return SDL_const_cast(char *, strstr(haystack, needle));
#else
    size_t length = SDL_strlen(needle);
    while (*haystack) {
        if (SDL_strncmp(haystack, needle, length) == 0) {
            return (char *)haystack;
        }
        ++haystack;
    }
    return NULL;
#endif /* HAVE_STRSTR */
}

char *SDL_strcasestr(const char *haystack, const char *needle)
{
#if defined(HAVE_STRCASESTR)
    return SDL_const_cast(char *, strcasestr(haystack, needle));
#else
    size_t length = SDL_strlen(needle);
    while (*haystack) {
        if (SDL_strncasecmp(haystack, needle, length) == 0) {
            return (char *)haystack;
        }
        ++haystack;
    }
    return NULL;
#endif /* HAVE_STRCASESTR */
}

#if !defined(HAVE__LTOA) || !defined(HAVE__I64TOA) || \
    !defined(HAVE__ULTOA) || !defined(HAVE__UI64TOA)
static const char ntoa_table[] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
    'U', 'V', 'W', 'X', 'Y', 'Z'
};
#endif /* ntoa() conversion table */

char *SDL_itoa(int value, char *string, int radix)
{
#ifdef HAVE_ITOA
    return itoa(value, string, radix);
#else
    return SDL_ltoa((long)value, string, radix);
#endif /* HAVE_ITOA */
}

char *SDL_uitoa(unsigned int value, char *string, int radix)
{
#ifdef HAVE__UITOA
    return _uitoa(value, string, radix);
#else
    return SDL_ultoa((unsigned long)value, string, radix);
#endif /* HAVE__UITOA */
}

char *SDL_ltoa(long value, char *string, int radix)
{
#if defined(HAVE__LTOA)
    return _ltoa(value, string, radix);
#else
    char *bufp = string;

    if (value < 0) {
        *bufp++ = '-';
        SDL_ultoa(-value, bufp, radix);
    } else {
        SDL_ultoa(value, bufp, radix);
    }

    return string;
#endif /* HAVE__LTOA */
}

char *SDL_ultoa(unsigned long value, char *string, int radix)
{
#if defined(HAVE__ULTOA)
    return _ultoa(value, string, radix);
#else
    char *bufp = string;

    if (value) {
        while (value > 0) {
            *bufp++ = ntoa_table[value % radix];
            value /= radix;
        }
    } else {
        *bufp++ = '0';
    }
    *bufp = '\0';

    /* The numbers went into the string backwards. :) */
    SDL_strrev(string);

    return string;
#endif /* HAVE__ULTOA */
}

char *SDL_lltoa(Sint64 value, char *string, int radix)
{
#if defined(HAVE__I64TOA)
    return _i64toa(value, string, radix);
#else
    char *bufp = string;

    if (value < 0) {
        *bufp++ = '-';
        SDL_ulltoa(-value, bufp, radix);
    } else {
        SDL_ulltoa(value, bufp, radix);
    }

    return string;
#endif /* HAVE__I64TOA */
}

char *SDL_ulltoa(Uint64 value, char *string, int radix)
{
#if defined(HAVE__UI64TOA)
    return _ui64toa(value, string, radix);
#else
    char *bufp = string;

    if (value) {
        while (value > 0) {
            *bufp++ = ntoa_table[value % radix];
            value /= radix;
        }
    } else {
        *bufp++ = '0';
    }
    *bufp = '\0';

    /* The numbers went into the string backwards. :) */
    SDL_strrev(string);

    return string;
#endif /* HAVE__UI64TOA */
}

int SDL_atoi(const char *string)
{
#ifdef HAVE_ATOI
    return atoi(string);
#else
    return SDL_strtol(string, NULL, 10);
#endif /* HAVE_ATOI */
}

double SDL_atof(const char *string)
{
#ifdef HAVE_ATOF
    return atof(string);
#else
    return SDL_strtod(string, NULL);
#endif /* HAVE_ATOF */
}

long SDL_strtol(const char *string, char **endp, int base)
{
#if defined(HAVE_STRTOL)
    return strtol(string, endp, base);
#else
    long value = 0;
    size_t len = SDL_ScanLong(string, 0, base, &value);
    if (endp) {
        *endp = (char *)string + len;
    }
    return value;
#endif /* HAVE_STRTOL */
}

unsigned long
SDL_strtoul(const char *string, char **endp, int base)
{
#if defined(HAVE_STRTOUL)
    return strtoul(string, endp, base);
#else
    unsigned long value = 0;
    size_t len = SDL_ScanUnsignedLong(string, 0, base, &value);
    if (endp) {
        *endp = (char *)string + len;
    }
    return value;
#endif /* HAVE_STRTOUL */
}

Sint64 SDL_strtoll(const char *string, char **endp, int base)
{
#if defined(HAVE_STRTOLL)
    return strtoll(string, endp, base);
#else
    long long value = 0;
    size_t len = SDL_ScanLongLong(string, 0, base, &value);
    if (endp) {
        *endp = (char *)string + len;
    }
    return value;
#endif /* HAVE_STRTOLL */
}

Uint64 SDL_strtoull(const char *string, char **endp, int base)
{
#if defined(HAVE_STRTOULL)
    return strtoull(string, endp, base);
#else
    unsigned long long value = 0;
    size_t len = SDL_ScanUnsignedLongLong(string, 0, base, &value);
    if (endp) {
        *endp = (char *)string + len;
    }
    return value;
#endif /* HAVE_STRTOULL */
}

double
SDL_strtod(const char *string, char **endp)
{
#if defined(HAVE_STRTOD)
    return strtod(string, endp);
#else
    size_t len;
    double value = 0.0;

    len = SDL_ScanFloat(string, &value);
    if (endp) {
        *endp = (char *)string + len;
    }
    return value;
#endif /* HAVE_STRTOD */
}

int SDL_strcmp(const char *str1, const char *str2)
{
#if defined(HAVE_STRCMP)
    return strcmp(str1, str2);
#else
    int result;

    while (1) {
        result = ((unsigned char)*str1 - (unsigned char)*str2);
        if (result != 0 || (*str1 == '\0' /* && *str2 == '\0'*/)) {
            break;
        }
        ++str1;
        ++str2;
    }
    return result;
#endif /* HAVE_STRCMP */
}

int SDL_strncmp(const char *str1, const char *str2, size_t maxlen)
{
#if defined(HAVE_STRNCMP)
    return strncmp(str1, str2, maxlen);
#else
    int result;

    while (maxlen) {
        result = (int)(unsigned char)*str1 - (unsigned char)*str2;
        if (result != 0 || *str1 == '\0' /* && *str2 == '\0'*/) {
            break;
        }
        ++str1;
        ++str2;
        --maxlen;
    }
    if (!maxlen) {
        result = 0;
    }
    return result;
#endif /* HAVE_STRNCMP */
}

int SDL_strcasecmp(const char *str1, const char *str2)
{
#ifdef HAVE_STRCASECMP
    return strcasecmp(str1, str2);
#elif defined(HAVE__STRICMP)
    return _stricmp(str1, str2);
#else
    int a, b, result;

    while (1) {
        a = SDL_toupper((unsigned char)*str1);
        b = SDL_toupper((unsigned char)*str2);
        result = a - b;
        if (result != 0 || a == 0 /*&& b == 0*/) {
            break;
        }
        ++str1;
        ++str2;
    }
    return result;
#endif /* HAVE_STRCASECMP */
}

int SDL_strncasecmp(const char *str1, const char *str2, size_t maxlen)
{
#ifdef HAVE_STRNCASECMP
    return strncasecmp(str1, str2, maxlen);
#elif defined(HAVE__STRNICMP)
    return _strnicmp(str1, str2, maxlen);
#else
    int a, b, result;

    while (maxlen) {
        a = SDL_tolower((unsigned char)*str1);
        b = SDL_tolower((unsigned char)*str2);
        result = a - b;
        if (result != 0 || a == 0 /*&& b == 0*/) {
            break;
        }
        ++str1;
        ++str2;
        --maxlen;
    }
    if (maxlen == 0) {
        result = 0;
    }
    return result;
#endif /* HAVE_STRNCASECMP */
}

int SDL_sscanf(const char *text, SDL_SCANF_FORMAT_STRING const char *fmt, ...)
{
    int rc;
    va_list ap;
    va_start(ap, fmt);
    rc = SDL_vsscanf(text, fmt, ap);
    va_end(ap);
    return rc;
}

#ifdef HAVE_VSSCANF
int SDL_vsscanf(const char *text, const char *fmt, va_list ap)
{
    return vsscanf(text, fmt, ap);
}
#else
static SDL_bool CharacterMatchesSet(char c, const char *set, size_t set_len)
{
    SDL_bool invert = SDL_FALSE;
    SDL_bool result = SDL_FALSE;

    if (*set == '^') {
        invert = SDL_TRUE;
        ++set;
        --set_len;
    }
    while (set_len > 0 && !result) {
        if (set_len >= 3 && set[1] == '-') {
            char low_char = SDL_min(set[0], set[2]);
            char high_char = SDL_max(set[0], set[2]);
            if (c >= low_char && c <= high_char) {
                result = SDL_TRUE;
            }
            set += 3;
            set_len -= 3;
        } else {
            if (c == *set) {
                result = SDL_TRUE;
            }
            ++set;
            --set_len;
        }
    }
    if (invert) {
        result = result ? SDL_FALSE : SDL_TRUE;
    }
    return result;
}

/* NOLINTNEXTLINE(readability-non-const-parameter) */
int SDL_vsscanf(const char *text, const char *fmt, va_list ap)
{
    const char *start = text;
    int retval = 0;

    if (!text || !*text) {
        return -1;
    }

    while (*fmt) {
        if (*fmt == ' ') {
            while (SDL_isspace((unsigned char)*text)) {
                ++text;
            }
            ++fmt;
            continue;
        }
        if (*fmt == '%') {
            SDL_bool done = SDL_FALSE;
            long count = 0;
            int radix = 10;
            enum
            {
                DO_SHORT,
                DO_INT,
                DO_LONG,
                DO_LONGLONG,
                DO_SIZE_T
            } inttype = DO_INT;
            size_t advance;
            SDL_bool suppress = SDL_FALSE;

            ++fmt;
            if (*fmt == '%') {
                if (*text == '%') {
                    ++text;
                    ++fmt;
                    continue;
                }
                break;
            }
            if (*fmt == '*') {
                suppress = SDL_TRUE;
                ++fmt;
            }
            fmt += SDL_ScanLong(fmt, 0, 10, &count);

            if (*fmt == 'c') {
                if (!count) {
                    count = 1;
                }
                if (suppress) {
                    while (count--) {
                        ++text;
                    }
                } else {
                    char *valuep = va_arg(ap, char *);
                    while (count--) {
                        *valuep++ = *text++;
                    }
                    ++retval;
                }
                continue;
            }

            while (SDL_isspace((unsigned char)*text)) {
                ++text;
            }

            /* FIXME: implement more of the format specifiers */
            while (!done) {
                switch (*fmt) {
                case '*':
                    suppress = SDL_TRUE;
                    break;
                case 'h':
                    if (inttype == DO_INT) {
                        inttype = DO_SHORT;
                    } else if (inttype > DO_SHORT) {
                        ++inttype;
                    }
                    break;
                case 'l':
                    if (inttype < DO_LONGLONG) {
                        ++inttype;
                    }
                    break;
                case 'I':
                    if (SDL_strncmp(fmt, "I64", 3) == 0) {
                        fmt += 2;
                        inttype = DO_LONGLONG;
                    }
                    break;
                case 'z':
                    inttype = DO_SIZE_T;
                    break;
                case 'i':
                {
                    int index = 0;
                    if (text[index] == '-') {
                        ++index;
                    }
                    if (text[index] == '0') {
                        if (SDL_tolower((unsigned char)text[index + 1]) == 'x') {
                            radix = 16;
                        } else {
                            radix = 8;
                        }
                    }
                }
                    SDL_FALLTHROUGH;
                case 'd':
                    if (inttype == DO_LONGLONG) {
                        Sint64 value = 0;
                        advance = SDL_ScanLongLong(text, count, radix, &value);
                        text += advance;
                        if (advance && !suppress) {
                            Sint64 *valuep = va_arg(ap, Sint64 *);
                            *valuep = value;
                            ++retval;
                        }
                    } else if (inttype == DO_SIZE_T) {
                        Sint64 value = 0;
                        advance = SDL_ScanLongLong(text, count, radix, &value);
                        text += advance;
                        if (advance && !suppress) {
                            size_t *valuep = va_arg(ap, size_t *);
                            *valuep = (size_t)value;
                            ++retval;
                        }
                    } else {
                        long value = 0;
                        advance = SDL_ScanLong(text, count, radix, &value);
                        text += advance;
                        if (advance && !suppress) {
                            switch (inttype) {
                            case DO_SHORT:
                            {
                                short *valuep = va_arg(ap, short *);
                                *valuep = (short)value;
                            } break;
                            case DO_INT:
                            {
                                int *valuep = va_arg(ap, int *);
                                *valuep = (int)value;
                            } break;
                            case DO_LONG:
                            {
                                long *valuep = va_arg(ap, long *);
                                *valuep = value;
                            } break;
                            case DO_LONGLONG:
                            case DO_SIZE_T:
                                /* Handled above */
                                break;
                            }
                            ++retval;
                        }
                    }
                    done = SDL_TRUE;
                    break;
                case 'o':
                    if (radix == 10) {
                        radix = 8;
                    }
                    SDL_FALLTHROUGH;
                case 'x':
                case 'X':
                    if (radix == 10) {
                        radix = 16;
                    }
                    SDL_FALLTHROUGH;
                case 'u':
                    if (inttype == DO_LONGLONG) {
                        Uint64 value = 0;
                        advance = SDL_ScanUnsignedLongLong(text, count, radix, &value);
                        text += advance;
                        if (advance && !suppress) {
                            Uint64 *valuep = va_arg(ap, Uint64 *);
                            *valuep = value;
                            ++retval;
                        }
                    } else if (inttype == DO_SIZE_T) {
                        Uint64 value = 0;
                        advance = SDL_ScanUnsignedLongLong(text, count, radix, &value);
                        text += advance;
                        if (advance && !suppress) {
                            size_t *valuep = va_arg(ap, size_t *);
                            *valuep = (size_t)value;
                            ++retval;
                        }
                    } else {
                        unsigned long value = 0;
                        advance = SDL_ScanUnsignedLong(text, count, radix, &value);
                        text += advance;
                        if (advance && !suppress) {
                            switch (inttype) {
                            case DO_SHORT:
                            {
                                short *valuep = va_arg(ap, short *);
                                *valuep = (short)value;
                            } break;
                            case DO_INT:
                            {
                                int *valuep = va_arg(ap, int *);
                                *valuep = (int)value;
                            } break;
                            case DO_LONG:
                            {
                                long *valuep = va_arg(ap, long *);
                                *valuep = value;
                            } break;
                            case DO_LONGLONG:
                            case DO_SIZE_T:
                                /* Handled above */
                                break;
                            }
                            ++retval;
                        }
                    }
                    done = SDL_TRUE;
                    break;
                case 'p':
                {
                    uintptr_t value = 0;
                    advance = SDL_ScanUintPtrT(text, 16, &value);
                    text += advance;
                    if (advance && !suppress) {
                        void **valuep = va_arg(ap, void **);
                        *valuep = (void *)value;
                        ++retval;
                    }
                }
                    done = SDL_TRUE;
                    break;
                case 'f':
                {
                    double value = 0.0;
                    advance = SDL_ScanFloat(text, &value);
                    text += advance;
                    if (advance && !suppress) {
                        float *valuep = va_arg(ap, float *);
                        *valuep = (float)value;
                        ++retval;
                    }
                }
                    done = SDL_TRUE;
                    break;
                case 's':
                    if (suppress) {
                        while (!SDL_isspace((unsigned char)*text)) {
                            ++text;
                            if (count) {
                                if (--count == 0) {
                                    break;
                                }
                            }
                        }
                    } else {
                        char *valuep = va_arg(ap, char *);
                        while (!SDL_isspace((unsigned char)*text)) {
                            *valuep++ = *text++;
                            if (count) {
                                if (--count == 0) {
                                    break;
                                }
                            }
                        }
                        *valuep = '\0';
                        ++retval;
                    }
                    done = SDL_TRUE;
                    break;
                case 'n':
                    switch (inttype) {
                    case DO_SHORT:
                    {
                        short *valuep = va_arg(ap, short *);
                        *valuep = (short)(text - start);
                    } break;
                    case DO_INT:
                    {
                        int *valuep = va_arg(ap, int *);
                        *valuep = (int)(text - start);
                    } break;
                    case DO_LONG:
                    {
                        long *valuep = va_arg(ap, long *);
                        *valuep = (long)(text - start);
                    } break;
                    case DO_LONGLONG:
                    {
                        long long *valuep = va_arg(ap, long long *);
                        *valuep = (long long)(text - start);
                    } break;
                    case DO_SIZE_T:
                    {
                        size_t *valuep = va_arg(ap, size_t *);
                        *valuep = (size_t)(text - start);
                    } break;
                    }
                    done = SDL_TRUE;
                    break;
                case '[':
                {
                    const char *set = fmt + 1;
                    while (*fmt && *fmt != ']') {
                        ++fmt;
                    }
                    if (*fmt) {
                        size_t set_len = (fmt - set);
                        if (suppress) {
                            while (CharacterMatchesSet(*text, set, set_len)) {
                                ++text;
                                if (count) {
                                    if (--count == 0) {
                                        break;
                                    }
                                }
                            }
                        } else {
                            SDL_bool had_match = SDL_FALSE;
                            char *valuep = va_arg(ap, char *);
                            while (CharacterMatchesSet(*text, set, set_len)) {
                                had_match = SDL_TRUE;
                                *valuep++ = *text++;
                                if (count) {
                                    if (--count == 0) {
                                        break;
                                    }
                                }
                            }
                            *valuep = '\0';
                            if (had_match) {
                                ++retval;
                            }
                        }
                    }
                }
                    done = SDL_TRUE;
                    break;
                default:
                    done = SDL_TRUE;
                    break;
                }
                ++fmt;
            }
            continue;
        }
        if (*text == *fmt) {
            ++text;
            ++fmt;
            continue;
        }
        /* Text didn't match format specifier */
        break;
    }

    return retval;
}
#endif /* HAVE_VSSCANF */

int SDL_snprintf(SDL_OUT_Z_CAP(maxlen) char *text, size_t maxlen, SDL_PRINTF_FORMAT_STRING const char *fmt, ...)
{
    va_list ap;
    int retval;

    va_start(ap, fmt);
    retval = SDL_vsnprintf(text, maxlen, fmt, ap);
    va_end(ap);

    return retval;
}

#if defined(HAVE_LIBC) && defined(__WATCOMC__)
/* _vsnprintf() doesn't ensure nul termination */
int SDL_vsnprintf(SDL_OUT_Z_CAP(maxlen) char *text, size_t maxlen, const char *fmt, va_list ap)
{
    int retval;
    if (!fmt) {
        fmt = "";
    }
    retval = _vsnprintf(text, maxlen, fmt, ap);
    if (maxlen > 0) {
        text[maxlen - 1] = '\0';
    }
    if (retval < 0) {
        retval = (int)maxlen;
    }
    return retval;
}
#elif defined(HAVE_VSNPRINTF)
int SDL_vsnprintf(SDL_OUT_Z_CAP(maxlen) char *text, size_t maxlen, const char *fmt, va_list ap)
{
    if (!fmt) {
        fmt = "";
    }
    return vsnprintf(text, maxlen, fmt, ap);
}
#else
#define TEXT_AND_LEN_ARGS (length < maxlen) ? &text[length] : NULL, (length < maxlen) ? (maxlen - length) : 0

/* FIXME: implement more of the format specifiers */
typedef enum
{
    SDL_CASE_NOCHANGE,
    SDL_CASE_LOWER,
    SDL_CASE_UPPER
} SDL_letter_case;

typedef struct
{
    SDL_bool left_justify;
    SDL_bool force_sign;
    SDL_bool force_type; /* for now: used only by float printer, ignored otherwise. */
    SDL_bool pad_zeroes;
    SDL_letter_case force_case;
    int width;
    int radix;
    int precision;
} SDL_FormatInfo;

static size_t SDL_PrintString(char *text, size_t maxlen, SDL_FormatInfo *info, const char *string)
{
    const char fill = (info && info->pad_zeroes) ? '0' : ' ';
    size_t width = 0;
    size_t filllen = 0;
    size_t length = 0;
    size_t slen, sz;

    if (!string) {
        string = "(null)";
    }

    sz = SDL_strlen(string);
    if (info && info->width > 0 && (size_t)info->width > sz) {
        width = info->width - sz;
        if (info->precision >= 0 && (size_t)info->precision < sz) {
            width += sz - (size_t)info->precision;
        }

        filllen = SDL_min(width, maxlen);
        if (!info->left_justify) {
            SDL_memset(text, fill, filllen);
            text += filllen;
            maxlen -= filllen;
            length += width;
            filllen = 0;
        }
    }

    SDL_strlcpy(text, string, maxlen);
    length += sz;

    if (filllen > 0) {
        SDL_memset(text + sz, fill, filllen);
        length += width;
    }

    if (info) {
        if (info->precision >= 0 && (size_t)info->precision < sz) {
            slen = (size_t)info->precision;
            if (slen < maxlen) {
                text[slen] = '\0';
            }
            length -= (sz - slen);
        }
        if (maxlen > 1) {
            if (info->force_case == SDL_CASE_LOWER) {
                SDL_strlwr(text);
            } else if (info->force_case == SDL_CASE_UPPER) {
                SDL_strupr(text);
            }
        }
    }
    return length;
}

static size_t SDL_PrintStringW(char *text, size_t maxlen, SDL_FormatInfo *info, const wchar_t *wide_string)
{
    size_t length = 0;
    if (wide_string) {
        char *string = SDL_iconv_string("UTF-8", "WCHAR_T", (char *)(wide_string), (SDL_wcslen(wide_string) + 1) * sizeof(*wide_string));
        length = SDL_PrintString(TEXT_AND_LEN_ARGS, info, string);
        SDL_free(string);
    } else {
        length = SDL_PrintString(TEXT_AND_LEN_ARGS, info, NULL);
    }
    return length;
}

static void SDL_IntPrecisionAdjust(char *num, size_t maxlen, SDL_FormatInfo *info)
{ /* left-pad num with zeroes. */
    size_t sz, pad, have_sign;

    if (!info) {
        return;
    }

    have_sign = 0;
    if (*num == '-' || *num == '+') {
        have_sign = 1;
        ++num;
        --maxlen;
    }
    sz = SDL_strlen(num);
    if (info->precision > 0 && sz < (size_t)info->precision) {
        pad = (size_t)info->precision - sz;
        if (pad + sz + 1 <= maxlen) { /* otherwise ignore the precision */
            SDL_memmove(num + pad, num, sz + 1);
            SDL_memset(num, '0', pad);
        }
    }
    info->precision = -1; /* so that SDL_PrintString() doesn't make a mess. */

    if (info->pad_zeroes && info->width > 0 && (size_t)info->width > sz + have_sign) {
        /* handle here: spaces are added before the sign
           but zeroes must be placed _after_ the sign. */
        /* sz hasn't changed: we ignore pad_zeroes if a precision is given. */
        pad = (size_t)info->width - sz - have_sign;
        if (pad + sz + 1 <= maxlen) {
            SDL_memmove(num + pad, num, sz + 1);
            SDL_memset(num, '0', pad);
        }
        info->width = 0; /* so that SDL_PrintString() doesn't make a mess. */
    }
}

static size_t SDL_PrintLong(char *text, size_t maxlen, SDL_FormatInfo *info, long value)
{
    char num[130], *p = num;

    if (info->force_sign && value >= 0L) {
        *p++ = '+';
    }

    SDL_ltoa(value, p, info ? info->radix : 10);
    SDL_IntPrecisionAdjust(num, sizeof(num), info);
    return SDL_PrintString(text, maxlen, info, num);
}

static size_t SDL_PrintUnsignedLong(char *text, size_t maxlen, SDL_FormatInfo *info, unsigned long value)
{
    char num[130];

    SDL_ultoa(value, num, info ? info->radix : 10);
    SDL_IntPrecisionAdjust(num, sizeof(num), info);
    return SDL_PrintString(text, maxlen, info, num);
}

static size_t SDL_PrintLongLong(char *text, size_t maxlen, SDL_FormatInfo *info, Sint64 value)
{
    char num[130], *p = num;

    if (info->force_sign && value >= (Sint64)0) {
        *p++ = '+';
    }

    SDL_lltoa(value, p, info ? info->radix : 10);
    SDL_IntPrecisionAdjust(num, sizeof(num), info);
    return SDL_PrintString(text, maxlen, info, num);
}

static size_t SDL_PrintUnsignedLongLong(char *text, size_t maxlen, SDL_FormatInfo *info, Uint64 value)
{
    char num[130];

    SDL_ulltoa(value, num, info ? info->radix : 10);
    SDL_IntPrecisionAdjust(num, sizeof(num), info);
    return SDL_PrintString(text, maxlen, info, num);
}

static size_t SDL_PrintFloat(char *text, size_t maxlen, SDL_FormatInfo *info, double arg)
{
    size_t length = 0;

    /* This isn't especially accurate, but hey, it's easy. :) */
    unsigned long value;

    if (arg < 0) {
        if (length < maxlen) {
            text[length] = '-';
        }
        ++length;
        arg = -arg;
    } else if (info->force_sign) {
        if (length < maxlen) {
            text[length] = '+';
        }
        ++length;
    }
    value = (unsigned long)arg;
    length += SDL_PrintUnsignedLong(TEXT_AND_LEN_ARGS, NULL, value);
    arg -= value;
    if (info->precision < 0) {
        info->precision = 6;
    }
    if (info->force_type || info->precision > 0) {
        int mult = 10;
        if (length < maxlen) {
            text[length] = '.';
        }
        ++length;
        while (info->precision-- > 0) {
            value = (unsigned long)(arg * mult);
            length += SDL_PrintUnsignedLong(TEXT_AND_LEN_ARGS, NULL, value);
            arg -= (double)value / mult;
            mult *= 10;
        }
    }

    if (info->width > 0 && (size_t)info->width > length) {
        const char fill = info->pad_zeroes ? '0' : ' ';
        size_t width = info->width - length;
        size_t filllen, movelen;

        filllen = SDL_min(width, maxlen);
        movelen = SDL_min(length, (maxlen - filllen));
        SDL_memmove(&text[filllen], text, movelen);
        SDL_memset(text, fill, filllen);
        length += width;
    }

    return length;
}

static size_t SDL_PrintPointer(char *text, size_t maxlen, SDL_FormatInfo *info, const void *value)
{
    char num[130];
    size_t length;

    if (!value) {
        return SDL_PrintString(text, maxlen, info, NULL);
    }

    SDL_ulltoa((unsigned long long)(uintptr_t)value, num, 16);
    length = SDL_PrintString(text, maxlen, info, "0x");
    return length + SDL_PrintString(TEXT_AND_LEN_ARGS, info, num);
}

/* NOLINTNEXTLINE(readability-non-const-parameter) */
int SDL_vsnprintf(SDL_OUT_Z_CAP(maxlen) char *text, size_t maxlen, const char *fmt, va_list ap)
{
    size_t length = 0;

    if (!text) {
        maxlen = 0;
    }
    if (!fmt) {
        fmt = "";
    }
    while (*fmt) {
        if (*fmt == '%') {
            SDL_bool done = SDL_FALSE;
            SDL_bool check_flag;
            SDL_FormatInfo info;
            enum
            {
                DO_INT,
                DO_LONG,
                DO_LONGLONG,
                DO_SIZE_T
            } inttype = DO_INT;

            SDL_zero(info);
            info.radix = 10;
            info.precision = -1;

            check_flag = SDL_TRUE;
            while (check_flag) {
                ++fmt;
                switch (*fmt) {
                case '-':
                    info.left_justify = SDL_TRUE;
                    break;
                case '+':
                    info.force_sign = SDL_TRUE;
                    break;
                case '#':
                    info.force_type = SDL_TRUE;
                    break;
                case '0':
                    info.pad_zeroes = SDL_TRUE;
                    break;
                default:
                    check_flag = SDL_FALSE;
                    break;
                }
            }

            if (*fmt >= '0' && *fmt <= '9') {
                info.width = SDL_strtol(fmt, (char **)&fmt, 0);
            } else if (*fmt == '*') {
                ++fmt;
                info.width = va_arg(ap, int);
            }

            if (*fmt == '.') {
                ++fmt;
                if (*fmt >= '0' && *fmt <= '9') {
                    info.precision = SDL_strtol(fmt, (char **)&fmt, 0);
                } else if (*fmt == '*') {
                    ++fmt;
                    info.precision = va_arg(ap, int);
                } else {
                    info.precision = 0;
                }
                if (info.precision < 0) {
                    info.precision = 0;
                }
            }

            while (!done) {
                switch (*fmt) {
                case '%':
                    if (length < maxlen) {
                        text[length] = '%';
                    }
                    ++length;
                    done = SDL_TRUE;
                    break;
                case 'c':
                    /* char is promoted to int when passed through (...) */
                    if (length < maxlen) {
                        text[length] = (char)va_arg(ap, int);
                    }
                    ++length;
                    done = SDL_TRUE;
                    break;
                case 'h':
                    /* short is promoted to int when passed through (...) */
                    break;
                case 'l':
                    if (inttype < DO_LONGLONG) {
                        ++inttype;
                    }
                    break;
                case 'I':
                    if (SDL_strncmp(fmt, "I64", 3) == 0) {
                        fmt += 2;
                        inttype = DO_LONGLONG;
                    }
                    break;
                case 'z':
                    inttype = DO_SIZE_T;
                    break;
                case 'i':
                case 'd':
                    if (info.precision >= 0) {
                        info.pad_zeroes = SDL_FALSE;
                    }
                    switch (inttype) {
                    case DO_INT:
                        length += SDL_PrintLong(TEXT_AND_LEN_ARGS, &info,
                                                (long)va_arg(ap, int));
                        break;
                    case DO_LONG:
                        length += SDL_PrintLong(TEXT_AND_LEN_ARGS, &info,
                                                va_arg(ap, long));
                        break;
                    case DO_LONGLONG:
                        length += SDL_PrintLongLong(TEXT_AND_LEN_ARGS, &info,
                                                    va_arg(ap, Sint64));
                        break;
                    case DO_SIZE_T:
                        length += SDL_PrintLongLong(TEXT_AND_LEN_ARGS, &info,
                                                    va_arg(ap, size_t));
                        break;
                    }
                    done = SDL_TRUE;
                    break;
                case 'p':
                    info.force_case = SDL_CASE_LOWER;
                    length += SDL_PrintPointer(TEXT_AND_LEN_ARGS, &info, va_arg(ap, void *));
                    done = SDL_TRUE;
                    break;
                case 'x':
                    info.force_case = SDL_CASE_LOWER;
                    SDL_FALLTHROUGH;
                case 'X':
                    if (info.force_case == SDL_CASE_NOCHANGE) {
                        info.force_case = SDL_CASE_UPPER;
                    }
                    if (info.radix == 10) {
                        info.radix = 16;
                    }
                    SDL_FALLTHROUGH;
                case 'o':
                    if (info.radix == 10) {
                        info.radix = 8;
                    }
                    SDL_FALLTHROUGH;
                case 'u':
                    info.force_sign = SDL_FALSE;
                    if (info.precision >= 0) {
                        info.pad_zeroes = SDL_FALSE;
                    }
                    switch (inttype) {
                    case DO_INT:
                        length += SDL_PrintUnsignedLong(TEXT_AND_LEN_ARGS, &info,
                                                        (unsigned long)
                                                            va_arg(ap, unsigned int));
                        break;
                    case DO_LONG:
                        length += SDL_PrintUnsignedLong(TEXT_AND_LEN_ARGS, &info,
                                                        va_arg(ap, unsigned long));
                        break;
                    case DO_LONGLONG:
                        length += SDL_PrintUnsignedLongLong(TEXT_AND_LEN_ARGS, &info,
                                                            va_arg(ap, Uint64));
                        break;
                    case DO_SIZE_T:
                        length += SDL_PrintUnsignedLongLong(TEXT_AND_LEN_ARGS, &info,
                                                            va_arg(ap, size_t));
                        break;
                    }
                    done = SDL_TRUE;
                    break;
                case 'f':
                    length += SDL_PrintFloat(TEXT_AND_LEN_ARGS, &info, va_arg(ap, double));
                    done = SDL_TRUE;
                    break;
                case 'S':
                    info.pad_zeroes = SDL_FALSE;
                    length += SDL_PrintStringW(TEXT_AND_LEN_ARGS, &info, va_arg(ap, wchar_t *));
                    done = SDL_TRUE;
                    break;
                case 's':
                    info.pad_zeroes = SDL_FALSE;
                    if (inttype > DO_INT) {
                        length += SDL_PrintStringW(TEXT_AND_LEN_ARGS, &info, va_arg(ap, wchar_t *));
                    } else {
                        length += SDL_PrintString(TEXT_AND_LEN_ARGS, &info, va_arg(ap, char *));
                    }
                    done = SDL_TRUE;
                    break;
                default:
                    done = SDL_TRUE;
                    break;
                }
                ++fmt;
            }
        } else {
            if (length < maxlen) {
                text[length] = *fmt;
            }
            ++fmt;
            ++length;
        }
    }
    if (length < maxlen) {
        text[length] = '\0';
    } else if (maxlen > 0) {
        text[maxlen - 1] = '\0';
    }
    return (int)length;
}

#undef TEXT_AND_LEN_ARGS
#endif /* HAVE_VSNPRINTF */

int SDL_asprintf(char **strp, SDL_PRINTF_FORMAT_STRING const char *fmt, ...)
{
    va_list ap;
    int retval;

    va_start(ap, fmt);
    retval = SDL_vasprintf(strp, fmt, ap);
    va_end(ap);

    return retval;
}

int SDL_vasprintf(char **strp, const char *fmt, va_list ap)
{
    int retval;
    int size = 100; /* Guess we need no more than 100 bytes */
    char *p, *np;
    va_list aq;

    *strp = NULL;

    p = (char *)SDL_malloc(size);
    if (!p) {
        return -1;
    }

    while (1) {
        /* Try to print in the allocated space */
        va_copy(aq, ap);
        retval = SDL_vsnprintf(p, size, fmt, aq);
        va_end(aq);

        /* Check error code */
        if (retval < 0) {
            SDL_free(p);
            return retval;
        }

        /* If that worked, return the string */
        if (retval < size) {
            *strp = p;
            return retval;
        }

        /* Else try again with more space */
        size = retval + 1; /* Precisely what is needed */

        np = (char *)SDL_realloc(p, size);
        if (!np) {
            SDL_free(p);
            return -1;
        } else {
            p = np;
        }
    }
}
