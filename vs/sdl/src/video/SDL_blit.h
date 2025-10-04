/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2012 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

#ifndef _SDL_blit_h
#define _SDL_blit_h

#include "SDL_endian.h"

/* The structure passed to the low level blit functions */
typedef struct {
	Uint8 *s_pixels;
	int s_width;
	int s_height;
	int s_skip;
	Uint8 *d_pixels;
	int d_width;
	int d_height;
	int d_skip;
	void *aux_data;
	SDL_PixelFormat *src;
	Uint8 *table;
	SDL_PixelFormat *dst;
} SDL_BlitInfo;

/* The type definition for the low level blit functions */
typedef void (*SDL_loblit)(SDL_BlitInfo *info);

/* This is the private info structure for software accelerated blits */
struct private_swaccel {
	SDL_loblit blit;
	void *aux_data;
};

/* Blit mapping definition */
typedef struct SDL_BlitMap {
	SDL_Surface *dst;
	int identity;
	Uint8 *table;
	SDL_blit hw_blit;
	SDL_blit sw_blit;
	struct private_hwaccel *hw_data;
	struct private_swaccel *sw_data;

	/* the version count matches the destination; mismatch indicates
	   an invalid mapping */
        unsigned int format_version;
} SDL_BlitMap;


/* Functions found in SDL_blit.c */
extern int SDL_CalculateBlit(SDL_Surface *surface);

/* Functions found in SDL_blit_{0,1,N,A}.c */
extern SDL_loblit SDL_CalculateBlit0(SDL_Surface *surface, int complex);
extern SDL_loblit SDL_CalculateBlit1(SDL_Surface *surface, int complex);
extern SDL_loblit SDL_CalculateBlitN(SDL_Surface *surface, int complex);
extern SDL_loblit SDL_CalculateAlphaBlit(SDL_Surface *surface, int complex);

/*
 * Useful macros for blitting routines
 */

#define FORMAT_EQUAL(A, B)						\
    ((A)->BitsPerPixel == (B)->BitsPerPixel				\
     && ((A)->Rmask == (B)->Rmask) && ((A)->Amask == (B)->Amask))

/* Load pixel of the specified format from a buffer and get its R-G-B values */
/* FIXME: rescale values to 0..255 here? */
#define RGB_FROM_PIXEL(Pixel, fmt, r, g, b)				\
{									\
	r = (((Pixel&fmt->Rmask)>>fmt->Rshift)<<fmt->Rloss); 		\
	g = (((Pixel&fmt->Gmask)>>fmt->Gshift)<<fmt->Gloss); 		\
	b = (((Pixel&fmt->Bmask)>>fmt->Bshift)<<fmt->Bloss); 		\
}
#define RGB_FROM_RGB565(Pixel, r, g, b)					\
{									\
	r = (((Pixel&0xF800)>>11)<<3);		 			\
	g = (((Pixel&0x07E0)>>5)<<2); 					\
	b = ((Pixel&0x001F)<<3); 					\
}
#define RGB_FROM_RGB555(Pixel, r, g, b)					\
{									\
	r = (((Pixel&0x7C00)>>10)<<3);		 			\
	g = (((Pixel&0x03E0)>>5)<<3); 					\
	b = ((Pixel&0x001F)<<3); 					\
}
#define RGB_FROM_RGB888(Pixel, r, g, b)					\
{									\
	r = ((Pixel&0xFF0000)>>16);		 			\
	g = ((Pixel&0xFF00)>>8);		 			\
	b = (Pixel&0xFF);			 			\
}
#define RETRIEVE_RGB_PIXEL(buf, bpp, Pixel)				   \
do {									   \
	switch (bpp) {							   \
		case 2:							   \
			Pixel = *((Uint16 *)(buf));			   \
		break;							   \
									   \
		case 3: {						   \
		        Uint8 *B = (Uint8 *)(buf);			   \
			if(SDL_BYTEORDER == SDL_LIL_ENDIAN) {		   \
			        Pixel = B[0] + (B[1] << 8) + (B[2] << 16); \
			} else {					   \
			        Pixel = (B[0] << 16) + (B[1] << 8) + B[2]; \
			}						   \
		}							   \
		break;							   \
									   \
		case 4:							   \
			Pixel = *((Uint32 *)(buf));			   \
		break;							   \
									   \
		default:						   \
			Pixel = 0; /* appease gcc */			   \
		break;							   \
	}								   \
} while(0)

#define DISEMBLE_RGB(buf, bpp, fmt, Pixel, r, g, b)			   \
do {									   \
	switch (bpp) {							   \
		case 2:							   \
			Pixel = *((Uint16 *)(buf));			   \
		break;							   \
									   \
		case 3: {						   \
		        Uint8 *B = (Uint8 *)buf;			   \
			if(SDL_BYTEORDER == SDL_LIL_ENDIAN) {		   \
			        Pixel = B[0] + (B[1] << 8) + (B[2] << 16); \
			} else {					   \
			        Pixel = (B[0] << 16) + (B[1] << 8) + B[2]; \
			}						   \
		}							   \
		break;							   \
									   \
		case 4:							   \
			Pixel = *((Uint32 *)(buf));			   \
		break;							   \
									   \
	        default:						   \
		        Pixel = 0;	/* prevent gcc from complaining */ \
		break;							   \
	}								   \
	RGB_FROM_PIXEL(Pixel, fmt, r, g, b);				   \
} while(0)

/* Assemble R-G-B values into a specified pixel format and store them */
#ifdef __NDS__ /* FIXME */
#define PIXEL_FROM_RGB(Pixel, fmt, r, g, b)				\
{									\
	Pixel = ((r>>fmt->Rloss)<<fmt->Rshift)|				\
		((g>>fmt->Gloss)<<fmt->Gshift)|				\
		((b>>fmt->Bloss)<<fmt->Bshift) | (1<<15);				\
}
#else
#define PIXEL_FROM_RGB(Pixel, fmt, r, g, b)				\
{									\
	Pixel = ((r>>fmt->Rloss)<<fmt->Rshift)|				\
		((g>>fmt->Gloss)<<fmt->Gshift)|				\
		((b>>fmt->Bloss)<<fmt->Bshift);				\
}
#endif /* __NDS__ FIXME */
#define RGB565_FROM_RGB(Pixel, r, g, b)					\
{									\
	Pixel = ((r>>3)<<11)|((g>>2)<<5)|(b>>3);			\
}
#define RGB555_FROM_RGB(Pixel, r, g, b)					\
{									\
	Pixel = ((r>>3)<<10)|((g>>3)<<5)|(b>>3);			\
}
#define RGB888_FROM_RGB(Pixel, r, g, b)					\
{									\
	Pixel = (r<<16)|(g<<8)|b;					\
}
#define ASSEMBLE_RGB(buf, bpp, fmt, r, g, b) 				\
{									\
	switch (bpp) {							\
		case 2: {						\
			Uint16 Pixel;					\
									\
			PIXEL_FROM_RGB(Pixel, fmt, r, g, b);		\
			*((Uint16 *)(buf)) = Pixel;			\
		}							\
		break;							\
									\
		case 3: {						\
                        if(SDL_BYTEORDER == SDL_LIL_ENDIAN) {		\
			        *((buf)+fmt->Rshift/8) = r;		\
				*((buf)+fmt->Gshift/8) = g;		\
				*((buf)+fmt->Bshift/8) = b;		\
			} else {					\
			        *((buf)+2-fmt->Rshift/8) = r;		\
				*((buf)+2-fmt->Gshift/8) = g;		\
				*((buf)+2-fmt->Bshift/8) = b;		\
			}						\
		}							\
		break;							\
									\
		case 4: {						\
			Uint32 Pixel;					\
									\
			PIXEL_FROM_RGB(Pixel, fmt, r, g, b);		\
			*((Uint32 *)(buf)) = Pixel;			\
		}							\
		break;							\
	}								\
}
#define ASSEMBLE_RGB_AMASK(buf, bpp, fmt, r, g, b, Amask)		\
{									\
	switch (bpp) {							\
		case 2: {						\
			Uint16 *bufp;					\
			Uint16 Pixel;					\
									\
			bufp = (Uint16 *)buf;				\
			PIXEL_FROM_RGB(Pixel, fmt, r, g, b);		\
			*bufp = Pixel | (*bufp & Amask);		\
		}							\
		break;							\
									\
		case 3: {						\
                        if(SDL_BYTEORDER == SDL_LIL_ENDIAN) {		\
			        *((buf)+fmt->Rshift/8) = r;		\
				*((buf)+fmt->Gshift/8) = g;		\
				*((buf)+fmt->Bshift/8) = b;		\
			} else {					\
			        *((buf)+2-fmt->Rshift/8) = r;		\
				*((buf)+2-fmt->Gshift/8) = g;		\
				*((buf)+2-fmt->Bshift/8) = b;		\
			}						\
		}							\
		break;							\
									\
		case 4: {						\
			Uint32 *bufp;					\
			Uint32 Pixel;					\
									\
			bufp = (Uint32 *)buf;				\
			PIXEL_FROM_RGB(Pixel, fmt, r, g, b);		\
			*bufp = Pixel | (*bufp & Amask);		\
		}							\
		break;							\
	}								\
}

/* FIXME: Should we rescale alpha into 0..255 here? */
#define RGBA_FROM_PIXEL(Pixel, fmt, r, g, b, a)				\
{									\
	r = ((Pixel&fmt->Rmask)>>fmt->Rshift)<<fmt->Rloss; 		\
	g = ((Pixel&fmt->Gmask)>>fmt->Gshift)<<fmt->Gloss; 		\
	b = ((Pixel&fmt->Bmask)>>fmt->Bshift)<<fmt->Bloss; 		\
	a = ((Pixel&fmt->Amask)>>fmt->Ashift)<<fmt->Aloss;	 	\
}
#define RGBA_FROM_8888(Pixel, fmt, r, g, b, a)	\
{						\
	r = (Pixel&fmt->Rmask)>>fmt->Rshift;	\
	g = (Pixel&fmt->Gmask)>>fmt->Gshift;	\
	b = (Pixel&fmt->Bmask)>>fmt->Bshift;	\
	a = (Pixel&fmt->Amask)>>fmt->Ashift;	\
}
#define RGBA_FROM_RGBA8888(Pixel, r, g, b, a)				\
{									\
	r = (Pixel>>24);						\
	g = ((Pixel>>16)&0xFF);						\
	b = ((Pixel>>8)&0xFF);						\
	a = (Pixel&0xFF);						\
}
#define RGBA_FROM_ARGB8888(Pixel, r, g, b, a)				\
{									\
	r = ((Pixel>>16)&0xFF);						\
	g = ((Pixel>>8)&0xFF);						\
	b = (Pixel&0xFF);						\
	a = (Pixel>>24);						\
}
#define RGBA_FROM_ABGR8888(Pixel, r, g, b, a)				\
{									\
	r = (Pixel&0xFF);						\
	g = ((Pixel>>8)&0xFF);						\
	b = ((Pixel>>16)&0xFF);						\
	a = (Pixel>>24);						\
}
#define DISEMBLE_RGBA(buf, bpp, fmt, Pixel, r, g, b, a)			   \
do {									   \
	switch (bpp) {							   \
		case 2:							   \
			Pixel = *((Uint16 *)(buf));			   \
		break;							   \
									   \
		case 3:	{/* FIXME: broken code (no alpha) */		   \
		        Uint8 *b = (Uint8 *)buf;			   \
			if(SDL_BYTEORDER == SDL_LIL_ENDIAN) {		   \
			        Pixel = b[0] + (b[1] << 8) + (b[2] << 16); \
			} else {					   \
			        Pixel = (b[0] << 16) + (b[1] << 8) + b[2]; \
			}						   \
		}							   \
		break;							   \
									   \
		case 4:							   \
			Pixel = *((Uint32 *)(buf));			   \
		break;							   \
									   \
		default:						   \
		        Pixel = 0; /* stop gcc complaints */		   \
		break;							   \
	}								   \
	RGBA_FROM_PIXEL(Pixel, fmt, r, g, b, a);			   \
	Pixel &= ~fmt->Amask;						   \
} while(0)

/* FIXME: this isn't correct, especially for Alpha (maximum != 255) */
#ifdef __NDS__ /* FIXME */
#define PIXEL_FROM_RGBA(Pixel, fmt, r, g, b, a)				\
{									\
	Pixel = ((r>>fmt->Rloss)<<fmt->Rshift)|				\
		((g>>fmt->Gloss)<<fmt->Gshift)|				\
		((b>>fmt->Bloss)<<fmt->Bshift)|				\
		((a>>fmt->Aloss)<<fmt->Ashift) | (1<<15);				\
}
#else
#define PIXEL_FROM_RGBA(Pixel, fmt, r, g, b, a)				\
{									\
	Pixel = ((r>>fmt->Rloss)<<fmt->Rshift)|				\
		((g>>fmt->Gloss)<<fmt->Gshift)|				\
		((b>>fmt->Bloss)<<fmt->Bshift)|				\
		((a>>fmt->Aloss)<<fmt->Ashift);				\
}
#endif /* __NDS__ FIXME */
#define ASSEMBLE_RGBA(buf, bpp, fmt, r, g, b, a)			\
{									\
	switch (bpp) {							\
		case 2: {						\
			Uint16 Pixel;					\
									\
			PIXEL_FROM_RGBA(Pixel, fmt, r, g, b, a);	\
			*((Uint16 *)(buf)) = Pixel;			\
		}							\
		break;							\
									\
		case 3: { /* FIXME: broken code (no alpha) */		\
                        if(SDL_BYTEORDER == SDL_LIL_ENDIAN) {		\
			        *((buf)+fmt->Rshift/8) = r;		\
				*((buf)+fmt->Gshift/8) = g;		\
				*((buf)+fmt->Bshift/8) = b;		\
			} else {					\
			        *((buf)+2-fmt->Rshift/8) = r;		\
				*((buf)+2-fmt->Gshift/8) = g;		\
				*((buf)+2-fmt->Bshift/8) = b;		\
			}						\
		}							\
		break;							\
									\
		case 4: {						\
			Uint32 Pixel;					\
									\
			PIXEL_FROM_RGBA(Pixel, fmt, r, g, b, a);	\
			*((Uint32 *)(buf)) = Pixel;			\
		}							\
		break;							\
	}								\
}

/* Blend the RGB values of two Pixels based on a source alpha value */
#define ALPHA_BLEND(sR, sG, sB, A, dR, dG, dB)	\
do {						\
	dR = (((sR-dR)*(A)+255)>>8)+dR;		\
	dG = (((sG-dG)*(A)+255)>>8)+dG;		\
	dB = (((sB-dB)*(A)+255)>>8)+dB;		\
} while(0)


/* This is a very useful loop for optimizing blitters */
#if defined(_MSC_VER) && (_MSC_VER == 1300)
/* There's a bug in the Visual C++ 7 optimizer when compiling this code */
#else
#define USE_DUFFS_LOOP
#endif
#ifdef USE_DUFFS_LOOP

/* 8-times unrolled loop */
#define DUFFS_LOOP8(pixel_copy_increment, width)			\
{ int n = (width+7)/8;							\
	switch (width & 7) {						\
	case 0: do {	pixel_copy_increment;				\
	case 7:		pixel_copy_increment;				\
	case 6:		pixel_copy_increment;				\
	case 5:		pixel_copy_increment;				\
	case 4:		pixel_copy_increment;				\
	case 3:		pixel_copy_increment;				\
	case 2:		pixel_copy_increment;				\
	case 1:		pixel_copy_increment;				\
		} while ( --n > 0 );					\
	}								\
}

/* 4-times unrolled loop */
#define DUFFS_LOOP4(pixel_copy_increment, width)			\
{ int n = (width+3)/4;							\
	switch (width & 3) {						\
	case 0: do {	pixel_copy_increment;				\
	case 3:		pixel_copy_increment;				\
	case 2:		pixel_copy_increment;				\
	case 1:		pixel_copy_increment;				\
		} while ( --n > 0 );					\
	}								\
}

/* 2 - times unrolled loop */
#define DUFFS_LOOP_DOUBLE2(pixel_copy_increment,			\
				double_pixel_copy_increment, width)	\
{ int n, w = width;							\
	if( w & 1 ) {							\
	    pixel_copy_increment;					\
	    w--;							\
	}								\
	if ( w > 0 )	{						\
	    n = ( w + 2) / 4;						\
	    switch( w & 2 ) {						\
	    case 0: do {	double_pixel_copy_increment;		\
	    case 2:		double_pixel_copy_increment;		\
		    } while ( --n > 0 );					\
	    }								\
	}								\
}

/* 2 - times unrolled loop 4 pixels */
#define DUFFS_LOOP_QUATRO2(pixel_copy_increment,			\
				double_pixel_copy_increment,		\
				quatro_pixel_copy_increment, width)	\
{ int n, w = width;								\
        if(w & 1) {							\
	  pixel_copy_increment;						\
	  w--;								\
	}								\
	if(w & 2) {							\
	  double_pixel_copy_increment;					\
	  w -= 2;							\
	}								\
	if ( w > 0 ) {							\
	    n = ( w + 7 ) / 8;						\
	    switch( w & 4 ) {						\
	    case 0: do {	quatro_pixel_copy_increment;		\
	    case 4:		quatro_pixel_copy_increment;		\
		    } while ( --n > 0 );					\
	    }								\
	}								\
}

/* Use the 8-times version of the loop by default */
#define DUFFS_LOOP(pixel_copy_increment, width)				\
	DUFFS_LOOP8(pixel_copy_increment, width)

#else

/* Don't use Duff's device to unroll loops */
#define DUFFS_LOOP_DOUBLE2(pixel_copy_increment,			\
			 double_pixel_copy_increment, width)		\
{ int n = width;								\
    if( n & 1 ) {							\
	pixel_copy_increment;						\
	n--;								\
    }									\
    n=n>>1;								\
    for(; n > 0; --n) {   						\
	double_pixel_copy_increment;					\
    }									\
}

/* Don't use Duff's device to unroll loops */
#define DUFFS_LOOP_QUATRO2(pixel_copy_increment,			\
				double_pixel_copy_increment,		\
				quatro_pixel_copy_increment, width)	\
{ int n = width;								\
        if(n & 1) {							\
	  pixel_copy_increment;						\
	  n--;								\
	}								\
	if(n & 2) {							\
	  double_pixel_copy_increment;					\
	  n -= 2;							\
	}								\
	n=n>>2;								\
	for(; n > 0; --n) {   						\
	  quatro_pixel_copy_increment;					\
        }								\
}

/* Don't use Duff's device to unroll loops */
#define DUFFS_LOOP(pixel_copy_increment, width)				\
{ int n;								\
	for ( n=width; n > 0; --n ) {					\
		pixel_copy_increment;					\
	}								\
}
#define DUFFS_LOOP8(pixel_copy_increment, width)			\
	DUFFS_LOOP(pixel_copy_increment, width)
#define DUFFS_LOOP4(pixel_copy_increment, width)			\
	DUFFS_LOOP(pixel_copy_increment, width)

#endif /* USE_DUFFS_LOOP */

/* Prevent Visual C++ 6.0 from printing out stupid warnings */
#if defined(_MSC_VER) && (_MSC_VER >= 600)
#pragma warning(disable: 4550)
#endif

#endif /* _SDL_blit_h */
