/*
 * Wine internal Unicode definitions
 *
 * Copyright 2000 Alexandre Julliard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef __WINE_WINE_UNICODE_H
#define __WINE_WINE_UNICODE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WINE_UNICODE_API
#define WINE_UNICODE_API
#endif

#ifndef WINE_UNICODE_INLINE
#define WINE_UNICODE_INLINE static inline
#endif

#define WCHAR wchar_t

extern WINE_UNICODE_API const WCHAR wine_casemap_lower[];
extern WINE_UNICODE_API const WCHAR wine_casemap_upper[];

WINE_UNICODE_INLINE WCHAR tolowerW( WCHAR ch )
{
    return ch + wine_casemap_lower[wine_casemap_lower[ch >> 8] + (ch & 0xff)];
}

WINE_UNICODE_INLINE WCHAR toupperW( WCHAR ch )
{
    return ch + wine_casemap_upper[wine_casemap_upper[ch >> 8] + (ch & 0xff)];
}

WINE_UNICODE_INLINE WCHAR *strlwrW( WCHAR *str )
{
    WCHAR *ret = str;
    while ((*str = tolowerW(*str))) str++;
    return ret;
}

WINE_UNICODE_INLINE WCHAR *struprW( WCHAR *str )
{
    WCHAR *ret = str;
    while ((*str = toupperW(*str))) str++;
    return ret;
}

#ifdef __cplusplus
}
#endif

#endif  /* __WINE_WINE_UNICODE_H */
