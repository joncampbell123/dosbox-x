//**************************************************************
//*            OpenGLide - Glide to OpenGL Wrapper
//*             http://openglide.sourceforge.net
//*
//*                     Render Header
//*
//*         OpenGLide is OpenSource under LGPL license
//*              Originaly made by Fabio Barros
//*      Modified by Paul for Glidos (http://www.glidos.net)
//**************************************************************

#ifndef __GLRENDER_H__
#define __GLRENDER_H__

//**************************************************************
// Defines
//**************************************************************

#define MAXTRIANGLES			500


//**************************************************************
// Structs
//**************************************************************

struct GlVertex
{
	GLfloat x, y, z;
	GLfloat s, t, q, oow;
	GLfloat r, g, b, a;
	GLfloat r2, g2, b2, a2;
	GLfloat f;
};

struct Triangle
{
	GlVertex a, b, c;
};

struct ColorStruct
{
	GLfloat r, g, b, a;
};

struct TColorStruct
{
	GLfloat ar, ag, ab, aa;
	GLfloat br, bg, bb, ba;
	GLfloat cr, cg, cb, ca;
};

struct TVertexStruct
{
	GLfloat ax, ay, az, aw;
	GLfloat bx, by, bz, bw;
	GLfloat cx, cy, cz, cw;
};

struct TTextureStruct
{
	GLfloat as, at, aq, aoow;
	GLfloat bs, bt, bq, boow;
	GLfloat cs, ct, cq, coow;
};

struct TFogStruct
{
	GLfloat af;
	GLfloat bf;
	GLfloat cf;
};

struct RenderStruct
{
	TColorStruct	*TColor;
	TColorStruct	*TColor2;
	TTextureStruct	*TTexture;
	TVertexStruct	*TVertex;
	TFogStruct		*TFog;
	long NumberOfTriangles;

#ifdef OGL_DEBUG
	float MinX, MinY, MinZ, MinW;
	float MaxX, MaxY, MaxZ, MaxW;
	float MinS, MinT, MaxS, MaxT;
	float MinR, MinG, MinB, MinA;
	float MaxR, MaxG, MaxB, MaxA;
	float MaxF, MinF;
	long FrameTriangles, MaxTriangles, MaxSequencedTriangles;
#endif
};

//**************************************************************
// Function Prototypes
//**************************************************************

// Main Render functions
void RenderInitialize( void );
void RenderFree( void );
void RenderUpdateArrays( void );
void RenderAddTriangle( const GrVertex *a, const GrVertex *b, const GrVertex *c, bool unsnap );
void RenderAddLine( const GrVertex *a, const GrVertex *b, bool unsnap );
void RenderAddPoint( const GrVertex *a, bool unsnap );
void RenderDrawTriangles( void );

// Main Render variables
extern RenderStruct		OGLRender;
extern float (*AlphaFactorFunc)( float LocalAlpha, float OtherAlpha );
extern void  (*ColorFactor3Func)( TColorStruct *Result, TColorStruct *ColorComponent, TColorStruct *OtherAlpha );
extern void  (*ColorFunctionFunc)( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other );

// Prototypes for the color combining
float AlphaFactorZero( float LocalAlpha, float OtherAlpha );
float AlphaFactorLocal( float LocalAlpha, float OtherAlpha );
float AlphaFactorOther( float LocalAlpha, float OtherAlpha );
float AlphaFactorOneMinusLocal( float LocalAlpha, float OtherAlpha );
float AlphaFactorOneMinusOther( float LocalAlpha, float OtherAlpha );
float AlphaFactorOne( float LocalAlpha, float OtherAlpha );

void ColorFactor3Zero( TColorStruct *Result, TColorStruct *ColorComponent, TColorStruct *OtherAlpha );
void ColorFactor3Local( TColorStruct *Result, TColorStruct *ColorComponent, TColorStruct *OtherAlpha );
void ColorFactor3LocalAlpha( TColorStruct *Result, TColorStruct *ColorComponent, TColorStruct *OtherAlpha );
void ColorFactor3OneMinusLocal( TColorStruct *Result, TColorStruct *ColorComponent, TColorStruct *OtherAlpha );
void ColorFactor3OneMinusLocalAlpha( TColorStruct *Result, TColorStruct *ColorComponent, TColorStruct *OtherAlpha );
void ColorFactor3OtherAlpha( TColorStruct *Result, TColorStruct *ColorComponent, TColorStruct *OtherAlpha );
void ColorFactor3OneMinusOtherAlpha( TColorStruct *Result, TColorStruct *ColorComponent, TColorStruct *OtherAlpha );
void ColorFactor3One( TColorStruct *Result, TColorStruct *ColorComponent, TColorStruct *OtherAlpha );

void ColorFunctionZero( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other );
void ColorFunctionLocal( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other );
void ColorFunctionLocalAlpha( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other );
void ColorFunctionScaleOther( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other );
void ColorFunctionScaleOtherAddLocal( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other );
void ColorFunctionScaleOtherAddLocalAlpha( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other );
void ColorFunctionScaleOtherMinusLocal( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other );
void ColorFunctionScaleOtherMinusLocalAddLocal( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other );
void ColorFunctionScaleOtherMinusLocalAddLocalAlpha( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other );
void ColorFunctionMinusLocalAddLocal( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other );
void ColorFunctionMinusLocalAddLocalAlpha( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other );

#endif