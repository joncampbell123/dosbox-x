/*
 *  gui_tk - framework-agnostic GUI toolkit
 *  Copyright (C) 2005-2013 Jörg Walter
 *
 *  gui_tk is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  gui_tk is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

/* TODO:
  - make menu a bufferedwindow with shadow
*/

/** \file
 *  \brief Implementation file for gui_tk.
 *
 *  It contains implementations for all non-inlined class methods.
 *
 *  Also contained is a small test program that demonstrates all features of
 *  gui_tk. It is enabled by defining the preprocessor macro TESTING.
 */

#include "config.h"
#include "dosbox.h"

#include <SDL.h>
#include "gui_tk.h"

namespace GUI {

/* start <= y < stop, region reserved for top level window title bar */
int titlebar_y_start = 5;
int titlebar_y_stop = 25;

/* region where title bar is drawn */
int titlebox_y_start = 4;
int titlebox_y_height = 20;

/* width of the system menu */
int titlebox_sysmenu_width = 20; // includes black divider line

namespace Color {
	RGB Background3D =		0xffc0c0c0;
	RGB Light3D =			0xfffcfcfc;
	RGB Shadow3D =			0xff808080;
	RGB Border =			0xff000000;
	RGB Text =			0xff000000;
	RGB Background =		0xffc0c0c0;
	RGB SelectionBackground =	0xff000080;
	RGB SelectionForeground =	0xffffffff;
	RGB EditableBackground =	0xffffffff;
	RGB Titlebar =			0xffa4c8f0;
	RGB TitlebarText =		0xff000000;
	RGB TitlebarInactive =			0xffffffff;
	RGB TitlebarInactiveText =		0xff000000;
}

std::map<const char *,Font *,Font::ltstr> Font::registry;

bool ToplevelWindow::mouseDoubleClicked(int x, int y, MouseButton button) {
	if (button == Left && x < (6+titlebox_sysmenu_width) && x > 6 && y >= titlebar_y_start && y < titlebar_y_stop) {
		systemMenu->executeAction("Close");
		return true;
	}
	BorderedWindow::mouseClicked(x,y,button);
	return true;
}

void Drawable::drawText(const String& text, bool interpret, Size start, Size len) {
	if (interpret) {
		if (len > text.size()-start) len = (Size)(text.size()-start);
		len += start;

		Size wordstart = start;
		int width = 0;

		while (start < len) {
			switch (font->toSpecial(text[start])) {
			case Font::CR:
				if (wordstart != start) {
					drawText(text,false,wordstart,start-wordstart);
					wordstart = start;
					width = 0;
				}
				wordstart++;
				gotoXY(0,y);
				break;
			case Font::LF:
				if (wordstart != start) {
					drawText(text,false,wordstart,start-wordstart);
					wordstart = start;
					width = 0;
				}
				wordstart++;
				gotoXY(0,y+font->getHeight());
				break;
			case Font::BS:
				if (wordstart != start) {
					drawText(text,false,wordstart,start-wordstart);
					wordstart = start;
					width = 0;
				}
				wordstart++;
				gotoXY(imax(0,x-font->getWidth()),y);
				break;
			case Font::Tab:
				if (wordstart != start) {
					drawText(text,false,wordstart,start-wordstart);
					wordstart = start;
					width = 0;
				}
				wordstart++;
				gotoXY((((int)(x/font->getWidth()/8))+1)*8*font->getWidth(),y);
				break;
			case Font::Space:
				if (wordstart != start) {
					drawText(text,false,wordstart,start-wordstart);
					wordstart = start;
					width = 0;
				}
				wordstart++;
				font->drawString(this,text,start,1);
				break;
			case Font::ESC: // ignore ANSI sequences except for colors
				if (wordstart != start) {
					drawText(text,false,wordstart,start-wordstart);
					wordstart = start;
					width = 0;
				}
				wordstart++;
				do {
					Size seqstart = start+1u;
					Char c;
					do {
						start++;
						wordstart++;
						c = font->toSpecial(text[start]);
					} while (start < len && ((c >= '0' && c <= '9') || c == ';' || c == '['));
					if (c == 'm' && start < len) {
						if (text[seqstart++] != '[') break;
						c = font->toSpecial(text[seqstart++]);
						while (c != 'm') {
							unsigned int param = 0;
							if (c == ';') c = '0';
							while (c != 'm' && c != ';') {
								param = param * 10u + (unsigned int)c - '0';
								c = font->toSpecial(text[seqstart++]);
							}
							const RGB bright = 0x00808080u;
							const RGB intensity = (color&bright?~0u:~bright);
							switch (param) {
							case 0: setColor(Color::Black); break;
							case 1: setColor(color | 0x00808080u); break;
							case 30: setColor((Color::Black|bright) & intensity); break;
							case 31: setColor(Color::Red & intensity); break;
							case 32: setColor(Color::Green & intensity); break;
							case 33: setColor(Color::Yellow & intensity); break;
							case 34: setColor(Color::Blue & intensity); break;
							case 35: setColor(Color::Magenta & intensity); break;
							case 36: setColor(Color::Cyan & intensity); break;
							case 37: setColor(Color::White & intensity); break;
							default: break;
							}
						}
					}
				} while (0);
			default:
				width += font->getWidth(text[start]);
				if (x > 0 && x+width > (this->fw)) gotoXY(0,y+font->getHeight());
			}
			start++;
		}
		if (wordstart != start) drawText(text,false,wordstart,start-wordstart);
		return;
	}

	font->drawString(this,text,start,len);
}

bool ToplevelWindow::mouseDown(int x, int y, MouseButton button) {
	if (button == Left && x >= (6+titlebox_sysmenu_width) && x < width-6 && y >= titlebar_y_start && y < titlebar_y_stop) {
		dragx = x;
		dragy = y;
		mouseChild = NULL;
		systemMenu->setVisible(false);
		return true;
	} else if (button == Left && x < (6+titlebox_sysmenu_width) && x >= 6 && y >= titlebar_y_start && y < titlebar_y_stop) {
		mouseChild = NULL;
		raise();
		systemMenu->setVisible(!systemMenu->isVisible());
		return true;
	}
	systemMenu->setVisible(false);
	BorderedWindow::mouseDown(x,y,button);
	return true;
}

Drawable::Drawable(int w, int h, RGB clear) :
	buffer(new RGB[w*h]),
	width(w), height(h),
	owner(true),
	color(Color::Black),
	font(NULL),
	lineWidth(1),
	tx(0), ty(0),
	cx(0), cy(0),
	cw(w), ch(h),
    fw(w), fh(h),
	x(0), y(0)
{
	this->clear(clear);
}

Drawable::Drawable(Drawable &src, RGB clear) :
	buffer(new RGB[src.cw*src.ch]),
	width(src.cw), height(src.ch),
	owner(true),
	color(src.color),
	font(src.font),
	lineWidth(src.lineWidth),
	tx(0), ty(0),
	cx(0), cy(0),
	cw(src.cw), ch(src.ch),
    fw(src.fw), fh(src.fh),
	x(src.x), y(src.y)
{
	if (clear != 0) {
		this->clear(clear);
	} else {
		for (unsigned int h = 0; (int)h < src.ch; h++) {
			memcpy(buffer+(size_t)src.cw*h,src.buffer+src.width*(h+(size_t)src.ty)+src.tx,4u*(size_t)src.cw);
		}
	}
}

Drawable::Drawable(Drawable &src, int x, int y, int w, int h) :
	buffer(src.buffer),
	width(src.width), height(src.height),
	owner(false),
	color(src.color),
	font(src.font),
	lineWidth(src.lineWidth),
	tx(src.tx+x), ty(src.ty+y),
	cx(imax(imax(-x,src.cx-x),0)), cy(imax(imax(-y,src.cy-y),0)),
	cw(imax(0,imin(src.cw-x,w))), ch(imax(0,imin(src.ch-y,h))),
	fw(w), fh(h),
	x(imax(0,imin(src.tx-tx,cw))), y(imax(0,imin(src.ty-ty,cw)))
{
}

Drawable::~Drawable()
{
	if (owner) delete[] buffer;
}

void Drawable::clear(RGB clear)
{
	for (int y = cy; y < ch; y++) {
		for (int x = cx; x < cw; x++) {
			buffer[(y+ty)*width+x+tx] = clear;
		}
	}
}

void Drawable::drawDotLine(int x2, int y2)
{
	int x0 = x2, x1 = x, y0 = y2, y1 = y;
	int dx = x2-x1, dy = y2-y1;
	drawPixel();

	if (abs(dx) > abs(dy)) {
		if (x1 > x2) {
			x = x2; x2 = x1; x1 = x;
			y = y2; y2 = y1; y1 = y;
		}
		for (x = x1; x <= x2; x++) {
			y = y1+(x-x1)*dy/dx-lineWidth/2;
			for (int i = 0; i < lineWidth; i++, y++) {
                if (((x^y)&1) == 0)
                    drawPixel();
            }
        }
    } else if (y1 != y2) {
        if (y1 > y2) {
            x = x2; x2 = x1; x1 = x;
            y = y2; y2 = y1; y1 = y;
        }
        for (y = y1; y <= y2; y ++) {
            x = x1+(y-y1)*dx/dy-lineWidth/2;
            for (int i = 0; i < lineWidth; i++, x++) {
                if (((x^y)&1) == 0)
                    drawPixel();
            }
		}
	}

	drawPixel(x0,y0);
}

void Drawable::drawLine(int x2, int y2)
{
	int x0 = x2, x1 = x, y0 = y2, y1 = y;
	int dx = x2-x1, dy = y2-y1;
	drawPixel();

	if (abs(dx) > abs(dy)) {
		if (x1 > x2) {
			x = x2; x2 = x1; x1 = x;
			y = y2; y2 = y1; y1 = y;
		}
		for (x = x1; x <= x2; x++) {
			y = y1+(x-x1)*dy/dx-lineWidth/2;
			for (int i = 0; i < lineWidth; i++, y++) {
				drawPixel();
			}
		}
	} else if (y1 != y2) {
		if (y1 > y2) {
			x = x2; x2 = x1; x1 = x;
			y = y2; y2 = y1; y1 = y;
		}
		for (y = y1; y <= y2; y ++) {
			x = x1+(y-y1)*dx/dy-lineWidth/2;
			for (int i = 0; i < lineWidth; i++, x++) {
				drawPixel();
			}
		}
	}

	drawPixel(x0,y0);
}

void Drawable::drawCircle(int d) {
	int xo = 0, yo = d/2, rest = (d+1)/2-yo, x0 = x, y0 = y, rsq = d*d/4, lwo = lineWidth/2;

	while (xo <= yo) {
		while (xo*xo+(2*yo-1)*(2*yo-1)/4 > rsq) yo--;
		for (int i = 0, yow = yo+lwo; i < lineWidth; i++, yow--) {
			drawPixel(x0+xo,y0-yow-rest);
			drawPixel(x0+yow,y0-xo-rest);
			drawPixel(x0+yow,y0+xo);
			drawPixel(x0+xo,y0+yow);

			drawPixel(x0-xo-rest,y0-yow-rest);
			drawPixel(x0-yow-rest,y0-xo-rest);
			drawPixel(x0-yow-rest,y0+xo);
			drawPixel(x0-xo-rest,y0+yow);
		}

		xo++;
	}
	gotoXY(x0,y0);
}

void Drawable::drawRect(int w, int h)
{
	gotoXY(x-lineWidth/2,y);
	drawLine(x+w+lineWidth-1,y);
	gotoXY(x-(lineWidth-1)/2,y);
	drawLine(x,y+h);
	gotoXY(x+(lineWidth-1)/2,y);
	drawLine(x-w-lineWidth+1,y);
	gotoXY(x+lineWidth/2,y);
	drawLine(x,y-h);
}

void Drawable::drawDotRect(int w, int h)
{
	gotoXY(x-lineWidth/2,y);
	drawDotLine(x+w+lineWidth-1,y);
	gotoXY(x-(lineWidth-1)/2,y);
	drawDotLine(x,y+h);
	gotoXY(x+(lineWidth-1)/2,y);
	drawDotLine(x-w-lineWidth+1,y);
	gotoXY(x+lineWidth/2,y);
	drawDotLine(x,y-h);
}

void Drawable::fill()
{
	int x0 = x, xmin;
	RGB color = getPixel();

	if (color == this->color) return;

	for (x--; x >= 0 && getPixel() == color; x--) drawPixel();
	xmin = ++x;
	for (x = x0; x < cw && getPixel() == color; x++) drawPixel();
	y++;
	for (x--; x >= xmin; x--) {
		if (getPixel() == color) fill();
		y -= 2;
		if (getPixel() == color) fill();
		y += 2;
	}
	y--;
	x = x0;
}

void Drawable::fillCircle(int d)
{
	int xo = 0, yo = d/2, rest = (d+1)/2-yo, x0 = x, y0 = y, rsq = d*d/4;

	while (xo <= yo) {
		while (xo*xo+(2*yo-1)*(2*yo-1)/4 > rsq) yo--;
		x = x0+xo;
		for (y = y0-yo-rest; y <= y0+yo; y++) drawPixel();
		x = x0-xo-rest;
		for (y = y0-yo-rest; y <= y0+yo; y++) drawPixel();

		y = y0-xo-rest;
		for (x = x0-yo-rest; x <= x0+yo; x++) drawPixel();
		y = y0+xo;
		for (x = x0-yo-rest; x <= x0+yo; x++) drawPixel();

		xo++;
	}
	gotoXY(x0,y0);
}

void Drawable::fillRect(int w, int h)
{
	int x0 = x, y0 = y, w0 = w;
	for (; h > 0; h--, y++) {
		for (x = x0, w = w0; w > 0; w--, x++) {
			drawPixel();
		}
	}
	gotoXY(x0,y0);
}

void Drawable::drawDrawable(Drawable &d, unsigned char alpha)
{
	int scw = d.cw, sch = d.ch, w, h;
	RGB *src, *dest;

	for (h = imax(d.cy,-ty-y); h < sch && y+h < ch; h++) {
		src = d.buffer+d.width*((size_t)h+d.ty)+d.tx;
		dest = buffer+width*((size_t)y+ty+h)+tx+x;
		for (w = imax(d.cx,-tx-x); w < scw && x+w < cw; w++) {
			RGB srcb = src[w], destb = dest[w];
			unsigned int sop = Color::A(srcb)*((unsigned int)alpha)/255;
			unsigned int rop = Color::A(destb) + sop - Color::A(destb)*sop/255;
			if (rop == 0) {
				dest[w] = Color::Transparent;
			} else {
				unsigned int dop = Color::A(destb)*(255-sop)/255;
				unsigned int magval = ((destb&Color::MagentaMask)*dop+(srcb&Color::MagentaMask)*sop);
				dest[w] = (((magval&0xffff)/rop)&Color::BlueMask) |
					(((magval&0xffff0000)/rop)&Color::RedMask) |
					((((destb&Color::GreenMask)*dop+(srcb&Color::GreenMask)*sop)/rop)&Color::GreenMask) |
					(rop<<Color::AlphaShift);
			}
		}
	}
}

void Drawable::drawText(const Char c, bool interpret)
{
	if (interpret) {
		switch (font->toSpecial(c)) {
		case Font::CR: gotoXY(0,y); return;
		case Font::LF: gotoXY(0,y+font->getHeight()); return;
		case Font::BS: gotoXY(imax(0,x-font->getWidth()),y); return;
		case Font::Tab: gotoXY((((int)(x/font->getWidth()/8))+1)*8*font->getWidth(),y); return;
		default: break;
		}
		if (font->getWidth(c)+x > (this->fw)) gotoXY(0,y+font->getHeight());
	}
	font->drawChar(this,c);
}

void BitmapFont::drawChar(Drawable *d, const Char c) const {
#define move(x) (ptr += ((x)+bit)/8-(((x)+bit)<0), bit = ((x)+bit+(((x)+bit)<0?8:0))%8)
	const unsigned char *ptr = bitmap;
	int bit = 0;

	if (c > last) return;

	if (char_position != NULL) {
		ptr = char_position[c];
		bit = 0;
	} else {
		move(character_step*((int)c));
	}

	int rs = row_step;
	int w = (widths != NULL?widths[c]:width);
	int h = (ascents != NULL?ascents[c]:height);
	Drawable out(*d,d->getX(),d->getY()-ascent,w,h);

	if (rs == 0) rs = isign(col_step)*w;
	if (rs < 0) move(-rs*(h-1));
	if (col_step < 0) move(abs(rs)-1);

	for (int row = height-h; row < height; row++, move(rs-w*col_step)) {
		for (int col = 0; col < w; col++, move(col_step)) {
			if (!background_set != !(*ptr&(1<<bit)))
				out.drawPixel(col,row);
		}
	}
	d->gotoXY(d->getX()+w,d->getY());
#undef move
}

void Timer::check_to(unsigned int ticks) {
	if (ticks >= Timer::ticks) check(ticks - Timer::ticks);
}

void Timer::check(unsigned int ticks)
{
	if (timers.empty()) {
		Timer::ticks += ticks;
		return;
	}

	if (Timer::ticks > (Timer::ticks+ticks)) {
		ticks -= (unsigned int)(-(int)Timer::ticks) - 1u;
		check((unsigned int)(-(int)Timer::ticks) - 1u);
	}

	std::multimap<unsigned int,Timer_Callback*,Timer::ltuint>::iterator old, i = timers.lower_bound(Timer::ticks+1);
	Timer::ticks += ticks;

	while (i != timers.end() && (*i).first <= Timer::ticks) {
		Timer_Callback *c = (*i).second;
		unsigned int time = (*i).first;
		old = i;
		++i;
		timers.erase(old);
		unsigned int next = c->timerExpired(time);
		if (next) add(c, time+next-Timer::ticks);
	}
}

void Timer::remove(const Timer_Callback *const timer)
{
	if (timers.empty()) return;

	std::multimap<unsigned int,Timer_Callback*,Timer::ltuint>::iterator old, i = timers.begin();

	while (i != timers.end()) {
		old = i;
		++i;
		if ((*old).second == timer) timers.erase(old);
	}
}

unsigned int Timer::next()
{
	if (timers.empty()) return 0;

	std::multimap<unsigned int,Timer_Callback*,Timer::ltuint>::iterator i = timers.upper_bound(ticks);

	if (i == timers.end()) return 0;
	return (*i).first-Timer::ticks;
}

std::multimap<unsigned int,Timer_Callback*,Timer::ltuint> Timer::timers;
unsigned int Timer::ticks = 0;

BitmapFont::BitmapFont(const unsigned char *data, int height, int ascent, bool owner,
		int width, bool background_set,
		int col_step, int row_step, int character_step, Char last,
		const int *widths, const int *ascents, const unsigned char *const* char_position,
		const Font::SpecialChar *special) :
		bitmap(data),
		width(width), height(height), ascent(ascent), widths(widths), ascents(ascents),
		background_set(background_set), col_step(col_step), row_step(row_step),
		character_step(character_step?character_step:abs((row_step?row_step:width*col_step)*height)),
		char_position(char_position), special(special), owner(owner), last(last)
{
}

BitmapFont::~BitmapFont() {
	if (owner) {
		if (bitmap != NULL) delete bitmap;
		if (ascents != NULL) delete ascents;
		if (widths != NULL) delete widths;
		if (special != NULL) delete special;
	}
}

Window::Window(Window *parent, int x, int y, int w, int h) :
	width(w), height(h),
	x(x), y(y),
	dirty(true),
	visible(true),
    tabbable(true),
	parent(parent),
	mouseChild(NULL),
    transient(false),
    toplevel(false),
    mouse_in_window(false),
    first_tabbable(false),
    last_tabbable(false)
{
	parent->addChild(this);
}

Window::Window() :
	width(0), height(0),
	x(0), y(0),
	dirty(false),
	visible(true),
    tabbable(true),
	parent(NULL),
	mouseChild(NULL),
    transient(false),
    toplevel(false),
    mouse_in_window(false)
{
}


Window::~Window()
{
    // WARNING: Child windows will call parent->removeChild() each child we delete here.
    //          That means the list is modified per call, therefore it's not safe to
    //          blindly iterate over the children list.
    //
    //          Furthermore, modifying the code so that removeChild() does nothing during
    //          the destructor, and then iterating over the children list normally and
    //          deleting the child windows, works OK except for a segfault when the user
    //          uses the "Close" menu item from the system menu.
    //
    //          This code is a good framework for the GUI but a knotted mess when it comes
    //          to pointer ownership and when things are valid.
	while (!children.empty()) delete children.front();
	if (parent) parent->removeChild(this);
	if (parent && parent->mouseChild == this) parent->mouseChild = NULL;
}

void Window::addChild(Window *child)
{
	children.push_back(child);
	setDirty();
}

void Window::removeChild(Window *child)
{
	children.remove(child);
	setDirty();
}

void Window::move(int x, int y)
{
	this->x = x;
	this->y = y;
	std::list<Window_Callback*>::iterator i = movehandlers.begin();
	bool end = (i == movehandlers.end());
	while (!end) {
		Window_Callback *c = *i;
		++i;
		end = (i == movehandlers.end());
		c->windowMoved(this,x,y);
	}
	parent->setDirty();
}

void Window::resize(int w, int h)
{
	this->width = w;
	this->height = h;
	setDirty();
}

void Window::paintAll(Drawable &d) const
{
	paint(d);
	std::list<Window *>::const_iterator i = children.begin();
	while (i != children.end()) {
		Window *child = *i;
		++i;
		if (child->visible) {
			Drawable *cd = new Drawable(d,child->x,child->y,child->width,child->height);
			child->paintAll(*cd);
			delete cd;
		}
	}
}

bool Window::keyDown(const Key &key)
{
	if (children.empty()) return false;
	if ((*children.rbegin())->keyDown(key)) return true;
	if (key.ctrl || key.alt || key.windows || key.special != Key::Tab) return false;

	if (key.shift) {
		std::list<Window *>::reverse_iterator i = children.rbegin(), e = children.rend();
		++i;
        while (i != e) {
            if ((*i)->tabbable) {
                if ((*i)->raise())
                    break;
            }

            ++i;
        }
        return (i != e) || toplevel/*prevent TAB escape to another window*/;
	} else {
		std::list<Window *>::iterator i = children.begin(), e = children.end();
        --e;
		while (i != e) {
            if ((*i)->tabbable) {
                if ((*i)->raise())
                    break;
            }

            ++i;
        }
		return (i != e) || toplevel/*prevent TAB escape to another window*/;
	}
}

void WindowInWindow::scrollToWindow(Window *child) {
    if (child->parent != this) {
        fprintf(stderr,"BUG: scrollToWindow given a window not a child of this parent\n");
        return;
    }

    int xadj = -scroll_pos_x;
    int yadj = -scroll_pos_y;
    int bw = width;
    int bh = height;

    if (border) {
        bw -= 2 + (vscroll?vscroll_display_width:0);
        bh -= 2;
    }

    int rx = child->getX() + xadj;
    int ry = child->getY() + yadj;

    if (rx < 0)
        scroll_pos_x += rx;
    if (ry < 0)
        scroll_pos_y += ry;

    {/*UNTESTED*/
        int ext = (rx+child->getWidth()) - bw;
        if (ext > 0) scroll_pos_x += ext;
    }

    {
        int ext = (ry+child->getHeight()) - bh;
        if (ext > 0) scroll_pos_y += ext;
    }

    if (scroll_pos_x < 0) scroll_pos_x = 0;
    if (scroll_pos_y < 0) scroll_pos_y = 0;
    if (scroll_pos_x > scroll_pos_w) scroll_pos_x = scroll_pos_w;
    if (scroll_pos_y > scroll_pos_h) scroll_pos_y = scroll_pos_h;
}

bool WindowInWindow::keyDown(const Key &key)
{
	if (children.empty()) return false;
	if ((*children.rbegin())->keyDown(key)) return true;
    if (dragging || vscroll_dragging) return true;

    if (key.special == Key::Up) {
        scroll_pos_y -= 64;
        if (scroll_pos_y < 0) scroll_pos_y = 0;
        if (scroll_pos_y > scroll_pos_h) scroll_pos_y = scroll_pos_h;
        return true;
    }

    if (key.special == Key::Down) {
        scroll_pos_y += 64;
        if (scroll_pos_y < 0) scroll_pos_y = 0;
        if (scroll_pos_y > scroll_pos_h) scroll_pos_y = scroll_pos_h;
        return true;
    }

    if (key.special == Key::PageUp) {
        scroll_pos_y -= height - 16;
        if (scroll_pos_y < 0) scroll_pos_y = 0;
        if (scroll_pos_y > scroll_pos_h) scroll_pos_y = scroll_pos_h;
        return true;
    }

    if (key.special == Key::PageDown) {
        scroll_pos_y += height - 16;
        if (scroll_pos_y < 0) scroll_pos_y = 0;
        if (scroll_pos_y > scroll_pos_h) scroll_pos_y = scroll_pos_h;
        return true;
    }

	if (key.ctrl || key.alt || key.windows || key.special != Key::Tab) return false;

    bool tab_quit = false;

	if (key.shift) {
		std::list<Window *>::reverse_iterator i = children.rbegin(), e = children.rend();
		++i;
        while (i != e) {
            if ((*i)->last_tabbable)
                tab_quit = true;

            if ((*i)->tabbable) {
                // WARNING: remember raise() changes the order of children, therefore using
                //          *i after raise() is invalid (stale reference)
                scrollToWindow(*i);
                if ((*i)->raise())
                    break;
            }

            ++i;
        }
        if (tab_quit) return false;
        return (i != e) || toplevel/*prevent TAB escape to another window*/;
    } else {
        std::list<Window *>::iterator i = children.begin(), e = children.end();
        --e;
        while (i != e) {
            if ((*i)->first_tabbable)
                tab_quit = true;

            if ((*i)->tabbable) {
                // WARNING: remember raise() changes the order of children, therefore using
                //          *i after raise() is invalid (stale reference)
                scrollToWindow(*i);
                if ((*i)->raise())
                    break;
            }

            ++i;
        }
        if (tab_quit) return false;
		return (i != e) || toplevel/*prevent TAB escape to another window*/;
	}
}

bool Window::keyUp(const Key &key)
{
	if (children.empty()) return false;
	return (*children.rbegin())->keyUp(key);
}

void Window::mouseMovedOutside(void) {
}

bool Window::mouseMoved(int x, int y)
{
	std::list<Window *>::reverse_iterator i = children.rbegin();
	bool end = (i == children.rend());
	while (!end) {
		Window *w = *i;
		++i;
		end = (i == children.rend());
		if (w->visible && x >= w->x && x <= w->x+w->width
			&& y >= w->y && y <= w->y+w->height
			&& w->mouseMoved(x-w->x, y-w->y)) {
            w->mouse_in_window = true;
            return true;
        }
        else if (w->mouse_in_window) {
            w->mouse_in_window = false;
            w->mouseMovedOutside();
        }
	}

	return false;
}

bool Window::mouseDragged(int x, int y, MouseButton button)
{
	if (mouseChild == NULL) return false;
	return mouseChild->mouseDragged(x-mouseChild->x, y-mouseChild->y, button);
}

bool Window::mouseDownOutside(MouseButton button) {
	std::list<Window *>::reverse_iterator i = children.rbegin();
    bool handled = false;

	while (i != children.rend()) {
		Window *w = *i;

		if (w->hasFocus()) {
			if (w->mouseDownOutside(button))
				handled = true;
		}

		++i;
	}

    return handled;
}

bool Window::mouseDown(int x, int y, MouseButton button)
{
	std::list<Window *>::reverse_iterator i = children.rbegin();
    bool handled = false;
    bool doraise = !(button == GUI::WheelUp || button == GUI::WheelDown); /* do not raise if the scroll wheel */
	Window *last = NULL;

	while (i != children.rend()) {
		Window *w = *i;

		if (w->visible && x >= w->x && x < (w->x+w->width) && y >= w->y && y < (w->y+w->height)) {
            if (handled) {
                mouseChild = NULL;
                return true;
            }
			mouseChild = last = w;
			if (w->mouseDown(x-w->x, y-w->y, button)) {
                if (doraise) w->raise();
				return true;
			}
		}
        else if (w->transient) {
            handled |= w->mouseDownOutside(button);
        }

		++i;
	}

	mouseChild = NULL;
	if (last != NULL && doraise) last->raise();
	return handled;
}

bool Window::mouseUp(int x, int y, MouseButton button)
{
	if (mouseChild == NULL) return false;
	return mouseChild->mouseUp(x-mouseChild->x, y-mouseChild->y, button);
}

bool Window::mouseClicked(int x, int y, MouseButton button)
{
	if (mouseChild == NULL) return false;
	return mouseChild->mouseClicked(x-mouseChild->x, y-mouseChild->y, button);
}

bool Window::mouseDoubleClicked(int x, int y, MouseButton button)
{
	if (mouseChild == NULL) return false;
	return mouseChild->mouseDoubleClicked(x-mouseChild->x, y-mouseChild->y, button);
}

bool BorderedWindow::mouseDown(int x, int y, MouseButton button)
{
	mouseChild = NULL;
	if (x > width-border_right || y > height-border_bottom) return false;
	x -= border_left; y -= border_top;
	if (x < 0 || y < 0) return false;
	return Window::mouseDown(x,y,button);
}

bool BorderedWindow::mouseUp(int x, int y, MouseButton button) {
	if (mouseChild == NULL && (x > width-border_right || y > height-border_bottom)) return false;
	x -= border_left; y -= border_top;
    if (mouseChild == NULL && (x < 0 || y < 0)) return false;
	return Window::mouseUp(x,y,button);
}

bool BorderedWindow::mouseMoved(int x, int y)
{
	if (x > width-border_right || y > height-border_bottom) return false;
	x -= border_left; y -= border_top;
	if (x < 0 || y < 0) return false;
	return Window::mouseMoved(x,y);
}

bool BorderedWindow::mouseDragged(int x, int y, MouseButton button)
{
	if (mouseChild == NULL && (x > width-border_right || y > height-border_bottom)) return false;
	x -= border_left; y -= border_top;
    if (mouseChild == NULL && (x < 0 || y < 0)) return false;
	return Window::mouseDragged(x,y,button);
}

void ToplevelWindow::paint(Drawable &d) const
{
	unsigned int mask = (systemMenu->isVisible()?Color::RedMask|Color::GreenMask|Color::BlueMask:0);
	d.clear(Color::Background);

	d.setColor(Color::Border);
	d.drawLine(0,height-1,width-1,height-1);
	d.drawLine(width-1,0,width-1,height-1);

	d.setColor(Color::Shadow3D);
	d.drawLine(0,0,width-2,0);
	d.drawLine(0,0,0,height-2);
	d.drawLine(0,height-2,width-2,height-2);
	d.drawLine(width-2,0,width-2,height-2);

	d.drawLine(5,titlebox_y_start,width-7,titlebox_y_start);
	d.drawLine(5,titlebox_y_start,5,titlebox_y_start+titlebox_y_height-2);

	d.setColor(Color::Light3D);
	d.drawLine(1,1,width-3,1);
	d.drawLine(1,1,1,height-3);

	d.drawLine(5,titlebox_y_start+titlebox_y_height-1,width-6,titlebox_y_start+titlebox_y_height-1);
	d.drawLine(width-6,5,width-6,titlebox_y_start+titlebox_y_height-1);

	d.setColor(Color::Background3D^mask);
	d.fillRect(6,titlebox_y_start+1,titlebox_sysmenu_width-1,titlebox_y_height-2);
    {
        int y = titlebox_y_start+((titlebox_y_height-4)/2);
        int x = 8;
        int w = (titlebox_sysmenu_width * 20) / 27;
        int h = 4;

        d.setColor(Color::Grey50^mask);
        d.fillRect(x+1,y+1,w,  h);
        d.setColor(Color::Black^mask);
        d.fillRect(x,  y,  w,  h);
        d.setColor(Color::White^mask);
        d.fillRect(x+1,y+1,w-2,h-2);
    }

	d.setColor(Color::Border);
	d.drawLine(6+titlebox_sysmenu_width-1,titlebox_y_start+1,6+titlebox_sysmenu_width-1,titlebox_y_start+titlebox_y_height-2);

    bool active = hasFocus();

    // FIX: "has focus" is defined as "being at the back of the child list".
    //      Transient windows such as menus will steal focus, but we don't
    //      want the title to flash inactive every time a menu comes up.
    //      Avoid that by searching backwards past transient windows above
    //      us to check if we'd be at the top anyway without the transient windows.
    if (!active) {
        auto i = parent->children.rbegin();
        while (i != parent->children.rend()) {
            Window *w = *i;

            if (w->transient) {
                i++;
            }
            else if (w == this) {
                active = true;
                break;
            }
            else {
                break;
            }
        }
    }

	d.setColor(active ? Color::Titlebar : Color::TitlebarInactive);
	d.fillRect(6+titlebox_sysmenu_width,titlebox_y_start+1,width-(6+titlebox_sysmenu_width+6),titlebox_y_height-2);

	const Font *font = Font::getFont("title");
	d.setColor(active ? Color::TitlebarText : Color::TitlebarInactiveText);
	d.setFont(font);
	d.drawText(31+(width-39-font->getWidth(title))/2,titlebox_y_start+(titlebox_y_height-font->getHeight())/2+font->getAscent(),title,false,0);
}

void Input::posToEnd(void) {
	pos = (GUI::Size)text.size();
	checkOffset();
}

void Input::paint(Drawable &d) const
{
	d.clear(Color::EditableBackground);

	d.setColor(Color::Shadow3D);
	d.drawLine(0,0,width-2,0);
	d.drawLine(0,0,0,height-2);

	d.setColor(Color::Background3D);
	d.drawLine(1,height-2,width-2,height-2);
	d.drawLine(width-2,1,width-2,height-2);

	d.setColor(Color::Text);
	d.drawLine(1,1,width-3,1);
	d.drawLine(1,1,1,height-3);

	const Font *f = Font::getFont("input");
	d.setFont(f);

	Drawable d1(d,3,4,width-6,height-8);
	Drawable dr(d1,(multi?0:-offset),(multi?-offset:0),width-6+(multi?0:offset),height-8+(multi?offset:0));

	const Size start = imin(start_sel, end_sel), end = imax(start_sel, end_sel);
	dr.drawText(0,f->getAscent()+1,text,multi,0,start);

	int sx = dr.getX(), sy = dr.getY();
	dr.drawText(text, multi, start, end-start);
	int ex = dr.getX(), ey = dr.getY();

	if (sx != ex || sy != ey) {
		dr.setColor(Color::SelectionBackground);
		if (sy == ey) dr.fillRect(sx,sy-f->getAscent(),ex-sx,f->getHeight()+1);
		else {
			dr.fillRect(sx, sy-f->getAscent(),		width-sx+offset, f->getHeight()	);
			dr.fillRect(0,  sy-f->getAscent()+f->getHeight(), width+offset,    ey-sy-f->getHeight());
			dr.fillRect(0,  ey-f->getAscent(),		ex,	      f->getHeight()	);
		}
		dr.setColor(Color::SelectionForeground);
		dr.drawText(sx, sy, text, multi, start, end-start);
	}

	dr.setColor(Color::Text);

	dr.drawText(text, multi, end);

	if (blink && hasFocus()) {
		if (insert) dr.drawLine(posx,posy,posx,posy+f->getHeight()+1);
		else dr.fillRect(posx,posy,f->getWidth(text[pos]),f->getHeight()+1);
	}
}

Size Input::findPos(int x, int y) {
	const Font *f = Font::getFont("input");
	if (multi) y += offset;
	else x += offset;
	y = (y-4) / f->getHeight();
	int line = 0;
	Size pos = 0;
	while (line < y && pos < text.size()) if (f->toSpecial(text[pos++]) == Font::LF) line++;
	Drawable d(width-6,1);
	d.setFont(f);
	while (pos <= text.size() && d.getY() == 0 && x > d.getX()) {
		d.drawText(String(text), multi, pos, 1);
		pos++;
	}
	if (pos > 0) pos--;
	return pos;
}

bool Input::mouseDown(int x, int y, MouseButton button)
{
	if (button == Left || (button == Middle && start_sel == end_sel)) {
		end_sel = start_sel = pos = findPos(x,y);
		blink = true;
		checkOffset();
	}
	if (button == Middle) keyDown(Key(0,Key::Insert,true,false,false,false));
	return true;
}

bool Input::mouseDragged(int x, int y, MouseButton button)
{
	if (button == Left) {
		end_sel = pos = findPos(x,y);
		blink = true;
		checkOffset();
	}
	return true;
}

bool Input::keyDown(const Key &key)
{
	const Font *f = Font::getFont("input");
	switch (key.special) {
	case Key::None:
		if (key.ctrl) {
			switch (key.character) {
			case 1:
			case 'a':
			case 'A':
				if (key.shift) {
					start_sel = end_sel = pos;
				} else {
					start_sel = 0;
					pos = end_sel = (Size)text.size();
				}
				break;
			case 24:
			case 'x':
			case 'X':
				cutSelection();
				break;
			case 3:
			case 'c':
			case 'C':
				copySelection();
				break;
			case 22:
			case 'v':
			case 'V':
				pasteSelection();
				break;
			default: printf("Ctrl-0x%x\n",key.character); break;
			}
			break;
		}
		if (start_sel != end_sel) clearSelection();
		if (insert || pos >= text.size() ) text.insert(text.begin()+int(pos++),key.character);
		else text[pos++] = key.character;
		break;
	case Key::Left:
		if (pos > 0) pos--;
		break;
	case Key::Right:
		if (pos < text.size()) pos++;
		break;
	case Key::Down:
		if (multi) pos = findPos(posx+3, posy-offset+f->getHeight()+4);
        else return false;
		break;
	case Key::Up:
		if (multi) pos = findPos(posx+3, posy-offset-f->getHeight()+4);
        else return false;
		break;
	case Key::Home:
		if (multi) {
			while (pos > 0 && f->toSpecial(text[pos-1]) != Font::LF) pos--;
		} else pos = 0;
		break;
	case Key::End:
		if (multi) {
			while (pos < text.size() && f->toSpecial(text[pos]) != Font::LF) pos++;
		} else pos = (Size)text.size();
		break;
	case Key::Backspace:
		if (!key.shift && start_sel != end_sel) clearSelection();
		else if (pos > 0) {
			text.erase(text.begin()+int(--pos));
			if (start_sel > pos) start_sel = pos;
			if (end_sel > pos) end_sel = pos;
		}
		break;
	case Key::Delete:
		if (key.shift) cutSelection();
		else if (start_sel != end_sel) clearSelection();
		else if (pos < text.size()) text.erase(text.begin()+int(pos));
		break;
	case Key::Insert:
		if (key.ctrl) copySelection();
		else if (key.shift) pasteSelection();
		else insert = !insert;
		break;
	case Key::Enter:
		if (multi) {
			if (start_sel != end_sel) clearSelection();
			if (insert || pos >= text.size() ) text.insert(text.begin()+int(pos++),f->fromSpecial(Font::LF));
			else text[pos++] = f->fromSpecial(Font::LF);
		} else executeAction(text);
		break;
	case Key::Tab:
		if (multi && enable_tab_input) {
			if (start_sel != end_sel) clearSelection();
			if (insert || pos >= text.size() ) text.insert(text.begin()+int(pos++),f->fromSpecial(Font::Tab));
			else text[pos++] = f->fromSpecial(Font::Tab);
		} else return false;
		break;
	default:
		return false;
	}
	if (!key.ctrl) {
		if (!key.shift || key.special == Key::None) start_sel = end_sel = pos;
		else end_sel = pos;
	}
	checkOffset();
	blink = true;
	return true;
}

void BorderedWindow::paintAll(Drawable &d) const
{
	this->paint(d);
	Drawable dchild(d,border_left,border_top,width-border_left-border_right,height-border_top-border_bottom);
	for (std::list<Window *>::const_iterator i = children.begin(); i != children.end(); ++i) {
		Window *child = *i;
		if (child->isVisible()) {
			Drawable cd(dchild,child->getX(),child->getY(),child->getWidth(),child->getHeight());
			child->paintAll(cd);
		}
	}
}

void Button::paint(Drawable &d) const
{
	int offset = -1;

	if (hasFocus()) {
		offset = 0;
		d.setColor(Color::Border);
		d.drawLine(0,0,width,0);
		d.drawLine(0,0,0,height);

		d.drawLine(0,height-1,width,height-1);
		d.drawLine(width-1,0,width-1,height);
	}

	d.setColor(Color::Background3D);
	d.fillRect(2,2,width-4,height-4);

	if (pressed) {
		d.setColor(Color::Shadow3D);

		d.drawLine(1+offset,1+offset,width-2-offset,1+offset);
		d.drawLine(1+offset,1+offset,1+offset,height-2-offset);
	} else {
		d.setColor(Color::Background3D);

		d.drawLine(1+offset,1+offset,width-3-offset,1+offset);
		d.drawLine(1+offset,1+offset,1+offset,height-3-offset);

		d.setColor(Color::Light3D);

		d.drawLine(2+offset,2+offset,width-4-offset,2+offset);
		d.drawLine(2+offset,2+offset,2+offset,height-4-offset);

		d.setColor(Color::Shadow3D);

		d.drawLine(2+offset,height-3-offset,width-2-offset,height-3-offset);
		d.drawLine(width-3-offset,2+offset,width-3-offset,height-2-offset);

		d.setColor(Color::Border);

		d.drawLine(width-2-offset,1+offset,width-2-offset,height-2-offset);
		d.drawLine(1+offset,height-2-offset,width-2-offset,height-2-offset);
	}
}

bool Checkbox::keyDown(const Key &key)
{
	switch (key.special) {
	case Key::None:
		if (key.character != ' ') return false;
	case Key::Enter:
		break;
	default: return false;
	}
	mouseDown(0,0,Left);
	return true;
}

bool Checkbox::keyUp(const Key &key)
{
	if (key.ctrl || key.alt || key.windows || (key.character != ' ' && key.special != Key::Enter)) return false;
	mouseUp(0,0,Left);
	mouseClicked(0,0,Left);
	return true;
}

void Checkbox::paint(Drawable &d) const
{
	d.setColor(Color::Background3D);
	d.fillRect(2,(height/2)-7,14,14);

	d.setColor(Color::Shadow3D);
	d.drawLine(2,(height/2)-7,13,(height/2)-7);
	d.drawLine(2,(height/2)-7,2,(height/2)+5);

	d.setColor(Color::Light3D);
	d.drawLine(2,(height/2)+5,14,(height/2)+5);
	d.drawLine(14,(height/2)-7,14,(height/2)+5);

	d.setColor(Color::EditableBackground);
	d.fillRect(4,(height/2)-5,9,9);

	d.setColor(Color::Border);
	d.drawLine(3,(height/2)-6,12,(height/2)-6);
	d.drawLine(3,(height/2)-6,3,(height/2)+4);

    if (hasFocus()) {
        d.setColor(Color::Black);
        d.drawDotRect(1,(height/2)-8,14,14);
    }

	if (checked) {
		d.setColor(Color::Text);
		d.drawLine(5,(height/2)-2,7,(height/2)  );
		d.drawLine(11,(height/2)-4);
		d.drawLine(5,(height/2)-1,7,(height/2)+1);
		d.drawLine(11,(height/2)-3);
		d.drawLine(5,(height/2)  ,7,(height/2)+2);
		d.drawLine(11,(height/2)-2);
	}
}

Radiobox::Radiobox(Frame *parent, int x, int y, int w, int h) : BorderedWindow(static_cast<Window *>(parent),x,y,w,h,16,0,0,0), ActionEventSource("GUI::Radiobox"), checked(0)
{
	 addActionHandler(parent);
}

bool Radiobox::keyDown(const Key &key)
{
	switch (key.special) {
	case Key::None:
		if (key.character != ' ') return false;
	case Key::Enter:
		break;
	default: return false;
	}
	mouseDown(0,0,Left);
	return true;
}

bool Radiobox::keyUp(const Key &key)
{
	if (key.ctrl || key.alt || key.windows || (key.character != ' ' && key.special != Key::Enter)) return false;
	mouseUp(0,0,Left);
	mouseClicked(0,0,Left);
	return true;
}

void Radiobox::paint(Drawable &d) const
{
	d.setColor(Color::Light3D);
	d.drawLine(6,(height/2)+6,9,(height/2)+6);
	d.drawLine(4,(height/2)+5,11,(height/2)+5);
	d.drawLine(13,(height/2)-1,13,(height/2)+2);
	d.drawLine(12,(height/2)-2,12,(height/2)+4);

	d.setColor(Color::Background3D);
	d.drawLine(6,(height/2)+5,9,(height/2)+5);
	d.drawLine(4,(height/2)+4,11,(height/2)+4);
	d.drawLine(12,(height/2)-1,12,(height/2)+2);
	d.drawLine(11,(height/2)-2,11,(height/2)+4);

	d.setColor(Color::Shadow3D);
	d.drawLine(6,(height/2)-5,9,(height/2)-5);
	d.drawLine(4,(height/2)-4,11,(height/2)-4);
	d.drawLine(2,(height/2)-1,2,(height/2)+2);
	d.drawLine(3,(height/2)-3,3,(height/2)+4);

	d.setColor(Color::Border);
	d.drawLine(6,(height/2)-4,9,(height/2)-4);
	d.drawLine(4,(height/2)-3,11,(height/2)-3);
	d.drawLine(3,(height/2)-1,3,(height/2)+2);
	d.drawLine(4,(height/2)-3,4,(height/2)+3);

	d.setColor(Color::EditableBackground);
	d.fillRect(5,(height/2)-2,6,6);
	d.fillRect(4,(height/2)-1,8,4);
	d.fillRect(6,(height/2)-3,4,8);

	if (checked) {
		d.setColor(Color::Text);
		d.fillRect(6,(height/2),4,2);
		d.fillRect(7,(height/2)-1,2,4);
	}
}

void Menu::paint(Drawable &d) const
{
	d.clear(Color::Background3D);

	d.setColor(Color::Border);
	d.drawLine(0,height-1,width-1,height-1);
	d.drawLine(width-1,0,width-1,height-1);

	d.setColor(Color::Shadow3D);
	d.drawLine(0,0,width-2,0);
	d.drawLine(0,0,0,height-2);
	d.drawLine(0,height-2,width-2,height-2);
	d.drawLine(width-2,0,width-2,height-2);

	d.setColor(Color::Light3D);
	d.drawLine(1,1,width-3,1);
	d.drawLine(1,1,1,height-3);

	d.setFont(Font::getFont("menu"));
	const int asc = Font::getFont("menu")->getAscent()+1;
	const int height = Font::getFont("menu")->getHeight()+2;
    int x = 3,cwidth = width-3-x;
	int y = asc+3;
	int index = 0;
    unsigned int coli = 0;

    if (coli < colx.size()) {
        x = colx[coli++];
        cwidth = width-3-x;
        if (coli < colx.size()) {
            cwidth = colx[coli] - x;
        }
    }

	for (std::vector<String>::const_iterator i = items.begin(); i != items.end(); ++i) {
		if ((*i).empty()) {
			d.setColor(Color::Shadow3D);
			d.drawLine(x+1,y-asc+6,cwidth,y-asc+6);
			d.setColor(Color::Light3D);
			d.drawLine(x+1,y-asc+7,cwidth,y-asc+7);
			y += 12;
        } else if (*i == "|") {
            y = asc+3;
            if (coli < colx.size()) {
                x = colx[coli++];
                cwidth = width-3-x;
                if (coli < colx.size()) {
                    cwidth = colx[coli] - x;
                }

                d.setColor(Color::Shadow3D);
                d.drawLine(x-2,2,x-2,this->height-4);
                d.setColor(Color::Light3D);
                d.drawLine(x-1,2,x-1,this->height-4);
            }
        } else {
			if (index == selected && hasFocus()) {
				d.setColor(Color::SelectionBackground);
				d.fillRect(x,y-asc,cwidth,height);
				d.setColor(Color::SelectionForeground);
			} else {
				d.setColor(Color::Text);
			}
			d.drawText(x+17,y,(*i),false,0);
			y += height;
		}
		index++;
	}
}

void Menubar::paint(Drawable &d) const
{
	const Font *f = Font::getFont("menu");

	d.setColor(Color::Light3D);
	d.drawLine(0,height-1,width-1,height-1);
	d.setColor(Color::Shadow3D);
	d.drawLine(0,height-2,width-1,height-2);

	d.gotoXY(7,f->getAscent()+2);

	int index = 0;
	for (std::vector<Menu*>::const_iterator i = menus.begin(); i != menus.end(); ++i, ++index) {
		if (index == selected && (*i)->isVisible()) {
			int w = f->getWidth((*i)->getName());
			d.setColor(Color::SelectionBackground);
			d.fillRect(d.getX()-7,0,w+14,height-2);
			d.setColor(Color::SelectionForeground);
			d.gotoXY(d.getX()+7,f->getAscent()+2);
		} else {
			d.setColor(Color::Text);
		}
		d.drawText((*i)->getName(),false);
		d.gotoXY(d.getX()+14,f->getAscent()+2);
	}
}

bool Button::keyDown(const Key &key)
{
	switch (key.special) {
	case Key::None:
		if (key.character != ' ') return false;
	case Key::Enter:
		break;
	default: return false;
	}
	mouseDown(0,0,Left);
	return true;
}

bool Button::keyUp(const Key &key)
{
	if (key.ctrl || key.alt || key.windows || (key.character != ' ' && key.special != Key::Enter)) return false;
	mouseUp(0,0,Left);
	mouseClicked(0,0,Left);
	return true;
}

void Frame::paint(Drawable &d) const {
	const Font *f = Font::getFont("default");
	const int top = (label.empty()?1:f->getAscent()/2+1);

	d.setColor(Color::Shadow3D);
	d.drawLine(1,height-2,1,top);
	d.drawLine(8,top);
	d.drawLine((label.empty()?8:f->getWidth(label)+14),top,width-2,top);
	d.drawLine(2,height-3,width-3,height-3);
	d.drawLine(width-3,top+1);
	
	d.setColor(Color::Light3D);
	d.drawLine(2,height-3,2,top+1);
	d.drawLine(8,top+1);
	d.drawLine((label.empty()?8:f->getWidth(label)+14),top+1,width-3,top+1);
	d.drawLine(2,height-2,width-2,height-2);
	d.drawLine(width-2,top+1);

	d.setColor(Color::Text);
	d.drawText(11,f->getAscent()+1,label,false,0);
}

Screen *Window::getScreen() { return (parent == NULL?dynamic_cast<Screen*>(this):parent->getScreen()); }

Screen::Screen(unsigned int width, unsigned int height) :
	Window(),
	buffer(new Drawable((int)width, (int)height)),
    buffer_i_alloc(true)
{
	this->width = (int)width;
	this->height = (int)height;
}

Screen::Screen(Drawable *d) :
	Window(),
	buffer(d),
    buffer_i_alloc(true) /* our primary use is from ScreenSDL that calls new */
{
	this->width = d->getClipWidth();
	this->height = d->getClipHeight();
}

Screen::~Screen()
{
    if (buffer_i_alloc) delete buffer;
}

void Screen::paint(Drawable &d) const
{
	d.clear(Color::Transparent);
}

unsigned int Screen::update(void *surface, unsigned int ticks)
{
    return 0; // FIXME
    (void)ticks;//UNUSED
	paintAll(*buffer);
	RGB *buf = buffer->buffer;
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++, buf++) {
			RGB sval = surfaceToRGB(surface);
			RGB bval = *buf;
			unsigned int a = Color::A(bval);
			bval = ((((sval&Color::MagentaMask)*a+(bval&Color::MagentaMask)*(256-a))>>8)&Color::MagentaMask)
				| ((((sval&Color::GreenMask)*a+(bval&Color::GreenMask)*(256-a))>>8)&Color::GreenMask);
			rgbToSurface(bval, &surface);
		}
	}

	return Timer::next();
}

void Screen::move(int x, int y)
{
    (void)x;//UNUSED
    (void)y;//UNUSED
}

void Screen::resize(int w, int h)
{
    (void)w;//UNUSED
    (void)h;//UNUSED
}

#ifdef TESTING
static void test(Drawable &d) {
	const int width = d.getClipWidth();
	const int height = d.getClipHeight();

	d.clear(Color::rgba(0,0,255,128));

	d.setColor(Color::Black);
	for (int x = 0; x < width; x += 10) d.drawLine(x,0,x,height);
	for (int y = 0; y < height; y += 10) d.drawLine(0,y,width,y);

	d.setColor(Color::Red);
	for (int x = 10; x <= 130 ; x += 10) {
		d.drawLine(x,10,70,70);
		d.drawLine(x,130);
	}
	for (int y = 10; y <= 130 ; y += 10) {
		d.drawLine(10,y,70,70);
		d.drawLine(130,y);
	}

	d.setColor(Color::Yellow);
	d.fillRect(150,10,30,30);
	d.setColor(Color::Blue);
	d.drawRect(30,30);

	d.drawRect(150,60,30,30);
	d.setColor(Color::Yellow);
	d.fillRect(30,30);

	for (int x = 0; x <= 100 ; x += 10) {
		d.setColor(Color::rgba(0xff,0x00,0xff,(255*x/100)&255));
		d.fillRect(200+2*x,0,20,20);
	}

	d.setColor(Color::Yellow);
	d.fillCircle(210,60,40);
	d.setColor(Color::Blue);
	d.drawCircle(40);

	d.drawCircle(210,110,40);
	d.setColor(Color::Yellow);
	d.fillCircle(40);

	d.setColor(Color::rgba(0,0,255,128));
	d.fillRect(251,41,9,59);
	d.fillRect(251,41,59,9);
	d.fillRect(301,41,9,59);
	d.fillRect(291,91,19,9);
	d.fillRect(291,51,9,49);
	d.fillRect(261,51,39,9);
	d.fillRect(261,51,9,49);
	d.fillRect(261,91,29,9);
	d.fillRect(281,61,9,39);
	d.fillRect(271,61,19,9);
	d.fillRect(271,61,9,29);
	d.fillRect(241,41,9,59);
	d.fillRect(241,41,19,9);
	d.fillRect(241,91,19,9);
	d.setColor(Color::rgba(255,0,0,128));
	d.fill(255,64);

	d.setColor(Color::Green);
	Drawable(d,500,355,30,30).fillCircle(65);

	for (int i = 0; i <= 100; i += 10) {
		Drawable(d,25,155+i*3,420,30).drawDrawable(0,0,d,255*i/100);
	}

	d.setColor(Color::White);
	d.setFont(Font::getFont("VGA14"));
	d.drawText(270,110,"GUI:: Test Program\n");
	d.drawText("Still testing\tTable\n");
	d.drawText("More of...\tTable\n");
	d.drawText("Overwrite\rXXXXXXXXX\n");
	d.drawText("Fake int'l chars: O\b/e\b\"\n");
	d.drawText("Real ones: \211\222\234\345\246\321");
}
#else
static void test(Drawable &d) { (void)d; }
#endif


void ScreenRGB32le::paint(Drawable &d) const
{
	parent->paint(d);
	test(d);
}

static MouseButton SDL_to_GUI(const int button)
{
	switch (button) {
	case SDL_BUTTON_LEFT:      return GUI::Left;
	case SDL_BUTTON_RIGHT:     return GUI::Right;
	case SDL_BUTTON_MIDDLE:    return GUI::Middle;
#if !defined(C_SDL2)
	case SDL_BUTTON_WHEELUP:   return GUI::WheelUp;
	case SDL_BUTTON_WHEELDOWN: return GUI::WheelDown;
#endif /* !C_SDL2 */
	default: return GUI::NoButton;
	}
}

#if defined(C_SDL2)
static GUI::Char SDLSymToChar(const SDL_Keysym &key) {
    /* SDL will not uppercase the char for us with shift, etc. */
    /* Additionally we have to filter out non-char values */
    if (key.sym == 0 || key.sym > 0x7f) return 0;

    GUI::Char ret = key.sym;

    if (key.mod & KMOD_SHIFT) {
        switch (ret) {
            case '[':
                ret = (GUI::Char)('{');
                break;
            case ']':
                ret = (GUI::Char)('}');
                break;
            case '\\':
                ret = (GUI::Char)('|');
                break;
            case ';':
                ret = (GUI::Char)(':');
                break;
            case '\'':
                ret = (GUI::Char)('"');
                break;
            case ',':
                ret = (GUI::Char)('<');
                break;
            case '.':
                ret = (GUI::Char)('>');
                break;
            case '/':
                ret = (GUI::Char)('?');
                break;
            case '-':
                ret = (GUI::Char)('_');
                break;
            case '=':
                ret = (GUI::Char)('+');
                break;
            case '1':
                ret = (GUI::Char)('!');
                break;
            case '2':
                ret = (GUI::Char)('@');
                break;
            case '3':
                ret = (GUI::Char)('#');
                break;
            case '4':
                ret = (GUI::Char)('$');
                break;
            case '5':
                ret = (GUI::Char)('%');
                break;
            case '6':
                ret = (GUI::Char)('^');
                break;
            case '7':
                ret = (GUI::Char)('&');
                break;
            case '8':
                ret = (GUI::Char)('*');
                break;
            case '9':
                ret = (GUI::Char)('(');
                break;
            case '0':
                ret = (GUI::Char)(')');
                break;
            default:
                ret = (GUI::Char)toupper((int)ret);
                break;
        }
    }

    return ret;
}
#endif

#if defined(C_SDL2)
static const Key SDL_to_GUI(const SDL_Keysym &key)
#else
static const Key SDL_to_GUI(const SDL_keysym &key)
#endif
{
	GUI::Key::Special ksym = GUI::Key::None;
	switch (key.sym) {
#if !defined(C_SDL2) /* hack for SDL1 that fails to send char code for spacebar up event */
    case SDLK_SPACE:
    	return Key(' ', ksym,
    		(key.mod&KMOD_SHIFT)>0,
    		(key.mod&KMOD_CTRL)>0,
    		(key.mod&KMOD_ALT)>0,
    		(key.mod&KMOD_META)>0);
#endif
	case SDLK_ESCAPE: ksym = GUI::Key::Escape; break;
	case SDLK_BACKSPACE: ksym = GUI::Key::Backspace; break;
	case SDLK_TAB: ksym = GUI::Key::Tab; break;
	case SDLK_LEFT: ksym = GUI::Key::Left; break;
	case SDLK_RIGHT: ksym = GUI::Key::Right; break;
	case SDLK_UP: ksym = GUI::Key::Up; break;
	case SDLK_DOWN: ksym = GUI::Key::Down; break;
	case SDLK_HOME: ksym = GUI::Key::Home; break;
	case SDLK_END: ksym = GUI::Key::End; break;
	case SDLK_DELETE: ksym = GUI::Key::Delete; break;
	case SDLK_INSERT: ksym = GUI::Key::Insert; break;
	case SDLK_RETURN: ksym = GUI::Key::Enter; break;
	case SDLK_MENU: ksym = GUI::Key::Menu; break;
	case SDLK_PAGEUP: ksym = GUI::Key::PageUp; break;
	case SDLK_PAGEDOWN: ksym = GUI::Key::PageDown; break;
#if !defined(C_SDL2)
	case SDLK_PRINT: ksym = GUI::Key::Print; break;
#endif
	case SDLK_PAUSE: ksym = GUI::Key::Pause; break;
#if !defined(C_SDL2)
	case SDLK_BREAK: ksym = GUI::Key::Break; break;
#endif
	case SDLK_CAPSLOCK: ksym = GUI::Key::CapsLock; break;
#if !defined(C_SDL2)
	case SDLK_NUMLOCK: ksym = GUI::Key::NumLock; break;
	case SDLK_SCROLLOCK: ksym = GUI::Key::ScrollLock; break;
#endif
	case SDLK_F1:case SDLK_F2:case SDLK_F3:case SDLK_F4:case SDLK_F5:case SDLK_F6:
	case SDLK_F7:case SDLK_F8:case SDLK_F9:case SDLK_F10:case SDLK_F11:case SDLK_F12:
		ksym = (GUI::Key::Special)(GUI::Key::F1 + key.sym-SDLK_F1);
    /* do not provide a character code for these keys */
    case SDLK_LSHIFT: case SDLK_RSHIFT:
    case SDLK_LCTRL:  case SDLK_RCTRL:
    case SDLK_LALT:   case SDLK_RALT:
#if defined(C_SDL2)
    	return Key(0, ksym,
    		(key.mod&KMOD_SHIFT)>0,
    		(key.mod&KMOD_CTRL)>0,
    		(key.mod&KMOD_ALT)>0,
            false);
#else
    	return Key(0, ksym,
    		(key.mod&KMOD_SHIFT)>0,
    		(key.mod&KMOD_CTRL)>0,
    		(key.mod&KMOD_ALT)>0,
    		(key.mod&KMOD_META)>0);
#endif
    /* anything else, go ahead */
	default:
        break;
	}
#if defined(C_SDL2)
	return Key(SDLSymToChar(key), ksym,
		(key.mod&KMOD_SHIFT)>0,
		(key.mod&KMOD_CTRL)>0,
		(key.mod&KMOD_ALT)>0,
        false);
#else
	return Key(key.unicode, ksym,
		(key.mod&KMOD_SHIFT)>0,
		(key.mod&KMOD_CTRL)>0,
		(key.mod&KMOD_ALT)>0,
		(key.mod&KMOD_META)>0);
#endif
}

/** \brief Internal class that handles different screen bit depths and layouts the SDL way */
class SDL_Drawable : public Drawable {
protected:
	SDL_Surface *const surface;

public:
	SDL_Drawable(int width, int height, RGB clear = Color::Transparent) : Drawable(width, height, clear), surface(SDL_CreateRGBSurfaceFrom(buffer, width, height, 32, width*4, Color::RedMask, Color::GreenMask, Color::BlueMask, Color::AlphaMask)) {
#if !defined(C_SDL2)
	    surface->flags |= SDL_SRCALPHA;
#endif
	}

	~SDL_Drawable() {
	    SDL_FreeSurface(surface);
	}

    static bool get_dest_pix
    (   char* sp, SDL_PixelFormat *sf, SDL_PixelFormat *df, Uint32* pix )
    {   Uint8 r, g, b, a;
               SDL_GetRGBA( *(Uint32*)sp, sf, &r, &g, &b, &a );
        *pix = SDL_MapRGBA( df, r, g, b, a );
        return a>0;
    }

	void update(SDL_Surface *dest, int scale) const {
	    char            *dp, *sp, *sr0, *dr, *dr0;
        int              x, y, v, h;
        uint32_t         pix;
        SDL_PixelFormat *fmt, *df;
        bool             is_alpha;

        // Direct blitting for pixel-to-pixel scale:
        if( scale == 1 )
        {   SDL_BlitSurface(surface, NULL, dest, NULL);
            return;
        }

        fmt  = surface->format;
        df   = dest   ->format;
        SDL_LockSurface( dest    );
        SDL_LockSurface( surface );
        dr0 = (char*)dest   ->pixels;
        sr0 = (char*)surface->pixels;

        for( y = 0; y < surface->h; y += 1 )
        {   dp = dr0;
            sp = sr0;
            for( x = 0; x < surface->w; x += 1 )
            {   is_alpha = get_dest_pix( sp, fmt, df, &pix ) > 0;
                for( h = 0; h < scale; h += 1 )
                {   if( is_alpha )
                    {   memcpy( dp, &pix, df->BytesPerPixel );  }
                    dp += df->BytesPerPixel;
                }
                sp += fmt->BytesPerPixel;
            }
            sr0 = sr0 + surface->pitch;
            
            dr = dr0 + dest->pitch;
            for( v = 0; v < scale - 1; v += 1 )
            {   memcpy( dr, dr0, scale * surface->w * dest->format->BytesPerPixel );
                dr = dr + dest->pitch;
            }
            dr0 = dr;
        }
        SDL_UnlockSurface( dest    );
        SDL_UnlockSurface( surface );
	}
};

ScreenSDL::~ScreenSDL() {
}

ScreenSDL::ScreenSDL(SDL_Surface *surface, int scale) : Screen(new SDL_Drawable(surface->w/scale, surface->h/scale)), surface(surface), downx(0), downy(0), lastclick(0), lastdown(0), scale(scale) {
	current_abs_time = start_abs_time = SDL_GetTicks();
	current_time = 0;
}

Ticks ScreenSDL::update(Ticks ticks)
{
	Timer::check_to(ticks);
	paintAll(*buffer);
	static_cast<SDL_Drawable*>(buffer)->update(surface, scale);

	return Timer::next();
}

void ScreenSDL::watchTime() {
	current_abs_time = SDL_GetTicks();
	current_time = current_abs_time - start_abs_time;
}

void ScreenSDL::paint(Drawable &d) const {
	d.clear(Color::Transparent);
	test(d);
}

bool ScreenSDL::event(SDL_Event &event) {
	bool rc;

    event.button.x /= scale;
    event.button.y /= scale;
	
#if defined(C_SDL2)
    /* handle mouse events only if it comes from the mouse.
     * ignore the fake mouse events some OSes generate from the touchscreen.
     * Note that Windows will fake mouse events, Linux/X11 wil not */
    if (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
        if (event.button.which == SDL_TOUCH_MOUSEID) /* don't handle mouse events faked by touchscreen */
            return true;/*eat the event or else it will just keep calling objects until processed*/
    }
#endif

	switch (event.type) {
#if defined(C_SDL2) && !defined(IGNORE_TOUCHSCREEN)
    case SDL_FINGERUP:
    case SDL_FINGERDOWN:
    case SDL_FINGERMOTION: {
        SDL_Event fake;
        bool comb = false;

        memset(&fake,0,sizeof(fake));
        fake.type = SDL_MOUSEMOTION;
        fake.motion.state = SDL_BUTTON(1);
        fake.motion.x = (Sint32)(event.tfinger.x * surface->w);
        fake.motion.y = (Sint32)(event.tfinger.y * surface->h);
        fake.motion.xrel = (Sint32)event.tfinger.dx;
        fake.motion.yrel = (Sint32)event.tfinger.dy;
        comb |= this->event(fake);

        if (event.type == SDL_FINGERUP || event.type == SDL_FINGERDOWN) {
            memset(&fake,0,sizeof(fake));
            fake.button.button = SDL_BUTTON_LEFT;
            fake.button.x = (Sint32)(event.tfinger.x * surface->w);
            fake.button.y = (Sint32)(event.tfinger.y * surface->h);
            fake.type = (event.type == SDL_FINGERUP) ? SDL_MOUSEBUTTONUP : SDL_MOUSEBUTTONDOWN;
            comb |= this->event(fake);
        }

        return comb;
    }
#endif
	case SDL_KEYUP: {
		const Key &key = SDL_to_GUI(event.key.keysym);
		if (key.special == GUI::Key::None && key.character == 0) break;
		if (key.special == GUI::Key::CapsLock || key.special == GUI::Key::NumLock) keyDown(key);
		return keyUp(key);
	}
	case SDL_KEYDOWN: {
		const Key &key = SDL_to_GUI(event.key.keysym);
		if (key.special == GUI::Key::None && key.character == 0) break;
		rc = keyDown(key);
		if (key.special == GUI::Key::CapsLock || key.special == GUI::Key::NumLock) keyUp(key);
		return rc;
	}
	case SDL_MOUSEMOTION:
		if (event.motion.state) {
			if (abs(event.motion.x-downx) <= 10 && abs(event.motion.y-downy) <= 10)
				break;
			downx = -11; downy = -11;
			if (event.motion.state&SDL_BUTTON(1))
				return mouseDragged(event.motion.x, event.motion.y, GUI::Left);
			else if (event.motion.state&SDL_BUTTON(2))
				return mouseDragged(event.motion.x, event.motion.y, GUI::Middle);
			else if (event.motion.state&SDL_BUTTON(3))
				return mouseDragged(event.motion.x, event.motion.y, GUI::Right);
			break;
		}

		return mouseMoved(event.motion.x, event.motion.y);

	case SDL_MOUSEBUTTONDOWN:
		rc = mouseDown(event.button.x, event.button.y, SDL_to_GUI(event.button.button));
		if (abs(event.button.x-downx) > 10 || abs(event.button.y-downy) > 10) lastclick = 0;
		downx = event.button.x; downy = event.button.y;
		lastdown = GUI::Timer::now();
		return rc;

	case SDL_MOUSEBUTTONUP:
		rc = mouseUp(event.button.x, event.button.y, SDL_to_GUI(event.button.button));
		if (lastdown != 0 && abs(event.button.x-downx) < 10 && abs(event.button.y-downy) < 10) {
			if (lastclick == 0 || (GUI::Timer::now()-lastclick) > 200) {
				lastclick = GUI::Timer::now();
				rc |= mouseClicked(downx, downy, SDL_to_GUI(event.button.button));
			} else if (lastclick != 0) {
				rc |= mouseDoubleClicked(downx, downy, SDL_to_GUI(event.button.button));
				lastclick = 0;
			} else {
				lastclick = 0;
			}
		} else {
			lastclick = 0;
		}
		lastdown = 0;
		return rc;
	}

	return false;
}

void WindowInWindow::paintAll(Drawable &d) const {
    int xadj = -scroll_pos_x;
    int yadj = -scroll_pos_y;

    if (border) {
        xadj++;
        yadj++;
    }

	Drawable dchild(d,0,0,width,height);
	for (std::list<Window *>::const_iterator i = children.begin(); i != children.end(); ++i) {
		Window *child = *i;
		if (child->isVisible()) {
			Drawable cd(dchild,child->getX() + xadj,child->getY() + yadj,child->getWidth(),child->getHeight());
			child->paintAll(cd);
		}
	}

    if (border) {
        int w = width-1;
        int h = height-1;

        if (vscroll)
            w -= (vscroll?vscroll_display_width:0);

        dchild.setColor(Color::Shadow3D);
        dchild.drawLine(0,0,w,0);
        dchild.drawLine(0,0,0,h);

        dchild.setColor(Color::Light3D);
        dchild.drawLine(0,h,w,h);
        dchild.drawLine(w,0,w,h);
    }

    if (vscroll && vscroll_display_width >= 4) {
        // TODO: Need a vertical scrollbar window object

        Drawable dscroll(d,width - vscroll_display_width,0,vscroll_display_width,height);

        bool disabled = (scroll_pos_h == 0);

        /* scroll bar border, gray background */
        dscroll.setColor(disabled ? Color::Shadow3D : Color::Black);
        dscroll.drawRect(0,0,vscroll_display_width-1,height-1);

        dscroll.setColor(Color::Background3D);
        dscroll.fillRect(1,1,vscroll_display_width-2,height-2);

        /* the "thumb". make it fixed size, Windows 3.1 style.
         * this code could adapt to the more range-aware visual style of Windows 95 later. */
        int thumbwidth = vscroll_display_width - 2;
        int thumbheight = vscroll_display_width - 2;
        int thumbtravel = height - 2 - thumbheight;
        if (thumbtravel < 0) thumbtravel = 0;
        int ytop = 1 + ((scroll_pos_h > 0) ?
            ((thumbtravel * scroll_pos_y) / scroll_pos_h) :
            0);

        if (thumbheight <= (height + 2) && !disabled) {
            int xleft = 1;
            dscroll.setColor(Color::Light3D);
            dscroll.drawLine(xleft,ytop,xleft+thumbwidth-1,ytop);
            dscroll.drawLine(xleft,ytop,xleft,ytop+thumbheight-1);

            // Windows 3.1 renders the shadow two pixels wide
            dscroll.setColor(Color::Shadow3D);
            dscroll.drawLine(xleft,ytop+thumbheight-1,xleft+thumbwidth-1,ytop+thumbheight-1);
            dscroll.drawLine(xleft+thumbwidth-1,ytop,xleft+thumbwidth-1,ytop+thumbheight-1);

            dscroll.drawLine(xleft+1,ytop+thumbheight-2,xleft+thumbwidth-2,ytop+thumbheight-2);
            dscroll.drawLine(xleft+thumbwidth-2,ytop+1,xleft+thumbwidth-2,ytop+thumbheight-2);

            // Windows 3.1 also draws a hard black line around the thumb that can coincide with the border
            dscroll.setColor(Color::Black);
            dscroll.drawLine(xleft,ytop-1,xleft+thumbwidth-1,ytop-1);
            dscroll.drawLine(xleft,ytop+thumbheight,xleft+thumbwidth-1,ytop+thumbheight);

            // Windows 3.1 also draws an inverted dotted rectangle around the thumb where it WOULD be
            // before quantization to scroll position.
            if (vscroll_dragging) {
                xleft = 0;
                ytop = drag_y - ((thumbheight + 2) / 2);
                if (ytop < 0) ytop = 0;
                if (ytop > thumbtravel) ytop = thumbtravel;
                dscroll.setColor(Color::Light3D);
                dscroll.drawDotRect(xleft,ytop,thumbwidth+1,thumbheight+1);
            }
        }
    }
}

bool WindowInWindow::mouseDragged(int x, int y, MouseButton button)
{
    if (vscroll_dragging) {
        int thumbheight = vscroll_display_width - 2;
        int thumbtravel = height - 2 - thumbheight;
        if (thumbtravel < 0) thumbtravel = 0;

        double npos = (double(y - 1 - (thumbheight / 2)) * scroll_pos_h) / thumbtravel;
        int nipos = int(floor(npos + 0.5));
        if (nipos < 0) nipos = 0;
        if (nipos > scroll_pos_h) nipos = scroll_pos_h;
        scroll_pos_y = nipos;

        drag_x = x;
        drag_y = y;
        return true;
    }

    if (dragging) {
        scroll_pos_x -= x - drag_x;
        scroll_pos_y -= y - drag_y;
        if (scroll_pos_x < 0) scroll_pos_x = 0;
        if (scroll_pos_y < 0) scroll_pos_y = 0;
        if (scroll_pos_x > scroll_pos_w) scroll_pos_x = scroll_pos_w;
        if (scroll_pos_y > scroll_pos_h) scroll_pos_y = scroll_pos_h;
        drag_x = x;
        drag_y = y;
        return true;
    }

    int xadj = -scroll_pos_x;
    int yadj = -scroll_pos_y;

    if (border) {
        xadj++;
        yadj++;
    }

    return Window::mouseDragged(x-xadj,y-yadj,button);
}

bool WindowInWindow::mouseDown(int x, int y, MouseButton button)
{
    if (vscroll && x >= (width - vscroll_display_width) && button == GUI::Left) {
        mouseChild = this;
        vscroll_dragging = true;
        drag_x = x;
        drag_y = y;

        int thumbheight = vscroll_display_width - 2;
        int thumbtravel = height - 2 - thumbheight;
        if (thumbtravel < 0) thumbtravel = 0;

        double npos = (double(y - 1 - (thumbheight / 2)) * scroll_pos_h) / thumbtravel;
        int nipos = int(floor(npos + 0.5));
        if (nipos < 0) nipos = 0;
        if (nipos > scroll_pos_h) nipos = scroll_pos_h;
        scroll_pos_y = nipos;

        return true;
    }
    if (mouseChild == NULL && button == GUI::WheelUp) {
        scroll_pos_y -= 50;
        if (scroll_pos_y < 0) scroll_pos_y = 0;
        mouseChild = this;
        dragging = true;
        return true;
    }
    if (mouseChild == NULL && button == GUI::WheelDown) {
        scroll_pos_y += 50;
        if (scroll_pos_y > scroll_pos_h) scroll_pos_y = scroll_pos_h;
        mouseChild = this;
        dragging = true;
        return true;
    }

    int xadj = -scroll_pos_x;
    int yadj = -scroll_pos_y;

    if (border) {
        xadj++;
        yadj++;
    }

    bool ret = Window::mouseDown(x-xadj,y-yadj,button);

    if (!ret && mouseChild == NULL && button == GUI::Left) {
        drag_x = x;
        drag_y = y;
        mouseChild = this;
        dragging = true;
        ret = true;
    }

    return ret;
}

bool WindowInWindow::mouseUp(int x, int y, MouseButton button)
{
    if (vscroll_dragging) {
        int thumbheight = vscroll_display_width - 2;
        int thumbtravel = height - 2 - thumbheight;
        if (thumbtravel < 0) thumbtravel = 0;

        double npos = (double(y - 1 - (thumbheight / 2)) * scroll_pos_h) / thumbtravel;
        int nipos = int(floor(npos + 0.5));
        if (nipos < 0) nipos = 0;
        if (nipos > scroll_pos_h) nipos = scroll_pos_h;
        scroll_pos_y = nipos;

        vscroll_dragging = false;
        mouseChild = NULL;
        return true;
    }

    if (hscroll_dragging) {
        hscroll_dragging = false;
        mouseChild = NULL;
        return true;
    }

    if (dragging) {
        mouseChild = NULL;
        dragging = false;
        return true;
    }

    int xadj = -scroll_pos_x;
    int yadj = -scroll_pos_y;

    if (border) {
        xadj++;
        yadj++;
    }

    return Window::mouseUp(x-xadj,y-xadj,button);
}

bool WindowInWindow::mouseMoved(int x, int y) {
    if (dragging) return true;

    int xadj = -scroll_pos_x;
    int yadj = -scroll_pos_y;

    if (border) {
        xadj++;
        yadj++;
    }

    return Window::mouseMoved(x-xadj,y-xadj);
}

bool WindowInWindow::mouseClicked(int x, int y, MouseButton button) {
    if (dragging) return true;

    int xadj = -scroll_pos_x;
    int yadj = -scroll_pos_y;

    if (border) {
        xadj++;
        yadj++;
    }

    return Window::mouseClicked(x-xadj,y-xadj,button);
}

bool WindowInWindow::mouseDoubleClicked(int x, int y, MouseButton button) {
    if (dragging) return true;

    int xadj = -scroll_pos_x;
    int yadj = -scroll_pos_y;

    if (border) {
        xadj++;
        yadj++;
    }

    return Window::mouseDoubleClicked(x-xadj,y-xadj,button);
}

void WindowInWindow::resize(int w, int h) {
    int mw = 0,mh = 0;
    int cmpw = w;
    int cmph = h;

    if (vscroll) cmpw -= vscroll_display_width;
    if (border) {
        cmpw -= 2;
        cmph -= 2;
    }

	for (std::list<Window *>::const_iterator i = children.begin(); i != children.end(); ++i) {
		Window *child = *i;
        int mx = child->getX() + child->getWidth();
        int my = child->getY() + child->getHeight();
        if (mw < mx) mw = mx;
        if (mh < my) mh = my;
	}

    mw -= cmpw;
    mh -= cmph;
    if (mw < 0) mw = 0;
    if (mh < 0) mh = 0;
    scroll_pos_w = mw;
    scroll_pos_h = mh;

    Window::resize(w,h);
}

void WindowInWindow::enableBorder(bool en) {
    if (border != en) {
        border = en;
        resize(width, height);
    }
}

void WindowInWindow::enableScrollBars(bool hs,bool vs) {
    if (hs != hscroll || vs != vscroll) {
        hscroll = hs;
        vscroll = vs;
        resize(width, height);
    }
}

} /* end namespace GUI */
