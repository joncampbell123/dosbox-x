/*
 * Copyright 1989 O'Reilly and Associates, Inc.

     The X Consortium, and any party obtaining a copy of these files from
     the X Consortium, directly or indirectly, is granted, free of charge, a
     full and unrestricted irrevocable, world-wide, paid up, royalty-free,
     nonexclusive right and license to deal in this software and
     documentation files (the "Software"), including without limitation the
     rights to use, copy, modify, merge, publish, distribute, sublicense,
     and/or sell copies of the Software, and to permit persons who receive
     copies from any such party to do so.  This license includes without
     limitation a license to do the foregoing actions under any patents of
     the party supplying this software to the X Consortium.
 */

/*
 * scrollBoxP.h - Private definitions for scrollBox widget
 *
 */

#ifndef _XORAscrollBoxP_h
#define _XORAscrollBoxP_h

/************************************************************************
 *                                                                      *
 * scrollBox Widget Private Data                                        *
 *                                                                      *
 ************************************************************************/

#include "x11/ScrollBox.h"

#include <X11/CompositeP.h>

/* New fields for the scrollBox widget class record */
typedef struct _ScrollBoxClass {
    int empty;
} ScrollBoxClassPart;

/* Full class record declaration */
typedef struct _ScrollBoxClassRec {
    CoreClassPart core_class;
    CompositeClassPart composite_class;
    ScrollBoxClassPart scrollBox_class;
} ScrollBoxClassRec;

extern ScrollBoxClassRec scrollBoxClassRec;

/* New fields for the scrollBox widget record */
typedef struct {
    Dimension h_space, v_space;
    Dimension preferred_width, preferred_height;
    Dimension last_query_width, last_query_height;
    Dimension increment_width, increment_height;
    XtGeometryMask last_query_mode;
} ScrollBoxPart;


/************************************************************************
 *                                                                      *
 * Full instance record declaration                                     *
 *                                                                      *
 ************************************************************************/

typedef struct _ScrollBoxRec {
    CorePart core;
    CompositePart composite;
    ScrollBoxPart scrollBox;
} ScrollBoxRec;

#endif /* _XORAscrollBoxP_h */
