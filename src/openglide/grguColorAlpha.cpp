//**************************************************************
//*            OpenGLide - Glide to OpenGL Wrapper
//*             http://openglide.sourceforge.net
//*
//*                  Color and Alpha File
//*
//*         OpenGLide is OpenSource under LGPL license
//*              Originaly made by Fabio Barros
//*      Modified by Paul for Glidos (http://www.glidos.net)
//**************************************************************

#include <math.h>

#include "GlOgl.h"
#include "GLRender.h"
#include "Glextensions.h"
#include "PGTexture.h"


// extern functions
void ConvertColorB( GrColor_t GlideColor, BYTE &R, BYTE &G, BYTE &B, BYTE &A );
void ConvertColorF( GrColor_t GlideColor, float &R, float &G, float &B, float &A );
GrColor_t ConvertConstantColor( float R, float G, float B, float A );


//*************************************************
//* Sets the Dithering Mode 24->16 bits
//*************************************************
DLLEXPORT void __stdcall
grDitherMode( GrDitherMode_t mode )
{
#ifdef OGL_DONE
    GlideMsg( "grDitherMode( %d )\n", mode );
#endif

    RenderDrawTriangles( );

    Glide.State.DitherMode = mode;

    if ( mode != GR_DITHER_DISABLE )
    {
        // GR_DITHER_2x2 or GR_DITHER_4x4
        glEnable( GL_DITHER );
    }
    else
    {
        glDisable( GL_DITHER );
    }

#ifdef OPENGL_DEBUG
    GLErro( "grDitherMode" );
#endif
}

//*************************************************
//* Sets the Constant color
//*************************************************
DLLEXPORT void __stdcall
grConstantColorValue( GrColor_t value )
{
#ifdef OGL_DONE
    GlideMsg( "grConstantColorValue( %x )\n", value );
#endif

    Glide.State.ConstantColorValue = value;
    ConvertColorF( value, 
                   OpenGL.ConstantColor[ 0 ], 
                   OpenGL.ConstantColor[ 1 ], 
                   OpenGL.ConstantColor[ 2 ], 
                   OpenGL.ConstantColor[ 3 ] );
}


//*************************************************
//* Sets the Constant color
//*************************************************
DLLEXPORT void __stdcall
grConstantColorValue4( float a, float r, float g, float b )
{
#ifdef OGL_DONE
    GlideMsg( "grConstantColorValue4( %f, %f, %f, %f )\n", a, r, g, b );
#endif

    Glide.State.ConstantColorValue = ConvertConstantColor( r, g, b, a );
    OpenGL.ConstantColor[ 0 ] = r * D1OVER255;
    OpenGL.ConstantColor[ 1 ] = g * D1OVER255;
    OpenGL.ConstantColor[ 2 ] = b * D1OVER255;
    OpenGL.ConstantColor[ 3 ] = a * D1OVER255;
}

//*************************************************
//* Sets the Color and Alpha mask
//*************************************************
DLLEXPORT void __stdcall
grColorMask( FxBool rgb, FxBool a )
{
#ifdef OGL_PARTDONE
    GlideMsg( "grColorMask( %d, %d )\n", rgb, a );
#endif

    RenderDrawTriangles( );

    Glide.State.ColorMask = rgb;
    Glide.State.AlphaMask = a;
    OpenGL.ColorMask = rgb;

    glColorMask( rgb, rgb, rgb, a );

//  if (Glide.DepthBufferMode == GR_DEPTHBUFFER_DISABLE)
//  {
//      if (a)
//          OpenGL.AlphaBuffer = true;
//      else
//          OpenGL.AlphaBuffer = false;
//      glColorMask( rgb, rgb, rgb, a );
//  }
//  else
//  {
//      glColorMask( rgb, rgb, rgb, GL_TRUE );
//  }

#ifdef OPENGL_DEBUG
    GLErro( "grColorMask" );
#endif
}
//grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_CONSTANT, GR_COMBINE_OTHER_TEXTURE, FXFALSE )
DLLEXPORT void __stdcall
grColorCombine( GrCombineFunction_t function, GrCombineFactor_t factor,
                GrCombineLocal_t local, GrCombineOther_t other,
                FxBool invert )
{
#ifdef OGL_PARTDONE
    GlideMsg( "grColorCombine( %d, %d, %d, %d, %d )\n",
        function, factor, local, other, invert );
#endif

    RenderDrawTriangles( );

    Glide.State.ColorCombineFunction    = function;
    Glide.State.ColorCombineFactor      = factor;
    Glide.State.ColorCombineLocal       = local;
    Glide.State.ColorCombineOther       = other;
    Glide.State.ColorCombineInvert      = invert;
    
    Glide.COther = false;
    Glide.CLocal = false;

    switch ( function )
    {
    case GR_COMBINE_FUNCTION_ZERO:
        ColorFunctionFunc = ColorFunctionZero;
        OpenGL.ColorTexture = false;
        break;

    case GR_COMBINE_FUNCTION_LOCAL_ALPHA:
        ColorFunctionFunc = ColorFunctionLocalAlpha;
        Glide.ALocal = true;
        OpenGL.ColorTexture = false;
        break;

    case GR_COMBINE_FUNCTION_LOCAL:
        ColorFunctionFunc = ColorFunctionLocal;
        OpenGL.ColorTexture = false;
        Glide.CLocal = true;
        break;

    case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL:
        ColorFunctionFunc = ColorFunctionMinusLocalAddLocal;;
        OpenGL.ColorTexture = false;
        Glide.CLocal = true;
        break;

    case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL_ALPHA:
        ColorFunctionFunc = ColorFunctionMinusLocalAddLocalAlpha;
        OpenGL.ColorTexture = false;
        Glide.CLocal = true;
        Glide.ALocal = true;
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER:
        ColorFunctionFunc = ColorFunctionScaleOther;
        Glide.COther = true;
        if ( ( other == GR_COMBINE_OTHER_TEXTURE ) && 
             ( Glide.State.TextureCombineCFunction != GR_COMBINE_FUNCTION_ZERO ) &&
             ( factor != GR_COMBINE_FACTOR_ZERO ) )
        {
            OpenGL.ColorTexture = true;
        }
        else
        {
            OpenGL.ColorTexture = false;
        }
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA:
        ColorFunctionFunc = ColorFunctionScaleOtherAddLocalAlpha;
        Glide.ALocal = true;
        Glide.COther = true;
        if ( ( other == GR_COMBINE_OTHER_TEXTURE ) && 
             ( Glide.State.TextureCombineCFunction != GR_COMBINE_FUNCTION_ZERO ) &&
             ( factor != GR_COMBINE_FACTOR_ZERO ) )
        {
            OpenGL.ColorTexture = true;
        }
        else
        {
            OpenGL.ColorTexture = false;
        }
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL:
        ColorFunctionFunc = ColorFunctionScaleOtherAddLocal;
        Glide.COther = true;
        Glide.CLocal = true;
        if ( ( other == GR_COMBINE_OTHER_TEXTURE ) && 
             ( Glide.State.TextureCombineCFunction != GR_COMBINE_FUNCTION_ZERO ) &&
             ( factor != GR_COMBINE_FACTOR_ZERO ) )
        {
            OpenGL.ColorTexture = true;
        }
        else
        {
            OpenGL.ColorTexture = false;
        }
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL:
        ColorFunctionFunc = ColorFunctionScaleOtherMinusLocal;
        Glide.COther = true;
        Glide.CLocal = true;
        if ( ( other == GR_COMBINE_OTHER_TEXTURE ) && 
             ( Glide.State.TextureCombineCFunction != GR_COMBINE_FUNCTION_ZERO ) &&
             ( factor != GR_COMBINE_FACTOR_ZERO ) )
        {
            OpenGL.ColorTexture = true;
        }
        else
        {
            OpenGL.ColorTexture = false;
        }
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL:
        ColorFunctionFunc = ColorFunctionScaleOtherMinusLocalAddLocal;
        Glide.COther = true;
        Glide.CLocal = true;
        if ( ( other == GR_COMBINE_OTHER_TEXTURE ) &&
             ( Glide.State.TextureCombineCFunction != GR_COMBINE_FUNCTION_ZERO ) &&
             ( factor != GR_COMBINE_FACTOR_ZERO ) )
        {
            OpenGL.ColorTexture = true;
        }
        else
        {
            OpenGL.ColorTexture = false;
        }
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL_ALPHA:
        ColorFunctionFunc = ColorFunctionScaleOtherMinusLocalAddLocalAlpha;
        Glide.ALocal = true;
        Glide.COther = true;
        Glide.CLocal = true;
        if ( ( other == GR_COMBINE_OTHER_TEXTURE ) &&
             ( Glide.State.TextureCombineCFunction != GR_COMBINE_FUNCTION_ZERO ) &&
             ( factor != GR_COMBINE_FACTOR_ZERO ) )
        {
            OpenGL.ColorTexture = true;
        }
        else
        {
            OpenGL.ColorTexture = false;
        }
        break;
    }

    switch ( factor )
    {
    case GR_COMBINE_FACTOR_ZERO:
        ColorFactor3Func = ColorFactor3Zero;
        break;

    case GR_COMBINE_FACTOR_ONE:
    case GR_COMBINE_FACTOR_ONE_MINUS_LOD_FRACTION:
        ColorFactor3Func = ColorFactor3One;
        break;

    case GR_COMBINE_FACTOR_TEXTURE_ALPHA:
    case GR_COMBINE_FACTOR_ONE_MINUS_TEXTURE_ALPHA:
    case GR_COMBINE_FACTOR_TEXTURE_RGB:     //  case GR_COMBINE_FACTOR_LOD_FRACTION:
        if ( Glide.State.TextureCombineCFunction != GR_COMBINE_FUNCTION_ZERO )
        {
            OpenGL.ColorTexture = true;
        }
        ColorFactor3Func = ColorFactor3One;
        break;

    case GR_COMBINE_FACTOR_LOCAL:
        ColorFactor3Func = ColorFactor3Local;
        Glide.CLocal = true;
        break;

    case GR_COMBINE_FACTOR_ONE_MINUS_LOCAL:
        ColorFactor3Func = ColorFactor3OneMinusLocal;
        Glide.CLocal = true;
        break;

    case GR_COMBINE_FACTOR_LOCAL_ALPHA:
        ColorFactor3Func = ColorFactor3LocalAlpha;
        Glide.ALocal = true;
        break;

    case GR_COMBINE_FACTOR_ONE_MINUS_LOCAL_ALPHA:
        ColorFactor3Func = ColorFactor3OneMinusLocalAlpha;
        Glide.ALocal = true;
        break;

    case GR_COMBINE_FACTOR_OTHER_ALPHA:
        ColorFactor3Func = ColorFactor3OtherAlpha;
        Glide.AOther = true;
        break;

    case GR_COMBINE_FACTOR_ONE_MINUS_OTHER_ALPHA:
        ColorFactor3Func = ColorFactor3OneMinusOtherAlpha;
        Glide.AOther = true;
        break;

    }

    if ( ( Glide.State.ColorCombineFactor == GR_COMBINE_FACTOR_TEXTURE_ALPHA ) &&
         ( Glide.State.ColorCombineOther == GR_COMBINE_OTHER_TEXTURE ) )
    {
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );
    }
    else
    if ( ( Glide.State.ColorCombineFactor == GR_COMBINE_FACTOR_TEXTURE_RGB ) &&
         ( Glide.State.ColorCombineOther == GR_COMBINE_OTHER_TEXTURE ) )
    {
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND );
    }
    else
/*    if (( Glide.State.ColorCombineFactor == GR_COMBINE_FACTOR_ONE ) &&
        ( Glide.State.ColorCombineOther == GR_COMBINE_OTHER_TEXTURE ) &&
        ( ( Glide.State.ColorCombineFunction == GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA ) || 
        ( Glide.State.ColorCombineFunction == GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL ) ) )
    {
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD );
    }
    else*/
    {
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
    }

    OpenGL.Texture = ( OpenGL.ColorTexture || ( OpenGL.Blend && OpenGL.AlphaTexture ) );
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall
guColorCombineFunction( GrColorCombineFnc_t fnc )
{
#ifdef OGL_PARTDONE
    GlideMsg( "guColorCombineFunction( %d )\n", fnc );
#endif

    switch ( fnc )
    {
        case GR_COLORCOMBINE_ZERO:                              //0x0
            grColorCombine( GR_COMBINE_FUNCTION_ZERO, GR_COMBINE_FACTOR_NONE,
                        GR_COMBINE_LOCAL_NONE, GR_COMBINE_OTHER_NONE, FXFALSE );
            break;

        case GR_COLORCOMBINE_CCRGB:                             //0x1
            grColorCombine( GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, 
                GR_COMBINE_LOCAL_CONSTANT, GR_COMBINE_OTHER_NONE, FXFALSE );
            break;

        case GR_COLORCOMBINE_ITRGB:                             //0x2
            grColorCombine( GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE,
                        GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_NONE, FXFALSE );
            break;

        case GR_COLORCOMBINE_DECAL_TEXTURE:                     //0x4
            grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, 
                GR_COMBINE_LOCAL_NONE, GR_COMBINE_OTHER_TEXTURE, FXFALSE );
            break;

        case GR_COLORCOMBINE_TEXTURE_TIMES_CCRGB:               //0x5
            grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, 
                GR_COMBINE_LOCAL_CONSTANT, GR_COMBINE_OTHER_TEXTURE, FXFALSE );
            break;

        case GR_COLORCOMBINE_TEXTURE_TIMES_ITRGB:               //0x6
            grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, 
                GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE );
            break;

        case GR_COLORCOMBINE_TEXTURE_TIMES_ITRGB_ADD_ALPHA:     //0x8
            grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA, GR_COMBINE_FACTOR_LOCAL, 
                GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE );
            break;

        case GR_COLORCOMBINE_TEXTURE_TIMES_ALPHA:               //0x9
            grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL_ALPHA, 
                GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE );
            break;

        case GR_COLORCOMBINE_TEXTURE_TIMES_ALPHA_ADD_ITRGB:     //0xa
            grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL, GR_COMBINE_FACTOR_LOCAL_ALPHA, 
                GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE );
            break;

        case GR_COLORCOMBINE_TEXTURE_ADD_ITRGB:                 //0xb
            grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL, GR_COMBINE_FACTOR_ONE, 
                GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE );
            break;

        case GR_COLORCOMBINE_TEXTURE_SUB_ITRGB:                 //0xc
            grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL, GR_COMBINE_FACTOR_ONE, 
                GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE );
            break;

        case GR_COLORCOMBINE_DIFF_SPEC_A:                       //0xe
            grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL, GR_COMBINE_FACTOR_LOCAL_ALPHA, 
                GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE );
            break;

        case GR_COLORCOMBINE_DIFF_SPEC_B:                       //0xf
            grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA, GR_COMBINE_FACTOR_LOCAL, 
                GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE );
            break;

        case GR_COLORCOMBINE_ONE:                               //0x10
            grColorCombine( GR_COMBINE_FUNCTION_ZERO, GR_COMBINE_FACTOR_ONE,
                        GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_NONE, FXTRUE );
            break;

        case GR_COLORCOMBINE_ITRGB_DELTA0:                      //0x3
            grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE,
                        GR_COMBINE_LOCAL_CONSTANT, GR_COMBINE_OTHER_CONSTANT, FXFALSE);
            break;

        case GR_COLORCOMBINE_TEXTURE_TIMES_ITRGB_DELTA0:        //0x7
        case GR_COLORCOMBINE_CCRGB_BLEND_ITRGB_ON_TEXALPHA:     //0xd
            break;
    }
}

//*************************************************
//* Sets the Alpha Test Reference Value
//*************************************************
DLLEXPORT void __stdcall
grAlphaTestReferenceValue( GrAlpha_t value )
{
#ifdef OGL_DONE
    GlideMsg( "grAlphaTestReferenceValue( %d )\n", value );
#endif

    RenderDrawTriangles( );

    Glide.State.AlphaReferenceValue = value;
    OpenGL.AlphaReferenceValue = value * D1OVER255;

    glAlphaFunc( OpenGL.AlphaTestFunction, OpenGL.AlphaReferenceValue );

#ifdef OPENGL_DEBUG
    GLErro( "grAlphaTestReferenceValue" );
#endif
}

//*************************************************
//* Sets the Alpha Test Function
//*************************************************
DLLEXPORT void __stdcall
grAlphaTestFunction( GrCmpFnc_t function )
{
#ifdef OGL_DONE
    GlideMsg( "grAlphaTestFunction( %d )\n", function );
#endif

    RenderDrawTriangles( );

    Glide.State.AlphaTestFunction = function;

    // We can do this just because we know the constant values for both OpenGL and Glide
    // To port it to anything else than OpenGL we NEED to change this code
    OpenGL.AlphaTestFunction = GL_NEVER + function;

    glEnable( GL_ALPHA_TEST );
    glAlphaFunc( OpenGL.AlphaTestFunction, OpenGL.AlphaReferenceValue );

#ifdef OPENGL_DEBUG
    GLErro( "grAlphaTestFunction" );
#endif
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall
grAlphaBlendFunction(GrAlphaBlendFnc_t rgb_sf,   GrAlphaBlendFnc_t rgb_df,
                     GrAlphaBlendFnc_t alpha_sf, GrAlphaBlendFnc_t alpha_df )
{
#ifdef OGL_PARTDONE
    GlideMsg( "grAlphaBlendFunction( %d, %d, %d, %d )\n",
        rgb_sf, rgb_df, alpha_sf, alpha_df );
#endif

    RenderDrawTriangles( );

    Glide.State.AlphaBlendRgbSf     = rgb_sf;
    Glide.State.AlphaBlendRgbDf     = rgb_df;
    Glide.State.AlphaBlendAlphaSf   = alpha_sf;
    Glide.State.AlphaBlendAlphaDf   = alpha_df;

    switch ( rgb_sf )
    {
    case GR_BLEND_ZERO:                 OpenGL.SrcBlend = GL_ZERO;                  break;
    case GR_BLEND_ONE:                  OpenGL.SrcBlend = GL_ONE;                   break;
    case GR_BLEND_DST_COLOR:            OpenGL.SrcBlend = GL_DST_COLOR;             break;
    case GR_BLEND_ONE_MINUS_DST_COLOR:  OpenGL.SrcBlend = GL_ONE_MINUS_DST_COLOR;   break;
    case GR_BLEND_SRC_ALPHA:            OpenGL.SrcBlend = GL_SRC_ALPHA;             break;
    case GR_BLEND_ONE_MINUS_SRC_ALPHA:  OpenGL.SrcBlend = GL_ONE_MINUS_SRC_ALPHA;   break;
    case GR_BLEND_DST_ALPHA:            OpenGL.SrcBlend = GL_DST_ALPHA;             break;
    case GR_BLEND_ONE_MINUS_DST_ALPHA:  OpenGL.SrcBlend = GL_ONE_MINUS_DST_ALPHA;   break;
    case GR_BLEND_ALPHA_SATURATE:       OpenGL.SrcBlend = GL_SRC_ALPHA_SATURATE;    break;

#ifdef OGL_DEBUG
    default:
        Error( "grAlphaBlendFunction: Unknow RGB source blend factor.\n" );
        OpenGL.SrcBlend = GL_ONE;
        break;
#endif
    }

    switch ( rgb_df )
    {
    case GR_BLEND_ZERO:                 OpenGL.DstBlend = GL_ZERO;                  break;
    case GR_BLEND_ONE:                  OpenGL.DstBlend = GL_ONE;                   break;
    case GR_BLEND_SRC_COLOR:            OpenGL.DstBlend = GL_SRC_COLOR;             break;
    case GR_BLEND_ONE_MINUS_SRC_COLOR:  OpenGL.DstBlend = GL_ONE_MINUS_SRC_COLOR;   break;
    case GR_BLEND_SRC_ALPHA:            OpenGL.DstBlend = GL_SRC_ALPHA;             break;
    case GR_BLEND_ONE_MINUS_SRC_ALPHA:  OpenGL.DstBlend = GL_ONE_MINUS_SRC_ALPHA;   break;
    case GR_BLEND_DST_ALPHA:            OpenGL.DstBlend = GL_DST_ALPHA;             break;
    case GR_BLEND_ONE_MINUS_DST_ALPHA:  OpenGL.DstBlend = GL_ONE_MINUS_DST_ALPHA;   break;
    case GR_BLEND_PREFOG_COLOR:         OpenGL.DstBlend = GL_ONE;                   break;

#ifdef OGL_DEBUG
    default:
        Error( "grAlphaBlendFunction: Unknow RGB destination blend factor.\n" );
        OpenGL.DstBlend = GL_ZERO;
        break;
#endif
    }

    switch ( alpha_sf )
    {
    case GR_BLEND_ZERO:                 OpenGL.SrcAlphaBlend = GL_ZERO;                 break;
    case GR_BLEND_ONE:                  OpenGL.SrcAlphaBlend = GL_ONE;                  break;
    case GR_BLEND_DST_COLOR:            OpenGL.SrcAlphaBlend = GL_DST_COLOR;            break;
    case GR_BLEND_ONE_MINUS_DST_COLOR:  OpenGL.SrcAlphaBlend = GL_ONE_MINUS_DST_COLOR;  break;
    case GR_BLEND_SRC_ALPHA:            OpenGL.SrcAlphaBlend = GL_SRC_ALPHA;            break;
    case GR_BLEND_ONE_MINUS_SRC_ALPHA:  OpenGL.SrcAlphaBlend = GL_ONE_MINUS_SRC_ALPHA;  break;
    case GR_BLEND_DST_ALPHA:            OpenGL.SrcAlphaBlend = GL_DST_ALPHA;            break;
    case GR_BLEND_ONE_MINUS_DST_ALPHA:  OpenGL.SrcAlphaBlend = GL_ONE_MINUS_DST_ALPHA;  break;
    case GR_BLEND_ALPHA_SATURATE:       OpenGL.SrcAlphaBlend = GL_SRC_ALPHA_SATURATE;   break;
    }

    switch ( alpha_df )
    {
    case GR_BLEND_ZERO:                 OpenGL.DstAlphaBlend = GL_ZERO;                 break;
    case GR_BLEND_ONE:                  OpenGL.DstAlphaBlend = GL_ONE;                  break;
    case GR_BLEND_SRC_COLOR:            OpenGL.DstAlphaBlend = GL_SRC_COLOR;            break;
    case GR_BLEND_ONE_MINUS_SRC_COLOR:  OpenGL.DstAlphaBlend = GL_ONE_MINUS_SRC_COLOR;  break;
    case GR_BLEND_SRC_ALPHA:            OpenGL.DstAlphaBlend = GL_SRC_ALPHA;            break;
    case GR_BLEND_ONE_MINUS_SRC_ALPHA:  OpenGL.DstAlphaBlend = GL_ONE_MINUS_SRC_ALPHA;  break;
    case GR_BLEND_DST_ALPHA:            OpenGL.DstAlphaBlend = GL_DST_ALPHA;            break;
    case GR_BLEND_ONE_MINUS_DST_ALPHA:  OpenGL.DstAlphaBlend = GL_ONE_MINUS_DST_ALPHA;  break;
    case GR_BLEND_PREFOG_COLOR:         OpenGL.DstAlphaBlend = GL_ONE;                  break;
    }

//    if ( ! InternalConfig.BlendFuncSeparateEXTEnable )
    {
        glBlendFunc( OpenGL.SrcBlend, OpenGL.DstBlend );
    }
//    else
    {
//        glBlendFuncSeparateEXT( OpenGL.SrcBlend, OpenGL.DstBlend, OpenGL.SrcAlphaBlend, OpenGL.DstAlphaBlend );
    }

    OpenGL.Blend = !(( rgb_sf == GR_BLEND_ONE ) && ( rgb_df == GR_BLEND_ZERO ));

    OpenGL.Texture = ( OpenGL.ColorTexture || ( OpenGL.Blend && OpenGL.AlphaTexture ) );

#ifdef OPENGL_DEBUG
    GLErro( "grAlphaBlendFunction" );
#endif
}

//----------------------------------------------------------------
//grAlphaCombine( 3, 8, 1, 2, 0 ) from Unreal
DLLEXPORT void __stdcall
grAlphaCombine(GrCombineFunction_t function, GrCombineFactor_t factor,
               GrCombineLocal_t local, GrCombineOther_t other,
               FxBool invert )
{
#ifdef OGL_PARTDONE
    GlideMsg( "grAlphaCombine( %d, %d, %d, %d, %d )\n",
        function, factor, local, other, invert );
#endif

    RenderDrawTriangles( );

    Glide.State.AlphaFunction = function;
    Glide.State.AlphaFactor = factor;
    Glide.State.AlphaLocal = local;
    Glide.State.AlphaOther = other;
    Glide.State.AlphaInvert = invert;

    Glide.ALocal = false;
    Glide.AOther = false;

    OpenGL.AlphaTexture = false;

    switch ( function )
    {
    case GR_COMBINE_FUNCTION_LOCAL:
    case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL:
    case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL_ALPHA:
        Glide.ALocal = true;
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER:
        if ( Glide.State.AlphaOther == GR_COMBINE_OTHER_TEXTURE )
        {
            OpenGL.AlphaTexture = true;
        }
        Glide.AOther = true;
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL:
    case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA:
    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL:
    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL:
    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL_ALPHA:
        if ( Glide.State.AlphaOther == GR_COMBINE_OTHER_TEXTURE )
        {
            OpenGL.AlphaTexture = true;
        }
        Glide.AOther = true;
        Glide.ALocal = true;
        break;
    }

    switch ( factor )
    {
    case GR_COMBINE_FACTOR_LOCAL:
    case GR_COMBINE_FACTOR_LOCAL_ALPHA:
        AlphaFactorFunc = AlphaFactorLocal;
        Glide.ALocal = true;
        break;

    case GR_COMBINE_FACTOR_ONE_MINUS_LOCAL:
    case GR_COMBINE_FACTOR_ONE_MINUS_LOCAL_ALPHA:
        AlphaFactorFunc = AlphaFactorOneMinusLocal;
        Glide.ALocal = true;
        break;

    case GR_COMBINE_FACTOR_OTHER_ALPHA:
        if ( Glide.State.AlphaOther == GR_COMBINE_OTHER_TEXTURE )
        {
            OpenGL.AlphaTexture = true;
        }
        AlphaFactorFunc = AlphaFactorOther;
        Glide.AOther = true;
        break;

    case GR_COMBINE_FACTOR_ONE_MINUS_OTHER_ALPHA:
        if ( Glide.State.AlphaOther == GR_COMBINE_OTHER_TEXTURE )
        {
            OpenGL.AlphaTexture = true;
        }
        AlphaFactorFunc = AlphaFactorOneMinusOther;
        Glide.AOther = true;
        break;

    case GR_COMBINE_FACTOR_TEXTURE_ALPHA:
    case GR_COMBINE_FACTOR_TEXTURE_RGB:     //GR_COMBINE_FACTOR_LOD_FRACTION:
    case GR_COMBINE_FACTOR_ONE_MINUS_TEXTURE_ALPHA:
    case GR_COMBINE_FACTOR_ONE_MINUS_LOD_FRACTION:
        OpenGL.AlphaTexture = true;

    case GR_COMBINE_FACTOR_ONE:
        AlphaFactorFunc = AlphaFactorOne;
        Glide.AOther = true;
        break;
    }

    OpenGL.Texture = ( OpenGL.ColorTexture || ( OpenGL.Blend && OpenGL.AlphaTexture ) );

//      OpenGL.TextureMode = GL_DECAL;
//      OpenGL.TextureMode = GL_REPLACE;
//      OpenGL.TextureMode = GL_MODULATE;
//      OpenGL.TextureMode = GL_BLEND;
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall
grAlphaControlsITRGBLighting( FxBool enable )
{
#ifdef OGL_NOTDONE
    GlideMsg("grAlphaControlsTGBALighting( %d )\n", enable );
#endif
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall
guAlphaSource( GrAlphaSource_t dwMode )
{
#ifdef OGL_DONE
    GlideMsg( "guAlphaSource(%d)\n", dwMode );
#endif

    switch ( dwMode )
    {
        case GR_ALPHASOURCE_CC_ALPHA:                               //0x00
            grAlphaCombine( GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, 
                GR_COMBINE_LOCAL_CONSTANT, GR_COMBINE_OTHER_NONE, FXFALSE );
            break;

        case GR_ALPHASOURCE_ITERATED_ALPHA:                         //0x01
            grAlphaCombine( GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, 
                GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_NONE, FXFALSE );
            break;

        case GR_ALPHASOURCE_TEXTURE_ALPHA:                          //0x02
            grAlphaCombine( GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, 
                GR_COMBINE_LOCAL_NONE, GR_COMBINE_OTHER_TEXTURE, FXFALSE );
            break;

        case GR_ALPHASOURCE_TEXTURE_ALPHA_TIMES_ITERATED_ALPHA:     //0x03
            grAlphaCombine( GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, 
                GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE ); 
            break;
    }
}

//*************************************************
//* Sets the ChromaKey Value for comparision
//*************************************************
DLLEXPORT void __stdcall
grChromakeyValue( GrColor_t value )
{
#ifdef OGL_PARTDONE
    GlideMsg( "grChromakeyValue( %d )\n", value );
#endif

    RenderDrawTriangles( );

    Textures->ChromakeyValue( value );

    Glide.State.ChromakeyValue = value;
    ConvertColorB(  value, 
                    OpenGL.ChromaColor[ 0 ], 
                    OpenGL.ChromaColor[ 1 ], 
                    OpenGL.ChromaColor[ 2 ], 
                    OpenGL.ChromaColor[ 3 ] );
}

//*************************************************
//* Sets the ChromaKey Mode
//*************************************************
DLLEXPORT void __stdcall
grChromakeyMode( GrChromakeyMode_t mode )
{
#ifdef OGL_PARTDONE
    GlideMsg( "grChromakeyMode( %d )\n", mode );
#endif
    RenderDrawTriangles( );

    Textures->ChromakeyMode( mode );

    Glide.State.ChromaKeyMode = mode;

    switch ( mode )
    {
    case GR_CHROMAKEY_DISABLE:
        OpenGL.ChromaKey = false;
        break;

    case GR_CHROMAKEY_ENABLE:
        #ifdef OGL_PARTDONE
            GlideMsg( "Chromakey Enabled\n" );
        #endif
        OpenGL.ChromaKey = true;
        break;
    }
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall
grGammaCorrectionValue( float value )
{
#ifdef OGL_PARTDONE
    GlideMsg( "grGammaCorrectionValue( %f )\n", value );
#endif
    RenderDrawTriangles();

    OpenGL.Gamma = value;
    {
        struct
        {
            WORD red[256];
            WORD green[256];
            WORD blue[256];
        } ramp;
        int i;
        HDC pDC = GetDC( NULL );

        for(i = 0; i < 256; i++)
        {
            WORD v = (WORD)( 0xffff * pow( i / 255.0, 1.0 / value ) );

            ramp.red[i] = ramp.green[i] = ramp.blue[i] = ( v & 0xff00 );
        }

        BOOL res = SetDeviceGammaRamp( pDC, &ramp );

        ReleaseDC( NULL, pDC );
    }

#ifdef OPENGL_DEBUG
    GLErro( "grGammaCorrectionValue" );
#endif
}

