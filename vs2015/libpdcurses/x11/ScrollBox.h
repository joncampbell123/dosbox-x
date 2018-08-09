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

#ifndef _XORAscrollBox_h
#define _XORAscrollBox_h

/************************************************************************
 *                                                                      *
 * scrollBox Widget (subclass of CompositeClass)                        *
 *                                                                      *
 ************************************************************************/

/* Parameters:

 Name               Class              RepType      Default Value
 ----               -----              -------      -------------
 background         Background         Pixel        XtDefaultBackground
 border             BorderColor        Pixel        XtDefaultForeground
 borderWidth        BorderWidth        Dimension    1
 destroyCallback    Callback           Pointer      NULL
 hSpace             HSpace             Dimension    4
 height             Height             Dimension    0
 mappedWhenManaged  MappedWhenManaged  Boolean      True
 vSpace             VSpace             Dimension    4
 width              Width              Dimension    0
 x                  Position           Position     0
 y                  Position           Position     0

*/


/* Class record constants */

extern WidgetClass scrollBoxWidgetClass;

typedef struct _ScrollBoxClassRec *ScrollBoxWidgetClass;
typedef struct _ScrollBoxRec      *ScrollBoxWidget;

#endif /* _XORAscrollBox_h */
