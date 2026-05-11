/* Functions that emulate UNIX catgets, some small DOS file functions */

/* Copyright (C) 1999,2000 Jim Hall <jhall@freedos.org> */
/* Kitten version by Tom Ehlert, heavily modified by Eric Auer 2003 */

/*
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#ifndef _CATGETS_H
#define _CATGETS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Data types */

typedef int nl_catd;

/* Functions */

#define catgets(catalog, set,message_number,message) kittengets(set,message_number,message)
#define catopen(name,flag) kittenopen(name)
#define catclose(catalog)  kittenclose()


char *  kittengets( int set_number, int message_number,char *message);
nl_catd kittenopen(char *name);
void    kittenclose (void);

int get_line (int file, char *buffer, int size);

int dos_open(char *filename, int mode);
#define open(filename,mode) dos_open(filename,mode)

int dos_read(int file, void *ptr, unsigned count);
#define read(file, ptr, count) dos_read(file,ptr,count)

int dos_write(int file, void *ptr, unsigned count);
#define write(file, ptr, count) dos_write(file,ptr,count)

void dos_close(int file);
#define close(file) dos_close(file)

#ifdef __cplusplus
}
#endif

#endif /* _CATGETS_H */
