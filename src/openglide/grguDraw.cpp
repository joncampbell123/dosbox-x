//**************************************************************
//*            OpenGLide - Glide to OpenGL Wrapper
//*             http://openglide.sourceforge.net
//*
//*                    Drawing Functions
//*
//*         OpenGLide is OpenSource under LGPL license
//*              Originaly made by Fabio Barros
//*      Modified by Paul for Glidos (http://www.glidos.net)
//**************************************************************

#include "GlOgl.h"
#include "GLRender.h"

//*************************************************
//* Draws a Triangle on the screen
//*************************************************
DLLEXPORT void __stdcall
grDrawTriangle( const GrVertex *a, const GrVertex *b, const GrVertex *c )
{
#ifdef OGL_CRITICAL
    GlideMsg( "grDrawTriangle( ---, ---, --- )\n" );
#endif

    RenderAddTriangle( a, b, c, true );

    if ( Glide.State.RenderBuffer == GR_BUFFER_FRONTBUFFER )
    {
        RenderDrawTriangles( );
        glFlush( );
    }
}

//*************************************************
//* Draws a Triangle on the screen
//*************************************************
DLLEXPORT void __stdcall
grDrawPlanarPolygonVertexList( int nVertices, const GrVertex vlist[] )
{
#ifdef OGL_CRITICAL
    GlideMsg("grDrawPlanarPolygonVertexList( %d, --- )\n", nVertices );
#endif

    for ( int i = 2; i < nVertices; i++ )
    {
        RenderAddTriangle( &vlist[ 0 ], &vlist[ i - 1 ], &vlist[ i ], true );
    }

    if ( Glide.State.RenderBuffer == GR_BUFFER_FRONTBUFFER )
    {
        RenderDrawTriangles( );
        glFlush( );
    }
}

//*************************************************
//* Draws a Line on the screen
//*************************************************
DLLEXPORT void __stdcall
grDrawLine( const GrVertex *a, const GrVertex *b )
{
#ifdef OGL_CRITICAL
    GlideMsg("grDrawLine( ---, --- )\n");
#endif
    
   /*
    * RenderAddLine actually renders the line, so
    * we must render the queued triangles first to
    * avoid out-of-order rendering.
    */
    RenderDrawTriangles( );
    RenderAddLine( a, b, true );
}

//*************************************************
//* Draws a Point on the screen
//*************************************************
DLLEXPORT void __stdcall
grDrawPoint( const GrVertex *a )
{
#ifdef OGL_CRITICAL
    GlideMsg( "grDrawPoint( --- )\n" );
#endif

    RenderAddPoint( a, true );
}

//*************************************************
//* Draw a convex non-planar polygon
//*************************************************
DLLEXPORT void __stdcall
grDrawPolygon( int nverts, const int ilist[], const GrVertex vlist[] )
{
#ifdef OGL_CRITICAL
    GlideMsg( "grDrawPolygon( %d, ---, --- )\n" );
#endif

    for ( int i = 2; i < nverts; i++ )
    {
        RenderAddTriangle( &vlist[ ilist[ 0 ] ], 
                           &vlist[ ilist[ i - 1 ] ], 
                           &vlist[ ilist[ i ] ],
                           true );
    }

    if ( Glide.State.RenderBuffer == GR_BUFFER_FRONTBUFFER )
    {
        RenderDrawTriangles( );
        glFlush( );
    }
}

//*************************************************
//* Draw a convex planar polygon
//*************************************************
DLLEXPORT void __stdcall
grDrawPlanarPolygon( int nverts, const int ilist[], const GrVertex vlist[] )
{
#ifdef OGL_CRITICAL
    GlideMsg( "grDrawPlanarPolygon( %d, ---, --- )\n", nverts );
#endif

    for ( int i = 2; i < nverts; i++ )
    {
        RenderAddTriangle( &vlist[ ilist[ 0 ] ], 
                           &vlist[ ilist[ i - 1 ] ],
                           &vlist[ ilist[ i ] ],
                           true );
    }

    if ( Glide.State.RenderBuffer == GR_BUFFER_FRONTBUFFER )
    {
        RenderDrawTriangles( );
        glFlush( );
    }
}

//*************************************************
//* Draw a convex non-planar polygon
//*************************************************
DLLEXPORT void __stdcall
grDrawPolygonVertexList( int nVertices, const GrVertex vlist[] )
{
#ifdef OGL_CRITICAL
    GlideMsg( "grDrawPolygonVertexList( %d, --- )\n", nVertices );
#endif

    for ( int i = 2; i < nVertices; i++ )
    {
        RenderAddTriangle( &vlist[ 0 ],
                           &vlist[ i - 1 ],
                           &vlist[ i ],
                           true );
    }

    if ( Glide.State.RenderBuffer == GR_BUFFER_FRONTBUFFER )
    {
        RenderDrawTriangles( );
        glFlush( );
    }
}

DLLEXPORT void __stdcall
guAADrawTriangleWithClip( const GrVertex *a, const GrVertex *b, 
                          const GrVertex *c )
{
#ifdef OGL_CRITICAL
    GlideMsg("guAADrawTriangleWithClip( ---, ---, --- )\n");
#endif

    RenderAddTriangle( a, b, c, false );

    if ( Glide.State.RenderBuffer == GR_BUFFER_FRONTBUFFER )
    {
        RenderDrawTriangles( );
        glFlush( );
    }
}

DLLEXPORT void __stdcall
guDrawTriangleWithClip( const GrVertex *a,
                        const GrVertex *b,
                        const GrVertex *c )
{
#ifdef OGL_CRITICAL
    GlideMsg("guDrawTriangleWithClip( ---, ---, --- )\n");
#endif

    RenderAddTriangle( a, b, c, false );

    if ( Glide.State.RenderBuffer == GR_BUFFER_FRONTBUFFER )
    {
        RenderDrawTriangles( );
        glFlush( );
    }
}

DLLEXPORT void __stdcall
guDrawPolygonVertexListWithClip( int nverts, const GrVertex vlist[] )
{
#ifdef OGL_CRITICAL
    GlideMsg( "guDrawPolygonVertexListWithClip( %d, --- )\n", nverts );
#endif

    for ( int i = 2; i < nverts; i++ )
    {
        RenderAddTriangle( &vlist[ 0 ], 
                           &vlist[ i - 1 ],
                           &vlist[ i ],
                           false );
    }

    if ( Glide.State.RenderBuffer == GR_BUFFER_FRONTBUFFER )
    {
        RenderDrawTriangles( );
        glFlush( );
    }
}

DLLEXPORT void __stdcall
grAADrawLine( const GrVertex *a, const GrVertex *b )
{
#ifdef OGL_CRITICAL
    GlideMsg( "grAADrawLine( ---, --- )\n" );
#endif

    RenderAddLine( a, b, true );
}

DLLEXPORT void __stdcall
grAADrawPoint(const GrVertex *a )
{
#ifdef OGL_CRITICAL
    GlideMsg("grAADrawPoint( --- )\n");
#endif

    RenderAddPoint( a, true );
}

DLLEXPORT void __stdcall
grAADrawPolygon( const int nverts, const int ilist[], const GrVertex vlist[] )
{
#ifdef OGL_CRITICAL
    GlideMsg( "grAADrawPolygon( %d, ---, --- )\n", nverts );
#endif

    for ( int i = 2; i < nverts; i++ )
    {
        RenderAddTriangle( &vlist[ ilist[ 0 ] ],
                           &vlist[ ilist[ i - 1 ] ],
                           &vlist[ ilist[ i ] ],
                           true );
    }

    if ( Glide.State.RenderBuffer == GR_BUFFER_FRONTBUFFER )
    {
        RenderDrawTriangles( );
        glFlush( );
    }
}

DLLEXPORT void __stdcall
grAADrawPolygonVertexList( const int nverts, const GrVertex vlist[] )
{
#ifdef OGL_CRITICAL
    GlideMsg( "grAADrawPolygonVertexList( %d, --- )\n", nverts );
#endif

    for ( int i = 2; i < nverts; i++ )
    {
        RenderAddTriangle( &vlist[ 0 ],
                           &vlist[ i - 1 ],
                           &vlist[ i ],
                           true );
    }

    if ( Glide.State.RenderBuffer == GR_BUFFER_FRONTBUFFER )
    {
        RenderDrawTriangles( );
        glFlush( );
    }
}

DLLEXPORT void __stdcall
grAADrawTriangle( const GrVertex *a, const GrVertex *b, const GrVertex *c,
                  FxBool ab_antialias, FxBool bc_antialias, FxBool ca_antialias )
{
#ifdef OGL_CRITICAL
    GlideMsg("grAADrawTriangle( ---, ---, ---, %d, %d, %d )\n",
        ab_antialias, bc_antialias, ca_antialias );
#endif

    RenderAddTriangle( a, b, c, true );

    if ( Glide.State.RenderBuffer == GR_BUFFER_FRONTBUFFER )
    {
        RenderDrawTriangles( );
        glFlush( );
    }
}

