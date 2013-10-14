//**************************************************************
//*            OpenGLide - Glide to OpenGL Wrapper
//*             http://openglide.sourceforge.net
//*
//*                         Main File
//*
//*         OpenGLide is OpenSource under LGPL license
//*              Originaly made by Fabio Barros
//*      Modified by Paul for Glidos (http://www.glidos.net)
//**************************************************************

#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <GL\gl.h>
#include <GL\glu.h>

#include "GlOgl.h"
#include "GLRender.h"
#include "GLextensions.h"
#include "PGTexture.h"
#include "PGUTexture.h"

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Version ///////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////


char *OpenGLideVersion = "Version0.08";


///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////


// Main Structs
GlideStruct     Glide;
OpenGLStruct    OpenGL;

// Classes
PGTexture       *Textures;
PGUTexture       UTextures;

// Profiling variables
__int64         InitialTick,FinalTick;
DWORD           Frame;
double          Fps, FpsAux, ClockFreq;

// Error Function variable
void (*ExternErrorFunction)( const char *string, FxBool fatal );

// Number of Errors
unsigned long NumberOfErrors;

// Other Functions
float ClockFrequency( void );
void GetOptions( void );
void InitialiseOpenGLWindow( HWND hwnd, int x, int y, UINT width, UINT height );
void FinaliseOpenGLWindow( void );
void PrepareTables( void );

// Support DLL functions

BOOL ClearAndGenerateLogFile( void )
{
    FILE    * GlideFile;
    char    tmpbuf[ 128 ];

    remove( ERRORFILE );
    GlideFile = fopen( GLIDEFILE, "w");
    if ( !GlideFile )
    {
        return FALSE;
    }

    fprintf( GlideFile, "-------------------------------------------\n" );
    fprintf( GlideFile, "OpenGLide Log File\n");
    fprintf( GlideFile, "-------------------------------------------\n" );
    fprintf( GlideFile, "***** OpenGLide %s *****\n", OpenGLideVersion );
    fprintf( GlideFile, "-------------------------------------------\n" );
    _strdate( tmpbuf );
    fprintf( GlideFile, "Date: %s\n", tmpbuf );
    _strtime( tmpbuf );
    fprintf( GlideFile, "Time: %s\n", tmpbuf );
    fprintf( GlideFile, "-------------------------------------------\n" );
    fprintf( GlideFile, "-------------------------------------------\n" );
    ClockFreq = ClockFrequency( );
    fprintf( GlideFile, "Clock Frequency: %-4.2f Mhz\n", ClockFreq / 1000000.0f);
    fprintf( GlideFile, "-------------------------------------------\n" );
    fprintf( GlideFile, "-------------------------------------------\n" );

    fclose( GlideFile );

    return true;
}

void CloseLogFile( void )
{
    char tmpbuf[128];
    GlideMsg( "-------------------------\n" );
    _strtime( tmpbuf );
    GlideMsg( "Time: %s\n", tmpbuf );
    GlideMsg( "-------------------------\n" );

#ifdef OGL_DEBUG
    Fps = (float) Frame * ClockFreq / FpsAux;
    GlideMsg( "FPS = %f\n", Fps );
    GlideMsg( "-------------------------\n" );
#endif
}

BOOL GenerateErrorFile( void )
{
    char    tmpbuf[ 128 ];
    FILE    * ErrorFile;

    ErrorFile = fopen( ERRORFILE, "w");
    if( !ErrorFile )
    {
        return FALSE;
    }

    fprintf( ErrorFile, "-------------------------------------------\n" );
    fprintf( ErrorFile, "OpenGLide Error File\n");
    fprintf( ErrorFile, "-------------------------------------------\n" );
    _strdate( tmpbuf );
    fprintf( ErrorFile, "Date: %s\n", tmpbuf );
    _strtime( tmpbuf );
    fprintf( ErrorFile, "Time: %s\n", tmpbuf );
    fprintf( ErrorFile, "-------------------------------------------\n" );
    fprintf( ErrorFile, "-------------------------------------------\n" );

    fclose( ErrorFile );

    return true;
}

void InitMainVariables( void )
{
    OpenGL.WinOpen = false;
    OpenGL.GlideInit = false;
    NumberOfErrors = 0;
    GetOptions( );
}

//*************************************************
//* Initializes the DLL
//*************************************************
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpvreserved )
{
    int Priority;

    switch ( dwReason )
    {
    case DLL_THREAD_ATTACH:
        break;

    case DLL_PROCESS_ATTACH:
        if ( !ClearAndGenerateLogFile( ) )
        {
            return false;
        }
        InitMainVariables( );

        if ( SetPriorityClass( GetCurrentProcess( ), NORMAL_PRIORITY_CLASS ) == 0 )
        {
            Error( "Could not set Class Priority.\n" );
        }
        else
        {
            GlideMsg( "-----------------------------------------\n" );
            GlideMsg( "Wrapper Class Priority of %d\n", NORMAL_PRIORITY_CLASS );
        }

        switch ( UserConfig.Priority )
        {
        case 0:
            Priority = THREAD_PRIORITY_HIGHEST;
            break;

        case 1:
            Priority = THREAD_PRIORITY_ABOVE_NORMAL;
            break;

        case 2:
            Priority = THREAD_PRIORITY_NORMAL;
            break;

        case 3:
            Priority = THREAD_PRIORITY_BELOW_NORMAL;
            break;

        case 4:
            Priority = THREAD_PRIORITY_LOWEST;
            break;

        case 5:
            Priority = THREAD_PRIORITY_IDLE;
            break;

        default:
            Priority = THREAD_PRIORITY_NORMAL;
            break;
        }
        if ( SetThreadPriority( GetCurrentThread(), Priority ) == 0 )
        {
            Error( "Could not set Thread Priority.\n" );
        }
        else
        {
            GlideMsg( "Wrapper Priority of %d\n", UserConfig.Priority );
            GlideMsg( "-----------------------------------------\n" );
        }
        break;

    case DLL_THREAD_DETACH:
        break;

    case DLL_PROCESS_DETACH:
        grGlideShutdown( );
        CloseLogFile( );
        break;
    }
    return TRUE;
}

bool InitWindow( HWND hwnd )
{
    InitialiseOpenGLWindow( hwnd, 0, 0,  OpenGL.WindowWidth, OpenGL.WindowHeight );

    if ( !strcmp( (char*)glGetString( GL_RENDERER ), "GDI Generic" ) )
    {
        MessageBox( NULL, "You are running in a Non-Accelerated OpenGL!!!\nThings can become really slow", "Warning", MB_OK );
    }

    ValidateUserConfig( );

    GlideMsg( "-------------------------------------------\n" );
    GlideMsg( " Setting in Use: \n" );
    GlideMsg( "-------------------------------------------\n" );
    GlideMsg( "Init Full Screen = %s\n", InternalConfig.InitFullScreen ? "true" : "false" );
    GlideMsg( "Fog = %s\n", InternalConfig.FogEnable ? "true" : "false" );
    GlideMsg( "Precision Fix = %s\n", InternalConfig.PrecisionFixEnable ? "true" : "false" );
    GlideMsg( "MultiTexture = %s\n", InternalConfig.MultiTextureEXTEnable ? "true" : "false" );
    GlideMsg( "Palette Extension = %s\n", InternalConfig.PaletteEXTEnable ? "true" : "false" );
    GlideMsg( "Vertex Array Extension = %s\n", InternalConfig.VertexArrayEXTEnable ? "true" : "false" );
    GlideMsg( "Fog Coord Extension = %s\n", InternalConfig.FogCoordEXTEnable ? "true" : "false" );
    GlideMsg( "Blend Function Separate = %s\n", InternalConfig.BlendFuncSeparateEXTEnable ? "true" : "false" );
    GlideMsg( "Texture LOD bias = %s\n", InternalConfig.TextureLodBiasEXTEnable ? "true" : "false" );
    GlideMsg( "Texture Memory Size = %d Mb\n", InternalConfig.TextureMemorySize );
    GlideMsg( "Frame Buffer Memory Size = %d Mb\n", InternalConfig.FrameBufferMemorySize );
    GlideMsg( "MMX is %s\n", InternalConfig.MMXEnable ? "enabled" : "disabled" );
    GlideMsg( "-------------------------------------------\n" );
    GlideMsg( "-------------------------------------------\n" );
    GlideMsg( "** OpenGL Information **\n" );
    GlideMsg( "-------------------------------------------\n" );
    GlideMsg( "Vendor:      %s\n", glGetString( GL_VENDOR ) );
    GlideMsg( "Renderer:    %s\n", glGetString( GL_RENDERER ) );
    GlideMsg( "Version:     %s\n", glGetString( GL_VERSION ) );
    GlideMsg( "Extensions:  %s\n", glGetString( GL_EXTENSIONS ) );

#ifdef OGL_DEBUG
    GlideMsg( "-------------------------------------------\n" );
    GlideMsg( "GlideState size = %d\n", sizeof( GlideState ) );
    GlideMsg( "GrState size = %d\n", sizeof( GrState ) );
    GlideMsg( "-------------------------------------------\n" );
#endif

    GlideMsg( "-------------------------------------------\n" );
    GlideMsg( "** Glide Calls **\n" );
    GlideMsg( "-------------------------------------------\n" );

    return true;
}

//*************************************************
//* Initializes OpenGL
//*************************************************
void InitOpenGL( void )
{
    OpenGL.ZNear = ZBUFFERNEAR;
    OpenGL.ZFar = ZBUFFERFAR;

    glMatrixMode( GL_PROJECTION );
    glLoadIdentity( );
    glOrtho( 0, Glide.WindowWidth, 0, Glide.WindowHeight, OpenGL.ZNear, OpenGL.ZFar );
    glViewport( 0, 0, OpenGL.WindowWidth, OpenGL.WindowHeight );

    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity( );

    glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
//  glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );
//  glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND );
//  glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD );
//  glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );

    glPixelStorei( GL_PACK_ALIGNMENT, 2);
    glPixelStorei( GL_UNPACK_ALIGNMENT, 2);
}

//*****************************************************************
//*****************************************************************
//*****************************************************************
//*****************************************************************
//*****************************************************************
//*****************************************************************
//******* Glide Functions
//*****************************************************************
//*****************************************************************
//*****************************************************************
//*****************************************************************
//*****************************************************************


//*************************************************
//* Returns the current Glide Version
//*************************************************
DLLEXPORT void __stdcall
grGlideGetVersion( char version[80] )
{
#ifdef OGL_DONE
    GlideMsg( "grGlideGetVersion( --- )\n" );
#endif
    strcpy( version, "Glide 2.43" );
}

//*************************************************
//* Initializes what is needed
//*************************************************
DLLEXPORT void __stdcall
grGlideInit( void )
{
#ifdef OGL_DONE
    GlideMsg( "grGlideInit( )\n" );
#endif
    if ( OpenGL.GlideInit )
    {
        grGlideShutdown();
    }

    ZeroMemory( &Glide, sizeof(GlideStruct) );
    ZeroMemory( &OpenGL, sizeof(OpenGLStruct) );

    Glide.ActiveVoodoo      = 0;
    Glide.State.VRetrace    = FXTRUE;

    ExternErrorFunction = NULL;

#ifdef OGL_DEBUG
    RDTSC( FinalTick );
    RDTSC( InitialTick );
    Fps = FpsAux = Frame = 0;
#endif

    OpenGL.GlideInit = true;

    RenderInitialize( );

    Glide.TextureMemory = UserConfig.TextureMemorySize * 1024 * 1024;

    Textures = new PGTexture( Glide.TextureMemory );
    if ( Textures == NULL )
    {
        Error( "Cannot allocate enough memory for Texture Buffer in User setting, using default" );
    }

    Glide.TexMemoryMaxPosition  = (FxU32)Glide.TextureMemory;
}

//*************************************************
//* Finishes everything
//*************************************************
DLLEXPORT void __stdcall
grGlideShutdown( void )
{
    if ( !OpenGL.GlideInit )
    {
        return;
    }

    OpenGL.GlideInit = false;

    RDTSC( FinalTick );

#ifdef OGL_DONE
    GlideMsg( "grGlideShutdown()\n" );
#endif

    grSstWinClose( );

    RenderFree( );
    delete Textures;
}

//*************************************************
//* Sets all Glide State Variables
//*************************************************
DLLEXPORT void __stdcall
grGlideSetState( const GrState *state )
{
#ifdef OGL_PARTDONE
    GlideMsg( "grGlideSetState( --- )\n" );
#endif
    GlideState StateTemp;

    CopyMemory( &StateTemp, state, sizeof( GlideState ) );

    Glide.State.ColorFormat = StateTemp.ColorFormat;

    grRenderBuffer( StateTemp.RenderBuffer );
    grDepthBufferMode( StateTemp.DepthBufferMode );
    grDepthBufferFunction( StateTemp.DepthFunction );
    grDepthMask( StateTemp.DepthBufferWritting );
    grDepthBiasLevel( StateTemp.DepthBiasLevel );
    grDitherMode( StateTemp.DitherMode );
    grChromakeyValue( StateTemp.ChromakeyValue );
    grChromakeyMode( StateTemp.ChromaKeyMode );
    grAlphaTestReferenceValue( StateTemp.AlphaReferenceValue );
    grAlphaTestFunction( StateTemp.AlphaFunction );
    grColorMask( StateTemp.ColorMask, StateTemp.AlphaMask );
    grConstantColorValue( StateTemp.ConstantColorValue );
    grFogColorValue( StateTemp.FogColorValue );
    grFogMode( StateTemp.FogMode );
    grCullMode( StateTemp.CullMode );
    grTexClampMode( GR_TMU0, StateTemp.SClampMode, StateTemp.TClampMode );
    grTexFilterMode( GR_TMU0, StateTemp.MinFilterMode, StateTemp.MagFilterMode );
    grTexMipMapMode( GR_TMU0, StateTemp.MipMapMode, StateTemp.LodBlend );
    grColorCombine( StateTemp.ColorCombineFunction, StateTemp.ColorCombineFactor, 
                    StateTemp.ColorCombineLocal, StateTemp.ColorCombineOther, StateTemp.ColorCombineInvert );
    grAlphaCombine( StateTemp.AlphaFunction, StateTemp.AlphaFactor, StateTemp.AlphaLocal, StateTemp.AlphaOther, StateTemp.AlphaInvert );
    grTexCombine( GR_TMU0, StateTemp.TextureCombineCFunction, StateTemp.TextureCombineCFactor,
                  StateTemp.TextureCombineAFunction, StateTemp.TextureCombineAFactor,
                  StateTemp.TextureCombineRGBInvert, StateTemp.TextureCombineAInvert );
    grAlphaBlendFunction( StateTemp.AlphaBlendRgbSf, StateTemp.AlphaBlendRgbDf, StateTemp.AlphaBlendAlphaSf, StateTemp.AlphaBlendAlphaDf );
    grClipWindow( StateTemp.ClipMinX, StateTemp.ClipMinY, StateTemp.ClipMaxX, StateTemp.ClipMaxY );
//  grSstOrigin( StateTemp.OriginInformation );
//  grTexSource( GR_TMU0, StateTemp.TexSource.StartAddress, StateTemp.TexSource.EvenOdd, &StateTemp.TexSource.Info );
}

//*************************************************
//* Gets all Glide State Variables
//*************************************************
DLLEXPORT void __stdcall
grGlideGetState( GrState *state )
{
#ifdef OGL_PARTDONE
    GlideMsg( "grGlideGetState( --- )\n" );
#endif

    CopyMemory( state, &Glide.State, sizeof( GlideState ) );
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall
grGlideShamelessPlug( const FxBool on )
{
#ifdef OGL_NOTDONE
    GlideMsg( "grGlideShamelessPlug( %d )\n", on );
#endif
}

//*************************************************
//* Returns the number of Voodoo Boards Instaled
//*************************************************
DLLEXPORT FxBool __stdcall
grSstQueryBoards( GrHwConfiguration *hwConfig )
{
#ifdef OGL_DONE
    GlideMsg( "grSstQueryBoards( --- )\n" );
#endif

    ZeroMemory( hwConfig, sizeof(GrHwConfiguration) );
    hwConfig->num_sst = 1;

    return FXTRUE;
}

//*************************************************
DLLEXPORT FxBool __stdcall
grSstWinOpen(   FxU32 hwnd,
                GrScreenResolution_t res,
                GrScreenRefresh_t ref,
                GrColorFormat_t cformat,
                GrOriginLocation_t org_loc,
                int num_buffers,
                int num_aux_buffers )
{
    if ( OpenGL.WinOpen )
    {
        grSstWinClose( );
    }

    Glide.Resolution = res;
    switch ( Glide.Resolution )
    {
    case GR_RESOLUTION_320x200:
        Glide.WindowWidth = 320;
        Glide.WindowHeight = 200;
        break;

    case GR_RESOLUTION_320x240:
        Glide.WindowWidth = 320;
        Glide.WindowHeight = 240;
        break;

    case GR_RESOLUTION_400x256:
        Glide.WindowWidth = 400;
        Glide.WindowHeight = 256;
        break;

    case GR_RESOLUTION_512x384:
        Glide.WindowWidth = 512;
        Glide.WindowHeight = 384;
        break;

    case GR_RESOLUTION_640x200:
        Glide.WindowWidth = 640;
        Glide.WindowHeight = 200;
        break;

    case GR_RESOLUTION_640x350:
        Glide.WindowWidth = 640;
        Glide.WindowHeight = 350;
        break;

    case GR_RESOLUTION_640x400:
        Glide.WindowWidth = 640;
        Glide.WindowHeight = 400;
        break;

    case GR_RESOLUTION_640x480:
        Glide.WindowWidth = 640;
        Glide.WindowHeight = 480;
        break;

    case GR_RESOLUTION_800x600:
        Glide.WindowWidth = 800;
        Glide.WindowHeight = 600;
        break;

    case GR_RESOLUTION_960x720:
        Glide.WindowWidth = 960;
        Glide.WindowHeight = 720;
        break;

    case GR_RESOLUTION_856x480:
        Glide.WindowWidth = 856;
        Glide.WindowHeight = 480;
        break;

    case GR_RESOLUTION_512x256:
        Glide.WindowWidth = 512;
        Glide.WindowHeight = 256;
        break;

    case GR_RESOLUTION_1024x768:
        Glide.WindowWidth = 1024;
        Glide.WindowHeight = 768;
        break;

    case GR_RESOLUTION_1280x1024:
        Glide.WindowWidth = 1280;
        Glide.WindowHeight = 1024;
        break;

    case GR_RESOLUTION_1600x1200:
        Glide.WindowWidth = 1600;
        Glide.WindowHeight = 1200;
        break;

    case GR_RESOLUTION_400x300:
        Glide.WindowWidth = 400;
        Glide.WindowHeight = 300;
        break;

    case GR_RESOLUTION_NONE:
        Error( "grSstWinOpen: res = GR_RESOLUTION_NONE\n" );
        return FXFALSE;

    default:
        Error( "grSstWinOpen: Resolution Incorrect\n" );
        return FXFALSE;
    }
    OpenGL.WindowWidth = Glide.WindowWidth;
    OpenGL.WindowHeight = Glide.WindowHeight;

    Glide.Refresh = ref;
    switch ( Glide.Refresh )
    {
    case GR_REFRESH_60Hz:   OpenGL.Refresh = 60;    break;
    case GR_REFRESH_70Hz:   OpenGL.Refresh = 70;    break;
    case GR_REFRESH_72Hz:   OpenGL.Refresh = 72;    break;
    case GR_REFRESH_75Hz:   OpenGL.Refresh = 75;    break;
    case GR_REFRESH_80Hz:   OpenGL.Refresh = 80;    break;
    case GR_REFRESH_90Hz:   OpenGL.Refresh = 90;    break;
    case GR_REFRESH_100Hz:  OpenGL.Refresh = 100;   break;
    case GR_REFRESH_85Hz:   OpenGL.Refresh = 85;    break;
    case GR_REFRESH_120Hz:  OpenGL.Refresh = 120;   break;
    case GR_REFRESH_NONE:   OpenGL.Refresh = 60;    break;
    default:
        Error( "grSstWinOpen: Refresh Incorrect\n" );
        return FXFALSE;
    }
    OpenGL.WaitSignal = (DWORD)( 1000 / OpenGL.Refresh );

    // Initing OpenGL Window
    if ( !InitWindow( (HWND)hwnd ) )
    {
        return FXFALSE;
    }

    Glide.State.ColorFormat = cformat;
    Glide.NumBuffers        = num_buffers;
    Glide.AuxBuffers        = num_aux_buffers;

#ifdef OGL_DONE
    GlideMsg( "grSstWinOpen( %d, %d, %d, %d, %d, %d, %d )\n",
        hwnd, res, ref, cformat, org_loc, num_buffers, num_aux_buffers );
#endif

    // Initializing Glide and OpenGL
    InitOpenGL( );

    Glide.SrcBuffer.Address = new WORD[ OPENGLBUFFERMEMORY * 2 ];
    Glide.DstBuffer.Address = new WORD[ OPENGLBUFFERMEMORY * 2 ];
    
    // Just checking
    if ( ( !Glide.SrcBuffer.Address ) || ( !Glide.DstBuffer.Address ) )
    {
        Error( "Could NOT allocate sufficient memory for Buffers... Sorry\n" );
        exit( -1 );
    }

    ZeroMemory( Glide.SrcBuffer.Address, OPENGLBUFFERMEMORY * 2 );
    ZeroMemory( Glide.DstBuffer.Address, OPENGLBUFFERMEMORY * 2 );

#ifdef OGL_DONE
    GlideMsg( "----Start of grSstWinOpen()\n" );
#endif
    // All should be disabled
    //depth buffering, fog, chroma-key, alpha blending, alpha testing
    grSstOrigin( org_loc );
    grTexClampMode( 0, GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_WRAP );
    grTexMipMapMode( 0, GR_MIPMAP_DISABLE, FXFALSE );
    grTexFilterMode( 0, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR );
    grChromakeyMode( GR_CHROMAKEY_DISABLE );
    grFogMode( GR_FOG_DISABLE );
    grCullMode( GR_CULL_DISABLE );
    grRenderBuffer( GR_BUFFER_BACKBUFFER );
    grAlphaTestFunction( GR_CMP_ALWAYS );
    grDitherMode( GR_DITHER_4x4 );
    grColorCombine( GR_COMBINE_FUNCTION_SCALE_OTHER,
                    GR_COMBINE_FACTOR_ONE,
                    GR_COMBINE_LOCAL_ITERATED,
                    GR_COMBINE_OTHER_ITERATED,
                    FXFALSE );
    grAlphaCombine( GR_COMBINE_FUNCTION_SCALE_OTHER,
                    GR_COMBINE_FACTOR_ONE,
                    GR_COMBINE_LOCAL_NONE,
                    GR_COMBINE_OTHER_CONSTANT,
                    FXFALSE );
    grTexCombine( GR_TMU0,GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE,
                GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, FXFALSE, FXFALSE );
    grAlphaControlsITRGBLighting( FXFALSE );
    grAlphaBlendFunction( GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO );
    grColorMask( FXTRUE, FXFALSE );
    grDepthMask( FXFALSE );
    grDepthBufferMode( GR_DEPTHBUFFER_DISABLE );
    grDepthBufferFunction( GR_CMP_LESS );

    //(chroma-key value, alpha test reference, constant depth value,
    //constant alpha value, etc.) and pixel rendering statistic counters 
    //are initialized to 0x00.
    grChromakeyValue( 0x00 );
    grAlphaTestReferenceValue( 0x00 );
    grDepthBiasLevel( 0x00 );
    grFogColorValue( 0x00 );
    grConstantColorValue( 0xFFFFFFFF );
    grClipWindow( 0, 0, Glide.WindowWidth, Glide.WindowHeight );
//  grGammaCorrectionValue( 1.6f );
    grHints(GR_HINT_STWHINT, 0);

#ifdef OGL_DONE
    GlideMsg( "----End of grSstWinOpen()\n" );
#endif

    OpenGL.WinOpen = true;

#ifdef OPENGL_DEBUG
    GLErro( "grSstWinOpen" );
#endif

    glFinish( );

    return FXTRUE;
}

//*************************************************
//* Close the graphics display device
//*************************************************
DLLEXPORT void __stdcall
grSstWinClose( void )
{
#ifdef OGL_DONE
    GlideMsg( "grSstWinClose()\n" );
#endif
    if ( !OpenGL.WinOpen )
    {
        return;
    }

    OpenGL.WinOpen = false;

#ifdef OGL_DEBUG
    GlideMsg( "-----------------------------------------------------\n" );
    GlideMsg( "** Debug Information **\n" );
    GlideMsg( "-----------------------------------------------------\n" );
    GlideMsg( "MaxTriangles in Frame = %d\n", OGLRender.MaxTriangles );
    GlideMsg( "MaxTriangles in Sequence = %d\n", OGLRender.MaxSequencedTriangles );
    GlideMsg( "MaxZ = %f\nMinZ = %f\n", OGLRender.MaxZ, OGLRender.MinZ );
    GlideMsg( "MaxX = %f\nMinX = %f\nMaxY = %f\nMinY = %f\n", 
              OGLRender.MaxX, OGLRender.MinX, OGLRender.MaxY, OGLRender.MinY );
    GlideMsg( "MaxS = %f\nMinS = %f\nMaxT = %f\nMinT = %f\n", 
              OGLRender.MaxS, OGLRender.MinS, OGLRender.MaxT, OGLRender.MinT );
    GlideMsg( "MaxF = %f\nMinF = %f\n", OGLRender.MaxF, OGLRender.MinF );
    GlideMsg( "MaxR = %f\nMinR = %f\n", OGLRender.MaxR, OGLRender.MinR );
    GlideMsg( "MaxG = %f\nMinG = %f\n", OGLRender.MaxG, OGLRender.MinG );
    GlideMsg( "MaxB = %f\nMinB = %f\n", OGLRender.MaxB, OGLRender.MinR );
    GlideMsg( "MaxA = %f\nMinA = %f\n", OGLRender.MaxA, OGLRender.MinA );
    GlideMsg( "-----------------------------------------------------\n" );
    GlideMsg( "Texture Information:\n" );
    GlideMsg( "  565 = %d\n", Textures->Num_565_Tex );
    GlideMsg( " 1555 = %d\n", Textures->Num_1555_Tex );
    GlideMsg( " 4444 = %d\n", Textures->Num_4444_Tex );
    GlideMsg( "  332 = %d\n", Textures->Num_332_Tex );
    GlideMsg( " 8332 = %d\n", Textures->Num_8332_Tex );
    GlideMsg( "Alpha = %d\n", Textures->Num_Alpha_Tex );
    GlideMsg( " AI88 = %d\n", Textures->Num_AlphaIntensity88_Tex );
    GlideMsg( " AI44 = %d\n", Textures->Num_AlphaIntensity44_Tex );
    GlideMsg( " AP88 = %d\n", Textures->Num_AlphaPalette_Tex );
    GlideMsg( "   P8 = %d\n", Textures->Num_Palette_Tex );
    GlideMsg( "Inten = %d\n", Textures->Num_Intensity_Tex );
    GlideMsg( "  YIQ = %d\n", Textures->Num_YIQ_Tex );
    GlideMsg( " AYIQ = %d\n", Textures->Num_AYIQ_Tex );
    GlideMsg( "Other = %d\n", Textures->Num_Other_Tex );
    GlideMsg( "-----------------------------------------------------\n" );
    GlideMsg( "-----------------------------------------------------\n" );
#endif

    Textures->Clear( );

    FinaliseOpenGLWindow( );

    delete[] Glide.SrcBuffer.Address;
    delete[] Glide.DstBuffer.Address;
}

//*************************************************
//* Returns the Hardware Configuration
//*************************************************
DLLEXPORT FxBool __stdcall
grSstQueryHardware( GrHwConfiguration *hwconfig )
{
#ifdef OGL_DONE
    GlideMsg( "grSstQueryHardware( --- )\n" );
#endif

    hwconfig->num_sst = 1;
    hwconfig->SSTs[0].type = GR_SSTTYPE_VOODOO;
//  hwconfig->SSTs[0].type = GR_SSTTYPE_Voodoo2;
    hwconfig->SSTs[0].sstBoard.VoodooConfig.fbRam = UserConfig.FrameBufferMemorySize;
    hwconfig->SSTs[0].sstBoard.VoodooConfig.fbiRev = 2;
    hwconfig->SSTs[0].sstBoard.VoodooConfig.nTexelfx = 1;
//  hwconfig->SSTs[0].sstBoard.VoodooConfig.nTexelfx = 2;
    hwconfig->SSTs[0].sstBoard.VoodooConfig.sliDetect = FXFALSE;
    hwconfig->SSTs[0].sstBoard.VoodooConfig.tmuConfig[0].tmuRev = 1;
    hwconfig->SSTs[0].sstBoard.VoodooConfig.tmuConfig[0].tmuRam = UserConfig.TextureMemorySize;

    return FXTRUE;
}

//*************************************************
//* Selects which Voodoo Board is Active
//*************************************************
DLLEXPORT void __stdcall
grSstSelect( int which_sst )
{
#ifdef OGL_DONE
    GlideMsg( "grSstSelect( %d )\n", which_sst );
#endif
    // Nothing Needed Here but...
    Glide.ActiveVoodoo = which_sst;
}

//*************************************************
//* Returns the Screen Height
//*************************************************
DLLEXPORT FxU32 __stdcall
grSstScreenHeight( void )
{
#ifdef OGL_DONE
    GlideMsg( "grSstScreenHeight()\n" );
#endif

    return Glide.WindowHeight;
}

//*************************************************
//* Returns the Screen Width
//*************************************************
DLLEXPORT FxU32 __stdcall
grSstScreenWidth( void )
{
#ifdef OGL_DONE
    GlideMsg( "grSstScreenWidth()\n" );
#endif

    return Glide.WindowWidth;
}

//*************************************************
//* Sets the Y Origin
//*************************************************
DLLEXPORT void __stdcall
grSstOrigin( GrOriginLocation_t  origin )
{
#ifdef OGL_DONE
    GlideMsg( "grSstSetOrigin( %d )\n", origin );
#endif

    RenderDrawTriangles( );

    Glide.State.OriginInformation = origin;

    switch ( origin )
    {
    case GR_ORIGIN_LOWER_LEFT:
        glMatrixMode( GL_PROJECTION );
        glLoadIdentity( );
        glOrtho( 0, Glide.WindowWidth, 0, Glide.WindowHeight, OpenGL.ZNear, OpenGL.ZFar );
        glViewport( 0, 0, OpenGL.WindowWidth, OpenGL.WindowHeight );
        glMatrixMode( GL_MODELVIEW );
        break;

    case GR_ORIGIN_UPPER_LEFT:
        glMatrixMode( GL_PROJECTION );
        glLoadIdentity( );
        glOrtho( 0, Glide.WindowWidth, Glide.WindowHeight, 0, OpenGL.ZNear, OpenGL.ZFar );
        glViewport( 0, 0, OpenGL.WindowWidth, OpenGL.WindowHeight );
        glMatrixMode( GL_MODELVIEW );
        break;
    }
    grCullMode( Glide.State.CullMode );

#ifdef OPENGL_DEBUG
    GLErro( "grSstOrigin" );
#endif
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall
grSstPerfStats( GrSstPerfStats_t * pStats )
{
#ifdef OGL_NOTDONE
    GlideMsg( "grSstPerfStats\n" );
#endif
}

//----------------------------------------------------------------
DLLEXPORT void __stdcall
grSstResetPerfStats( void )
{
#ifdef OGL_NOTDONE
    GlideMsg( "grSstResetPerfStats( )\n" );
#endif
}

//----------------------------------------------------------------
DLLEXPORT FxU32 __stdcall 
grSstVideoLine( void )
{
#ifdef OGL_NOTDONE
    GlideMsg( "grSstVideoLine( )\n" );
#endif

    return 0;
}

//----------------------------------------------------------------
DLLEXPORT FxBool __stdcall 
grSstVRetraceOn( void )
{
#ifdef OGL_NOTDONE
    GlideMsg( "grSstVRetraceOn( )\n" );
#endif

    return Glide.State.VRetrace;
}

//----------------------------------------------------------------
DLLEXPORT FxBool __stdcall 
grSstIsBusy( void )
{ 
#ifdef OGL_NOTDONE
    GlideMsg( "grSstIsBusy( )\n" ); 
#endif

    return FXFALSE; 
}

//----------------------------------------------------------------
DLLEXPORT FxBool __stdcall
grSstControl( FxU32 code )
{ 
#ifdef OGL_NOTDONE
    GlideMsg( "grSstControl( %lu )\n", code ); 
#endif

    return GR_CONTROL_ACTIVATE; 
}

//----------------------------------------------------------------
DLLEXPORT FxBool __stdcall
grSstControlMode( GrControl_t mode )
{ 
#ifdef OGL_NOTDONE
    GlideMsg( "grSstControlMode( %d )\n", mode );
#endif

    switch ( mode )
    {
    case GR_CONTROL_ACTIVATE:
        break;
    case GR_CONTROL_DEACTIVATE:
        break;
    case GR_CONTROL_RESIZE:
    case GR_CONTROL_MOVE:
        break;
    }

    return FXTRUE; 
}

//*************************************************
//* Return the Value of the graphics status register
//*************************************************
DLLEXPORT FxU32 __stdcall 
grSstStatus( void )
{
#ifdef OGL_PARTDONE
    GlideMsg( "grSstStatus( )\n" );
#endif

//    FxU32 Status = 0x0FFFF43F;
    FxU32 Status = 0x0FFFF03F;
    
    // Vertical Retrace
    Status      |= ( ! Glide.State.VRetrace ) << 6;

    return Status;
// Bits
// 5:0      PCI FIFO free space (0x3F = free)
// 6        Vertical Retrace ( 0 = active, 1 = inactive )
// 7        PixelFx engine busy ( 0 = engine idle )
// 8        TMU busy ( 0 = engine idle )
// 9        Voodoo Graphics busy ( 0 = idle )
// 11:10    Displayed buffer ( 0 = buffer 0, 1 = buffer 1, 2 = auxiliary buffer, 3 = reserved )
// 27:12    Memory FIFO ( 0xFFFF = FIFO empty )
// 30:28    Number of swap buffers commands pending
// 31       PCI interrupt generated ( not implemented )
}

//*************************************************
//* Returns when Glides is Idle
//*************************************************
DLLEXPORT void __stdcall
grSstIdle( void )
{
#ifdef OGL_DONE
    GlideMsg( "grSetIdle()\n" );
#endif

    RenderDrawTriangles( );
    glFlush( );
    glFinish( );

#ifdef OPENGL_DEBUG
    GLErro( "grSstIdle" );
#endif
}

