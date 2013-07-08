#if 0
/*
 *  gui_tk - framework-agnostic GUI toolkit
 *  Copyright (C) 2005-2007 Jörg Walter
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
/* $Id: gui_tk.cpp,v 1.5 2009-02-01 16:06:26 qbix79 Exp $ */

/** \file
 *  \brief Implementation file for gui_tk.
 *
 *  It contains implementations for all non-inlined class methods.
 *
 *  Also contained is a small test program that demonstrates all features of
 *  gui_tk. It is enabled by defining the preprocessor macro TESTING.
 */

#include <SDL.h>
#include "gui_tk.h"

namespace GUI {

namespace Color {

RGB Background3D = 0xffc0c0c0;

RGB Light3D = 0xfffcfcfc;

RGB Shadow3D = 0xff808080;

RGB Border = 0xff000000;

RGB Text = 0xff000000;

RGB Background = 0xffc0c0c0;

RGB SelectionBackground = 0xff000080;

RGB SelectionForeground = 0xffffffff;

RGB EditableBackground = 0xffffffff;

RGB Titlebar = 0xff000080;

RGB TitlebarText = 0xffffffff;

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
					int seqstart = start+1;
					Char c;
					do {
						start++;
						wordstart++;
						c = font->toSpecial(text[start]);
					} while (start < len && ((c >= '0' && c <= '9') || c == ';' || c == '['));
					if (c == 'm' && start < len) {
						if (font->toSpecial(text[seqstart++]) != '[') break;
						c = font->toSpecial(text[seqstart++]);
						while (c != 'm') {
							int param = 0;
							if (c == ';') c = '0';
							while (c != 'm' && c != ';') {
								param = param * 10 + c-'0';
								c = font->toSpecial(text[seqstart++]);
							}
							const RGB bright = 0x00808080;
							const RGB intensity = (color&bright?~0:~bright);
							switch (param) {
							case 0: setColor(Color::Black); break;
							case 1: setColor(color | 0x00808080); break;
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
				if (x > 0 && x+width > cw) gotoXY(0,y+font->getHeight());
			}
			start++;
		}
		if (wordstart != start) drawText(text,false,wordstart,start-wordstart);
		return;
	}

	font->drawString(this,text,start,len);
}

bool ToplevelWindow::mouseDown(int x, int y, MouseButton button) {
	if (button == Left && x > 32 && x < width-6 && y > 4 && y < 31) {
		dragx = x;
		dragy = y;
		mouseChild = NULL;
		systemMenu->setVisible(false);
		return true;
	} else if (button == Left && x < 32 && x > 6 && y > 4 && y < 31) {
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
	x(src.x), y(src.y)
{
	if (clear != 0) {
		this->clear(clear);
	} else {
		for (int h = 0; h < src.ch; h++) {
			memcpy(buffer+src.cw*h,src.buffer+src.width*(h+src.ty)+src.tx,4*src.cw);
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
		src = d.buffer+d.width*(h+d.ty)+d.tx;
		dest = buffer+width*(y+ty+h)+tx+x;
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
		if (font->getWidth(c)+x > cw) gotoXY(0,y+font->getHeight());
	}
	font->drawChar(this,c);
}

#define move(x) (ptr += ((x)+bit)/8-(((x)+bit)<0), bit = ((x)+bit+(((x)+bit)<0?8:0))%8)
void BitmapFont::drawChar(Drawable *d, const Char c) const {
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
			if (!background_set ^ !(*ptr&(1<<bit)))
				out.drawPixel(col,row);
		}
	}
	d->gotoXY(d->getX()+w,d->getY());
}
#undef move

std::map<const char *,Font *,Font::ltstr> Font::registry;

void Timer::check(unsigned int ticks)
{
	if (timers.empty()) return;

	if (Timer::ticks > (Timer::ticks+ticks)) {
		ticks -= -1-Timer::ticks;
		check(-1-Timer::ticks);
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
	parent(parent),
	mouseChild(NULL)
{
	parent->addChild(this);
}

Window::Window() :
	width(0), height(0),
	x(0), y(0),
	dirty(false),
	visible(true),
	parent(NULL),
	mouseChild(NULL)
{
}


Window::~Window()
{
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
		while (i != e && !(*i)->raise()) ++i;
		return i != e;
	} else {
		std::list<Window *>::iterator i = children.begin(), e = children.end();
		while (i != e && !(*i)->raise()) ++i;
		return (i != e);
	}
}

bool Window::keyUp(const Key &key)
{
	if (children.empty()) return false;
	return (*children.rbegin())->keyUp(key);
}

bool Window::mouseMoved(int x, int y)
{
	std::list<Window *>::reverse_iterator i = children.rbegin();
	bool end = (i == children.rend());
	while (!end) {
		Window *w = *i;
		i++;
		end = (i == children.rend());
		if (w->visible && x >= w->x && x <= w->x+w->width
			&& y >= w->y && y <= w->y+w->height
			&& w->mouseMoved(x-w->x, y-w->y)) return true;
	}
	return false;
}

bool Window::mouseDragged(int x, int y, MouseButton button)
{
	if (mouseChild == NULL) return false;
	return mouseChild->mouseDragged(x-mouseChild->x, y-mouseChild->y, button);
}

bool Window::mouseDown(int x, int y, MouseButton button)
{
	Window *last = NULL;
	std::list<Window *>::reverse_iterator i = children.rbegin();
	bool end = (i == children.rend());
	while (!end) {
		Window *w = *i;
		i++;
		end = (i == children.rend());
		if (w->visible && x >= w->x && x <= w->x+w->width
			&& y >= w->y && y <= w->y+w->height
			&& (mouseChild = last = w)
			&& w->mouseDown(x-w->x, y-w->y, button)
			&& w->raise()) {
			return true;
		}
	}
	mouseChild = NULL;
	if (last != NULL) last->raise();
	return false;
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
	if (x > width-border_right || y > width-border_bottom) return false;
	x -= border_left; y -= border_top;
	if (x < 0 || y < 0) return false;
	return Window::mouseDown(x,y,button);
}

bool BorderedWindow::mouseMoved(int x, int y)
{
	if (x > width-border_right || y > width-border_bottom) return false;
	x -= border_left; y -= border_top;
	if (x < 0 || y < 0) return false;
	return Window::mouseMoved(x,y);
}

bool BorderedWindow::mouseDragged(int x, int y, MouseButton button)
{
	if (x > width-border_right || y > width-border_bottom) return false;
	x -= border_left; y -= border_top;
	if (x < 0 || y < 0) return false;
	return Window::mouseDragged(x,y,button);
}

void ToplevelWindow::paint(Drawable &d) const
{
	int mask = (systemMenu->isVisible()?Color::RedMask|Color::GreenMask|Color::BlueMask:0);
	d.clear(Color::Background);

	d.setColor(Color::Border);
	d.drawLine(0,height-1,width-1,height-1);
	d.drawLine(width-1,0,width-1,height-1);

	d.setColor(Color::Shadow3D);
	d.drawLine(0,0,width-2,0);
	d.drawLine(0,0,0,height-2);
	d.drawLine(0,height-2,width-2,height-2);
	d.drawLine(width-2,0,width-2,height-2);

	d.drawLine(5,4,width-7,4);
	d.drawLine(5,4,5,30);

	d.setColor(Color::Light3D);
	d.drawLine(1,1,width-3,1);
	d.drawLine(1,1,1,height-3);

	d.drawLine(5,31,width-6,31);
	d.drawLine(width-6,5,width-6,31);

	d.setColor(Color::Background3D^mask);
	d.fillRect(6,5,26,26);
	d.setColor(Color::Grey50^mask);
	d.fillRect(9,17,20,4);
	d.setColor(Color::Black^mask);
	d.fillRect(8,16,20,4);
	d.setColor(Color::White^mask);
	d.fillRect(9,17,18,2);

	d.setColor(Color::Border);
	d.drawLine(32,5,32,30);

	d.setColor(Color::Titlebar);
	d.fillRect(33,5,width-39,26);

	const Font *font = Font::getFont("title");
	d.setColor(Color::TitlebarText);
	d.setFont(font);
	d.drawText(31+(width-39-font->getWidth(title))/2,5+(26-font->getHeight())/2+font->getAscent(),title,false,0);
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
		if (insert || pos >= text.size() ) text.insert(text.begin()+pos++,key.character);
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
		break;
	case Key::Up:
		if (multi) pos = findPos(posx+3, posy-offset-f->getHeight()+4);
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
		else if (pos > 0) text.erase(text.begin()+ --pos);
		break;
	case Key::Delete:
		if (key.shift) cutSelection();
		else if (start_sel != end_sel) clearSelection();
		else if (pos < text.size()) text.erase(text.begin()+pos);
		break;
	case Key::Insert:
		if (key.ctrl) copySelection();
		else if (key.shift) pasteSelection();
		else insert = !insert;
		break;
	case Key::Enter:
		if (multi) {
			if (start_sel != end_sel) clearSelection();
			if (insert || pos >= text.size() ) text.insert(text.begin()+pos++,f->fromSpecial(Font::LF));
			else text[pos++] = f->fromSpecial(Font::LF);
		} else executeAction(text);
		break;
	case Key::Tab:
		if (multi) {
			if (start_sel != end_sel) clearSelection();
			if (insert || pos >= text.size() ) text.insert(text.begin()+pos++,f->fromSpecial(Font::Tab));
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
	int y = asc+3;
	int index = 0;
	for (std::vector<String>::const_iterator i = items.begin(); i != items.end(); ++i) {
		if ((*i).empty()) {
			d.setColor(Color::Shadow3D);
			d.drawLine(4,y-asc+6,width-5,y-asc+6);
			d.setColor(Color::Light3D);
			d.drawLine(4,y-asc+7,width-5,y-asc+7);
			y += 12;
		} else {
			if (index == selected && hasFocus()) {
				d.setColor(Color::SelectionBackground);
				d.fillRect(3,y-asc,width-6,height);
				d.setColor(Color::SelectionForeground);
			} else {
				d.setColor(Color::Text);
			}
			d.drawText(20,y,(*i),false,0);
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
	buffer(new Drawable(width, height))
{
	this->width = width;
	this->height = height;
}

Screen::Screen(Drawable *d) :
	Window(),
	buffer(d)
{
	this->width = d->getClipWidth();
	this->height = d->getClipHeight();
}

Screen::~Screen()
{
}

void Screen::paint(Drawable &d) const
{
	d.clear(Color::Transparent);
}

unsigned int Screen::update(void *surface, unsigned int ticks)
{
	Timer::check(ticks);

	paintAll(*buffer);
	RGB *buf = buffer->buffer;
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++, buf++) {
			RGB sval = surfaceToRGB(surface);
			RGB bval = *buf;
			int a = Color::A(bval);
			bval = ((((sval&Color::MagentaMask)*a+(bval&Color::MagentaMask)*(256-a))>>8)&Color::MagentaMask)
				| ((((sval&Color::GreenMask)*a+(bval&Color::GreenMask)*(256-a))>>8)&Color::GreenMask);
			rgbToSurface(bval, &surface);
		}
	}

	return Timer::next();
}

void Screen::move(int x, int y)
{
}

void Screen::resize(int w, int h)
{
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
	case SDL_BUTTON_WHEELUP:   return GUI::WheelUp;
	case SDL_BUTTON_WHEELDOWN: return GUI::WheelDown;
	default: return GUI::NoButton;
	}
}

static const Key SDL_to_GUI(const SDL_keysym &key)
{
	GUI::Key::Special ksym = GUI::Key::None;
	switch (key.sym) {
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
	case SDLK_PRINT: ksym = GUI::Key::Print; break;
	case SDLK_PAUSE: ksym = GUI::Key::Pause; break;
	case SDLK_BREAK: ksym = GUI::Key::Break; break;
	case SDLK_CAPSLOCK: ksym = GUI::Key::CapsLock; break;
	case SDLK_NUMLOCK: ksym = GUI::Key::NumLock; break;
	case SDLK_SCROLLOCK: ksym = GUI::Key::ScrollLock; break;
	case SDLK_F1:case SDLK_F2:case SDLK_F3:case SDLK_F4:case SDLK_F5:case SDLK_F6:
	case SDLK_F7:case SDLK_F8:case SDLK_F9:case SDLK_F10:case SDLK_F11:case SDLK_F12:
		ksym = (GUI::Key::Special)(GUI::Key::F1 + key.sym-SDLK_F1);
	default: break;
	}
	return Key(key.unicode, ksym,
		(key.mod&KMOD_SHIFT)>0,
		(key.mod&KMOD_CTRL)>0,
		(key.mod&KMOD_ALT)>0,
		(key.mod&KMOD_META)>0);
}

/** \brief Internal class that handles different screen bit depths and layouts the SDL way */
class SDL_Drawable : public Drawable {
protected:
	SDL_Surface *const surface;

public:
	SDL_Drawable(int width, int height, RGB clear = Color::Transparent) : Drawable(width, height, clear), surface(SDL_CreateRGBSurfaceFrom(buffer, width, height, 32, width*4, Color::RedMask, Color::GreenMask, Color::BlueMask, Color::AlphaMask)) {
	    surface->flags |= SDL_SRCALPHA;
	}

	~SDL_Drawable() {
	    SDL_FreeSurface(surface);
	}

	void update(SDL_Surface *dest) const {
	    SDL_BlitSurface(surface, NULL, dest, NULL);
	}
};

ScreenSDL::ScreenSDL(SDL_Surface *surface) : Screen(new SDL_Drawable(surface->w, surface->h)), surface(surface), downx(0), downy(0), lastclick(0) {}

Ticks ScreenSDL::update(Ticks ticks)
{
	Timer::check(ticks);

	paintAll(*buffer);
	static_cast<SDL_Drawable*>(buffer)->update(surface);

	return Timer::next();
}

void ScreenSDL::paint(Drawable &d) const {
	d.clear(Color::Transparent);
	test(d);
}

bool ScreenSDL::event(const SDL_Event &event) {
	bool rc;

	switch (event.type) {
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
		return rc;

	case SDL_MOUSEBUTTONUP:
		rc = mouseUp(event.button.x, event.button.y, SDL_to_GUI(event.button.button));
		if (abs(event.button.x-downx) < 10 && abs(event.button.y-downy) < 10) {
			if (lastclick == 0 || (GUI::Timer::now()-lastclick) > 20) {
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
		return rc;
	}

	return false;
}

}



#ifdef TESTING
#include <stdio.h>

/** \brief A test program that serves as an example for all GUI elements.
 *
 * Note that you need SDL installed and a file "testfont.h" for this
 * to compile, which must contain a bitmap font declared as
 *
 * \code
 * static const unsigned char testfont[256 * 14] = { ... };
 * \endcode
 *
 * (256 chars, 8x14 fixed-width font, for example the IBM PC VGA 14-line font,
 * you can get it from DOSBox)
 *
 * To compile it, use a command line like this:
 *
 * \code
 * g++ -DTESTING `sdl-config --cflags` `sdl-config --libs` gui_tk.cpp -o testgui_tk
 * \endcode
 *
 */

int main(int argc, char *argv[])
{
	printf("GUI:: test program\n");

	SDL_Surface *screen;

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}

	atexit(SDL_Quit);

	screen = SDL_SetVideoMode(640, 480, 32, SDL_SWSURFACE);
	if (screen == NULL) {
		fprintf(stderr, "Couldn't set 640x480x32 video mode: %s\n", SDL_GetError());
		exit(1);
	}
	printf("GUI:: color depth %i\n",screen->format->BitsPerPixel);

	SDL_EnableUNICODE(true);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,SDL_DEFAULT_REPEAT_INTERVAL);

	#include "testfont.h"
	GUI::Font::addFont("default",new GUI::BitmapFont(testfont,14,10));

	GUI::ScreenSDL guiscreen(screen);
	GUI::ToplevelWindow *frame = new GUI::ToplevelWindow(&guiscreen,205,100,380,250,"GUI::Frame");
	static struct delwin : public GUI::ActionEventSource_Callback {
		void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) {
			dynamic_cast<GUI::ToplevelWindow *>(dynamic_cast<GUI::Button*>(b)->getParent())->close();
		}
	} dw;
	struct newwin : public GUI::ActionEventSource_Callback {
		GUI::Screen *screen;
		int n;
		void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) {
			char title[256];
			sprintf(title,"Window %i",++n);
			GUI::ToplevelWindow *w = new GUI::ToplevelWindow(screen,405,100,120,150,title);
			GUI::Button *close = new GUI::Button(w,5,5,"Close");
			close->addActionHandler(&dw);
		}
	} nw;
	nw.screen = &guiscreen;
	nw.n = 0;
	struct quit : public GUI::ActionEventSource_Callback {
		GUI::ToplevelWindow *frame;
		void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) {
			if (arg == "Quit" && b->getName() != "GUI::Input") exit(0);
			frame->setTitle(arg);
		}
	} ex;
	ex.frame = frame;

	GUI::Button *b = new GUI::Button(frame,8,20,"Open a new Window");
	b->addActionHandler(&nw);
	(new GUI::Button(frame,200,20,"1"))->addActionHandler(&ex);
	(new GUI::Button(frame,235,20,"2"))->addActionHandler(&ex);
	b = new GUI::Button(frame,270,20,"Quit");
	b->addActionHandler(&ex);
	(new GUI::Input(frame,16,55,150))->addActionHandler(&ex);

	printf("Title: %s\n",(const char*)frame->getTitle());

	struct movewin : public GUI::Timer_Callback, public GUI::ToplevelWindow_Callback {
	public:
		GUI::ToplevelWindow *frame;
		GUI::Checkbox *cb1, *cb2;
		int x, y;
		virtual unsigned int timerExpired(unsigned int t) {
			if (cb2->isChecked()) frame->move(frame->getX()+x,frame->getY()+y);
			if (frame->getX() <= -frame->getWidth()) x = 1;
			if (frame->getX() >= 640) x = -1;
			if (frame->getY() <= -frame->getHeight()) y = 1;
			if (frame->getY() >= 480) y = -1;
			return 10;
		}
		virtual bool windowClosing(GUI::ToplevelWindow *win) {
			if (!cb1->isChecked()) return false;
			return true;
		}
		virtual void windowClosed(GUI::ToplevelWindow *win) {
			GUI::Timer::remove(this);
		}
	} mw;
	mw.frame = frame;
	mw.x = -1;
	mw.y = 1;
	GUI::Frame *box = new GUI::Frame(frame,16,80,300,50);
	mw.cb1 = new GUI::Checkbox(box,0,0,"Allow to close this window");
	mw.cb2 = new GUI::Checkbox(box,0,20,"Move this window");
	box = new GUI::Frame(frame,16,130,300,80,"Radio Buttons");
	(new GUI::Radiobox(box,0,0,"Normal"))->setChecked(true);
	new GUI::Radiobox(box,0,20,"Dynamic");
	new GUI::Radiobox(box,0,40,"Simple");
	box->addActionHandler(&ex);
	GUI::Timer::add(&mw,10);
	frame->addWindowHandler(&mw);
	GUI::Menubar *bar = new GUI::Menubar(frame,0,0,frame->getWidth());
	bar->addMenu("File");
	bar->addItem(0,"New...");
	bar->addItem(0,"Open...");
	bar->addItem(0,"");
	bar->addItem(0,"Save");
	bar->addItem(0,"Save as...");
	bar->addItem(0,"");
	bar->addItem(0,"Close");
	bar->addItem(0,"Quit");
	bar->addMenu("Edit");
	bar->addItem(1,"Undo");
	bar->addItem(1,"Redo");
	bar->addItem(1,"");
	bar->addItem(1,"Cut");
	bar->addItem(1,"Copy");
	bar->addItem(1,"Paste");
	bar->addItem(1,"");
	bar->addItem(1,"Select all");
	bar->addItem(1,"Select none");
	bar->addMenu("View");
	bar->addItem(2,"Zoom...");
	bar->addItem(2,"Zoom in");
	bar->addItem(2,"Zoom out");
	bar->addMenu("?");
	bar->addItem(3,"Manual");
	bar->addItem(3,"Search...");
	bar->addItem(3,"");
	bar->addItem(3,"About");
	bar->addActionHandler(&ex);

	SDL_Event event;
	while (1) {
		while (SDL_PollEvent(&event)) {
			if (!guiscreen.event(event)) {
				if (event.type == SDL_QUIT) exit(0);
			}
		}

		if (SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
		memset(screen->pixels,0xff,4*640*15);
		memset(((char *)screen->pixels)+4*640*15,0x00,4*640*450);
		memset(((char *)screen->pixels)+4*640*465,0xff,4*640*15);
		if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
		guiscreen.update(4);
		SDL_UpdateRect(screen, 0, 0, screen->w, screen->h);

		SDL_Delay(40);
	}
}
#endif

#endif
