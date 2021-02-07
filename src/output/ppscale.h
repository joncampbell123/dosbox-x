/* Copyright 2018-2020          Anton Shepelev          (anton.txt@gmail.com) */
/* Usage of the works is permitted  provided that this instrument is retained */
/* with the works, so that any entity that uses the works is notified of this */
/* instrument.    DISCLAIMER: THE WORKS ARE WITHOUT WARRANTY.                 */

/* ----------------------- Pixel-perfect scaling unit ----------------------- */
/* This unit uses the Horstmann indentation style.                            */
/* Repository: https://launchpad.net/ppscale                                  */

#ifndef PPSCALE_H
#define PPSCALE_H

int pp_getscale /* calculate integer scales for pixel-perfect magnification */
(	int    win,  int hin,  /* input dimensions                  */
	double par,            /* input pixel aspect ratio          */
	int    wout, int hout, /* output  dimensions                */
	double parweight,      /* weight of PAR in scale estimation */
	int    *sx,  int *sy   /* horisontal and vertical scales    */
); /* returns -1 on error and 0 on success */


/* RAT: the many scalar arguments are not unifined into one of more stucts */
/*      because doing so somehow thwarts GCC's O3 optimisations, and the   */
/*      alrorithm works up to 30% slower.                                  */
int pp_scale /* magnify an image in a pixel-perfect manner */
(	char* simg, int spitch, /* source image and pitch           */
	int   *rx,  int *ry,    /* location and                     */
	int   *rw,  int *rh,    /* size of the rectangle to process */
	char* dimg, int dpitch, /* destination image and pitch      */
	int   bypp,             /* bytes per pixel                  */
	int   sx,   int sy      /* horisontal and vertical scales   */
); /* return -1 on error and 0 on success */

#endif /* PPSCALE_H */