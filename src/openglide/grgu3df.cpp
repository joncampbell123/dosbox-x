//**************************************************************
//*            OpenGLide - Glide to OpenGL Wrapper
//*             http://openglide.sourceforge.net
//*
//*                     3DF functions
//*
//*         OpenGLide is OpenSource under LGPL license
//*              Originaly made by Fabio Barros
//*      Modified by Paul for Glidos (http://www.glidos.net)
//**************************************************************

#include <string.h>
#include <stdio.h>

#include "glogl.h"

// extern variables and functions
DWORD GetTexSize( const int Lod, const int aspectRatio, const int format );

// prototypes
int Read3dfHeader( const char *filename, Gu3dfInfo *data );

//----------------------------------------------------------------
DLLEXPORT FxBool __stdcall
gu3dfGetInfo( const char *filename, Gu3dfInfo *info )
{
#ifdef OGL_PARTDONE
    GlideMsg( "gu3dfGetInfo( %s, --- )\n", filename );
#endif

    if ( Read3dfHeader( filename, info ) )
    {
#ifdef OGL_DEBUG
        GlideMsg( "==========================================\n" );
        GlideMsg( "3df file Header: %s\n", filename );
        GlideMsg( "Texture Format = %d\n", info->header.format );
        GlideMsg( "Aspect = %d\n", info->header.aspect_ratio );
        GlideMsg( "LargeLod = %d\nSmall Lod = %d\n", info->header.large_lod, info->header.small_lod );
        GlideMsg( "Width = %d		Height = %d\n", info->header.width, info->header.height );
        GlideMsg( "MemRequired = %d\n", info->mem_required );
        GlideMsg( "==========================================\n" );
#endif
        return FXTRUE;
    }
    else
    {
        return FXFALSE;
    }
}


static FxU32 ReadDataLong( FILE *fp )
{
    FxU32   data;
    FxU8    byte[4];

    fread( byte, 4, 1, fp );
    data = (((FxU32) byte[ 0 ]) << 24) |
           (((FxU32) byte[ 1 ]) << 16) |
           (((FxU32) byte[ 2 ]) <<  8) |
            ((FxU32) byte[ 3 ]);

    return data;
}

static FxU32 ReadDataShort( FILE *fp )
{
    FxU32   data;
    FxU8    byte[2];

    fread( byte, 2, 1, fp );
    data = (((FxU32) byte[ 0 ]) << 8) |
            ((FxU32) byte[ 1 ]);

    return data;
}

//----------------------------------------------------------------
DLLEXPORT FxBool __stdcall
gu3dfLoad( const char *filename, Gu3dfInfo *data )
{
#ifdef OGL_PARTDONE
    GlideMsg( "gu3dfLoad( %s, --- )\n", filename );
#endif

    FILE    * file3df;
    int     jump = Read3dfHeader( filename, data );

#ifdef OGL_DEBUG
    GlideMsg( "Start of Data (Offset) = %d\n", jump );
    GlideMsg( "Total Bytes to be Read = %d\n", data->mem_required );
#endif

    file3df = fopen( filename, "rb" );

    fseek( file3df, jump, SEEK_SET );

    if ( ( data->header.format == GR_TEXFMT_P_8 ) ||
         ( data->header.format == GR_TEXFMT_AP_88 ) )
    {
        for( int i = 0; i < 256; i++ )
        {
            data->table.palette.data[i] = ReadDataLong( file3df );
        }
#ifdef OGL_DEBUG
        GlideMsg( "Reading Palette\n" );
#endif
    }

    if ( ( data->header.format == GR_TEXFMT_YIQ_422 ) ||
         ( data->header.format == GR_TEXFMT_AYIQ_8422 ) )
    {
        int   i;
        int   pi;
        FxU32 pack;

        GuNccTable *ncc = &(data->table.nccTable);

        for ( i = 0; i < 16; i++ )
        {
            ncc->yRGB[i] = (FxU8) ReadDataShort( file3df );
        }

        for ( i = 0; i < 4; i++ )
        {
            /* Masking with 0x1ff is strange but correct apparently */
            ncc->iRGB[i][0] = (FxI16) ( ReadDataShort( file3df ) & 0x1ff );
            ncc->iRGB[i][1] = (FxI16) ( ReadDataShort( file3df ) & 0x1ff );
            ncc->iRGB[i][2] = (FxI16) ( ReadDataShort( file3df ) & 0x1ff );
        }

        for ( i = 0; i < 4; i++ )
        {
            ncc->qRGB[i][0] = (FxI16) ( ReadDataShort( file3df ) & 0x1ff );
            ncc->qRGB[i][1] = (FxI16) ( ReadDataShort( file3df ) & 0x1ff );
            ncc->qRGB[i][2] = (FxI16) ( ReadDataShort( file3df ) & 0x1ff );
        }

        pi = 0;

        for ( i = 0; i < 4; i++ )
        {
            pack =  ( ncc->yRGB[i*4 + 0]       );
            pack |= ( ncc->yRGB[i*4 + 1] << 8  );
            pack |= ( ncc->yRGB[i*4 + 2] << 16 );
            pack |= ( ncc->yRGB[i*4 + 3] << 24 );

            ncc->packed_data[pi++] = pack;
        }

        for ( i = 0; i < 4; i++ )
        {
            pack =  ( ncc->iRGB[i][0] << 18 );
            pack |= ( ncc->iRGB[i][1] << 9  );
            pack |= ( ncc->iRGB[i][2]       );

            ncc->packed_data[pi++] = pack;
        }

        for ( i = 0; i < 4; i++ )
        {
            pack =  ( ncc->qRGB[i][0] << 18 );
            pack |= ( ncc->qRGB[i][1] << 9  );
            pack |= ( ncc->qRGB[i][2]       );

            ncc->packed_data[pi++] = pack;
        }
    }

    switch ( data->header.format )
    {
    case GR_TEXFMT_RGB_565:
    case GR_TEXFMT_ARGB_8332:
    case GR_TEXFMT_ARGB_1555:
    case GR_TEXFMT_AYIQ_8422:
    case GR_TEXFMT_ARGB_4444:
    case GR_TEXFMT_ALPHA_INTENSITY_88:
    case GR_TEXFMT_AP_88:
        {
            FxU16 *d = (FxU16 *) (data->data);
            int i;

            for ( i = data->mem_required; i > 0; i -= 2 )
            {
                *d++ = (FxU16) ReadDataShort( file3df );
            }
        }
        break;
        
    default:
        fread( data->data, sizeof( BYTE ), data->mem_required, file3df );
        break;
    }

    fclose( file3df );

    return FXTRUE;
}

GrTextureFormat_t ParseTextureFormat( const char * text )
{
    if ( !strcmp( text, "argb1555\n" ) )
    {
        return GR_TEXFMT_ARGB_1555;
    }
    if ( !strcmp( text, "argb4444\n" ) )
    {
        return GR_TEXFMT_ARGB_4444;
    }
    if ( !strcmp( text, "rgb565\n" ) )
    {
        return GR_TEXFMT_RGB_565;
    }
    if ( !strcmp( text, "rgb332\n" ) )
    {
        return GR_TEXFMT_RGB_332;
    }
    if ( !strcmp( text, "argb8332\n" ) )
    {
        return GR_TEXFMT_ARGB_8332;
    }
    if ( !strcmp( text, "p8\n" ) )
    {
        return GR_TEXFMT_P_8;
    }
    if ( !strcmp( text, "ap88\n" ) )
    {
        return GR_TEXFMT_AP_88;
    }
    if ( !strcmp( text, "ai44\n" ) )
    {
        return GR_TEXFMT_ALPHA_INTENSITY_44;
    }
    if ( !strcmp( text, "yiq\n" ) )
    {
        return GR_TEXFMT_YIQ_422;
    }
    if ( !strcmp( text, "ayiq8422\n" ) )
    {
        return GR_TEXFMT_AYIQ_8422;
    }

    return 0;
}

int ParseLod( int Lod )
{
    switch( Lod )
    {
    case 256:   return GR_LOD_256;
    case 128:   return GR_LOD_128;
    case 64:    return GR_LOD_64;
    case 32:    return GR_LOD_32;
    case 16:    return GR_LOD_16;
    case 8:     return GR_LOD_8;
    case 4:     return GR_LOD_4;
    case 2:     return GR_LOD_2;
    case 1:     return GR_LOD_1;
    }

    return -1;
}

GrAspectRatio_t ParseAspect( int h, int v )
{
    switch ( h )
    {
    case 8: return GR_ASPECT_8x1;
    case 4: return GR_ASPECT_4x1;
    case 2: return GR_ASPECT_2x1;
    case 1:
        switch ( v )
        {
        case 8: return GR_ASPECT_1x8;
        case 4: return GR_ASPECT_1x4;
        case 2: return GR_ASPECT_1x2;
        case 1: return GR_ASPECT_1x1;
        }
    }

    return 0;
}

int Read3dfHeader( const char *filename, Gu3dfInfo *data )
{
    FILE    * file3df;
    char    buffer[255];
    int     temp1, 
            temp2, 
            lod1, 
            lod2, 
            nWidth, 
            nHeight;

    file3df = fopen( filename, "rb" );

    if ( file3df == NULL )
    {
        return 0;
    }

    fgets( buffer, 255, file3df );
    fgets( buffer, 255, file3df );

    data->header.format = ParseTextureFormat( buffer );

    fgets( buffer, 255, file3df );
    sscanf( buffer, "lod range: %d %d\n", &lod1, &lod2 );

    data->header.small_lod = ParseLod( lod1 );
    data->header.large_lod = ParseLod( lod2 );

    fgets( buffer, 255, file3df );
    sscanf( buffer, "aspect ratio: %d %d\n", &temp1, &temp2 );

    data->header.aspect_ratio = ParseAspect( temp1, temp2 );

    switch ( data->header.aspect_ratio )
    {
    case GR_ASPECT_8x1: nWidth = lod2;      nHeight = lod2 >> 3;    break;
    case GR_ASPECT_4x1: nWidth = lod2;      nHeight = lod2 >> 2;    break;
    case GR_ASPECT_2x1: nWidth = lod2;      nHeight = lod2 >> 1;    break;
    case GR_ASPECT_1x1: nWidth = lod2;      nHeight = lod2;         break;
    case GR_ASPECT_1x2: nWidth = lod2 >> 1; nHeight = lod2;         break;
    case GR_ASPECT_1x4: nWidth = lod2 >> 2; nHeight = lod2;         break;
    case GR_ASPECT_1x8: nWidth = lod2 >> 3; nHeight = lod2;         break;
    }

    data->header.width = nWidth;
    data->header.height = nHeight;

    {
        GrLOD_t l;

        data->mem_required = 0;
        
        for ( l = data->header.large_lod; l <= data->header.small_lod; l++ )
            data->mem_required += GetTexSize( l, data->header.aspect_ratio, data->header.format );
    }

    temp1 = ftell( file3df );
    fclose( file3df );

    return temp1;
}
