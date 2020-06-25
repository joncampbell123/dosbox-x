/*
 *  Copyright (C) 2018-2020 Jon Campbell
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

extern unsigned char*		avi_io_buf;
extern unsigned char*		avi_io_read;
extern unsigned char*		avi_io_write;
extern unsigned char*		avi_io_fence;
extern size_t			avi_io_elemsize;
extern size_t			avi_io_next_adv;
extern size_t			avi_io_elemcount;
extern unsigned char*		avi_io_readfence;

unsigned char *avi_io_buffer_init(size_t structsize);
void avi_io_buffer_free();

