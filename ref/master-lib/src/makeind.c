/* 92/10/9 */
/* 92/11/30 */
/* 92/12/14 */
/* 93/3/20 makeind indfile objpath options.... */
/* 93/8/13 ‰Á‘¬ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dos.h>
#include "master.h"

struct find_t dta ;

struct name_time {
	char name[14] ;
	unsigned long time ;
} ;

typedef int (*cmp_func)( const void * a, const void * b ) ;

unsigned get_filelist( const char * wildcard, unsigned seg ) {
	unsigned num_entry = 0 ;
	struct name_time far * stp = (struct name_time far *)SEG2FP(seg) ;
	if ( dos_findfirst( wildcard, 0x21 ) ) {
		do {
			_fstrcpy( stp[num_entry].name, dta.name ) ;
			stp[num_entry].time = ((unsigned long)dta.wr_date << 16) | dta.wr_time ;
			++num_entry ;
		} while ( dos_findnext() ) ;
		qsort( stp, num_entry, sizeof *stp, (cmp_func)strcmp ) ;
	}
	return num_entry ;
}

unsigned long filetime( unsigned seg, unsigned numentry, char * filename ) {
	struct name_time far * stp = (struct name_time far *)SEG2FP(seg) ;
	struct name_time far * foundp ;
	strupr( filename ) ;
	foundp = bsearch( filename, stp, numentry, sizeof *stp, (cmp_func)strcmp ) ;
	if ( foundp ) {
		return foundp->time ;
	}
	return 0 ;
}

#define MAX_FILES 1024

int main( int argc, char *argv[] ) {
	char name[256] ;
	char cmdline[256] ;
	char objname[256] ;
	int objlen ;
	int i ;
	int l ;
	char * buf, * sp ;
	char c ;
	unsigned p, size ;

	unsigned asm_seg, obj_seg ;
	unsigned asm_entry, obj_entry ;

	if ( argc < 3  ||  !file_ropen( argv[1] ) )
		return 1 ;
	size = (unsigned)file_size() ;
	buf = malloc( size ) ;
	if ( !buf )
		return 1 ;
	file_read( buf, size ) ;
	file_close() ;

	dos_setdta( &dta ) ;

	strcpy( objname, argv[2] ) ;
	objlen = strlen(objname) ;
	cmdline[0] = '\0' ;
	for ( i = 3 ; i < argc ; ++i ) {
		strcat( cmdline, argv[i] ) ;
		strcat( cmdline, " " ) ;
	}

	strcpy( objname + objlen, "*.obj" ) ;
	asm_seg = hmem_lallocate( sizeof (struct name_time) * MAX_FILES ) ;
	obj_seg = hmem_lallocate( sizeof (struct name_time) * MAX_FILES ) ;
	if ( !asm_seg || !obj_seg ) {
		return 1 ;
	}
	asm_entry = get_filelist( "*.asm", asm_seg ) ;
	obj_entry = get_filelist( objname, obj_seg ) ;


	p = 0 ;
	c = buf[p++] ;
	while ( p <= size ) {
		l = 0 ;
		do {
			name[l++] = c ;
			c = buf[p++] ;
		} while ( p < size  &&  c != ' '  &&  c != '\r' ) ;

		/* ‹ó”’‚ð”ò‚Î‚· */
		do {
			c = buf[p++] ;
		} while ( p < size  &&  (c == ' ' || c == '\r' || c == '\n') ) ;

		if ( l == 0 )
			continue ;

		name[l] = '\0' ;

		strcpy( objname + objlen, name ) ;
		sp = strchr( objname + objlen, '.' ) ;
		if ( sp )
			*sp = '\0' ;
		strcat( objname + objlen, ".obj" ) ;
		if ( filetime(asm_seg, asm_entry, name) > filetime(obj_seg, obj_entry, objname + objlen) ) {
			fputs( cmdline, stdout ) ;
			fputs( name, stdout ) ;
			putchar( ',' ) ;
			fputs( objname, stdout ) ;
			putchar( ';' ) ;
			putchar( '\n' ) ;
		}
	}
	free( buf ) ;
	return 0 ;
}
