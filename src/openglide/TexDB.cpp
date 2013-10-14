//**************************************************************
//*            OpenGLide - Glide to OpenGL Wrapper
//*             http://openglide.sourceforge.net
//*
//*           implementation of the TexDB class
//*
//*         OpenGLide is OpenSource under LGPL license
//*              Originaly made by Fabio Barros
//*      Modified by Paul for Glidos (http://www.glidos.net)
//**************************************************************

#include "glogl.h"
#include "TexDB.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

TexDB::TexDB( unsigned int MemorySize )
{
    numberOfTexSections = MemorySize / ( 32 * 1024 );

    m_first = new Record*[ numberOfTexSections ];

    for ( unsigned int i = 0; i < numberOfTexSections; i++ )
    {
        m_first[ i ] = NULL;
    }
}

TexDB::~TexDB( void )
{
    Record * r;

    for ( unsigned int i = 0; i < numberOfTexSections; i++ )
    {
        r = m_first[ i ];
        
        while ( r != NULL )
        {
            Record * tmp = r;
            r = r->next;
            delete tmp;
        }
    }
    delete[] m_first;
}

GrTexInfo * TexDB::Find( FxU32 startAddress, GrTexInfo *info, FxU32 hash, 
                  GLuint *pTexNum, GLuint *pTex2Num, bool *pal_change )
{
    Record  * r;
    FxU32   sect = startAddress / ( 32 * 1024 );

    for ( r = m_first[ sect ]; r != NULL; r = r->next )
    {
        if ( r->Match( startAddress, info, ( pal_change == NULL ) ? hash : 0 ) )
        {
            *pTexNum = r->texNum;

            if ( pTex2Num )
            {
                *pTex2Num = r->tex2Num;
            }

            if ( ( pal_change != NULL ) && ( r->hash != hash ) )
            {
                r->hash = hash;
                *pal_change = true;
            }

#ifdef OGL_UTEX
            GlideMsg( "Found tex %d\n", r->texNum );
#endif
            return &r->info;
        }
    }

#ifdef OGL_UTEX
    GlideMsg( "Tex not found\n" );
#endif

    return NULL;
}

void TexDB::WipeRange(FxU32 startAddress, FxU32 endAddress, FxU32 hash)
{
    Record  ** p;
    FxU32   stt_sect;
    FxU32   end_sect;

    stt_sect = startAddress / ( 32 * 1024 );

   /*
    * Textures can be as large as 128K, so
    * one that starts 3 sections back can
    * extend into this one.
    */
    if ( stt_sect < 4 )
    {
        stt_sect = 0;
    }
    else
    {
        stt_sect -= 4;
    }
 
    end_sect = endAddress / ( 32 * 1024 );

    if ( end_sect >= numberOfTexSections )
    {
        end_sect = numberOfTexSections - 1;
    }

    for ( FxU32 i = stt_sect; i <= end_sect; i++ )
    {
        p = &( m_first[ i ] );
        while ( *p != NULL )
        {
            Record * r = *p;
            
            if ( ( startAddress < r->endAddress ) && 
                 ( r->startAddress < endAddress ) && 
                 ( ( hash == 0 ) || ( r->hash == hash ) ) )
            {
                *p = r->next;
#ifdef OGL_UTEX
                GlideMsg( "Wipe tex %d\n", r->texNum );
#endif
                delete r;
            }
            else
            {
                p = &(r->next);
            }
        }
    }
}

void TexDB::Add( FxU32 startAddress, FxU32 endAddress, GrTexInfo *info, FxU32 hash, GLuint *pTexNum, GLuint *pTex2Num )
{
    Record  *r = new Record( pTex2Num != NULL );
    FxU32   sect;

    sect = startAddress / ( 32 * 1024 );

    r->startAddress = startAddress;
    r->endAddress = endAddress;
    r->info = *info;
    r->hash = hash;

    r->next = m_first[ sect ];
    m_first[ sect ] = r;

#ifdef OGL_UTEX
    GlideMsg( "Add tex %d\n", r->texNum );
#endif

    *pTexNum = r->texNum;

    if ( pTex2Num )
    {
        *pTex2Num = r->tex2Num;
    }
}


void TexDB::Clear( void )
{
    Record  * r;

    for ( unsigned int i = 0; i < numberOfTexSections; i++ )
    {
        r = m_first[ i ];
        
        while ( r != NULL )
        {
            Record *tmp = r;
            r = r->next;
            delete tmp;
        }

        m_first[ i ] = NULL;
    }
}

// TexDB::Record Class implementation

TexDB::Record::Record( bool two_tex )
{
   glGenTextures( 1, &texNum );

   if ( two_tex )
   {
         glGenTextures( 1, &tex2Num );
   }
   else
   {
         tex2Num = 0;
   }
}

TexDB::Record::~Record( void )
{
   glDeleteTextures( 1, &texNum );

   if ( tex2Num != 0 )
   {
         glDeleteTextures( 1, &tex2Num );
   }
}

bool TexDB::Record::Match( FxU32 stt, GrTexInfo *inf, FxU32 h )
{
   return (startAddress == stt
         && inf->largeLod == info.largeLod
         && inf->aspectRatio == info.aspectRatio
         && inf->format == info.format
         && (hash == h || h == 0));
}
