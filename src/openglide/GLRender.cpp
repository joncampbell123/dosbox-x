//**************************************************************
//*            OpenGLide - Glide to OpenGL Wrapper
//*             http://openglide.sourceforge.net
//*
//*                     Render File
//*
//*         OpenGLide is OpenSource under LGPL license
//*              Originaly made by Fabio Barros
//*      Modified by Paul for Glidos (http://www.glidos.net)
//**************************************************************

#include "GlOGl.h"
#include "GLRender.h"
#include "GLextensions.h"
#include "PGTexture.h"


//**************************************************************
// Defines
//**************************************************************

#define DEBUG_MIN_MAX( var, maxvar, minvar )    \
    if ( var > maxvar ) maxvar = var;           \
    if ( var < minvar ) minvar = var;

//**************************************************************
// extern variables and functions prototypes
//**************************************************************

//**************************************************************
// Local variables
//**************************************************************

// The functions for the color combine
float   (*AlphaFactorFunc)( float LocalAlpha, float OtherAlpha );
void    (*ColorFactor3Func)( TColorStruct *Result, TColorStruct *ColorComponent, TColorStruct *OtherAlpha );
void    (*ColorFunctionFunc)( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other );

// Snapping constant
static const float vertex_snap_compare = 4096.0f;
static const float vertex_snap = float( 3L << 18 );

// Standard structs for the render
RenderStruct OGLRender;

// Varibles for the Add functions
static TColorStruct     Local, 
                        Other, 
                        CFactor;
static float            AFactor[3];
static TColorStruct     *pC,
                        *pC2;
static TVertexStruct    *pV;
static TTextureStruct   *pTS;
static TFogStruct       *pF;
static void             *pt1, 
                        *pt2, 
                        *pt3;
static float            atmuoow;
static float            btmuoow;
static float            ctmuoow;
static float            w, 
                        aoow, 
                        boow, 
                        coow;
static float            hAspect, 
                        wAspect,
                        maxoow;

//**************************************************************
// Functions definitions
//**************************************************************

// Intializes the render and allocates memory
void RenderInitialize( void )
{
    OGLRender.NumberOfTriangles = 0;

    OGLRender.TColor    = new TColorStruct[ MAXTRIANGLES + 1 ];
    OGLRender.TColor2   = new TColorStruct[ MAXTRIANGLES + 1 ];
    OGLRender.TTexture  = new TTextureStruct[ MAXTRIANGLES + 1 ];
    OGLRender.TVertex   = new TVertexStruct[ MAXTRIANGLES + 1 ];
    OGLRender.TFog      = new TFogStruct[ MAXTRIANGLES + 1 ];

#ifdef OGL_DEBUG
    OGLRender.FrameTriangles = 0;
    OGLRender.MaxTriangles = 0;
    OGLRender.MaxSequencedTriangles = 0;
    OGLRender.MinX = OGLRender.MinY = OGLRender.MinZ = OGLRender.MinW = 99999999.0f;
    OGLRender.MaxX = OGLRender.MaxY = OGLRender.MaxZ = OGLRender.MaxW = -99999999.0f;
    OGLRender.MinS = OGLRender.MinT = OGLRender.MinF = 99999999.0f;
    OGLRender.MaxS = OGLRender.MaxT = OGLRender.MaxF = -99999999.0f;

    OGLRender.MinR = OGLRender.MinG = OGLRender.MinB = OGLRender.MinA = 99999999.0f;
    OGLRender.MaxR = OGLRender.MaxG = OGLRender.MaxB = OGLRender.MaxA = -99999999.0f;
#endif
}

// Shutdowns the render and frees memory
void RenderFree( void )
{
    delete[] OGLRender.TColor;
    delete[] OGLRender.TColor2;
    delete[] OGLRender.TTexture;
    delete[] OGLRender.TVertex;
    delete[] OGLRender.TFog;
}

void RenderUpdateArrays( void )
{
    glVertexPointer( 3, GL_FLOAT, 4 * sizeof( GLfloat ), &OGLRender.TVertex[0] );
    glColorPointer( 4, GL_FLOAT, 0, &OGLRender.TColor[0] );
    if ( InternalConfig.MultiTextureEXTEnable )
    {
        glClientActiveTexture( GL_TEXTURE0_ARB );
    }
    glTexCoordPointer( 4, GL_FLOAT, 0, &OGLRender.TTexture[0] );
    if ( InternalConfig.MultiTextureEXTEnable )
    {
        glClientActiveTexture( GL_TEXTURE1_ARB );
        glTexCoordPointer( 4, GL_FLOAT, 0, &OGLRender.TTexture[0] );
    }
    glSecondaryColorPointerEXT( 3, GL_FLOAT, 4 * sizeof( GLfloat ), &OGLRender.TColor2[0] );
    if ( InternalConfig.FogCoordEXTEnable )
    {
        glFogCoordPointerEXT( 1, GL_FLOAT, &OGLRender.TFog[0] );
    }

#ifdef OPENGL_DEBUG
    GLErro( "Render::UpdateArrays" );
#endif
}

// Draw the current saved triangles
void RenderDrawTriangles( void )
{
    static int      i;
    static DWORD    Pixels;
    static BYTE     * Buffer1,
                    * Buffer2;
    static GLuint   TNumber;
    bool            use_two_tex = false;

    if ( ! OGLRender.NumberOfTriangles )
    {
        return;
    }

    if ( OpenGL.Texture )
    {
        glEnable( GL_TEXTURE_2D );

        use_two_tex = Textures->MakeReady( );

        if ( use_two_tex )
        {
            glActiveTextureARB( GL_TEXTURE1_ARB );

            glEnable( GL_TEXTURE_2D );

            glActiveTextureARB( GL_TEXTURE0_ARB );
        }
    }
    else
    {
        glDisable( GL_TEXTURE_2D );
    }

    if ( OpenGL.Blend )
    {
        glEnable( GL_BLEND );
    }
    else
    {
        glDisable( GL_BLEND );
    }

    // Alpha Fix
    if ( Glide.State.AlphaOther != GR_COMBINE_OTHER_TEXTURE )
    {
        glDisable( GL_ALPHA_TEST );
    }
    else 
    {
        if ( Glide.State.AlphaTestFunction != GR_CMP_ALWAYS )
        {
            glEnable( GL_ALPHA_TEST );
        }
    }
    
    if( !OpenGL.Blend && Glide.State.ChromaKeyMode )
    {
        glAlphaFunc( GL_GEQUAL, 0.5 );
        glEnable( GL_ALPHA_TEST );
        
        glBegin( GL_TRIANGLES );
        for ( i = 0; i < OGLRender.NumberOfTriangles; i++ )
        {
            glColor3fv( &OGLRender.TColor[ i ].ar );
            glSecondaryColor3fvEXT( &OGLRender.TColor2[ i ].ar );
            glFogCoordfEXT( OGLRender.TFog[ i ].af );
            glTexCoord4fv( &OGLRender.TTexture[ i ].as );
            glVertex3fv( &OGLRender.TVertex[ i ].ax );
            
            glColor3fv( &OGLRender.TColor[ i ].br );
            glSecondaryColor3fvEXT( &OGLRender.TColor2[ i ].br );
            glFogCoordfEXT( OGLRender.TFog[ i ].bf );
            glTexCoord4fv( &OGLRender.TTexture[ i ].bs );
            glVertex3fv( &OGLRender.TVertex[ i ].bx );
            
            glColor3fv( &OGLRender.TColor[ i ].cr );
            glSecondaryColor3fvEXT( &OGLRender.TColor2[ i ].cr );
            glFogCoordfEXT( OGLRender.TFog[ i ].cf );
            glTexCoord4fv( &OGLRender.TTexture[ i ].cs );
            glVertex3fv( &OGLRender.TVertex[ i ].cx );
        }
        glEnd( );
        
        glDisable( GL_ALPHA_TEST );
    }
    else
    {
        if ( InternalConfig.VertexArrayEXTEnable )
        {
            glDrawArrays( GL_TRIANGLES, 0, OGLRender.NumberOfTriangles * 3 );
        }
        else
        {
            glBegin( GL_TRIANGLES );
            for ( i = 0; i < OGLRender.NumberOfTriangles; i++ )
            {
                glColor4fv( &OGLRender.TColor[ i ].ar );
                glSecondaryColor3fvEXT( &OGLRender.TColor2[ i ].ar );
                glFogCoordfEXT( OGLRender.TFog[ i ].af );
                glTexCoord4fv( &OGLRender.TTexture[ i ].as );
                if ( use_two_tex )
                {
                    glMultiTexCoord4fvARB( GL_TEXTURE1_ARB, &OGLRender.TTexture[ i ].as );
                }
                glVertex3fv( &OGLRender.TVertex[ i ].ax );
                
                glColor4fv( &OGLRender.TColor[ i ].br );
                glSecondaryColor3fvEXT( &OGLRender.TColor2[ i ].br );
                glFogCoordfEXT( OGLRender.TFog[ i ].bf );
                glTexCoord4fv( &OGLRender.TTexture[ i ].bs );
                if ( use_two_tex )
                {
                    glMultiTexCoord4fvARB( GL_TEXTURE1_ARB, &OGLRender.TTexture[ i ].bs );
                }
                glVertex3fv( &OGLRender.TVertex[ i ].bx );
                
                glColor4fv( &OGLRender.TColor[ i ].cr );
                glSecondaryColor3fvEXT( &OGLRender.TColor2[ i ].cr );
                glFogCoordfEXT( OGLRender.TFog[ i ].cf );
                glTexCoord4fv( &OGLRender.TTexture[ i ].cs );
                if ( use_two_tex )
                {
                    glMultiTexCoord4fvARB( GL_TEXTURE1_ARB, &OGLRender.TTexture[ i ].cs );
                }
                glVertex3fv( &OGLRender.TVertex[ i ].cx );
            }
            glEnd( );
        }
    }
  
    if ( ! InternalConfig.SecondaryColorEXTEnable )
    {
        glBlendFunc( GL_ONE, GL_ONE );
        glEnable( GL_BLEND );
        glDisable( GL_TEXTURE_2D );

        if ( OpenGL.DepthBufferType )
        {
            glPolygonOffset( 1.0f, 0.5f );
        }
        else
        {
            glPolygonOffset( -1.0f, -0.5f );
        }

        glEnable( GL_POLYGON_OFFSET_FILL );

        if ( 0 && InternalConfig.VertexArrayEXTEnable ) // ????
        {
            glColorPointer( 4, GL_FLOAT, 0, &OGLRender.TColor2 );
            glDrawArrays( GL_TRIANGLES, 0, OGLRender.NumberOfTriangles * 3 );
            glColorPointer( 4, GL_FLOAT, 0, &OGLRender.TColor );
        }
        else
        {
            glBegin( GL_TRIANGLES );
            for ( i = 0; i < OGLRender.NumberOfTriangles; i++ )
            {
                glColor4fv( &OGLRender.TColor2[ i ].ar );
                glVertex3fv( &OGLRender.TVertex[ i ].ax );

                glColor4fv( &OGLRender.TColor2[ i ].br );
                glVertex3fv( &OGLRender.TVertex[ i ].bx );

                glColor4fv( &OGLRender.TColor2[ i ].cr );
                glVertex3fv( &OGLRender.TVertex[ i ].cx );
            }
            glEnd( );
        }

        if ( Glide.State.DepthBiasLevel )
        {
            glPolygonOffset( 1.0f, OpenGL.DepthBiasLevel );
        }
        else
        {
            glDisable( GL_POLYGON_OFFSET_FILL );
        }

        if ( OpenGL.Blend )
        {
            glBlendFunc( OpenGL.SrcBlend, OpenGL.DstBlend );
        }
    }

    if ( use_two_tex )
    {
        glActiveTextureARB( GL_TEXTURE1_ARB );

        glDisable( GL_TEXTURE_2D );

        glActiveTextureARB( GL_TEXTURE0_ARB );
    }

#ifdef OGL_DEBUG
    if ( OGLRender.NumberOfTriangles > OGLRender.MaxSequencedTriangles )
    {
        OGLRender.MaxSequencedTriangles = OGLRender.NumberOfTriangles;
    }

    GLErro( "Render::DrawTriangles" );
#endif

    OGLRender.NumberOfTriangles = 0;
}

void RenderAddTriangle( const GrVertex *a, const GrVertex *b, const GrVertex *c, bool unsnap )
{
    pC = &OGLRender.TColor[ OGLRender.NumberOfTriangles ];
    pC2 = &OGLRender.TColor2[ OGLRender.NumberOfTriangles ];
    pV = &OGLRender.TVertex[ OGLRender.NumberOfTriangles ];
    pTS = &OGLRender.TTexture[ OGLRender.NumberOfTriangles ];

    ZeroMemory( pC2, sizeof( TColorStruct ) );

    if ( ( Glide.State.STWHint & GR_STWHINT_W_DIFF_TMU0 ) == 0 )
    {
        atmuoow = a->oow;
        btmuoow = b->oow;
        ctmuoow = c->oow;
    }
    else
    {
        atmuoow = a->tmuvtx[ 0 ].oow;
        btmuoow = b->tmuvtx[ 0 ].oow;
        ctmuoow = c->tmuvtx[ 0 ].oow;
    }

    if ( Glide.ALocal )
    {
        switch ( Glide.State.AlphaLocal )
        {
        case GR_COMBINE_LOCAL_ITERATED:
            Local.aa = a->a * D1OVER255;
            Local.ba = b->a * D1OVER255;
            Local.ca = c->a * D1OVER255;
            break;

        case GR_COMBINE_LOCAL_CONSTANT:
            Local.aa = Local.ba = Local.ca = OpenGL.ConstantColor[ 3 ];
            break;

        case GR_COMBINE_LOCAL_DEPTH:
            Local.aa = a->z;
            Local.ba = b->z;
            Local.ca = c->z;
            break;
        }
    }

    if ( Glide.AOther )
    {
        switch ( Glide.State.AlphaOther )
        {
        case GR_COMBINE_OTHER_ITERATED:
            Other.aa = a->a * D1OVER255;
            Other.ba = b->a * D1OVER255;
            Other.ca = c->a * D1OVER255;
            break;

        case GR_COMBINE_OTHER_CONSTANT:
            Other.aa = Other.ba = Other.ca = OpenGL.ConstantColor[ 3 ];
            break;

        case GR_COMBINE_OTHER_TEXTURE:
            Other.aa = Other.ba = Other.ca = 1.0f;
            break;
        }
    }

    if ( Glide.CLocal )
    {
        switch ( Glide.State.ColorCombineLocal )
        {
        case GR_COMBINE_LOCAL_ITERATED:
            Local.ar = a->r * D1OVER255;
            Local.ag = a->g * D1OVER255;
            Local.ab = a->b * D1OVER255;
            Local.br = b->r * D1OVER255;
            Local.bg = b->g * D1OVER255;
            Local.bb = b->b * D1OVER255;
            Local.cr = c->r * D1OVER255;
            Local.cg = c->g * D1OVER255;
            Local.cb = c->b * D1OVER255;
            break;

        case GR_COMBINE_LOCAL_CONSTANT:
            Local.ar = Local.br = Local.cr = OpenGL.ConstantColor[ 0 ];
            Local.ag = Local.bg = Local.cg = OpenGL.ConstantColor[ 1 ];
            Local.ab = Local.bb = Local.cb = OpenGL.ConstantColor[ 2 ];
            break;
        }
    }

    if ( Glide.COther )
    {
        switch ( Glide.State.ColorCombineOther )
        {
        case GR_COMBINE_OTHER_ITERATED:
            Other.ar = a->r * D1OVER255;
            Other.ag = a->g * D1OVER255;
            Other.ab = a->b * D1OVER255;
            Other.br = b->r * D1OVER255;
            Other.bg = b->g * D1OVER255;
            Other.bb = b->b * D1OVER255;
            Other.cr = c->r * D1OVER255;
            Other.cg = c->g * D1OVER255;
            Other.cb = c->b * D1OVER255;
            break;

        case GR_COMBINE_OTHER_CONSTANT:
            Other.ar = Other.br = Other.cr = OpenGL.ConstantColor[ 0 ];
            Other.ag = Other.bg = Other.cg = OpenGL.ConstantColor[ 1 ];
            Other.ab = Other.bb = Other.cb = OpenGL.ConstantColor[ 2 ];
            break;

        case GR_COMBINE_OTHER_TEXTURE:
            Other.ar = Other.ag = Other.ab = 1.0f;
            Other.br = Other.bg = Other.bb = 1.0f;
            Other.cr = Other.cg = Other.cb = 1.0f;
            break;
        }
    }

    ColorFunctionFunc( pC, pC2, &Local, &Other );

// ???? Why is this here as we are zeroing the pC2 struct in the beggining ???????
//    pC2->aa = 0.0f;
//    pC2->ba = 0.0f;
//    pC2->ca = 0.0f;

    switch ( Glide.State.AlphaFunction )
    {
    case GR_COMBINE_FUNCTION_ZERO:
        pC->aa = pC->ba = pC->ca = 0.0f;
        break;

    case GR_COMBINE_FUNCTION_LOCAL:
    case GR_COMBINE_FUNCTION_LOCAL_ALPHA:
        pC->aa = Local.aa;
        pC->ba = Local.ba;
        pC->ca = Local.ca;
        break;

    case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL:
    case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL_ALPHA:
        pC->aa = ( 1.0f - AlphaFactorFunc( Local.aa, Other.aa ) ) * Local.aa;
        pC->ba = ( 1.0f - AlphaFactorFunc( Local.ba, Other.ba ) ) * Local.ba;
        pC->ca = ( 1.0f - AlphaFactorFunc( Local.ca, Other.ca ) ) * Local.ca;
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER:
        pC->aa = AlphaFactorFunc( Local.aa, Other.aa ) * Other.aa;
        pC->ba = AlphaFactorFunc( Local.ba, Other.ba ) * Other.ba;
        pC->ca = AlphaFactorFunc( Local.ca, Other.ca ) * Other.ca;
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL:
    case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA:
        pC->aa = AlphaFactorFunc( Local.aa, Other.aa ) * Other.aa + Local.aa;
        pC->ba = AlphaFactorFunc( Local.ba, Other.ba ) * Other.ba + Local.ba;
        pC->ca = AlphaFactorFunc( Local.ca, Other.ca ) * Other.ca + Local.ca;
//      pC2->aa =  Local.aa;
//      pC2->ba =  Local.ba;
//      pC2->ca =  Local.ca;
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL:
        pC->aa = AlphaFactorFunc( Local.aa, Other.aa ) * ( Other.aa - Local.aa );
        pC->ba = AlphaFactorFunc( Local.ba, Other.ba ) * ( Other.ba - Local.ba );
        pC->ca = AlphaFactorFunc( Local.ca, Other.ca ) * ( Other.ca - Local.ca );
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL:
    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL_ALPHA:
        pC->aa = AlphaFactorFunc( Local.aa, Other.aa ) * ( Other.aa - Local.aa ) + Local.aa;
        pC->ba = AlphaFactorFunc( Local.ba, Other.ba ) * ( Other.ba - Local.ba ) + Local.ba;
        pC->ca = AlphaFactorFunc( Local.ca, Other.ca ) * ( Other.ca - Local.ca ) + Local.ca;
//      pC2->aa =  Local.ba;
//      pC2->ba =  Local.ba;
//      pC2->ca =  Local.ca;
        break;
    }

    if ( Glide.State.ColorCombineInvert )
    {
        pC->ar = 1.0f - pC->ar - pC2->ar;
        pC->ag = 1.0f - pC->ag - pC2->ag;
        pC->ab = 1.0f - pC->ab - pC2->ab;
        pC->br = 1.0f - pC->br - pC2->br;
        pC->bg = 1.0f - pC->bg - pC2->bg;
        pC->bb = 1.0f - pC->bb - pC2->bb;
        pC->cr = 1.0f - pC->cr - pC2->cr;
        pC->cg = 1.0f - pC->cg - pC2->cg;
        pC->cb = 1.0f - pC->cb - pC2->cb;
        pC2->ar = pC2->ag = pC2->ab = 0.0f;
        pC2->br = pC2->bg = pC2->bb = 0.0f;
        pC2->cr = pC2->cg = pC2->cb = 0.0f;
    }

    if ( Glide.State.AlphaInvert )
    {
        pC->aa = 1.0f - pC->aa - pC2->aa;
        pC->ba = 1.0f - pC->ba - pC2->ba;
        pC->ca = 1.0f - pC->ca - pC2->ca;
        pC2->aa = pC2->ba = pC2->ca = 0.0f;
    }
    
    // Z-Buffering
    if ( ( Glide.State.DepthBufferMode == GR_DEPTHBUFFER_DISABLE ) || 
         ( Glide.State.DepthFunction == GR_CMP_ALWAYS ) )
    {
        pV->az = 0.0f;
        pV->bz = 0.0f;
        pV->cz = 0.0f;
    }
    else
    if ( OpenGL.DepthBufferType )
    {
        pV->az = a->ooz * D1OVER65536;
        pV->bz = b->ooz * D1OVER65536;
        pV->cz = c->ooz * D1OVER65536;
    }
    else
    {
       /*
        * For silly values of oow, depth buffering doesn't
        * seem to work, so map them to a sensible z.  When
        * games use these silly values, they probably don't
        * use z buffering anyway.
        */
        if ( a->oow > 1.0 )
        {
            pV->az = pV->bz = pV->cz = 0.9f;
        }
        else 
        if ( InternalConfig.PrecisionFixEnable )
        {
            w = 1.0f / a->oow;
            pV->az = 8.9375f - (float( ( (*(DWORD *)&w >> 11) & 0xFFFFF ) * D1OVER65536) );
            w = 1.0f / b->oow;
            pV->bz = 8.9375f - (float( ( (*(DWORD *)&w >> 11) & 0xFFFFF ) * D1OVER65536) );
            w = 1.0f / c->oow;
            pV->cz = 8.9375f - (float( ( (*(DWORD *)&w >> 11) & 0xFFFFF ) * D1OVER65536) );
        }
        else
        {
            pV->az = a->oow;
            pV->bz = b->oow;
            pV->cz = c->oow;
        }
    }

    if ( ( unsnap ) &&
         ( a->x > vertex_snap_compare ) )
    {
        pV->ax = a->x - vertex_snap;
        pV->ay = a->y - vertex_snap;
        pV->bx = b->x - vertex_snap;
        pV->by = b->y - vertex_snap;
        pV->cx = c->x - vertex_snap;
        pV->cy = c->y - vertex_snap;
    }
    else
    {
        pV->ax = a->x;
        pV->ay = a->y;
        pV->bx = b->x;
        pV->by = b->y;
        pV->cx = c->x;
        pV->cy = c->y;
    }

    if ( OpenGL.Texture )
    {
        maxoow = 1.0f / max( atmuoow, max( btmuoow, ctmuoow ) );

        Textures->GetAspect( &hAspect, &wAspect );

        pTS->as = a->tmuvtx[ 0 ].sow * wAspect * maxoow;
        pTS->at = a->tmuvtx[ 0 ].tow * hAspect * maxoow;
        pTS->bs = b->tmuvtx[ 0 ].sow * wAspect * maxoow;
        pTS->bt = b->tmuvtx[ 0 ].tow * hAspect * maxoow;
        pTS->cs = c->tmuvtx[ 0 ].sow * wAspect * maxoow;
        pTS->ct = c->tmuvtx[ 0 ].tow * hAspect * maxoow;

        pTS->aq = pTS->bq = pTS->cq = 0.0f;
        pTS->aoow = atmuoow * maxoow;
        pTS->boow = btmuoow * maxoow;
        pTS->coow = ctmuoow * maxoow;
    }

    if( InternalConfig.FogEnable )
    {
        pF = &OGLRender.TFog[ OGLRender.NumberOfTriangles ];
        if ( Glide.State.FogMode == GR_FOG_WITH_TABLE )
        {
            pF->af = (float)OpenGL.FogTable[ (WORD)(1.0f / a->oow) ] * D1OVER255;
            pF->bf = (float)OpenGL.FogTable[ (WORD)(1.0f / b->oow) ] * D1OVER255;
            pF->cf = (float)OpenGL.FogTable[ (WORD)(1.0f / c->oow) ] * D1OVER255;
        }
        else
        {
            pF->af = a->a * D1OVER255;
            pF->bf = b->a * D1OVER255;
            pF->cf = c->a * D1OVER255;
        }
    #ifdef OGL_DEBUG
       DEBUG_MIN_MAX( pF->af, OGLRender.MaxF, OGLRender.MinF );
       DEBUG_MIN_MAX( pF->bf, OGLRender.MaxF, OGLRender.MinF );
       DEBUG_MIN_MAX( pF->bf, OGLRender.MaxF, OGLRender.MinF );
    #endif
    }

#ifdef OGL_DEBUG
    DEBUG_MIN_MAX( pC->ar, OGLRender.MaxR, OGLRender.MinR );
    DEBUG_MIN_MAX( pC->br, OGLRender.MaxR, OGLRender.MinR );
    DEBUG_MIN_MAX( pC->cr, OGLRender.MaxR, OGLRender.MinR );
    
    DEBUG_MIN_MAX( pC->ag, OGLRender.MaxG, OGLRender.MinG );
    DEBUG_MIN_MAX( pC->bg, OGLRender.MaxG, OGLRender.MinG );
    DEBUG_MIN_MAX( pC->cg, OGLRender.MaxG, OGLRender.MinG );

    DEBUG_MIN_MAX( pC->ab, OGLRender.MaxB, OGLRender.MinB );
    DEBUG_MIN_MAX( pC->bb, OGLRender.MaxB, OGLRender.MinB );
    DEBUG_MIN_MAX( pC->cb, OGLRender.MaxB, OGLRender.MinB );

    DEBUG_MIN_MAX( pC->aa, OGLRender.MaxA, OGLRender.MinA );
    DEBUG_MIN_MAX( pC->ba, OGLRender.MaxA, OGLRender.MinA );
    DEBUG_MIN_MAX( pC->ca, OGLRender.MaxA, OGLRender.MinA );

    DEBUG_MIN_MAX( pV->az, OGLRender.MaxZ, OGLRender.MinZ );
    DEBUG_MIN_MAX( pV->bz, OGLRender.MaxZ, OGLRender.MinZ );
    DEBUG_MIN_MAX( pV->cz, OGLRender.MaxZ, OGLRender.MinZ );

    DEBUG_MIN_MAX( pV->ax, OGLRender.MaxX, OGLRender.MinX );
    DEBUG_MIN_MAX( pV->bx, OGLRender.MaxX, OGLRender.MinX );
    DEBUG_MIN_MAX( pV->cx, OGLRender.MaxX, OGLRender.MinX );

    DEBUG_MIN_MAX( pV->ay, OGLRender.MaxY, OGLRender.MinY );
    DEBUG_MIN_MAX( pV->by, OGLRender.MaxY, OGLRender.MinY );
    DEBUG_MIN_MAX( pV->cy, OGLRender.MaxY, OGLRender.MinY );

    DEBUG_MIN_MAX( pTS->as, OGLRender.MaxS, OGLRender.MinS );
    DEBUG_MIN_MAX( pTS->bs, OGLRender.MaxS, OGLRender.MinS );
    DEBUG_MIN_MAX( pTS->cs, OGLRender.MaxS, OGLRender.MinS );

    DEBUG_MIN_MAX( pTS->at, OGLRender.MaxT, OGLRender.MinT );
    DEBUG_MIN_MAX( pTS->bt, OGLRender.MaxT, OGLRender.MinT );
    DEBUG_MIN_MAX( pTS->ct, OGLRender.MaxT, OGLRender.MinT );

    OGLRender.FrameTriangles++;
#endif
    
    OGLRender.NumberOfTriangles++;

    if ( OGLRender.NumberOfTriangles >= ( MAXTRIANGLES - 1 ) )
    {
        RenderDrawTriangles( );
    }

#ifdef OPENGL_DEBUG
    GLErro( "Render::AddTriangle" );
#endif
}

void RenderAddLine( const GrVertex *a, const GrVertex *b, bool unsnap )
{
    pC  = &OGLRender.TColor[ MAXTRIANGLES ];
    pC2 = &OGLRender.TColor2[ MAXTRIANGLES ];
    pV  = &OGLRender.TVertex[ MAXTRIANGLES ];
    pTS = &OGLRender.TTexture[ MAXTRIANGLES ];
    pF  = &OGLRender.TFog[ MAXTRIANGLES ];

    // Color Stuff, need to optimize it
    ZeroMemory( pC2, sizeof( TColorStruct ) );

    if ( ( Glide.State.STWHint & GR_STWHINT_W_DIFF_TMU0 ) == 0 )
    {
        atmuoow = a->oow;
        btmuoow = b->oow;
    }
    else
    {
        atmuoow = a->tmuvtx[ 0 ].oow;
        btmuoow = b->tmuvtx[ 0 ].oow;
    }

    if ( Glide.ALocal )
    {
        switch ( Glide.State.AlphaLocal )
        {
        case GR_COMBINE_LOCAL_ITERATED:
            Local.aa = a->a * D1OVER255;
            Local.ba = b->a * D1OVER255;
            break;

        case GR_COMBINE_LOCAL_CONSTANT:
            Local.aa = Local.ba = OpenGL.ConstantColor[3];
            break;

        case GR_COMBINE_LOCAL_DEPTH:
            Local.aa = a->z;
            Local.ba = b->z;
            break;
        }
    }

    if ( Glide.AOther )
    {
        switch ( Glide.State.AlphaOther )
        {
        case GR_COMBINE_OTHER_ITERATED:
            Other.aa = a->a * D1OVER255;
            Other.ba = b->a * D1OVER255;
            break;

        case GR_COMBINE_OTHER_CONSTANT:
            Other.aa = Other.ba = OpenGL.ConstantColor[3];
            break;

        case GR_COMBINE_OTHER_TEXTURE:
            Other.aa = Other.ba = 1.0f;
            break;
        }
    }

    if ( Glide.CLocal )
    {
        switch ( Glide.State.ColorCombineLocal )
        {
        case GR_COMBINE_LOCAL_ITERATED:
            Local.ar = a->r * D1OVER255;
            Local.ag = a->g * D1OVER255;
            Local.ab = a->b * D1OVER255;
            Local.br = b->r * D1OVER255;
            Local.bg = b->g * D1OVER255;
            Local.bb = b->b * D1OVER255;
            break;

        case GR_COMBINE_LOCAL_CONSTANT:
            Local.ar = Local.br = OpenGL.ConstantColor[0];
            Local.ag = Local.bg = OpenGL.ConstantColor[1];
            Local.ab = Local.bb = OpenGL.ConstantColor[2];
            break;
        }
    }

    if ( Glide.COther )
    {
        switch ( Glide.State.ColorCombineOther )
        {
        case GR_COMBINE_OTHER_ITERATED:
            Other.ar = a->r * D1OVER255;
            Other.ag = a->g * D1OVER255;
            Other.ab = a->b * D1OVER255;
            Other.br = b->r * D1OVER255;
            Other.bg = b->g * D1OVER255;
            Other.bb = b->b * D1OVER255;
            break;

        case GR_COMBINE_OTHER_CONSTANT:
            Other.ar = Other.br = OpenGL.ConstantColor[0];
            Other.ag = Other.bg = OpenGL.ConstantColor[1];
            Other.ab = Other.bb = OpenGL.ConstantColor[2];
            break;

        case GR_COMBINE_OTHER_TEXTURE:
            Other.ar = Other.ag = Other.ab = 1.0f;
            Other.br = Other.bg = Other.bb = 1.0f;
            break;
        }
    }

    switch ( Glide.State.ColorCombineFunction )
    {
    case GR_COMBINE_FUNCTION_ZERO:
        pC->ar = pC->ag = pC->ab = 0.0f; 
        pC->br = pC->bg = pC->bb = 0.0f; 
        break;

    case GR_COMBINE_FUNCTION_LOCAL:
        pC->ar = Local.ar;
        pC->ag = Local.ag;
        pC->ab = Local.ab;
        pC->br = Local.br;
        pC->bg = Local.bg;
        pC->bb = Local.bb;
        break;

    case GR_COMBINE_FUNCTION_LOCAL_ALPHA:
        pC->ar = pC->ag = pC->ab = Local.aa;
        pC->br = pC->bg = pC->bb = Local.ba;
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER:
        ColorFactor3Func( &CFactor, &Local, &Other );
        pC->ar = CFactor.ar * Other.ar;
        pC->ag = CFactor.ag * Other.ag;
        pC->ab = CFactor.ab * Other.ab;
        pC->br = CFactor.br * Other.br;
        pC->bg = CFactor.bg * Other.bg;
        pC->bb = CFactor.bb * Other.bb;
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL:
        ColorFactor3Func( &CFactor, &Local, &Other );
        pC->ar = CFactor.ar * Other.ar;
        pC->ag = CFactor.ag * Other.ag;
        pC->ab = CFactor.ab * Other.ab;
        pC->br = CFactor.br * Other.br;
        pC->bg = CFactor.bg * Other.bg;
        pC->bb = CFactor.bb * Other.bb;
        pC2->ar = Local.ar;
        pC2->ag = Local.ag;
        pC2->ab = Local.ab;
        pC2->br = Local.br;
        pC2->bg = Local.bg;
        pC2->bb = Local.bb;
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA:
        ColorFactor3Func( &CFactor, &Local, &Other );
        pC->ar = CFactor.ar * Other.ar;
        pC->ag = CFactor.ag * Other.ag;
        pC->ab = CFactor.ab * Other.ab;
        pC->br = CFactor.br * Other.br;
        pC->bg = CFactor.bg * Other.bg;
        pC->bb = CFactor.bb * Other.bb;
        pC2->ar = pC2->ag = pC2->ab = Local.aa;
        pC2->br = pC2->bg = pC2->bb = Local.ba;
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL:
        ColorFactor3Func( &CFactor, &Local, &Other );
        pC->ar = CFactor.ar * (Other.ar - Local.ar);
        pC->ag = CFactor.ag * (Other.ag - Local.ag);
        pC->ab = CFactor.ab * (Other.ab - Local.ab);
        pC->br = CFactor.br * (Other.br - Local.br);
        pC->bg = CFactor.bg * (Other.bg - Local.bg);
        pC->bb = CFactor.bb * (Other.bb - Local.bb);
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL:
        if ((( Glide.State.ColorCombineFactor == GR_COMBINE_FACTOR_TEXTURE_ALPHA ) ||
            ( Glide.State.ColorCombineFactor == GR_COMBINE_FACTOR_TEXTURE_RGB )) &&
            (  Glide.State.ColorCombineOther == GR_COMBINE_OTHER_TEXTURE ) )
        {
            pC->ar = Local.ar;
            pC->ag = Local.ag;
            pC->ab = Local.ab;
            pC->br = Local.br;
            pC->bg = Local.bg;
            pC->bb = Local.bb;
        }
        else
        {
            ColorFactor3Func( &CFactor, &Local, &Other );
            pC->ar = CFactor.ar * (Other.ar - Local.ar);
            pC->ag = CFactor.ag * (Other.ag - Local.ag);
            pC->ab = CFactor.ab * (Other.ab - Local.ab);
            pC->br = CFactor.br * (Other.br - Local.br);
            pC->bg = CFactor.bg * (Other.bg - Local.bg);
            pC->bb = CFactor.bb * (Other.bb - Local.bb);
            pC2->ar = Local.ar;
            pC2->ag = Local.ag;
            pC2->ab = Local.ab;
            pC2->br = Local.br;
            pC2->bg = Local.bg;
            pC2->bb = Local.bb;
        }
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL_ALPHA:
        if ((( Glide.State.ColorCombineFactor == GR_COMBINE_FACTOR_TEXTURE_ALPHA ) ||
            ( Glide.State.ColorCombineFactor == GR_COMBINE_FACTOR_TEXTURE_RGB )) &&
            (  Glide.State.ColorCombineOther == GR_COMBINE_OTHER_TEXTURE ) )
        {
            pC->ar = pC->ag = pC->ab = Local.aa;
            pC->br = pC->bg = pC->bb = Local.ba;
        }
        else
        {
            ColorFactor3Func( &CFactor, &Local, &Other );
            pC->ar = CFactor.ar * (Other.ar - Local.ar);
            pC->ag = CFactor.ag * (Other.ag - Local.ag);
            pC->ab = CFactor.ab * (Other.ab - Local.ab);
            pC->br = CFactor.br * (Other.br - Local.br);
            pC->bg = CFactor.bg * (Other.bg - Local.bg);
            pC->bb = CFactor.bb * (Other.bb - Local.bb);
            pC2->ar = pC2->ag = pC2->ab = Local.aa;
            pC2->br = pC2->bg = pC2->bb = Local.ba;
        }
        break;

    case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL:
        ColorFactor3Func( &CFactor, &Local, &Other );
        pC->ar = ( 1.0f - CFactor.ar ) * Local.ar;
        pC->ag = ( 1.0f - CFactor.ag ) * Local.ag;
        pC->ab = ( 1.0f - CFactor.ab ) * Local.ab;
        pC->br = ( 1.0f - CFactor.br ) * Local.br;
        pC->bg = ( 1.0f - CFactor.bg ) * Local.bg;
        pC->bb = ( 1.0f - CFactor.bb ) * Local.bb;
        pC2->ar = Local.ar;
        pC2->ag = Local.ag;
        pC2->ab = Local.ab;
        pC2->br = Local.br;
        pC2->bg = Local.bg;
        pC2->bb = Local.bb;
        break;

    case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL_ALPHA:
        ColorFactor3Func( &CFactor, &Local, &Other );
        pC->ar = CFactor.ar * -Local.ar;
        pC->ag = CFactor.ag * -Local.ag;
        pC->ab = CFactor.ab * -Local.ab;
        pC->br = CFactor.br * -Local.br;
        pC->bg = CFactor.bg * -Local.bg;
        pC->bb = CFactor.bb * -Local.bb;
        pC2->ar = pC2->ag = pC2->ab = Local.aa;
        pC2->br = pC2->bg = pC2->bb = Local.ba;
        break;
    }

    switch ( Glide.State.AlphaFunction )
    {
    case GR_COMBINE_FUNCTION_ZERO:
        pC->aa = pC->ba = 0.0f;
        break;

    case GR_COMBINE_FUNCTION_LOCAL:
    case GR_COMBINE_FUNCTION_LOCAL_ALPHA:
        pC->aa = Local.aa;
        pC->ba = Local.ba;
        break;

    case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL:
    case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL_ALPHA:
        pC->aa = ((1.0f - AlphaFactorFunc( Local.aa, Other.aa )) * Local.aa);
        pC->ba = ((1.0f - AlphaFactorFunc( Local.ba, Other.ba )) * Local.ba);
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER:
        pC->aa = (AlphaFactorFunc( Local.aa, Other.aa ) * Other.aa);
        pC->ba = (AlphaFactorFunc( Local.ba, Other.ba ) * Other.ba);
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL:
    case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA:
        pC->aa = (AlphaFactorFunc( Local.aa, Other.aa ) * Other.aa + Local.aa);
        pC->ba = (AlphaFactorFunc( Local.ba, Other.ba ) * Other.ba + Local.ba);
//      pC2->aa =  Local.aa;
//      pC2->ba =  Local.ba;
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL:
        pC->aa = (AlphaFactorFunc( Local.aa, Other.aa ) * ( Other.aa - Local.aa ));
        pC->ba = (AlphaFactorFunc( Local.ba, Other.ba ) * ( Other.ba - Local.ba ));
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL:
    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL_ALPHA:
        pC->aa = (AlphaFactorFunc( Local.aa, Other.aa ) * ( Other.aa - Local.aa ) + Local.aa);
        pC->ba = (AlphaFactorFunc( Local.ba, Other.ba ) * ( Other.ba - Local.ba ) + Local.ba);
//      pC2->aa =  Local.aa;
//      pC2->ba =  Local.ba;
        break;
    }

    if ( Glide.State.ColorCombineInvert )
    {
        pC->ar = 1.0f - pC->ar - pC2->ar;
        pC->ag = 1.0f - pC->ag - pC2->ag;
        pC->ab = 1.0f - pC->ab - pC2->ab;
        pC->br = 1.0f - pC->br - pC2->br;
        pC->bg = 1.0f - pC->bg - pC2->bg;
        pC->bb = 1.0f - pC->bb - pC2->bb;
        pC2->ar = pC2->ag = pC2->ab = 0.0f;
        pC2->br = pC2->bg = pC2->bb = 0.0f;
    }

    if ( Glide.State.AlphaInvert )
    {
        pC->aa = 1.0f - pC->aa - pC2->aa;
        pC->ba = 1.0f - pC->ba - pC2->ba;
        pC2->aa = pC2->ba = 0.0f;
    }
    
    // Z-Buffering
    if ( ( Glide.State.DepthBufferMode == GR_DEPTHBUFFER_DISABLE ) || 
         ( Glide.State.DepthBufferMode == GR_CMP_ALWAYS ) )
    {
        pV->az = 0.0f;
        pV->bz = 0.0f;
    }
    else 
    if ( OpenGL.DepthBufferType )
    {
        pV->az = a->ooz * D1OVER65536;
        pV->bz = b->ooz * D1OVER65536;
    }
    else
    {
       /*
        * For silly values of oow, depth buffering doesn't
        * seem to work, so map them to a sensible z.  When
        * games use these silly values, they probably don't
        * use z buffering anyway.
        */
        if ( a->oow > 1.0 )
        {
            pV->az = pV->bz = pV->cz = 0.9f;
        }
        else 
        if ( InternalConfig.PrecisionFixEnable )
        {
            w = 1.0f / a->oow;
            pV->az = 1.0f - (float(((*(DWORD *)&w >> 11) & 0xFFFFF) - (127 << 12)) * D1OVER65536);
            w = 1.0f / b->oow;
            pV->bz = 1.0f - (float(((*(DWORD *)&w >> 11) & 0xFFFFF) - (127 << 12)) * D1OVER65536);
        }
        else
        {
            pV->az = a->oow;
            pV->bz = b->oow;
        }
    }

    if ( ( unsnap ) &&
         ( a->x > vertex_snap_compare ) )
    {
        pV->ax = a->x - vertex_snap;
        pV->ay = a->y - vertex_snap;
        pV->bx = b->x - vertex_snap;
        pV->by = b->y - vertex_snap;
    }
    else
    {
        pV->ax = a->x;
        pV->ay = a->y;
        pV->bx = b->x;
        pV->by = b->y;
    }

    if ( OpenGL.Texture )
    {
        Textures->GetAspect( &hAspect, &wAspect );

        pTS->as = a->tmuvtx[0].sow * wAspect; // / a->oow;
        pTS->at = a->tmuvtx[0].tow * hAspect; // / a->oow;
        pTS->bs = b->tmuvtx[0].sow * wAspect; // / b->oow;
        pTS->bt = b->tmuvtx[0].tow * hAspect; // / b->oow;

        pTS->aq = pTS->bq = 0.0f;
        pTS->aoow = atmuoow;
        pTS->boow = btmuoow;
    }

    if ( InternalConfig.FogEnable )
    {
        pF->af = (float)OpenGL.FogTable[ (WORD)(1.0f / a->oow) ] * D1OVER255;
        pF->bf = (float)OpenGL.FogTable[ (WORD)(1.0f / b->oow) ] * D1OVER255;

    #ifdef OGL_DEBUG
        DEBUG_MIN_MAX( pF->af, OGLRender.MaxF, OGLRender.MinF );
        DEBUG_MIN_MAX( pF->bf, OGLRender.MaxF, OGLRender.MinF );
    #endif
    }

#ifdef OGL_DEBUG
    DEBUG_MIN_MAX( pC->ar, OGLRender.MaxR, OGLRender.MinR );
    DEBUG_MIN_MAX( pC->br, OGLRender.MaxR, OGLRender.MinR );
    
    DEBUG_MIN_MAX( pC->ag, OGLRender.MaxG, OGLRender.MinG );
    DEBUG_MIN_MAX( pC->bg, OGLRender.MaxG, OGLRender.MinG );

    DEBUG_MIN_MAX( pC->ab, OGLRender.MaxB, OGLRender.MinB );
    DEBUG_MIN_MAX( pC->bb, OGLRender.MaxB, OGLRender.MinB );

    DEBUG_MIN_MAX( pC->aa, OGLRender.MaxA, OGLRender.MinA );
    DEBUG_MIN_MAX( pC->ba, OGLRender.MaxA, OGLRender.MinA );

    DEBUG_MIN_MAX( pV->az, OGLRender.MaxZ, OGLRender.MinZ );
    DEBUG_MIN_MAX( pV->bz, OGLRender.MaxZ, OGLRender.MinZ );

    DEBUG_MIN_MAX( pV->ax, OGLRender.MaxX, OGLRender.MinX );
    DEBUG_MIN_MAX( pV->bx, OGLRender.MaxX, OGLRender.MinX );

    DEBUG_MIN_MAX( pV->ay, OGLRender.MaxY, OGLRender.MinY );
    DEBUG_MIN_MAX( pV->by, OGLRender.MaxY, OGLRender.MinY );

    DEBUG_MIN_MAX( pTS->as, OGLRender.MaxS, OGLRender.MinS );
    DEBUG_MIN_MAX( pTS->bs, OGLRender.MaxS, OGLRender.MinS );

    DEBUG_MIN_MAX( pTS->at, OGLRender.MaxT, OGLRender.MinT );
    DEBUG_MIN_MAX( pTS->bt, OGLRender.MaxT, OGLRender.MinT );
#endif

    if ( OpenGL.Texture )
    {
        glEnable( GL_TEXTURE_2D );

        Textures->MakeReady( );
    }
    else
    {
        glDisable( GL_TEXTURE_2D );
    }

    if ( OpenGL.Blend )
    {
        glEnable( GL_BLEND );
    }
    else
    {
        glDisable( GL_BLEND );
    }

    // Alpha Fix
    if ( Glide.State.AlphaOther != GR_COMBINE_OTHER_TEXTURE )
    {
        glDisable( GL_ALPHA_TEST );
    }
    else 
    {
        if ( Glide.State.AlphaTestFunction != GR_CMP_ALWAYS )
        {
            glEnable( GL_ALPHA_TEST );
        }
    }
    
    glBegin( GL_LINES );
        glColor4fv( &pC->ar );
        glSecondaryColor3fvEXT( &pC2->ar );
        glTexCoord4fv( &pTS->as );
        glFogCoordfEXT( pF->af );
        glVertex3fv( &pV->ax );

        glColor4fv( &pC->br );
        glSecondaryColor3fvEXT( &pC2->br );
        glTexCoord4fv( &pTS->bs );
        glFogCoordfEXT( pF->bf );
        glVertex3fv( &pV->bx );
    glEnd();

#ifdef OPENGL_DEBUG
    GLErro( "Render::AddLine" );
#endif
}

void RenderAddPoint( const GrVertex *a, bool unsnap )
{
    pC  = &OGLRender.TColor[ MAXTRIANGLES ];
    pC2 = &OGLRender.TColor2[ MAXTRIANGLES ];
    pV  = &OGLRender.TVertex[ MAXTRIANGLES ];
    pTS = &OGLRender.TTexture[ MAXTRIANGLES ];
    pF  = &OGLRender.TFog[ MAXTRIANGLES ];

    // Color Stuff, need to optimize it
    ZeroMemory( pC2, sizeof( TColorStruct ) );

    if ( Glide.ALocal )
    {
        switch ( Glide.State.AlphaLocal )
        {
        case GR_COMBINE_LOCAL_ITERATED:
            Local.aa = a->a * D1OVER255;
            break;

        case GR_COMBINE_LOCAL_CONSTANT:
            Local.aa = OpenGL.ConstantColor[3];
            break;

        case GR_COMBINE_LOCAL_DEPTH:
            Local.aa = a->z;
            break;
        }
    }

    if ( Glide.AOther )
    {
        switch ( Glide.State.AlphaOther )
        {
        case GR_COMBINE_OTHER_ITERATED:
            Other.aa = a->a * D1OVER255;
            break;

        case GR_COMBINE_OTHER_CONSTANT:
            Other.aa = OpenGL.ConstantColor[3];
            break;

        case GR_COMBINE_OTHER_TEXTURE:
            Other.aa = 1.0f;
            break;
        }
    }

    if ( Glide.CLocal )
    {
        switch ( Glide.State.ColorCombineLocal )
        {
        case GR_COMBINE_LOCAL_ITERATED:
            Local.ar = a->r * D1OVER255;
            Local.ag = a->g * D1OVER255;
            Local.ab = a->b * D1OVER255;
            break;

        case GR_COMBINE_LOCAL_CONSTANT:
            Local.ar = OpenGL.ConstantColor[0];
            Local.ag = OpenGL.ConstantColor[1];
            Local.ab = OpenGL.ConstantColor[2];
            break;
        }
    }

    if ( Glide.COther )
    {
        switch ( Glide.State.ColorCombineOther )
        {
        case GR_COMBINE_OTHER_ITERATED:
            Other.ar = a->r * D1OVER255;
            Other.ag = a->g * D1OVER255;
            Other.ab = a->b * D1OVER255;
            break;

        case GR_COMBINE_OTHER_CONSTANT:
            Other.ar = OpenGL.ConstantColor[0];
            Other.ag = OpenGL.ConstantColor[1];
            Other.ab = OpenGL.ConstantColor[2];
            break;

        case GR_COMBINE_OTHER_TEXTURE:
            Other.ar = Other.ag = Other.ab = 1.0f;
            break;
        }
    }

    switch ( Glide.State.ColorCombineFunction )
    {
    case GR_COMBINE_FUNCTION_ZERO:
        pC->ar = pC->ag = pC->ab = 0.0f; 
        break;

    case GR_COMBINE_FUNCTION_LOCAL:
        pC->ar = Local.ar;
        pC->ag = Local.ag;
        pC->ab = Local.ab;
        break;

    case GR_COMBINE_FUNCTION_LOCAL_ALPHA:
        pC->ar = pC->ag = pC->ab = Local.aa;
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER:
        ColorFactor3Func( &CFactor, &Local, &Other );
        pC->ar = CFactor.ar * Other.ar;
        pC->ag = CFactor.ag * Other.ag;
        pC->ab = CFactor.ab * Other.ab;
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL:
        ColorFactor3Func( &CFactor, &Local, &Other );
        pC->ar = CFactor.ar * Other.ar;
        pC->ag = CFactor.ag * Other.ag;
        pC->ab = CFactor.ab * Other.ab;
        pC2->ar = Local.ar;
        pC2->ag = Local.ag;
        pC2->ab = Local.ab;
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA:
        ColorFactor3Func( &CFactor, &Local, &Other );
        pC->ar = CFactor.ar * Other.ar;
        pC->ag = CFactor.ag * Other.ag;
        pC->ab = CFactor.ab * Other.ab;
        pC2->ar = pC2->ag = pC2->ab = Local.aa;
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL:
        ColorFactor3Func( &CFactor, &Local, &Other );
        pC->ar = CFactor.ar * (Other.ar - Local.ar);
        pC->ag = CFactor.ag * (Other.ag - Local.ag);
        pC->ab = CFactor.ab * (Other.ab - Local.ab);
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL:
        if ((( Glide.State.ColorCombineFactor == GR_COMBINE_FACTOR_TEXTURE_ALPHA ) ||
            ( Glide.State.ColorCombineFactor == GR_COMBINE_FACTOR_TEXTURE_RGB )) &&
            (  Glide.State.ColorCombineOther == GR_COMBINE_OTHER_TEXTURE ) )
        {
            pC->ar = Local.ar;
            pC->ag = Local.ag;
            pC->ab = Local.ab;
        }
        else
        {
            ColorFactor3Func( &CFactor, &Local, &Other );
            pC->ar = CFactor.ar * (Other.ar - Local.ar);
            pC->ag = CFactor.ag * (Other.ag - Local.ag);
            pC->ab = CFactor.ab * (Other.ab - Local.ab);
            pC2->ar = Local.ar;
            pC2->ag = Local.ag;
            pC2->ab = Local.ab;
        }
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL_ALPHA:
        if ((( Glide.State.ColorCombineFactor == GR_COMBINE_FACTOR_TEXTURE_ALPHA ) ||
            ( Glide.State.ColorCombineFactor == GR_COMBINE_FACTOR_TEXTURE_RGB )) &&
            (  Glide.State.ColorCombineOther == GR_COMBINE_OTHER_TEXTURE ) )
        {
            pC->ar = pC->ag = pC->ab = Local.aa;
        }
        else
        {
            ColorFactor3Func( &CFactor, &Local, &Other );
            pC->ar = CFactor.ar * (Other.ar - Local.ar);
            pC->ag = CFactor.ag * (Other.ag - Local.ag);
            pC->ab = CFactor.ab * (Other.ab - Local.ab);
            pC2->ar = pC2->ag = pC2->ab = Local.aa;
        }
        break;

    case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL:
        ColorFactor3Func( &CFactor, &Local, &Other );
        pC->ar = ( 1.0f - CFactor.ar ) * Local.ar;
        pC->ag = ( 1.0f - CFactor.ag ) * Local.ag;
        pC->ab = ( 1.0f - CFactor.ab ) * Local.ab;
        pC2->ar = Local.ar;
        pC2->ag = Local.ag;
        pC2->ab = Local.ab;
        break;

    case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL_ALPHA:
        ColorFactor3Func( &CFactor, &Local, &Other );
        pC->ar = CFactor.ar * -Local.ar;
        pC->ag = CFactor.ag * -Local.ag;
        pC->ab = CFactor.ab * -Local.ab;
        pC2->ar = pC2->ag = pC2->ab = Local.aa;
        break;
    }

    switch ( Glide.State.AlphaFunction )
    {
    case GR_COMBINE_FUNCTION_ZERO:
        pC->aa = 0.0f;
        break;

    case GR_COMBINE_FUNCTION_LOCAL:
    case GR_COMBINE_FUNCTION_LOCAL_ALPHA:
        pC->aa = Local.aa;
        break;

    case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL:
    case GR_COMBINE_FUNCTION_SCALE_MINUS_LOCAL_ADD_LOCAL_ALPHA:
        pC->aa = ((1.0f - AlphaFactorFunc( Local.aa, Other.aa )) * Local.aa);
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER:
        pC->aa = (AlphaFactorFunc( Local.aa, Other.aa ) * Other.aa);
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL:
    case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL_ALPHA:
        pC->aa = AlphaFactorFunc( Local.aa, Other.aa ) * Other.aa + Local.aa;
//      pC2->aa =  Local.aa;
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL:
        pC->aa = (AlphaFactorFunc( Local.aa, Other.aa ) * ( Other.aa - Local.aa ));
        break;

    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL:
    case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL_ADD_LOCAL_ALPHA:
        pC->aa = AlphaFactorFunc( Local.aa, Other.aa ) * ( Other.aa - Local.aa ) + Local.aa;
//      pC2->aa =  Local.aa;
        break;
    }

    if ( Glide.State.ColorCombineInvert )
    {
        pC->ar = 1.0f - pC->ar - pC2->ar;
        pC->ag = 1.0f - pC->ag - pC2->ag;
        pC->ab = 1.0f - pC->ab - pC2->ab;
        pC2->ar = pC2->ag = pC2->ab = 0.0f;
    }

    if ( Glide.State.AlphaInvert )
    {
        pC->aa = 1.0f - pC->aa - pC2->aa;
        pC2->aa = 0.0f;
    }
    
    // Z-Buffering
    if ( ( Glide.State.DepthBufferMode == GR_DEPTHBUFFER_DISABLE ) || 
         ( Glide.State.DepthBufferMode == GR_CMP_ALWAYS ) )
    {
        pV->az = 0.0f;
    }
    else 
    if ( OpenGL.DepthBufferType )
    {
        pV->az = a->ooz * D1OVER65536;
    }
    else
    {
        if ( InternalConfig.PrecisionFixEnable )
        {
            w = 1.0f / a->oow;
            pV->az = 1.0f - (float(((*(DWORD *)&w >> 11) & 0xFFFFF) - (127 << 12)) * D1OVER65536);
        }
        else
        {
            pV->az = a->oow;
        }
    }

    if ( ( unsnap ) &&
         ( a->x > vertex_snap_compare ) )
    {
        pV->ax = a->x - vertex_snap;
        pV->ay = a->y - vertex_snap;
    }
    else
    {
        pV->ax = a->x;
        pV->ay = a->y;
    }

    if ( OpenGL.Texture )
    {
        Textures->GetAspect( &hAspect, &wAspect );

        pTS->as = a->tmuvtx[0].sow * wAspect;
        pTS->at = a->tmuvtx[0].tow * hAspect;

        pTS->aq = 0.0f;
        pTS->aoow = a->oow;
    }

    if( InternalConfig.FogEnable )
    {
        pF->af = (float)OpenGL.FogTable[ (WORD)(1.0f / a->oow) ] * D1OVER255;

    #ifdef OGL_DEBUG
        DEBUG_MIN_MAX( pF->af, OGLRender.MaxF, OGLRender.MinF );
    #endif
    }

#ifdef OGL_DEBUG
    DEBUG_MIN_MAX( pC->ar, OGLRender.MaxR, OGLRender.MinR );
    
    DEBUG_MIN_MAX( pC->ag, OGLRender.MaxG, OGLRender.MinG );

    DEBUG_MIN_MAX( pC->ab, OGLRender.MaxB, OGLRender.MinB );

    DEBUG_MIN_MAX( pC->aa, OGLRender.MaxA, OGLRender.MinA );

    DEBUG_MIN_MAX( pV->az, OGLRender.MaxZ, OGLRender.MinZ );

    DEBUG_MIN_MAX( pV->ax, OGLRender.MaxX, OGLRender.MinX );

    DEBUG_MIN_MAX( pV->ay, OGLRender.MaxY, OGLRender.MinY );

    DEBUG_MIN_MAX( pTS->as, OGLRender.MaxS, OGLRender.MinS );

    DEBUG_MIN_MAX( pTS->at, OGLRender.MaxT, OGLRender.MinT );
#endif

    if ( OpenGL.Texture )
    {
        glEnable( GL_TEXTURE_2D );

        Textures->MakeReady( );
    }
    else
    {
        glDisable( GL_TEXTURE_2D );
    }

    if ( OpenGL.Blend )
    {
        glEnable( GL_BLEND );
    }
    else
    {
        glDisable( GL_BLEND );
    }

    // Alpha Fix
    if ( Glide.State.AlphaOther != GR_COMBINE_OTHER_TEXTURE )
    {
        glDisable( GL_ALPHA_TEST );
    }
    else 
    {
        if ( Glide.State.AlphaTestFunction != GR_CMP_ALWAYS )
        {
            glEnable( GL_ALPHA_TEST );
        }
    }
    
    glBegin( GL_POINTS );
        glColor4fv( &pC->ar );
        glSecondaryColor3fvEXT( &pC2->ar );
        glTexCoord4fv( &pTS->as );
        glFogCoordfEXT( pF->af );
        glVertex3fv( &pV->ax );
    glEnd();

#ifdef OPENGL_DEBUG
    GLErro( "Render::AddPoint" );
#endif
}

// Color Factor functions

void __fastcall ColorFactor3Zero( TColorStruct *Result, TColorStruct *ColorComponent, TColorStruct *OtherAlpha )
{
    Result->ar = Result->ag = Result->ab = 0.0f;
    Result->br = Result->bg = Result->bb = 0.0f;
    Result->cr = Result->cg = Result->cb = 0.0f;
}

void __fastcall ColorFactor3Local( TColorStruct *Result, TColorStruct *ColorComponent, TColorStruct *OtherAlpha )
{
    Result->ar = ColorComponent->ar;
    Result->ag = ColorComponent->ag;
    Result->ab = ColorComponent->ab;
    Result->br = ColorComponent->br;
    Result->bg = ColorComponent->bg;
    Result->bb = ColorComponent->bb;
    Result->cr = ColorComponent->cr;
    Result->cg = ColorComponent->cg;
    Result->cb = ColorComponent->cb;
}

void __fastcall ColorFactor3OtherAlpha( TColorStruct *Result, TColorStruct *ColorComponent, TColorStruct *OtherAlpha )
{
    Result->ar = Result->ag = Result->ab = OtherAlpha->aa;
    Result->br = Result->bg = Result->bb = OtherAlpha->ba;
    Result->cr = Result->cg = Result->cb = OtherAlpha->ca;
}

void __fastcall ColorFactor3LocalAlpha( TColorStruct *Result, TColorStruct *ColorComponent, TColorStruct *OtherAlpha )
{
    Result->ar = Result->ag = Result->ab = ColorComponent->aa;
    Result->br = Result->bg = Result->bb = ColorComponent->ba;
    Result->cr = Result->cg = Result->cb = ColorComponent->ca;
}

void __fastcall ColorFactor3OneMinusLocal( TColorStruct *Result, TColorStruct *ColorComponent, TColorStruct *OtherAlpha )
{
    Result->ar = 1.0f - ColorComponent->ar;
    Result->ag = 1.0f - ColorComponent->ag;
    Result->ab = 1.0f - ColorComponent->ab;
    Result->br = 1.0f - ColorComponent->br;
    Result->bg = 1.0f - ColorComponent->bg;
    Result->bb = 1.0f - ColorComponent->bb;
    Result->cr = 1.0f - ColorComponent->cr;
    Result->cg = 1.0f - ColorComponent->cg;
    Result->cb = 1.0f - ColorComponent->cb;
}

void __fastcall ColorFactor3OneMinusOtherAlpha( TColorStruct *Result, TColorStruct *ColorComponent, TColorStruct *OtherAlpha )
{
    Result->ar = Result->ag = Result->ab = 1.0f - OtherAlpha->aa;
    Result->br = Result->bg = Result->bb = 1.0f - OtherAlpha->ba;
    Result->cr = Result->cg = Result->cb = 1.0f - OtherAlpha->ca;
}

void __fastcall ColorFactor3OneMinusLocalAlpha( TColorStruct *Result, TColorStruct *ColorComponent, TColorStruct *OtherAlpha )
{
    Result->ar = Result->ag = Result->ab = 1.0f - ColorComponent->aa;
    Result->br = Result->bg = Result->bb = 1.0f - ColorComponent->ba;
    Result->cr = Result->cg = Result->cb = 1.0f - ColorComponent->ca;
}

void __fastcall ColorFactor3One( TColorStruct *Result, TColorStruct *ColorComponent, TColorStruct *OtherAlpha )
{
    Result->ar = Result->ag = Result->ab = 1.0f;
    Result->br = Result->bg = Result->bb = 1.0f;
    Result->cr = Result->cg = Result->cb = 1.0f;
}

// Alpha Factor functions

float AlphaFactorZero( float LocalAlpha, float OtherAlpha )
{
    return 0.0f;
}

float AlphaFactorLocal( float LocalAlpha, float OtherAlpha )
{
    return LocalAlpha;
}

float AlphaFactorOther( float LocalAlpha, float OtherAlpha )
{
    return OtherAlpha;
}

float AlphaFactorOneMinusLocal( float LocalAlpha, float OtherAlpha )
{
    return 1.0f - LocalAlpha;
}

float AlphaFactorOneMinusOther( float LocalAlpha, float OtherAlpha )
{
    return 1.0f - OtherAlpha;
}

float AlphaFactorOne( float LocalAlpha, float OtherAlpha )
{
    return 1.0f;
}

// Color functions

void ColorFunctionZero( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other )
{
    pC->ar = pC->ag = pC->ab = 0.0f; 
    pC->br = pC->bg = pC->bb = 0.0f; 
    pC->cr = pC->cg = pC->cb = 0.0f; 
}

void ColorFunctionLocal( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other )
{
    pC->ar = Local->ar;
    pC->ag = Local->ag;
    pC->ab = Local->ab;
    pC->br = Local->br;
    pC->bg = Local->bg;
    pC->bb = Local->bb;
    pC->cr = Local->cr;
    pC->cg = Local->cg;
    pC->cb = Local->cb;
}

void ColorFunctionLocalAlpha( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other )
{
    pC->ar = pC->ag = pC->ab = Local->aa;
    pC->br = pC->bg = pC->bb = Local->ba;
    pC->cr = pC->cg = pC->cb = Local->ca;
}

void ColorFunctionScaleOther( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other )
{
    ColorFactor3Func( &CFactor, Local, Other );
    pC->ar = CFactor.ar * Other->ar;
    pC->ag = CFactor.ag * Other->ag;
    pC->ab = CFactor.ab * Other->ab;
    pC->br = CFactor.br * Other->br;
    pC->bg = CFactor.bg * Other->bg;
    pC->bb = CFactor.bb * Other->bb;
    pC->cr = CFactor.cr * Other->cr;
    pC->cg = CFactor.cg * Other->cg;
    pC->cb = CFactor.cb * Other->cb;
}

void ColorFunctionScaleOtherAddLocal( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other )
{
    ColorFactor3Func( &CFactor, Local, Other );
    pC->ar = CFactor.ar * Other->ar;
    pC->ag = CFactor.ag * Other->ag;
    pC->ab = CFactor.ab * Other->ab;
    pC->br = CFactor.br * Other->br;
    pC->bg = CFactor.bg * Other->bg;
    pC->bb = CFactor.bb * Other->bb;
    pC->cr = CFactor.cr * Other->cr;
    pC->cg = CFactor.cg * Other->cg;
    pC->cb = CFactor.cb * Other->cb;
    pC2->ar = Local->ar;
    pC2->ag = Local->ag;
    pC2->ab = Local->ab;
    pC2->br = Local->br;
    pC2->bg = Local->bg;
    pC2->bb = Local->bb;
    pC2->cr = Local->cr;
    pC2->cg = Local->cg;
    pC2->cb = Local->cb;
}

void ColorFunctionScaleOtherAddLocalAlpha( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other )
{
    ColorFactor3Func( &CFactor, Local, Other );
    pC->ar = CFactor.ar * Other->ar;
    pC->ag = CFactor.ag * Other->ag;
    pC->ab = CFactor.ab * Other->ab;
    pC->br = CFactor.br * Other->br;
    pC->bg = CFactor.bg * Other->bg;
    pC->bb = CFactor.bb * Other->bb;
    pC->cr = CFactor.cr * Other->cr;
    pC->cg = CFactor.cg * Other->cg;
    pC->cb = CFactor.cb * Other->cb;
    pC2->ar = pC2->ag = pC2->ab = Local->aa;
    pC2->br = pC2->bg = pC2->bb = Local->ba;
    pC2->cr = pC2->cg = pC2->cb = Local->ca;
}

void ColorFunctionScaleOtherMinusLocal( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other )
{
    ColorFactor3Func( &CFactor, Local, Other );
    pC->ar = CFactor.ar * (Other->ar - Local->ar);
    pC->ag = CFactor.ag * (Other->ag - Local->ag);
    pC->ab = CFactor.ab * (Other->ab - Local->ab);
    pC->br = CFactor.br * (Other->br - Local->br);
    pC->bg = CFactor.bg * (Other->bg - Local->bg);
    pC->bb = CFactor.bb * (Other->bb - Local->bb);
    pC->cr = CFactor.cr * (Other->cr - Local->cr);
    pC->cg = CFactor.cg * (Other->cg - Local->cg);
    pC->cb = CFactor.cb * (Other->cb - Local->cb);
}

void ColorFunctionScaleOtherMinusLocalAddLocal( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other )
{
    if ((( Glide.State.ColorCombineFactor == GR_COMBINE_FACTOR_TEXTURE_ALPHA ) ||
        ( Glide.State.ColorCombineFactor == GR_COMBINE_FACTOR_TEXTURE_RGB )) &&
        (  Glide.State.ColorCombineOther == GR_COMBINE_OTHER_TEXTURE ) )
    {
        pC->ar = Local->ar;
        pC->ag = Local->ag;
        pC->ab = Local->ab;
        pC->br = Local->br;
        pC->bg = Local->bg;
        pC->bb = Local->bb;
        pC->cr = Local->cr;
        pC->cg = Local->cg;
        pC->cb = Local->cb;
    }
    else
    {
        ColorFactor3Func( &CFactor, Local, Other );
        pC->ar = CFactor.ar * (Other->ar - Local->ar);
        pC->ag = CFactor.ag * (Other->ag - Local->ag);
        pC->ab = CFactor.ab * (Other->ab - Local->ab);
        pC->br = CFactor.br * (Other->br - Local->br);
        pC->bg = CFactor.bg * (Other->bg - Local->bg);
        pC->bb = CFactor.bb * (Other->bb - Local->bb);
        pC->cr = CFactor.cr * (Other->cr - Local->cr);
        pC->cg = CFactor.cg * (Other->cg - Local->cg);
        pC->cb = CFactor.cb * (Other->cb - Local->cb);
        pC2->ar = Local->ar;
        pC2->ag = Local->ag;
        pC2->ab = Local->ab;
        pC2->br = Local->br;
        pC2->bg = Local->bg;
        pC2->bb = Local->bb;
        pC2->cr = Local->cr;
        pC2->cg = Local->cg;
        pC2->cb = Local->cb;
    }
}

void ColorFunctionScaleOtherMinusLocalAddLocalAlpha( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other )
{
    if ((( Glide.State.ColorCombineFactor == GR_COMBINE_FACTOR_TEXTURE_ALPHA ) ||
        ( Glide.State.ColorCombineFactor == GR_COMBINE_FACTOR_TEXTURE_RGB )) &&
        (  Glide.State.ColorCombineOther == GR_COMBINE_OTHER_TEXTURE ) )
    {
        pC->ar = pC->ag = pC->ab = Local->aa;
        pC->br = pC->bg = pC->bb = Local->ba;
        pC->cr = pC->cg = pC->cb = Local->ca;
    }
    else
    {
        ColorFactor3Func( &CFactor, Local, Other );
        pC->ar = CFactor.ar * (Other->ar - Local->ar);
        pC->ag = CFactor.ag * (Other->ag - Local->ag);
        pC->ab = CFactor.ab * (Other->ab - Local->ab);
        pC->br = CFactor.br * (Other->br - Local->br);
        pC->bg = CFactor.bg * (Other->bg - Local->bg);
        pC->bb = CFactor.bb * (Other->bb - Local->bb);
        pC->cr = CFactor.cr * (Other->cr - Local->cr);
        pC->cg = CFactor.cg * (Other->cg - Local->cg);
        pC->cb = CFactor.cb * (Other->cb - Local->cb);
        pC2->ar = pC2->ag = pC2->ab = Local->aa;
        pC2->br = pC2->bg = pC2->bb = Local->ba;
        pC2->cr = pC2->cg = pC2->cb = Local->ca;
    }
}

void ColorFunctionMinusLocalAddLocal( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other )
{
    ColorFactor3Func( &CFactor, Local, Other );
    pC->ar = ( 1.0f - CFactor.ar ) * Local->ar;
    pC->ag = ( 1.0f - CFactor.ag ) * Local->ag;
    pC->ab = ( 1.0f - CFactor.ab ) * Local->ab;
    pC->br = ( 1.0f - CFactor.br ) * Local->br;
    pC->bg = ( 1.0f - CFactor.bg ) * Local->bg;
    pC->bb = ( 1.0f - CFactor.bb ) * Local->bb;
    pC->cr = ( 1.0f - CFactor.cr ) * Local->cr;
    pC->cg = ( 1.0f - CFactor.cg ) * Local->cg;
    pC->cb = ( 1.0f - CFactor.cb ) * Local->cb;
    pC2->ar = Local->ar;
    pC2->ag = Local->ag;
    pC2->ab = Local->ab;
    pC2->br = Local->br;
    pC2->bg = Local->bg;
    pC2->bb = Local->bb;
    pC2->cr = Local->cr;
    pC2->cg = Local->cg;
    pC2->cb = Local->cb;
}

void ColorFunctionMinusLocalAddLocalAlpha( TColorStruct * pC, TColorStruct * pC2, TColorStruct * Local, TColorStruct * Other )
{
    ColorFactor3Func( &CFactor, Local, Other );
    pC->ar = CFactor.ar * -Local->ar;
    pC->ag = CFactor.ag * -Local->ag;
    pC->ab = CFactor.ab * -Local->ab;
    pC->br = CFactor.br * -Local->br;
    pC->bg = CFactor.bg * -Local->bg;
    pC->bb = CFactor.bb * -Local->bb;
    pC->cr = CFactor.cr * -Local->cr;
    pC->cg = CFactor.cg * -Local->cg;
    pC->cb = CFactor.cb * -Local->cb;
    pC2->ar = pC2->ag = pC2->ab = Local->aa;
    pC2->br = pC2->bg = pC2->bb = Local->ba;
    pC2->cr = pC2->cg = pC2->cb = Local->ca;
}
