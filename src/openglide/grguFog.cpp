//**************************************************************
//*            OpenGLide - Glide to OpenGL Wrapper
//*             http://openglide.sourceforge.net
//*
//*                      Fog Functions
//*
//*         OpenGLide is OpenSource under LGPL license
//*              Originaly made by Fabio Barros
//*      Modified by Paul for Glidos (http://www.glidos.net)
//**************************************************************

#include <math.h>

#include "GlOgl.h"
#include "GLRender.h"


// extern functions
void ConvertColorB( GrColor_t GlideColor, BYTE &R, BYTE &G, BYTE &B, BYTE &A );
void ConvertColorF( GrColor_t GlideColor, float &R, float &G, float &B, float &A );
void MMXCopyMemory( void *Dst, void *Src, DWORD NumberOfBytes );

//*************************************************
//* download a fog table
//* Fog is applied after color combining and before alpha blending.
//*************************************************
DLLEXPORT void __stdcall
grFogTable( const GrFog_t *ft )
{
#ifdef OGL_DONE
    GlideMsg( "grFogTable( --- )\n" );
#endif

    static DWORD    i, 
                    j,
                    s_i, 
                    e_i,
                    s, 
                    e,
                    forth_root[ 4 ] = { 0x10000, 0x1306f, 0x16a09, 0x1ae89 };

    if ( InternalConfig.FogEnable )
    {
        if ( InternalConfig.MMXEnable )
        {
            MMXCopyMemory( Glide.FogTable, (GrFog_t *)ft, GR_FOG_TABLE_SIZE * sizeof( FxU8 ) );
        }
        else
        {
            CopyMemory( Glide.FogTable, ft, GR_FOG_TABLE_SIZE * sizeof( FxU8 ) );
        }

        for ( i = 0; i < GR_FOG_TABLE_SIZE; i++ )
        {
            s_i = ( forth_root[ i & 3 ] >> ( 16 - ( i >> 2 ) ) );
            e_i = ( forth_root[ ( i + 1 ) & 3 ] >> ( 16 - ( ( i + 1 ) >> 2 ) ) );
            s = ft[ i ];
            e = ( ( i + 1 ) < GR_FOG_TABLE_SIZE ) ? ft[ i + 1 ] : 255;

            for ( j = s_i; j < e_i; j++ )
            {
                OpenGL.FogTable[ j ] = (BYTE)( s + ( e - s ) * ( j - s_i ) / ( e_i - s_i ) );
            }
        }
    }
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall
grFogColorValue( GrColor_t fogcolor )
{
#ifdef OGL_PARTDONE
    GlideMsg( "grFogColorValue( %x )\n", fogcolor );
#endif

    RenderDrawTriangles( );

    Glide.State.FogColorValue = fogcolor;
    ConvertColorF( fogcolor, 
                   OpenGL.FogColor[ 0 ], 
                   OpenGL.FogColor[ 1 ], 
                   OpenGL.FogColor[ 2 ], 
                   OpenGL.FogColor[ 3 ] );
    glFogfv( GL_FOG_COLOR, &OpenGL.FogColor[0] );
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall
grFogMode( GrFogMode_t mode )
{
#ifdef OGL_PARTDONE
    GlideMsg( "grFogMode( %d )\n", mode );
#endif

    static GrFogMode_t  modeSource, 
                        modeAdd;
    static GLfloat      ZeroColor[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.0f };

    RenderDrawTriangles( );

    modeSource = mode & ( GR_FOG_WITH_TABLE | GR_FOG_WITH_ITERATED_ALPHA );
    modeAdd = mode & ( GR_FOG_MULT2 | GR_FOG_ADD2 );

    if ( modeSource )
    {
        OpenGL.Fog = true;
        if ( InternalConfig.FogCoordEXTEnable )
        {
            glEnable( GL_FOG );
        }
    }
    else
    {
        OpenGL.Fog = false;
        glDisable( GL_FOG );
    }

    switch ( modeAdd )
    {
    case GR_FOG_MULT2:
    case GR_FOG_ADD2:
        glFogfv( GL_FOG_COLOR, &ZeroColor[ 0 ] );
        break;

    default:
        glFogfv( GL_FOG_COLOR, &OpenGL.FogColor[ 0 ] );
        break;
    }
    
    Glide.State.FogMode = modeSource;
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall
guFogGenerateExp( GrFog_t *fogtable, float density )
{
#ifdef OGL_PARTDONE
    GlideMsg( "guFogGenerateExp( ---, %-4.2f )\n", density );
#endif
    
    float f;
    float scale;
    float dp;
    
    dp = density * guFogTableIndexToW( GR_FOG_TABLE_SIZE - 1 );
    scale = 255.0F / ( 1.0F - (float) exp( -dp ) );
    
    for ( int i = 0; i < GR_FOG_TABLE_SIZE; i++ )
    {
        dp = density * guFogTableIndexToW( i );
        f = ( 1.0F - (float) exp( -dp ) ) * scale;
        
        if ( f > 255.0F )
        {
            f = 255.0F;
        }
        else if ( f < 0.0F )
        {
            f = 0.0F;
        }
        
        fogtable[ i ] = (GrFog_t) f;
    }
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall
guFogGenerateExp2( GrFog_t *fogtable, float density )
{
#ifdef OGL_PARTDONE
    GlideMsg( "guFogGenerateExp2( ---, %-4.2f )\n", density );
#endif

    float Temp;

    for ( int i = 0; i < GR_FOG_TABLE_SIZE; i++ )
    {
        Temp = ( 1.0f - (float) exp( ( -density)  * guFogTableIndexToW( i ) ) * 
               (float)exp( (-density)  * guFogTableIndexToW( i ) ) )  * 255.0f;
        fogtable[ i ] = (BYTE) Temp;
    }
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall
guFogGenerateLinear( GrFog_t *fogtable,
                     float nearZ, float farZ )
{
#ifdef OGL_PARTDONE
    GlideMsg( "guFogGenerateLinear( ---, %-4.2f, %-4.2f )\n", nearZ, farZ );
#endif

    int Start, 
        End, 
        i;

    for( Start = 0; Start < GR_FOG_TABLE_SIZE; Start++ )
    {
        if ( guFogTableIndexToW( Start ) >= nearZ )
        {
            break;
        }
    }
    for( End = 0; End < GR_FOG_TABLE_SIZE; End++ )
    {
        if ( guFogTableIndexToW( End ) >= farZ )
        {
            break;
        }
    }

    ZeroMemory( fogtable, GR_FOG_TABLE_SIZE );
    for( i = Start; i <= End; i++ )
    {
        fogtable[ i ] = (BYTE)((float)( End - Start ) / 255.0f * (float)( i - Start ));
    }

    for( i = End; i < GR_FOG_TABLE_SIZE; i++ )
    {
        fogtable[ i ] = 255;
    }
}

//*************************************************
//* convert a fog table index to a floating point eye-space w value
//*************************************************
DLLEXPORT float __stdcall
guFogTableIndexToW( int i )
{
#ifdef OGL_DONE
    GlideMsg( "guFogTableIndexToW( %d )\n", i );
#endif
    static float tableIndexToW[ GR_FOG_TABLE_SIZE ] =
    {
            1.000000f,      1.142857f,      1.333333f,      1.600000f, 
            2.000000f,      2.285714f,      2.666667f,      3.200000f, 
            4.000000f,      4.571429f,      5.333333f,      6.400000f, 
            8.000000f,      9.142858f,     10.666667f,     12.800000f, 
           16.000000f,     18.285715f,     21.333334f,     25.600000f, 
           32.000000f,     36.571430f,     42.666668f,     51.200001f, 
           64.000000f,     73.142860f,     85.333336f,    102.400002f, 
          128.000000f,    146.285721f,    170.666672f,    204.800003f, 
          256.000000f,    292.571442f,    341.333344f,    409.600006f, 
          512.000000f,    585.142883f,    682.666687f,    819.200012f, 
         1024.000000f,   1170.285767f,   1365.333374f,   1638.400024f, 
         2048.000000f,   2340.571533f,   2730.666748f,   3276.800049f, 
         4096.000000f,   4681.143066f,   5461.333496f,   6553.600098f, 
         8192.000000f,   9362.286133f,  10922.666992f,  13107.200195f, 
        16384.000000f,  18724.572266f,  21845.333984f,  26214.400391f, 
        32768.000000f,  37449.144531f,  43690.667969f,  52428.800781f
    };

    if ( ( i < 0 ) ||
         ( i >= GR_FOG_TABLE_SIZE ) )
    {
        Error( "Error on guFogTableIndexToW( %d )\n", i );
    }

    return tableIndexToW[ i ];
}

