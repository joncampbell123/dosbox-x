//**************************************************************
//*            OpenGLide - Glide to OpenGL Wrapper
//*             http://openglide.sourceforge.net
//*
//*                  Glide Texture Functions
//*
//*         OpenGLide is OpenSource under LGPL license
//*              Originaly made by Fabio Barros
//*      Modified by Paul for Glidos (http://www.glidos.net)
//**************************************************************

#include "GlOgl.h"
#include "GLRender.h"
#include "GLextensions.h"
#include "PGTexture.h"
#include "PGUTexture.h"


// Functions

//*************************************************
//* Return the lowest start address for texture downloads
//*************************************************
DLLEXPORT FxU32 __stdcall
grTexMinAddress( GrChipID_t tmu )
{
#ifdef OGL_DONE
    GlideMsg( "grTexMinAddress( %d )\n", tmu );
#endif

    return (FxU32) 0;
}

//*************************************************
//* Return the highest start address for texture downloads
//*************************************************
DLLEXPORT FxU32 __stdcall
grTexMaxAddress( GrChipID_t tmu )
{
#ifdef OGL_DONE
    GlideMsg( "grTexMaxAddress( %d ) = %lu\n", tmu, Glide.TexMemoryMaxPosition );
#endif

    return (FxU32)( Glide.TexMemoryMaxPosition );
}

//*************************************************
//* Specify the current texture source for rendering
//*************************************************
DLLEXPORT void __stdcall
grTexSource( GrChipID_t tmu,
             FxU32      startAddress,
             FxU32      evenOdd,
             GrTexInfo  *info )
{
#ifdef OGL_DONE
    GlideMsg( "grTexSource( %d, %d, %d, --- )\n", tmu, startAddress, evenOdd );
#endif

    if ( tmu != GR_TMU0 )
    {
        return;
    }

    RenderDrawTriangles( );

    Glide.State.TexSource.StartAddress = startAddress;
    Glide.State.TexSource.EvenOdd = evenOdd;

    Textures->Source( startAddress, evenOdd, info );    
}

//*************************************************
//* Return the texture memory consumed by a texture
//*************************************************
DLLEXPORT FxU32 __stdcall
grTexTextureMemRequired( DWORD dwEvenOdd, GrTexInfo *texInfo )
{
#ifdef OGL_DONE
    GlideMsg( "grTexTextureMemRequired( %u, --- )\n", dwEvenOdd );
#endif

    return Textures->TextureMemRequired( dwEvenOdd, texInfo );
}

//*************************************************
//* Return the texture memory consumed by a texture
//*************************************************
DLLEXPORT void __stdcall
grTexDownloadMipMap( GrChipID_t tmu,
                     FxU32      startAddress,
                     FxU32      evenOdd,
                     GrTexInfo  *info )
{
#ifdef OGL_PARTDONE
    GlideMsg( "grTexDownloadMipMap( %d, %u, %u, --- )\n", tmu, 
        startAddress, evenOdd );
#endif

    if ( tmu != GR_TMU0 )
    {
        return;
    }

    RenderDrawTriangles( );

    info->smallLod = info->largeLod;
    Textures->DownloadMipMap( startAddress, evenOdd, info );
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall
grTexDownloadMipMapLevel( GrChipID_t        tmu,
                          FxU32             startAddress,
                          GrLOD_t           thisLod,
                          GrLOD_t           largeLod,
                          GrAspectRatio_t   aspectRatio,
                          GrTextureFormat_t format,
                          FxU32             evenOdd,
                          void              *data )
{
#ifdef OGL_PARTDONE
    GlideMsg( "grTexDownloadMipMapLevel( %d, %lu, %d, %d, %d, %d, %d, %lu )\n",
        tmu, startAddress, thisLod, largeLod, aspectRatio, format, evenOdd, data );
#endif

    if ( ( tmu != GR_TMU0 ) || ( thisLod != largeLod ) )
//    if ( tmu != GR_TMU0 )
    {
        return;
    }

    static GrTexInfo info;

    info.smallLod       = thisLod;
    info.largeLod       = largeLod;
    info.aspectRatio    = aspectRatio;
    info.format         = format;
    info.data           = data;

    Textures->DownloadMipMap( startAddress, evenOdd, &info );
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall
grTexDownloadMipMapLevelPartial( GrChipID_t        tmu,
                                 FxU32             startAddress,
                                 GrLOD_t           thisLod,
                                 GrLOD_t           largeLod,
                                 GrAspectRatio_t   aspectRatio,
                                 GrTextureFormat_t format,
                                 FxU32             evenOdd,
                                 void              *data,
                                 int               start,
                                 int               end )
{
#ifdef OGL_PARTDONE
    GlideMsg( "grTexDownloadMipMapLevelPartial( %d, %lu, %d, %d, %d, %d, %d, ---, %d, %d )\n",
        tmu, startAddress, thisLod, largeLod, aspectRatio, format, evenOdd, start, end );
#endif

    if ( tmu != GR_TMU0 )
    {
        return;
    }

    grTexDownloadMipMapLevel( tmu, startAddress, thisLod, largeLod, aspectRatio, format,
        evenOdd, data );
}

//*************************************************
//* Set the texture map clamping/wrapping mode
//*************************************************
DLLEXPORT void __stdcall
grTexClampMode( GrChipID_t tmu,
                GrTextureClampMode_t s_clampmode,
                GrTextureClampMode_t t_clampmode )
{
#ifdef OGL_DONE
    GlideMsg( "grTexClampMode( %d, %d, %d )\n",
        tmu, s_clampmode, t_clampmode );
#endif

    if ( tmu != GR_TMU0 )
    {
        return;
    }

    RenderDrawTriangles( );

    Glide.State.SClampMode = s_clampmode;
    Glide.State.TClampMode = t_clampmode;

    // We can do this just because we know the constant values for both OpenGL and Glide
    // To port it to anything else than OpenGL we NEED to change this code
    OpenGL.SClampMode = GL_REPEAT - s_clampmode;
    OpenGL.TClampMode = GL_REPEAT - t_clampmode;

/*    switch ( s_clampmode )
    {
    case GR_TEXTURECLAMP_CLAMP:     OpenGL.SClampMode = GL_CLAMP;   break;
    case GR_TEXTURECLAMP_WRAP:      OpenGL.SClampMode = GL_REPEAT;  break;
    }
    switch ( t_clampmode )
    {
    case GR_TEXTURECLAMP_CLAMP:     OpenGL.TClampMode = GL_CLAMP;   break;
    case GR_TEXTURECLAMP_WRAP:      OpenGL.TClampMode = GL_REPEAT;  break;
    }
*/
#ifdef OPENGL_DEBUG
    GLErro( "grTexClampMode" );
#endif
}

//*************************************************
//* Set the texture Min/Mag Filter
//*************************************************
DLLEXPORT void __stdcall
grTexFilterMode( GrChipID_t tmu,
                 GrTextureFilterMode_t minfilter_mode,
                 GrTextureFilterMode_t magfilter_mode )
{
#ifdef OGL_PARTDONE
    GlideMsg( "grTexFilterMode( %d, %d, %d )\n",
        tmu, minfilter_mode, magfilter_mode );
#endif

    if ( tmu != GR_TMU0 )
    {
        return;
    }

    RenderDrawTriangles( );

    Glide.State.MinFilterMode = minfilter_mode;
    Glide.State.MagFilterMode = magfilter_mode;

    switch ( minfilter_mode )
    {
    case GR_TEXTUREFILTER_POINT_SAMPLED:
        if ( ( Glide.State.MipMapMode != GR_MIPMAP_DISABLE ) && 
             ( InternalConfig.EnableMipMaps ) )
        {
            if ( Glide.State.LodBlend )
            {
                OpenGL.MinFilterMode = GL_NEAREST_MIPMAP_LINEAR;
            }
            else
            {
                OpenGL.MinFilterMode = GL_NEAREST_MIPMAP_NEAREST;
            }
        }
        else
        {
            OpenGL.MinFilterMode = GL_NEAREST;
        }
        break;

    case GR_TEXTUREFILTER_BILINEAR:
        if ( InternalConfig.EnableMipMaps )
        {
            if ( Glide.State.LodBlend )
            {
                OpenGL.MinFilterMode = GL_LINEAR_MIPMAP_LINEAR;
            }
            else
            {
                OpenGL.MinFilterMode = GL_LINEAR_MIPMAP_NEAREST;
            }
        }
        else
        {
            OpenGL.MinFilterMode = GL_LINEAR;
        }
        break;
    }
    switch ( magfilter_mode )
    {
    case GR_TEXTUREFILTER_POINT_SAMPLED:    OpenGL.MagFilterMode = GL_NEAREST;      break;
    case GR_TEXTUREFILTER_BILINEAR:         OpenGL.MagFilterMode = GL_LINEAR;       break;
    }

#ifdef OPENGL_DEBUG
    GLErro( "grTexFilterMode" );
#endif
}

//*************************************************
//* Set the texture MipMap Mode
//*************************************************
DLLEXPORT void __stdcall
grTexMipMapMode( GrChipID_t     tmu, 
                 GrMipMapMode_t mode,
                 FxBool         lodBlend )
{
#ifdef OGL_PARTDONE
    GlideMsg( "grTexMipMapMode( %d, %d, %d )\n",
        tmu, mode, lodBlend );
#endif

    if ( tmu != GR_TMU0 )
    {
        return;
    }

    Glide.State.MipMapMode = mode;
    Glide.State.LodBlend = lodBlend;

    grTexFilterMode( GR_TMU0,
                Glide.State.MinFilterMode,
                Glide.State.MagFilterMode );

#ifdef OPENGL_DEBUG
    GLErro( "grTexMipMapMode" );
#endif
}

//*************************************************
//* Returns the memory occupied by a texture
//*************************************************
DLLEXPORT FxU32 __stdcall
grTexCalcMemRequired( GrLOD_t lodmin, GrLOD_t lodmax,
                      GrAspectRatio_t aspect, GrTextureFormat_t fmt )
{
#ifdef OGL_DONE
    GlideMsg( "grTexCalcMemRequired( %d, %d, %d, %d )\n",
        lodmin, lodmax, aspect, fmt );
#endif

    static GrTexInfo texInfo;
    texInfo.aspectRatio = aspect;
    texInfo.format      = fmt;
    texInfo.largeLod    = lodmax;
    texInfo.smallLod    = lodmin;

    return Textures->TextureMemRequired( 0, &texInfo );
}

//*************************************************
//* Download a subset of an NCC table or color palette
//*************************************************
DLLEXPORT void __stdcall
grTexDownloadTablePartial( GrChipID_t   tmu,
                           GrTexTable_t type, 
                           void        *data,
                           int          start,
                           int          end )
{
#ifdef OGL_PARTDONE
    GlideMsg( "grTexDownloadTablePartial( %d, %d, ---, %d, %d )\n",
        tmu, type, start, end );
#endif

    if ( tmu != GR_TMU0 )
    {
        return;
    }

    RenderDrawTriangles( );

    Textures->DownloadTable( type, (FxU32*)data, start, end + 1 - start );
}

//*************************************************
//* download an NCC table or color palette
//*************************************************
DLLEXPORT void __stdcall
grTexDownloadTable( GrChipID_t   tmu,
                    GrTexTable_t type, 
                    void         *data )
{
#ifdef OGL_PARTDONE
    GlideMsg( "grTexDownloadTable( %d, %d, --- )\n", tmu, type );
#endif

    if ( tmu != GR_TMU0 )
    {
        return;
    }

    RenderDrawTriangles( );

    Textures->DownloadTable( type, (FxU32*)data, 0, 256 );
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall
grTexLodBiasValue( GrChipID_t tmu, float bias )
{
#ifdef OGL_NOTDONE
    GlideMsg( "grTexLodBiasValue( %d, %d )\n",
        tmu, bias );
#endif

    if ( InternalConfig.TextureLodBiasEXTEnable )
    {
        glTexEnvf( GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, bias );
    }
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall
grTexCombine( GrChipID_t tmu,
              GrCombineFunction_t rgb_function,
              GrCombineFactor_t rgb_factor, 
              GrCombineFunction_t alpha_function,
              GrCombineFactor_t alpha_factor,
              FxBool rgb_invert,
              FxBool alpha_invert )
{
#ifdef OGL_PARTDONE
    GlideMsg( "grTexCombine( %d, %d, %d, %d, %d, %d, %d )\n",
        tmu, rgb_function, rgb_factor, alpha_function, alpha_factor, 
        rgb_invert, alpha_invert );
#endif

    if ( tmu != GR_TMU0 )
    {
        return;
    }

    RenderDrawTriangles( );

    Glide.State.TextureCombineCFunction = rgb_function;
    Glide.State.TextureCombineCFactor   = rgb_factor;
    Glide.State.TextureCombineAFunction = alpha_function;
    Glide.State.TextureCombineAFactor   = alpha_factor;
    Glide.State.TextureCombineRGBInvert = rgb_invert;
    Glide.State.TextureCombineAInvert   = alpha_invert;

    if ( ( rgb_function != GR_COMBINE_FUNCTION_ZERO ) ||
         ( alpha_function != GR_COMBINE_FUNCTION_ZERO ) )
    {
        OpenGL.Texture = true;
    }
    else
    {
        OpenGL.Texture = false;
    }
/*
    switch ( rgb_function )
    {
    case GR_COMBINE_FUNCTION_ZERO:
        OpenGL.Texture = false;
//      GlideMsg( "GR_COMBINE_FUNCTION_ZERO," );
        break;
    case GR_COMBINE_FUNCTION_LOCAL:
//      GlideMsg( "GR_COMBINE_FUNCTION_LOCAL," );
        OpenGL.Texture = true;
        break;
    case GR_COMBINE_FUNCTION_LOCAL_ALPHA:
//      GlideMsg( "GR_COMBINE_FUNCTION_LOCAL_ALPHA," );
        OpenGL.Texture = true;
        break;
    case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL:
//      GlideMsg( "GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL," );
        OpenGL.Texture = true;
        break;
    case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL_ALPHA:
//      GlideMsg( "GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL_ALPHA," );
        OpenGL.Texture = true;
        break;
    case GR_COMBINE_FUNCTION_SCALE_OTHER:
//      GlideMsg( "GR_COMBINE_FUNCTION_SCALE_OTHER," );
        OpenGL.Texture = true;
        break;
    case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL:
//      GlideMsg( "GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL," );
        OpenGL.Texture = true;
        break;
    case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA:
//      GlideMsg( "GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA," );
        OpenGL.Texture = true;
        break;
    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL:
//      GlideMsg( "GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL," );
        OpenGL.Texture = true;
        break;
    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL:
//      GlideMsg( "GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL," );
        OpenGL.Texture = true;
        break;
    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL_ALPHA:
//      GlideMsg( "GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL_ALPHA," );
        OpenGL.Texture = true;
        break;
    }

    switch ( rgb_factor )
    {
    case GR_COMBINE_FACTOR_ZERO:
//      GlideMsg( "GR_COMBINE_FACTOR_ZERO," );
        break;
    case GR_COMBINE_FACTOR_LOCAL:
//      GlideMsg( "GR_COMBINE_FACTOR_LOCAL," );
        break;
    case GR_COMBINE_FACTOR_OTHER_ALPHA:
//      GlideMsg( "GR_COMBINE_FACTOR_OTHER_ALPHA," );
        break;
    case GR_COMBINE_FACTOR_LOCAL_ALPHA:
//      GlideMsg( "GR_COMBINE_FACTOR_LOCAL_ALPHA," );
        break;
    case GR_COMBINE_FACTOR_DETAIL_FACTOR:
//      GlideMsg( "GR_COMBINE_FACTOR_DETAIL_FACTOR," );
        break;
    case GR_COMBINE_FACTOR_LOD_FRACTION:
//      GlideMsg( "GR_COMBINE_FACTOR_LOD_FRACTION," );
        break;
    case GR_COMBINE_FACTOR_ONE:
//      GlideMsg( "GR_COMBINE_FACTOR_ONE," );
        break;
    case GR_COMBINE_FACTOR_ONE_MINUS_LOCAL:
//      GlideMsg( "GR_COMBINE_FACTOR_ONE_MINUS_LOCAL," );
        break;
    case GR_COMBINE_FACTOR_ONE_MINUS_OTHER_ALPHA:
//      GlideMsg( "GR_COMBINE_FACTOR_ONE_MINUS_OTHER_ALPHA," );
        break;
    case GR_COMBINE_FACTOR_ONE_MINUS_LOCAL_ALPHA:
//      GlideMsg( "GR_COMBINE_FACTOR_ONE_MINUS_LOCAL_ALPHA," );
        break;
    case GR_COMBINE_FACTOR_ONE_MINUS_DETAIL_FACTOR:
//      GlideMsg( "GR_COMBINE_FACTOR_ONE_MINUS_DETAIL_FACTOR," );
        break;
    case GR_COMBINE_FACTOR_ONE_MINUS_LOD_FRACTION:
//      GlideMsg( "GR_COMBINE_FACTOR_ONE_MINUS_LOD_FRACTION," );
        break;
    }

    switch ( alpha_function )
    {
    case GR_COMBINE_FUNCTION_ZERO:
//      OpenGL.Texture = false;
//      GlideMsg( "GR_COMBINE_FUNCTION_ZERO," );
        break;
    case GR_COMBINE_FUNCTION_LOCAL:
//      GlideMsg( "GR_COMBINE_FUNCTION_LOCAL," );
        OpenGL.Texture = true;
        break;
    case GR_COMBINE_FUNCTION_LOCAL_ALPHA:
//      GlideMsg( "GR_COMBINE_FUNCTION_LOCAL_ALPHA," );
        OpenGL.Texture = true;
        break;
    case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL:
//      GlideMsg( "GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL," );
        OpenGL.Texture = true;
        break;
    case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL_ALPHA:
//      GlideMsg( "GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL_ALPHA," );
        OpenGL.Texture = true;
        break;
    case GR_COMBINE_FUNCTION_SCALE_OTHER:
//      GlideMsg( "GR_COMBINE_FUNCTION_SCALE_OTHER," );
        OpenGL.Texture = true;
        break;
    case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL:
//      GlideMsg( "GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL," );
        OpenGL.Texture = true;
        break;
    case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA:
//      GlideMsg( "GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA," );
        OpenGL.Texture = true;
        break;
    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL:
//      GlideMsg( "GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL," );
        OpenGL.Texture = true;
        break;
    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL:
//      GlideMsg( "GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL," );
        OpenGL.Texture = true;
        break;
    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL_ALPHA:
//      GlideMsg( "GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL_ALPHA," );
        OpenGL.Texture = true;
        break;
    }

    switch ( alpha_factor )
    {
    case GR_COMBINE_FACTOR_ZERO:
//      GlideMsg( "GR_COMBINE_FACTOR_ZERO\n" );
        break;
    case GR_COMBINE_FACTOR_LOCAL:
//      GlideMsg( "GR_COMBINE_FACTOR_LOCAL\n" );
        break;
    case GR_COMBINE_FACTOR_OTHER_ALPHA:
//      GlideMsg( "GR_COMBINE_FACTOR_OTHER_ALPHA\n" );
        break;
    case GR_COMBINE_FACTOR_LOCAL_ALPHA:
//      GlideMsg( "GR_COMBINE_FACTOR_LOCAL_ALPHA\n" );
        break;
    case GR_COMBINE_FACTOR_DETAIL_FACTOR:
//      GlideMsg( "GR_COMBINE_FACTOR_DETAIL_FACTOR\n" );
        break;
    case GR_COMBINE_FACTOR_LOD_FRACTION:
//      GlideMsg( "GR_COMBINE_FACTOR_LOD_FRACTION\n" );
        break;
    case GR_COMBINE_FACTOR_ONE:
//      GlideMsg( "GR_COMBINE_FACTOR_ONE\n" );
        break;
    case GR_COMBINE_FACTOR_ONE_MINUS_LOCAL:
//      GlideMsg( "GR_COMBINE_FACTOR_ONE_MINUS_LOCAL\n" );
        break;
    case GR_COMBINE_FACTOR_ONE_MINUS_OTHER_ALPHA:
//      GlideMsg( "GR_COMBINE_FACTOR_ONE_MINUS_OTHER_ALPHA\n" );
        break;
    case GR_COMBINE_FACTOR_ONE_MINUS_LOCAL_ALPHA:
//      GlideMsg( "GR_COMBINE_FACTOR_ONE_MINUS_LOCAL_ALPHA\n" );
        break;
    case GR_COMBINE_FACTOR_ONE_MINUS_DETAIL_FACTOR:
//      GlideMsg( "GR_COMBINE_FACTOR_ONE_MINUS_DETAIL_FACTOR\n" );
        break;
    case GR_COMBINE_FACTOR_ONE_MINUS_LOD_FRACTION:
//      GlideMsg( "GR_COMBINE_FACTOR_ONE_MINUS_LOD_FRACTION\n" );
        break;
    }
    */
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall
grTexNCCTable( GrChipID_t tmu, GrNCCTable_t NCCTable )
{
#ifdef OGL_DONE
    GlideMsg( "grTexNCCTable( %d, %u )\n", tmu, NCCTable );
#endif

    if ( tmu == GR_TMU0 )
    {
        Textures->NCCTable( NCCTable );
    }
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall 
grTexDetailControl( GrChipID_t tmu,
                    int lod_bias,
                    FxU8 detail_scale,
                    float detail_max )
{
#ifdef OGL_NOTDONE
    GlideMsg( "grTexDetailControl( %d, %d, %d, %-4.2f )\n",
        tmu, lod_bias, detail_scale, detail_max );
#endif
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall 
grTexMultibase( GrChipID_t tmu,
                FxBool     enable )
{
#ifdef OGL_NOTDONE
    GlideMsg( "grTexMultibase( %d, %d )\n", tmu, enable );
#endif
    if (tmu != GR_TMU0)
        return;
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall
grTexMultibaseAddress( GrChipID_t       tmu,
                       GrTexBaseRange_t range,
                       FxU32            startAddress,
                       FxU32            evenOdd,
                       GrTexInfo        *info )
{
#ifdef OGL_NOTDONE
    GlideMsg( "grTexMultibaseAddress( %d, %d, %lu, %lu, --- )\n",
        tmu, range, startAddress, evenOdd );
#endif
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall
grTexCombineFunction( GrChipID_t tmu, GrTextureCombineFnc_t func )
{
#ifdef OGL_PARTDONE
    GlideMsg( "grTexCombineFunction( %d, %d )\n", tmu, func );
#endif

    if ( tmu != GR_TMU0 )
    {
        return;
    }

    switch ( func )
    {
    case GR_TEXTURECOMBINE_ZERO:            // 0x00 per component
        grTexCombine( tmu, GR_COMBINE_FUNCTION_ZERO, GR_COMBINE_FACTOR_ZERO,
            GR_COMBINE_FUNCTION_ZERO, GR_COMBINE_FACTOR_ZERO, FXFALSE, FXFALSE );
        break;

    case GR_TEXTURECOMBINE_DECAL:           // Clocal decal texture
        grTexCombine( tmu, GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE,
            GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, FXFALSE, FXFALSE );
        break;

    case GR_TEXTURECOMBINE_OTHER:           // Cother pass through
        grTexCombine( tmu, GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE,
            GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, FXFALSE, FXFALSE );
        break;

    case GR_TEXTURECOMBINE_ADD:             // Cother + Clocal additive texture
        grTexCombine( tmu, GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL, GR_COMBINE_FACTOR_ONE,
            GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL, GR_COMBINE_FACTOR_ONE, FXFALSE, FXFALSE );
        break;

    case GR_TEXTURECOMBINE_MULTIPLY:        // Cother * Clocal modulated texture
        grTexCombine( tmu, GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL,
            GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, FXFALSE, FXFALSE );
        break;

    case GR_TEXTURECOMBINE_SUBTRACT:        // Cother – Clocal subtractive texture
        grTexCombine( tmu, GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL, GR_COMBINE_FACTOR_ONE,
            GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL, GR_COMBINE_FACTOR_ONE, FXFALSE, FXFALSE );
        break;

    case GR_TEXTURECOMBINE_ONE:             // 255 0xFF per component
        grTexCombine( tmu, GR_COMBINE_FUNCTION_ZERO, GR_COMBINE_FACTOR_ZERO,
            GR_COMBINE_FUNCTION_ZERO, GR_COMBINE_FACTOR_ZERO, FXTRUE, FXTRUE );
        break;

//  case GR_TEXTURECOMBINE_DETAIL:          // blend (Cother, Clocal) detail textures with detail on selected TMU
//  case GR_TEXTURECOMBINE_DETAIL_OTHER:    // blend (Cother, Clocal) detail textures with detail on neighboring TMU
//  case GR_TEXTURECOMBINE_TRILINEAR_ODD:   // blend (Cother, Clocal) LOD blended textures with odd levels on selected TMU
//  case GR_TEXTURECOMBINE_TRILINEAR_EVEN:  // blend (Cother, Clocal) LOD blended textures with even levels on selected TMU
//      break;
    }
}

//*************************************************
//* Return the amount of unallocated texture memory on a Texture Mapping Unit
//*************************************************
DLLEXPORT FxU32 __stdcall 
guTexMemQueryAvail( GrChipID_t tmu )
{
#ifdef OGL_PARTDONE
    GlideMsg( "guTexMemQueryAvail( %d )\n", tmu );
#endif

    if ( tmu != GR_TMU0 )
    {
        return 0;
    }

    return UTextures.MemQueryAvail( tmu );
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall
guTexCombineFunction( GrChipID_t tmu, GrTextureCombineFnc_t func )
{
#ifdef OGL_PARTDONE
    GlideMsg( "guTexCombineFunction( %d, %d )\n", tmu, func );
#endif

    if ( tmu != GR_TMU0 )
    {
        return;
    }

    switch ( func )
    {
    case GR_TEXTURECOMBINE_ZERO:            // 0x00 per component
        grTexCombine( tmu, GR_COMBINE_FUNCTION_ZERO, GR_COMBINE_FACTOR_ZERO,
            GR_COMBINE_FUNCTION_ZERO, GR_COMBINE_FACTOR_ZERO, FXFALSE, FXFALSE );
        break;

    case GR_TEXTURECOMBINE_DECAL:           // Clocal decal texture
        grTexCombine( tmu, GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE,
            GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, FXFALSE, FXFALSE );
        break;

    case GR_TEXTURECOMBINE_OTHER:           // Cother pass through
        grTexCombine( tmu, GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE,
            GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, FXFALSE, FXFALSE );
        break;

    case GR_TEXTURECOMBINE_ADD:             // Cother + Clocal additive texture
        grTexCombine( tmu, GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL, GR_COMBINE_FACTOR_ONE,
            GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL, GR_COMBINE_FACTOR_ONE, FXFALSE, FXFALSE );
        break;

    case GR_TEXTURECOMBINE_MULTIPLY:        // Cother * Clocal modulated texture
        grTexCombine( tmu, GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL,
            GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, FXFALSE, FXFALSE );
        break;

    case GR_TEXTURECOMBINE_SUBTRACT:        // Cother – Clocal subtractive texture
        grTexCombine( tmu, GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL, GR_COMBINE_FACTOR_ONE,
            GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL, GR_COMBINE_FACTOR_ONE, FXFALSE, FXFALSE );
        break;

    case GR_TEXTURECOMBINE_ONE:             // 255 0xFF per component
        grTexCombine( tmu, GR_COMBINE_FUNCTION_ZERO, GR_COMBINE_FACTOR_ZERO,
            GR_COMBINE_FUNCTION_ZERO, GR_COMBINE_FACTOR_ZERO, FXTRUE, FXTRUE );
        break;

//  case GR_TEXTURECOMBINE_DETAIL:          // blend (Cother, Clocal) detail textures with detail on selected TMU
//  case GR_TEXTURECOMBINE_DETAIL_OTHER:    // blend (Cother, Clocal) detail textures with detail on neighboring TMU
//  case GR_TEXTURECOMBINE_TRILINEAR_ODD:   // blend (Cother, Clocal) LOD blended textures with odd levels on selected TMU
//  case GR_TEXTURECOMBINE_TRILINEAR_EVEN:  // blend (Cother, Clocal) LOD blended textures with even levels on selected TMU
//      break;
    }
}

//----------------------------------------------------------------
DLLEXPORT GrMipMapId_t __stdcall 
guTexGetCurrentMipMap( GrChipID_t tmu )
{
#ifdef OGL_DONE
    GlideMsg( "guTexGetCurrentMipMap( %d )\n", tmu );
#endif

    if ( tmu != GR_TMU0 )
    {
        return GR_NULL_MIPMAP_HANDLE;
    }

    return UTextures.GetCurrentMipMap( tmu );
}

//----------------------------------------------------------------
DLLEXPORT FxBool __stdcall 
guTexChangeAttributes( GrMipMapId_t mmid,
                       int width, int height,
                       GrTextureFormat_t fmt,
                       GrMipMapMode_t mm_mode,
                       GrLOD_t smallest_lod, GrLOD_t largest_lod,
                       GrAspectRatio_t aspect,
                       GrTextureClampMode_t s_clamp_mode,
                       GrTextureClampMode_t t_clamp_mode,
                       GrTextureFilterMode_t minFilterMode,
                       GrTextureFilterMode_t magFilterMode )
{
#ifdef OGL_DONE
    GlideMsg( "guTexChangeAttributes( %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d )\n",
        mmid, width, height, fmt, mm_mode, smallest_lod, largest_lod, aspect,
        s_clamp_mode, t_clamp_mode, minFilterMode, magFilterMode );
#endif

    return UTextures.ChangeAttributes( mmid, width, height, fmt, mm_mode,
        smallest_lod, largest_lod, aspect, s_clamp_mode, t_clamp_mode, 
        minFilterMode, magFilterMode );
}

//----------------------------------------------------------------
DLLEXPORT GrMipMapInfo * __stdcall 
guTexGetMipMapInfo( GrMipMapId_t mmid )
{
#ifdef OGL_DONE
    GlideMsg( "guTexGetMipMapInfo( )\n" );
#endif

    return UTextures.GetMipMapInfo( mmid );
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall 
guTexMemReset( void )
{
#ifdef OGL_PARTDONE
    GlideMsg( "guTexMemReset( )\n" );
#endif

    UTextures.MemReset( );
    Textures->Clear( );
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall 
guTexDownloadMipMapLevel( GrMipMapId_t mmid, GrLOD_t lod, const void **src )
{
#ifdef OGL_DONE
    GlideMsg( "guTexDownloadMipMapLevel( %d, %d, --- )\n", mmid, lod );
#endif

    UTextures.DownloadMipMapLevel( mmid, lod, src );
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall 
guTexDownloadMipMap( GrMipMapId_t mmid, const void *src, const GuNccTable *table )
{
#ifdef OGL_DONE
    GlideMsg( "guTexDownloadMipMap\n" );
#endif

    UTextures.DownloadMipMap( mmid, src, table );
}

//----------------------------------------------------------------
DLLEXPORT GrMipMapId_t __stdcall 
guTexAllocateMemory( GrChipID_t tmu,
                     FxU8 odd_even_mask,
                     int width, int height,
                     GrTextureFormat_t fmt,
                     GrMipMapMode_t mm_mode,
                     GrLOD_t smallest_lod, GrLOD_t largest_lod,
                     GrAspectRatio_t aspect,
                     GrTextureClampMode_t s_clamp_mode,
                     GrTextureClampMode_t t_clamp_mode,
                     GrTextureFilterMode_t minfilter_mode,
                     GrTextureFilterMode_t magfilter_mode,
                     float lod_bias,
                     FxBool trilinear )
{
#ifdef OGL_DONE
    GlideMsg( "guTexAllocateMemory( %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d )\n",
        tmu, odd_even_mask, width, height, fmt, mm_mode, smallest_lod, largest_lod, aspect,
        s_clamp_mode, t_clamp_mode, minfilter_mode, magfilter_mode, lod_bias, trilinear );
#endif

    if ( tmu != GR_TMU0 )
    {
        return GR_NULL_MIPMAP_HANDLE;
    }

    return UTextures.AllocateMemory( tmu, odd_even_mask, width, height, fmt, mm_mode,
        smallest_lod, largest_lod, aspect, s_clamp_mode, t_clamp_mode,
        minfilter_mode, magfilter_mode, lod_bias, trilinear);
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall 
guTexSource( GrMipMapId_t id )
{
#ifdef OGL_DONE
    GlideMsg( "guTexSource( %d )\n", id );
#endif

    RenderDrawTriangles( );

    UTextures.Source( id );
}

//----------------------------------------------------------------
DLLEXPORT FxU16 * __stdcall
guTexCreateColorMipMap( void )
{
#ifdef OGL_NOTDONE
    GlideMsg( "guTexCreateColorMipMap( )\n" );
#endif

    return 0;
}

