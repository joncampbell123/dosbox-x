//**************************************************************
//*            OpenGLide - Glide to OpenGL Wrapper
//*             http://openglide.sourceforge.net
//*
//*               Depth (Z/W-Buffer) Functions
//*
//*         OpenGLide is OpenSource under LGPL license
//*              Originaly made by Fabio Barros
//*      Modified by Paul for Glidos (http://www.glidos.net)
//**************************************************************

#include "GlOgl.h"
#include "GLRender.h"

//*************************************************
//* Changes Depth Buffer Mode
//*************************************************
DLLEXPORT void __stdcall
grDepthBufferMode( GrDepthBufferMode_t mode )
{
#ifdef OGL_DONE
    GlideMsg( "grDepthBufferMode( %d )\n", mode );
#endif

    RenderDrawTriangles( );

    Glide.State.DepthBufferMode = mode;

    /*
    * In AddTriangle etc.  Use of z or w for
    * depth buffering is determined by the
    * value of OpenGL.DepthBufferType.  So
    * I set it here.
    */
    switch ( mode )
    {
    case GR_DEPTHBUFFER_DISABLE:
        OpenGL.DepthBufferType = 0;
        glDisable( GL_DEPTH_TEST );
        return;

    case GR_DEPTHBUFFER_ZBUFFER:
    case GR_DEPTHBUFFER_ZBUFFER_COMPARE_TO_BIAS:
        OpenGL.DepthBufferType = 1;
        OpenGL.ZNear = ZBUFFERNEAR;
        OpenGL.ZFar = ZBUFFERFAR;
        break;

    case GR_DEPTHBUFFER_WBUFFER:
    case GR_DEPTHBUFFER_WBUFFER_COMPARE_TO_BIAS:
        OpenGL.DepthBufferType = 0;
        OpenGL.ZNear = WBUFFERNEAR;
        OpenGL.ZFar = WBUFFERFAR;
        break;
    }

    glEnable( GL_DEPTH_TEST );

    glMatrixMode( GL_PROJECTION );
    glLoadIdentity( );

    if ( Glide.State.OriginInformation == GR_ORIGIN_LOWER_LEFT )
    {
        glOrtho( Glide.State.ClipMinX, Glide.State.ClipMaxX, 
                 Glide.State.ClipMinY, Glide.State.ClipMaxY, 
                 OpenGL.ZNear, OpenGL.ZFar );
        glViewport( Glide.State.ClipMinX, Glide.State.ClipMinY, 
                    Glide.State.ClipMaxX - Glide.State.ClipMinX, 
                    Glide.State.ClipMaxY - Glide.State.ClipMinY ); 
    }
    else
    {
        glOrtho( Glide.State.ClipMinX, Glide.State.ClipMaxX, 
                 Glide.State.ClipMaxY, Glide.State.ClipMinY, 
                 OpenGL.ZNear, OpenGL.ZFar );
        glViewport( Glide.State.ClipMinX, OpenGL.WindowHeight - Glide.State.ClipMaxY, 
                    Glide.State.ClipMaxX - Glide.State.ClipMinX, 
                    Glide.State.ClipMaxY - Glide.State.ClipMinY ); 
    }

    glMatrixMode( GL_MODELVIEW );

#ifdef OPENGL_DEBUG
    GLErro( "grDepthBufferMode" );
#endif
}

//*************************************************
//* Enables or Disables Depth Buffer Writting
//*************************************************
DLLEXPORT void __stdcall
grDepthMask( FxBool enable )
{
#ifdef OGL_DONE
    GlideMsg( "grDepthMask( %d )\n", enable );
#endif

    RenderDrawTriangles( );

    Glide.State.DepthBufferWritting = OpenGL.DepthBufferWritting = enable;

    glDepthMask( OpenGL.DepthBufferWritting );

#ifdef OPENGL_DEBUG
    GLErro( "grDepthMask" );
#endif
}

//*************************************************
//* Sets the Depth Function to use
//*************************************************
DLLEXPORT void __stdcall
grDepthBufferFunction( GrCmpFnc_t func )
{
#ifdef OGL_DONE
    GlideMsg( "grDepthBufferFunction( %d )\n", func );
#endif

    RenderDrawTriangles( );

    Glide.State.DepthFunction = func;

    // We can do this just because we know the constant values for both OpenGL and Glide
    // To port it to anything else than OpenGL we NEED to change this code
    OpenGL.DepthFunction = GL_NEVER + func;

    glDepthFunc( OpenGL.DepthFunction );

#ifdef OPENGL_DEBUG
    GLErro( "grDepthBufferFunction" );
#endif
}

//*************************************************
//* Set the depth bias level
//*************************************************
DLLEXPORT void __stdcall
grDepthBiasLevel( FxI16 level )
{
#ifdef OGL_PARTDONE
    GlideMsg( "grDepthBiasLevel( %d )\n", level );
#endif

    RenderDrawTriangles( );

    Glide.State.DepthBiasLevel = level;
    //OpenGL.DepthBiasLevel = level * D1OVER65536;
    OpenGL.DepthBiasLevel = level * 10.0f;

    glPolygonOffset( 1.0f, OpenGL.DepthBiasLevel );

    if ( level != 0 )
    {
        glEnable( GL_POLYGON_OFFSET_FILL );
    }
    else
    {
        glDisable( GL_POLYGON_OFFSET_FILL );
    }

#ifdef OPENGL_DEBUG
    GLErro( "grDepthBiasLevel" );
#endif
}

