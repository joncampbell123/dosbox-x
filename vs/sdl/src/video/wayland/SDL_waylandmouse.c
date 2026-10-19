/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2012 Sam Lantinga

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
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

/* DOSBox-X: Wayland cursors for SDL 1.2 */

#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "SDL_mouse.h"
#include "../../events/SDL_events_c.h"
#include "../SDL_cursor_c.h"
#include "../default_cursor.h"

#include "SDL_waylandvideo.h"
#include "SDL_waylandmouse_c.h"
#include "SDL_waylandevents_c.h"

struct WMcursor {
	struct wl_buffer *buffer;
	void *data;
	size_t size;
	int w, h;
	int hot_x, hot_y;
	int is_default;		/* SDL's arrow: the desktop's cursor theme is used instead */
};

void Wayland_FreeWMCursor(_THIS, WMcursor *cursor)
{
	if ( this->hidden->cursor == cursor ) {
		this->hidden->cursor = NULL;
	}
	if ( cursor->buffer ) {
		wl_buffer_destroy(cursor->buffer);
	}
	if ( cursor->data ) {
		munmap(cursor->data, cursor->size);
	}
	SDL_free(cursor);
}

WMcursor *Wayland_CreateWMCursor(_THIS, Uint8 *data, Uint8 *mask, int w, int h, int hot_x, int hot_y)
{
	struct SDL_PrivateVideoData *wl = this->hidden;
	struct wl_shm_pool *pool;
	WMcursor *cursor;
	Uint32 *pixels;
	int x, y, fd;
	int bytes_per_row = (w + 7) / 8;

	cursor = (WMcursor *)SDL_malloc(sizeof(*cursor));
	if ( cursor == NULL ) {
		SDL_OutOfMemory();
		return NULL;
	}
	SDL_memset(cursor, 0, sizeof(*cursor));
	cursor->w = w;
	cursor->h = h;
	cursor->hot_x = hot_x;
	cursor->hot_y = hot_y;
	cursor->is_default = (w == DEFAULT_CWIDTH && h == DEFAULT_CHEIGHT &&
		hot_x == DEFAULT_CHOTX && hot_y == DEFAULT_CHOTY &&
		SDL_memcmp(data, default_cdata, sizeof(default_cdata)) == 0 &&
		SDL_memcmp(mask, default_cmask, sizeof(default_cmask)) == 0);

	cursor->size = (size_t)w * h * 4;
	fd = Wayland_CreateShmFile(cursor->size);
	if ( fd < 0 ) {
		SDL_free(cursor);
		SDL_SetError("Couldn't create a shared memory file for the cursor");
		return NULL;
	}
	cursor->data = mmap(NULL, cursor->size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if ( cursor->data == MAP_FAILED ) {
		close(fd);
		SDL_free(cursor);
		SDL_SetError("Couldn't map shared memory for the cursor");
		return NULL;
	}

	/* data 1 = black, mask 0 = transparent (data 1 with mask 0 would be inverted, black here) */
	pixels = (Uint32 *)cursor->data;
	for ( y = 0; y < h; y++ ) {
		for ( x = 0; x < w; x++ ) {
			int bit = 0x80 >> (x & 7);
			int d = data[y * bytes_per_row + x / 8] & bit;
			int m = mask[y * bytes_per_row + x / 8] & bit;
			if ( d ) {
				pixels[y * w + x] = 0xFF000000;
			} else if ( m ) {
				pixels[y * w + x] = 0xFFFFFFFF;
			} else {
				pixels[y * w + x] = 0x00000000;
			}
		}
	}

	pool = wl_shm_create_pool(wl->shm, fd, (int32_t)cursor->size);
	cursor->buffer = wl_shm_pool_create_buffer(pool, 0, w, h, w * 4, WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);
	return cursor;
}

static struct wl_cursor_image *Wayland_GetThemeCursor(_THIS)
{
	struct SDL_PrivateVideoData *wl = this->hidden;
	struct wl_cursor *wlcursor;

	if ( wl->cursor_theme == NULL ) {
		const char *size = SDL_getenv("XCURSOR_SIZE");
		int cursor_size = size ? SDL_atoi(size) : 0;
		if ( cursor_size <= 0 ) {
			cursor_size = 24;
		}
		wl->cursor_theme = wl_cursor_theme_load(SDL_getenv("XCURSOR_THEME"), cursor_size, wl->shm);
		if ( wl->cursor_theme == NULL ) {
			return NULL;
		}
	}
	wlcursor = wl_cursor_theme_get_cursor(wl->cursor_theme, "default");
	if ( wlcursor == NULL ) {
		wlcursor = wl_cursor_theme_get_cursor(wl->cursor_theme, "left_ptr");
	}
	if ( wlcursor == NULL || wlcursor->image_count == 0 ) {
		return NULL;
	}
	return wlcursor->images[0];
}

void Wayland_ApplyCursor(_THIS)
{
	struct SDL_PrivateVideoData *wl = this->hidden;
	WMcursor *cursor = wl->cursor;
	struct wl_cursor_image *image = NULL;
	struct wl_buffer *buffer;
	int w, h, hot_x, hot_y;

	if ( wl->pointer == NULL || !wl->pointer_focus ) {
		return;
	}
	if ( cursor == NULL ) {
		wl_pointer_set_cursor(wl->pointer, wl->pointer_serial, NULL, 0, 0);
		wl_display_flush(wl->display);
		return;
	}

	if ( cursor->is_default ) {
		image = Wayland_GetThemeCursor(this);
	}
	if ( image ) {
		buffer = wl_cursor_image_get_buffer(image);
		w = image->width;
		h = image->height;
		hot_x = image->hotspot_x;
		hot_y = image->hotspot_y;
	} else {
		buffer = cursor->buffer;
		w = cursor->w;
		h = cursor->h;
		hot_x = cursor->hot_x;
		hot_y = cursor->hot_y;
	}

	if ( wl->cursor_surface == NULL ) {
		wl->cursor_surface = wl_compositor_create_surface(wl->compositor);
	}
	wl_surface_attach(wl->cursor_surface, buffer, 0, 0);
	wl_surface_damage(wl->cursor_surface, 0, 0, w, h);
	wl_surface_commit(wl->cursor_surface);
	wl_pointer_set_cursor(wl->pointer, wl->pointer_serial, wl->cursor_surface, hot_x, hot_y);
	wl_display_flush(wl->display);
}

int Wayland_ShowWMCursor(_THIS, WMcursor *cursor)
{
	this->hidden->cursor = cursor;
	Wayland_ApplyCursor(this);
	return(1);
}

/* relative mouse motion when the input is grabbed and the cursor hidden */
void Wayland_CheckMouseMode(_THIS)
{
	struct SDL_PrivateVideoData *wl = this->hidden;

	wl->relative_wanted = (this->input_grab != SDL_GRAB_OFF) && !(SDL_cursorstate & CURSOR_VISIBLE);
	Wayland_UpdateRelativeMode(this);
}

void Wayland_QuitMouse(_THIS)
{
	struct SDL_PrivateVideoData *wl = this->hidden;

	wl->cursor = NULL;
	if ( wl->cursor_surface ) {
		wl_surface_destroy(wl->cursor_surface);
		wl->cursor_surface = NULL;
	}
	if ( wl->cursor_theme ) {
		wl_cursor_theme_destroy(wl->cursor_theme);
		wl->cursor_theme = NULL;
	}
}
