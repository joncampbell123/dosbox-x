//**************************************************************
//*            OpenGLide - Glide to OpenGL Wrapper
//*             http://openglide.sourceforge.net
//*
//*                      Main Header
//*
//*         OpenGLide is OpenSource under LGPL license
//*              Originaly made by Fabio Barros
//*      Modified by Paul for Glidos (http://www.glidos.net)
//**************************************************************

#ifndef __GLOGL_H__
#define __GLOGL_H__

#define __WIN32__

//#define OGL_ALL
//#define OGL_PARTDONE
//#define OGL_NOTDONE
#define OGL_DEBUG
//#define OGL_CRITICAL
#define OPENGL_DEBUG
//#define OGL_PROFILING
//#define OGL_UTEX

#ifdef OGL_ALL
 #define OGL_DONE
 #define OGL_PARTDONE
 #define OGL_NOTDONE
 #define OGL_DEBUG
 #define OGL_CRITICAL
 #define OPENGL_DEBUG
 #define OGL_PROFILING
 #define OGL_UTEX
#endif

#include <windows.h>
#include <GL\gl.h>
#include <GL\glu.h>

#include "sdk2_glide.h"

#define RDTSC(v)    __asm _emit 0x0f                \
                    __asm _emit 0x31                \
                    __asm mov dword ptr v, eax      \
                    __asm mov dword ptr v+4, edx

#define ERRORFILE               "OpenGLid.ERR"
#define GLIDEFILE               "OpenGLid.LOG"
#define INIFILE                 "OpenGLid.INI"

#define OPENGLBUFFERMEMORY      OpenGL.WindowWidth * OpenGL.WindowHeight

#define OPENGLFOGTABLESIZE      64 * 1024

#define D1OVER255               0.003921568627451f      // 1 / 255
#define D1OVER65536             0.0000152587890625f     // 1 / 65536
#define D1OVER65535             0.000015259021896696421759365224689097f     // 1 / 65535
#define D1OVER256               0.00390625f             // 1 / 256
#define D2OVER256               0.0078125f              // 2 / 256
#define D4OVER256               0.015625f               // 4 / 256
#define D8OVER256               0.03125f                // 8 / 256

#define WBUFFERNEAR             -1.0f
#define WBUFFERFAR               0.0f
#define ZBUFFERNEAR              0.0f
#define ZBUFFERFAR              -1.0f

// Class declarations

struct BufferStruct
{
    bool                    Lock;
    GrLock_t                Type;
    GrLfbWriteMode_t        WriteMode;
    GrBuffer_t              Buffer;
    WORD                    *Address;
};

struct TexSourceStruct
{
    FxU32       StartAddress;
    FxU32       EvenOdd;
    GrTexInfo   Info;
};

struct GlideState
{
    GrBuffer_t              RenderBuffer;
    GrDepthBufferMode_t     DepthBufferMode;
    GrCmpFnc_t              DepthFunction;
    FxBool                  DepthBufferWritting;
    FxI16                   DepthBiasLevel;
    GrDitherMode_t          DitherMode;
    GrColor_t               ChromakeyValue;
    GrChromakeyMode_t       ChromaKeyMode;
    GrAlpha_t               AlphaReferenceValue;
    GrCmpFnc_t              AlphaTestFunction;
    FxBool                  AlphaMask;
    FxBool                  ColorMask;
    GrColor_t               ConstantColorValue;
    GrColor_t               FogColorValue;
    GrFogMode_t             FogMode;
    GrCullMode_t            CullMode;
    GrTextureClampMode_t    SClampMode;
    GrTextureClampMode_t    TClampMode;
    GrTextureFilterMode_t   MinFilterMode;
    GrTextureFilterMode_t   MagFilterMode;
    GrMipMapMode_t          MipMapMode;
    FxBool                  LodBlend;
    GrCombineFunction_t     ColorCombineFunction;
    GrCombineFactor_t       ColorCombineFactor;
    GrCombineLocal_t        ColorCombineLocal;
    GrCombineOther_t        ColorCombineOther;
    FxBool                  ColorCombineInvert;
    GrCombineFunction_t     AlphaFunction;
    GrCombineFactor_t       AlphaFactor;
    GrCombineLocal_t        AlphaLocal;
    GrCombineOther_t        AlphaOther;
    FxBool                  AlphaInvert;
    GrCombineFunction_t     TextureCombineCFunction;
    GrCombineFactor_t       TextureCombineCFactor;
    GrCombineFunction_t     TextureCombineAFunction;
    GrCombineFactor_t       TextureCombineAFactor;
    FxBool                  TextureCombineRGBInvert;
    FxBool                  TextureCombineAInvert;
    GrOriginLocation_t      OriginInformation;
    TexSourceStruct         TexSource;
    GrAlphaBlendFnc_t       AlphaBlendRgbSf;
    GrAlphaBlendFnc_t       AlphaBlendRgbDf;
    GrAlphaBlendFnc_t       AlphaBlendAlphaSf;
    GrAlphaBlendFnc_t       AlphaBlendAlphaDf;
    FxU32                   ClipMinX;
    FxU32                   ClipMaxX;
    FxU32                   ClipMinY;
    FxU32                   ClipMaxY;
    GrColorFormat_t         ColorFormat;
    FxU32                   STWHint;
    FxBool                  VRetrace;
};

struct GlideStruct
{
    int                     ActiveVoodoo;
    // Frame Buffer Stuff
    int                     WindowWidth;
    int                     WindowHeight;
    GrScreenResolution_t    Resolution;
    GrScreenRefresh_t       Refresh;
    int                     NumBuffers,
                            AuxBuffers;
    // States and Constants
    FxU8                    FogTable[GR_FOG_TABLE_SIZE];
//  GuTexPalette            ColorPalette;
    FxU32                   TexMemoryMaxPosition;
    bool                    CLocal, COther;
    bool                    ALocal, AOther;
    GlideState              State;
    BufferStruct            SrcBuffer;
    BufferStruct            DstBuffer;
    DWORD                   TextureMemory;
};

struct OpenGLStruct
{
    bool                    GlideInit;
    bool                    WinOpen;
    long                    WindowWidth, WindowHeight;
    GLfloat                 Gamma;
    GLfloat                 AlphaReferenceValue;
    GLenum                  AlphaTestFunction;
    GLboolean               DepthBufferWritting;
    GLfloat                 DepthBiasLevel;
    GLenum                  DepthFunction;
    GLenum                  RenderBuffer;
    GLenum                  SClampMode;
    GLenum                  TClampMode;
    GLenum                  MinFilterMode;
    GLenum                  MagFilterMode;
    GLenum                  TextureMode;
    GLenum                  SrcBlend;
    GLenum                  DstBlend;
    GLenum                  SrcAlphaBlend;
    GLenum                  DstAlphaBlend;
    GLuint                  Refresh;
    GLboolean               ColorMask;
    GLfloat                 ConstantColor[4];
    GLfloat                 AlphaColor[4];///////////////
    GLfloat                 ZNear, ZFar;
    GLfloat                 FogColor[4];
    BYTE                    FogTable[OPENGLFOGTABLESIZE];
    BYTE                    ChromaColor[4];
    bool                    Fog;
    bool                    Texture;
    bool                    ColorTexture;
    bool                    AlphaTexture;
    bool                    Blend;
    bool                    AlphaBuffer;
    bool                    ChromaKey;
    bool                    Clipping;
    int                     MultiTextureTMUs;
    int                     DepthBufferType;
    BYTE                    PTable[256][4];
    DWORD                   WaitSignal;
};

struct ConfigStruct
{
    int     OGLVersion;
    int     FogEnable;
    int     InitFullScreen;
    int     PrecisionFixEnable;
    int     Priority;
    int     EnableMipMaps;
    int     BuildMipMaps;
    int     IgnorePaletteChange;

    int     MultiTextureEXTEnable;
    int     PaletteEXTEnable;
    int     TextureEnvEXTEnable;
    int     VertexArrayEXTEnable;
    int     FogCoordEXTEnable;
    int     BlendFuncSeparateEXTEnable;
    int     TextureLodBiasEXTEnable;
    int     SecondaryColorEXTEnable;

    int     MMXEnable;
    int     CreateWindow;

    DWORD   TextureMemorySize;
    DWORD   FrameBufferMemorySize;
};

// Extern variables
extern GlideStruct      Glide;                  // Glide Internal
extern OpenGLStruct     OpenGL;                 // OpenGL equivalents
extern ConfigStruct     UserConfig,
                        InternalConfig;

// Genral Prototypes
void __cdecl GlideMsg(char *szString, ...);
void __cdecl Error(char *szString, ...);
void GLErro( char *Funcao );

int DetectMMX();

#endif
