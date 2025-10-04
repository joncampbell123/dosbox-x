/*
   Copyright (c) 1999  XFree86 Inc
*/
/* $XFree86: xc/include/extensions/xf86dga.h,v 3.21 2001/08/01 00:44:36 tsi Exp $ */

#ifndef _XF86DGA_H_
#define _XF86DGA_H_

#include <X11/Xfuncproto.h>
#include "xf86dga1.h"
#include "SDL_name.h"

#define X_XDGAQueryVersion		0

/* 1 through 9 are in xf86dga1.h */

/* 10 and 11 are reserved to avoid conflicts with rogue DGA extensions */

#define X_XDGAQueryModes		12
#define X_XDGASetMode			13
#define X_XDGASetViewport		14
#define X_XDGAInstallColormap		15
#define X_XDGASelectInput		16
#define X_XDGAFillRectangle		17
#define X_XDGACopyArea			18
#define X_XDGACopyTransparentArea	19
#define X_XDGAGetViewportStatus		20
#define X_XDGASync			21
#define X_XDGAOpenFramebuffer		22
#define X_XDGACloseFramebuffer		23
#define X_XDGASetClientVersion		24
#define X_XDGAChangePixmapMode		25
#define X_XDGACreateColormap		26


#define XDGAConcurrentAccess	0x00000001
#define XDGASolidFillRect	0x00000002
#define XDGABlitRect		0x00000004
#define XDGABlitTransRect	0x00000008
#define XDGAPixmap    		0x00000010

#define XDGAInterlaced          0x00010000
#define XDGADoublescan          0x00020000

#define XDGAFlipImmediate	0x00000001
#define XDGAFlipRetrace		0x00000002

#define XDGANeedRoot		0x00000001

#define XF86DGANumberEvents		7

#define XDGAPixmapModeLarge		0
#define XDGAPixmapModeSmall		1

#define XF86DGAClientNotLocal		0
#define XF86DGANoDirectVideoMode	1
#define XF86DGAScreenNotActive		2
#define XF86DGADirectNotActivated	3
#define XF86DGAOperationNotSupported	4
#define XF86DGANumberErrors		(XF86DGAOperationNotSupported + 1)


typedef struct {
   int num;		/* A unique identifier for the mode (num > 0) */
   char *name;		/* name of mode given in the XF86Config */
   float verticalRefresh;
   int flags;		/* DGA_CONCURRENT_ACCESS, etc... */
   int imageWidth;	/* linear accessible portion (pixels) */
   int imageHeight;
   int pixmapWidth;	/* Xlib accessible portion (pixels) */
   int pixmapHeight;	/* both fields ignored if no concurrent access */
   int bytesPerScanline; 
   int byteOrder;	/* MSBFirst, LSBFirst */
   int depth;		
   int bitsPerPixel;
   unsigned long redMask;
   unsigned long greenMask;
   unsigned long blueMask;
   short visualClass;
   int viewportWidth;
   int viewportHeight;
   int xViewportStep;	/* viewport position granularity */
   int yViewportStep;
   int maxViewportX;	/* max viewport origin */
   int maxViewportY;
   int viewportFlags;	/* types of page flipping possible */
   int reserved1;
   int reserved2;
} SDL_NAME(XDGAMode);


typedef struct {
   SDL_NAME(XDGAMode) mode;
   unsigned char *data;
   Pixmap pixmap;
} SDL_NAME(XDGADevice);


#ifndef _XF86DGA_SERVER_
_XFUNCPROTOBEGIN

typedef struct {
   int type;
   unsigned long serial;
   Display *display;
   int screen;
   Time time;
   unsigned int state;
   unsigned int button;
} SDL_NAME(XDGAButtonEvent);

typedef struct {
   int type;
   unsigned long serial;
   Display *display;
   int screen;
   Time time;
   unsigned int state;
   unsigned int keycode;
} SDL_NAME(XDGAKeyEvent);

typedef struct {
   int type;
   unsigned long serial;
   Display *display;
   int screen;
   Time time;
   unsigned int state;
   int dx;
   int dy;
} SDL_NAME(XDGAMotionEvent);

typedef union {
  int type;
  SDL_NAME(XDGAButtonEvent) xbutton;
  SDL_NAME(XDGAKeyEvent)	  xkey;
  SDL_NAME(XDGAMotionEvent) xmotion;
  long		  pad[24];
} SDL_NAME(XDGAEvent);

Bool SDL_NAME(XDGAQueryExtension)(
    Display 	*dpy,
    int 	*eventBase,
    int 	*erroBase
);

Bool SDL_NAME(XDGAQueryVersion)(
    Display 	*dpy,
    int 	*majorVersion,
    int 	*minorVersion
);

SDL_NAME(XDGAMode)* SDL_NAME(XDGAQueryModes)(
    Display	*dpy,
    int 	screen,
    int		*num
);

SDL_NAME(XDGADevice)* SDL_NAME(XDGASetMode)(
    Display	*dpy,
    int		screen,
    int		mode
);

Bool SDL_NAME(XDGAOpenFramebuffer)(
    Display	*dpy,
    int 	screen
);

void SDL_NAME(XDGACloseFramebuffer)(
    Display	*dpy,
    int		screen
);

void SDL_NAME(XDGASetViewport)(
    Display	*dpy,
    int		screen,
    int		x,
    int		y,
    int		flags
);

void SDL_NAME(XDGAInstallColormap)(
    Display	*dpy,
    int		screen,
    Colormap	cmap
);

Colormap SDL_NAME(XDGACreateColormap)(
    Display	*dpy,
    int 	screen,
    SDL_NAME(XDGADevice)  *device,
    int 	alloc
);

void SDL_NAME(XDGASelectInput)(
    Display	*dpy,
    int		screen,
    long	event_mask
);

void SDL_NAME(XDGAFillRectangle)(
    Display	*dpy,
    int		screen,
    int		x,
    int		y,
    unsigned int	width,
    unsigned int	height,
    unsigned long	color
);


void SDL_NAME(XDGACopyArea)(
    Display	*dpy,
    int		screen,
    int		srcx,
    int		srcy,
    unsigned int	width,
    unsigned int	height,
    int		dstx,
    int		dsty
);


void SDL_NAME(XDGACopyTransparentArea)(
    Display	*dpy,
    int		screen,
    int		srcx,
    int		srcy,
    unsigned int	width,
    unsigned int	height,
    int		dstx,
    int		dsty,
    unsigned long key
);

int SDL_NAME(XDGAGetViewportStatus)(
    Display	*dpy,
    int		screen
);
   
void SDL_NAME(XDGASync)(
    Display	*dpy,
    int		screen
);

Bool SDL_NAME(XDGASetClientVersion)(
    Display	*dpy
);

void SDL_NAME(XDGAChangePixmapMode)(
    Display 	*dpy,
    int		screen,
    int		*x,
    int		*y,
    int		mode
);


void SDL_NAME(XDGAKeyEventToXKeyEvent)(SDL_NAME(XDGAKeyEvent)* dk, XKeyEvent* xk);


_XFUNCPROTOEND
#endif /* _XF86DGA_SERVER_ */
#endif /* _XF86DGA_H_ */
