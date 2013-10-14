//**************************************************************
//*            OpenGLide - Glide to OpenGL Wrapper
//*             http://openglide.sourceforge.net
//*
//*                 OpenGL Extensions Header
//*
//*         OpenGLide is OpenSource under LGPL license
//*              Originaly made by Fabio Barros
//*      Modified by Paul for Glidos (http://www.glidos.net)
//**************************************************************

#ifndef __GLEXTENSIONS__
#define __GLEXTENSIONS__

#include "Glext.h"

void ValidateUserConfig();

// Extensions Functions Declarations
extern PFNGLCLIENTACTIVETEXTUREPROC             glClientActiveTexture;
extern PFNGLMULTITEXCOORD4FARBPROC              glMultiTexCoord4fARB;
extern PFNGLMULTITEXCOORD4FVARBPROC             glMultiTexCoord4fvARB;
extern PFNGLACTIVETEXTUREARBPROC                glActiveTextureARB;
extern PFNGLSECONDARYCOLOR3UBVEXTPROC           glSecondaryColor3ubvEXT;
extern PFNGLSECONDARYCOLOR3UBEXTPROC            glSecondaryColor3ubEXT;
extern PFNGLSECONDARYCOLORPOINTEREXTPROC        glSecondaryColorPointerEXT;
extern PFNGLFOGCOORDFEXTPROC                    glFogCoordfEXT;
extern PFNGLFOGCOORDPOINTEREXTPROC              glFogCoordPointerEXT;
extern PFNGLSECONDARYCOLOR3FVEXTPROC            glSecondaryColor3fvEXT;
extern PFNGLSECONDARYCOLOR3FEXTPROC             glSecondaryColor3fEXT;

extern PFNGLCOLORTABLEEXTPROC                   glColorTableEXT;
extern PFNGLCOLORSUBTABLEEXTPROC                glColorSubTableEXT;
extern PFNGLGETCOLORTABLEEXTPROC                glGetColorTableEXT;
extern PFNGLGETCOLORTABLEPARAMETERIVEXTPROC     glGetColorTableParameterivEXT;
extern PFNGLGETCOLORTABLEPARAMETERFVEXTPROC     glGetColorTableParameterfvEXT;

extern PFNGLBLENDFUNCSEPARATEEXTPROC            glBlendFuncSeparateEXT;


void APIENTRY DummyV( const void *a );

#endif
