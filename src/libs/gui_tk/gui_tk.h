/** \page gui::tk - framework-agnostic C++ GUI toolkit
 *
 * \section i Introduction
 *
 * gui::tk is a simple one-file C++ GUI toolkit for use with arbitrary
 * memory framebuffers.
 *
 * \section f Features
 *
 * \li small source and binary code size
 * \li reasonable performance and memory usage
 * \li comfortable usage
 * \li suitable for embedded usage: integer math only
 * \li extensibility via OO
 * \li non-intrusive: can be integrated with any event mechanism of your liking
 * \li no dependencies apart from standards-conformant ANSI C++ (including a little STL)
 * \li support for different encodings, single- and multibyte
 * \li flexible font support
 *
 * \section o Overview
 *
 * The toolkit draws on a surface you provide, using any size or pixel format.
 * Create a GUI::Screen with the buffer to draw on, then pass that object
 * (or a GUI::Window created from it) to all other widgets' constructors.
 *
 * It doesn't provide an own event loop. Instead, it relies on you passing events
 * and updating the screen regularly. This way, it can easily be integrated with
 * any event loop available (SDL, Qt, glib, X11, Win32, selfmade, ...)
 *
 * Many functions and concepts were taken from other well-known toolkits, so if you
 * know Qt, Java or wxWindows, you may encounter (intentional) similarities. However,
 * simplicity of code has been given priority over maximum optimization and pixel-exact
 * replication of their appearance.
 *
 * A note about font support and encodings: All text-related functions use templates to
 * support any encoding. Fonts define the charset.
 *
 * \section g Getting Started
 *
 * gui::tk contains an SDL adapter, so if your program is already using SDL,
 * things are really easy. The rough sequence to get up and running is:
 *
 * \code
 * // setup a suitable video mode with SDL
 * SDL_surface *screen = SDL_SetVideoMode(640, 480, 32, SDL_SWSURFACE);
 *
 * // add a default font, you will most probably need it
 * GUI::Font::addFont("default",new GUI::BitmapFont(testfont,14,10));
 *
 * // create the root-level screen object, the parent of all top-level windows
 * GUI::ScreenSDL guiscreen(screen);
 *
 * // create any amount of toplevel windows you like
 * GUI::ToplevelWindow *frame = new GUI::ToplevelWindow(&guiscreen, 205, 100, 380, 250, "gui::tk example");
 *
 * // add some GUI elements to the toplevel window
 * GUI::Button *b = new GUI::Button(frame,8,20,"Quit");
 *
 * // Define and add an event handler
 * struct quit : public GUI::ActionEventSource_Callback {
 *     void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) {
 *	 exit(0);
 *     }
 * } ex;
 * b->addActionHandler(&ex);
 *
 * // Enter an event loop, calling update() regularly.
 * SDL_Event event;
 * while (1) {
 *     while (SDL_PollEvent(&event)) {
 *	 if (!guiscreen.event(event)) { // gui::tk didn't handle that event
 *	     if (event.type == SDL_QUIT) exit(0);
 *	 }
 *     }
 *
 *     guiscreen.update(4); // 4 ticks = 40ms
 *     SDL_UpdateRect(screen, 0, 0, screen->w, screen->h);
 *
 *     SDL_Delay(40); // wait 40ms
 * }
 * \endcode
 *
 * Look at gui_tk.h for more detailed documentation and reference of all classes.
 *
 * A test program that shows off the capabilities of gui::tk is available as part of gui_tk.cpp.
 *
 * \section l License
 *
 * Copyright (C) 2005-2007 Jörg Walter
 *
 * gui::tk is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * gui::tk is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */


#ifndef GUI__TOOLKIT_H
#define GUI__TOOLKIT_H

#define imin(x,y) ((x)<(y)?(x):(y))
#define imax(x,y) ((x)>(y)?(x):(y))
#define isign(x) (((x)<0?-1:1))

/** \file
 *  \brief Header file for gui::tk.
 *
 *  All you need to do is to include "gui_tk.h". If you want SDL support, then include \c SDL.h first.
 */

#ifdef _MSC_VER
typedef signed __int8   int8_t;
typedef signed __int16  int16_t;
typedef signed __int32  int32_t;
typedef unsigned __int8  uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
#else
#include <stdint.h>
#endif

#include <list>
#include <map>
#include <vector>
#include <typeinfo>
#include <string>
#include <iostream>

/// This namespace contains all GUI toolkit classes, types and functions.
namespace GUI {

/// ARGB 24-bit color value: (a<<24)|(r<<16)|(g<<8)|(b)
typedef uint32_t RGB;

/// Collection of all color-related constants and functions.
namespace Color {
/// A fully transparent pixel.
const RGB Transparent = 0x00ffffff;

/// A fully opaque black pixel.
const RGB Black = 0xff000000;

/// A fully opaque white pixel.
const RGB White = 0xffffffff;

/// Alpha mask.
const RGB AlphaMask = 0xff000000;

/// Offset of alpha value.
const int AlphaShift = 24;

/// Red mask.
const RGB RedMask = 0x00ff0000;

/// Full-intensity red.
const RGB Red = Black|RedMask;

/// Offset of red value.
const int RedShift = 16;

/// Green mask.
const RGB GreenMask = 0x0000ff00;

/// Full-intensity green.
const RGB Green = Black|GreenMask;

/// Offset of green value.
const int GreenShift = 8;

/// Blue mask.
const RGB BlueMask = 0x000000ff;

/// Full-intensity blue.
const RGB Blue = Black|BlueMask;

/// Offset of blue value.
const int BlueShift = 0;

/// Full-intensity Magenta.
const RGB Magenta = Red|Blue;

/// Magenta mask.
const RGB MagentaMask = RedMask|BlueMask;

/// Full-intensity Cyan.
const RGB Cyan = Green|Blue;

/// Cyan mask.
const RGB CyanMask = GreenMask|BlueMask;

/// Full-intensity Yellow.
const RGB Yellow = Red|Green;

/// Yellow mask.
const RGB YellowMask = RedMask|GreenMask;

/// 50% grey
const RGB Grey50 = 0xff808080;

/// Background color for 3D effects. May be customized.
extern RGB Background3D;

/// Light highlight color for 3D effects. May be customized.
extern RGB Light3D;

/// Dark highlight color (shadow) for 3D effects. May be customized.
extern RGB Shadow3D;

/// Generic border color for highlights or similar. May be customized.
extern RGB Border;

/// Foreground color for regular content (mainly text). May be customized.
extern RGB Text;

/// Background color for inactive areas. May be customized.
extern RGB Background;

/// Background color for selected areas. May be customized.
extern RGB SelectionBackground;

/// Foreground color for selected areas. May be customized.
extern RGB SelectionForeground;

/// Background color for inputs / application area. May be customized.
extern RGB EditableBackground;

/// Title bar color for windows. May be customized.
extern RGB Titlebar;

/// Title bar text color for windows. May be customized.
extern RGB TitlebarText;

/// Title bar color for windows. May be customized.
extern RGB TitlebarInactive;

/// Title bar text color for windows. May be customized.
extern RGB TitlebarInactiveText;

/// Convert separate r, g, b and a values (each 0-255) to an RGB value.
static inline RGB rgba(unsigned int r, unsigned int g, unsigned int b, unsigned int a=0) {
	return (((r&255)<<RedShift)|((g&255)<<GreenShift)|((b&255)<<BlueShift)|((a&255)<<AlphaShift));
}

/// Get red value (0-255) from an RGB value.
static inline unsigned int R(RGB val) { return ((val&Color::RedMask)>>Color::RedShift); }
/// Get green value (0-255) from an RGB value.
static inline unsigned int G(RGB val) { return ((val&Color::GreenMask)>>Color::GreenShift); }
/// Get blue value (0-255) from an RGB value.
static inline unsigned int B(RGB val) { return ((val&Color::BlueMask)>>Color::BlueShift); }
/// Get alpha value (0-255) from an RGB value.
static inline unsigned int A(RGB val) { return ((val&Color::AlphaMask)>>Color::AlphaShift); }

}

/// Identifies a mouse button.
enum MouseButton { NoButton, Left, Right, Middle, WheelUp, WheelDown, WheelLeft, WheelRight };

/// A type which holds size values that can be very large.
typedef unsigned int Size;

/// A type which holds a single character. Large enough for Unicode.
typedef uint32_t Char;

/// A type which holds a number of timer ticks.
typedef unsigned int Ticks;

/// Identifies a keyboard key.
class Key {
public:
	/// Translated character value.
	/** No encoding is implied. The Font that is used to display this character
	 *  determines the appearance. */
	Char character;
	/// Special keyboard keys.
	/** It is modeled after PC keyboards. When you feed keyboard events to a GUI::Screen,
	 *  try to map native keys to this set. Some special keys have a character value. Set
	 *  to \c None if the key has no special meaning. */
	enum Special {
		None,
		F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
		Up, Down, Left, Right, Backspace, Tab, Backtab, Enter, Escape,
		Home, End, PageUp, PageDown, Insert, Delete, Menu,
		Print, Pause, Break, CapsLock, NumLock, ScrollLock,
		Alt, Ctrl, Shift, Windows
	} special;
	/// Set if the Shift key is currently down.
	bool shift;
	/// Set if the Ctrl key is currently down.
	bool ctrl;
	/// Set if the Alt (PC) or Meta (classic Unix) key is currently down.
	bool alt;
	/// Set if the "Windows"/Meta (PC) or Super (classic Unix) key is currently down.
	/** Do not depend too heavily on this, as many keyboards and systems do not have such a key. */
	bool windows;

	/// Constructor.
	Key(GUI::Char character, Special special, bool shift, bool ctrl, bool alt, bool windows) :
		character(character), special(special),
		shift(shift), ctrl(ctrl), alt(alt), windows(windows) {}
};

class Drawable;
class String;

/** \brief Converts between strings of various types and String objects.
 *
 *  It is used to deal with string encoding. It is intended to be used with template
 *  specializations. Having such a specialization means you can feed the corresponding type to
 *  all functions that expect a string -- without any conversion. You can add specializations
 *  yourself in your program's code to deal with unsupported types.
 *
 *  As an example, see the std::string version.
 *
 *  Encodings, as opposed to character sets, define how bytes map to character values.
 *  ASCII, for example, has an \em encoding where each byte is one character, while the
 *  ASCII \em character \em set says that values 0-127 are valid and 65 is the upper case
 *  letter 'A'. UTF-8 has 1-6 bytes per character, and the character set is unicode.
 *
 *  GUI::Font deals with the character set, and this class encapsulates encodings.
 *
 */
template <typename STR> class NativeString {
protected:
	friend class String;

	/// Converts a native string into a String object
	static void getString(String &dest, const STR &src)  { (void)dest; (void)src; STR::_this_string_type_is_not_supported_(); }

	/** \brief Converts a string object to native representation.
	*
	*  If some characters cannot be converted, they should silently be skipped. Apart from that,
	*  \c nativeString(stringNative(String(),X)) should be value-equal to \c X.
	*/
	static STR& getNative(const String &src) { (void)src; STR::_this_string_type_is_not_supported_();return*new STR(); }
};

template <typename STR> class NativeString<STR*> {
protected:
	friend class String;
	static void getString(String &dest, const STR *src);
	static STR* getNative(const String &src);
};

template <typename STR, Size N> class NativeString<STR[N]> : public NativeString<STR*> {};
template <typename STR, Size N> class NativeString<const STR[N]> : public NativeString<STR*> {};
template <typename STR> class NativeString<const STR*> : public NativeString<STR*> {};

/// 'less-than' comparison between pointer addresses
struct ltvoid { bool operator()(const void* s1, const void* s2) const { return s1 < s2; } };

class Refcount {
public:
    /* NTS: Refcount starts at zero because there's so much of this damn code that expects to
     *      create UI elements with just "new Window blah blah" and for the class to delete it.
     *      This makes the transition to proper refcounting less painful and more gradual. */
    Refcount() : _refcount(0) { }
    virtual ~Refcount() {
        if (_refcount != 0) fprintf(stderr,"WARNING: GUI_TK::Refcount object %p refcount is nonzero (%d) on destructor\n",(void*)this,_refcount);
    }
public:
    int addref(void) {
        return ++_refcount;
    }
    int release(void) {
        int ref = --_refcount;

        if (ref < 0) fprintf(stderr,"WARNING: GUI_TK::Refcount object %p refcount is negative (%d) after release\n",(void*)this,_refcount);
        if (ref == 0) delete this;

        return ref;
    }
private:
    volatile int _refcount;
};

template <class C> class RefcountAuto : public Refcount {
public:
    RefcountAuto() : Refcount(), _ptr(NULL) { }
    RefcountAuto(C * const np) : _ptr(np) { if (_ptr != NULL) _ptr->addref(); }
    virtual ~RefcountAuto() { if (_ptr != NULL) _ptr->release(); }
public:
    C* operator=(C * const np) {
        if (_ptr != np) {
            C* _old = _ptr;
            _ptr = np;
            if (_ptr != NULL) _ptr->addref();
            if (_old != NULL) _old->release();
        }

        return _ptr;
    }
    bool operator==(const C * const np) const {
        return (_ptr == np);
    }
    bool operator!=(const C * const np) const {
        return !(*this == np);
    }
    bool operator!() const { /* ex: if (!ptr) ... equiv. if (ptr == NULL) */
        return (_ptr == NULL);
    }
    C* getptr(void) const {
        return _ptr;
    }
    C& operator*(void) const {
        return *_ptr;
    }
    C* operator->(void) const {
        return _ptr;
    }
private:
    C*              _ptr;
};

/** \brief Simple STL-based string class.
 *
 *  This is intended as internal helper class to allow gui::tk to work with any kind of
 *  string objects. While you can use this in normal application code, you should better
 *  use the string class of your application framework (like Qt). If you don't have any,
 *  use std::string.
 *
 *  It supports arbitrary characters, no character set is implied. Conversion from/to usual
 *  string types like \c char* is automatic but not thread-safe for non-class types.
 */
class String : public std::vector<Char> {
protected:
	template <typename STR> friend class NativeString;
	/// Simple pointer encapsulation class for memory management.
	class Native { public: virtual ~Native() {}; };
	/// Simple pointer encapsulation class for memory management.
	template <typename STR> class NativeArray: public Native {
		STR *data;
	public:
		NativeArray(STR *data) : data(data) {}
		virtual ~NativeArray() { delete[] data; }
	};
	template <typename STR> class NativeObject: public Native {
		STR *data;
	public:
		NativeObject(STR *data) : data(data) {}
		virtual ~NativeObject() { delete data; }
	};

private:
	/// Semi-static memory management for pointer string types.
	mutable std::map<const class std::type_info *, Native *, ltvoid> strings;

protected:
	/// Manage a native string's memory.
	void addNative(Native *dest) const {
		const class std::type_info &type = typeid(dest);
		if (strings[&type] != NULL) delete strings[&type];
		strings[&type] = dest;
	}

public:
	/// Allocate a new String initialized from native string.
	template <typename STR> String(const STR& src) { NativeString<STR>::getString(*this, src); }

	/// Taken from STL.
	template <class InputIterator> String(InputIterator a, InputIterator b) : std::vector<Char>(a,b) {}

	/// Copy-constructor
	String(const String &src) : std::vector<Char>(src), strings() {};

	/// Allocate a new String.
	String() { }

	/// Deallocate a String.
	~String() {
		for (std::map<const class std::type_info *, Native *, ltvoid>::iterator i = strings.begin(); i != strings.end(); ++i)
			delete (*i).second;
	};

	/// Convert to native representation.
	/** For pointer types like \c char*, the returned pointer is usually only valid as long as
	 *  this object exists, or until it is modified and cast to the same type again. */
	template <typename T> operator T() const { return NativeString<T>::getNative(*this); }

	/// Compare with native representation.
	template <typename T> bool operator==(const T &src) const { return *this == String(src); }
	/// Compare with other Strings.
	bool operator==(const String &src) const { return *(std::vector<Char>*)this == src; }

	/// Compare with native representation.
	template <typename T> bool operator!=(const T &src) const { return *this != String(src); }
	/// Compare with other Strings.
	bool operator!=(const String &src) const { return *(std::vector<Char>*)this != src; }

    /// Explicit declaration of default = operator
    String& operator=(const String&) = default;
};

template <typename STR> void NativeString<STR*>::getString(String &dest, const STR* src) {
	Size strlen = 0;
	while (src[strlen]) strlen++;
	dest.resize(strlen);
	for (strlen = 0; src[(unsigned int)strlen]; strlen++) dest[(unsigned int)strlen] = (unsigned int)(sizeof(STR)==1?(unsigned char)src[strlen]:sizeof(STR)==2?(unsigned short)src[strlen]:src[strlen]);
}

template <typename STR> STR* NativeString<STR*>::getNative(const String &src) {
	Size strlen = (Size)src.size();
	STR* dest = new STR[strlen+1];
	dest[strlen] = 0;
	for (; strlen > 0; strlen--) dest[strlen-1] = src[strlen-1];
	src.addNative(new String::NativeArray<const STR>(dest));
	return dest;
}

template <> class NativeString<std::string*> {
protected:
	friend class String;
	static void getString(String &dest, const std::string *src) {
		Size strlen = (Size)src->length();
		dest.resize(strlen);
		for (Size i = 0; i< strlen; i++) dest[i] = (unsigned int)((*src)[i]);
	}
	static std::string* getNative(const String &src) {
		Size strlen = (Size)src.size();
		std::string* dest = new std::string();
		for (Size i = 0; i < strlen; i++) dest->append(1,src[i]);
		src.addNative(new String::NativeObject<std::string>(dest));
		return dest;
	}
};

template <> class NativeString<const std::string*> : public NativeString<std::string*> {};

template <> class NativeString<std::string> {
protected:
	friend class String;
	static void getString(String &dest, const std::string &src) {
		Size strlen = (Size)src.length();
		dest.resize(strlen);
		for (Size i = 0; i< strlen; i++) dest[i] = (unsigned int)src[i];
	}
	static std::string& getNative(const String &src) {
		Size strlen = (Size)src.size();
		std::string* dest = new std::string();
		for (Size i = 0; i < strlen; i++) dest->append(1,src[i]);
		src.addNative(new String::NativeObject<std::string>(dest));
		return *dest;
	}
};

template <> class NativeString<const std::string> : public NativeString<std::string> {};

class ToplevelWindow;
class Menu;
class TransientWindow;
class Screen;
class Window;

/// Callback for window events.
struct Window_Callback {
public:
		/// Called whenever this window changes position.
		virtual void windowMoved(Window *win, int x, int y) = 0;
		virtual ~Window_Callback() {}
};


/** \brief A Window is a rectangular sub-area of another window.
 *
 *  Windows are arranged hierarchically. A Window cannot leave its parent's
 *  area. They may contain their own buffer or share it with their parent.
 *
 *  Usually, every GUI element is a subclass of Window. Note that this is
 *  \em not a GUI window with decorations like border and title bar. See ToplevelWindow
 *  for that.
 */
class Window : public Refcount {
protected:
	friend class ToplevelWindow;
	friend class TransientWindow;
    friend class WindowInWindow;
	friend class Menu;

	/// Width of the window.
	int width;
	/// Height of the window.
	int height;
	/// X position relative to parent.
	int x;
	/// Y position relative to parent.
	int y;

	/// \c true if anything changed in this window or one of it's children
	bool dirty;

	/// \c true if this window should be visible on screen.
	bool visible;

    /// \c true if the user should be allowed to TAB to this window.
    bool tabbable;

	/// Parent window.
	Window *const parent;

	/// Child window of last button-down event
	/** It receives all drag/up/click/doubleclick events until an up event is received */
	Window  *mouseChild;

    /// \c true if this window is transient (such as menu popus)
    bool transient;

    /// \c toplevel window
    bool toplevel;

    /// \c mouse is within the boundaries of the window
    bool mouse_in_window;
public:
    /// \c first element of a tabbable list
    bool first_tabbable = false;

    /// \c last element of a tabbable list
    bool last_tabbable = false;
protected:
	/// Child windows.
	/** Z ordering is done in list order. The first element is the lowermost
	 *  window. This window's content is below all children. */
	std::list<Window *> children;

	/// List of registered event handlers.
	std::list<Window_Callback *> movehandlers;

	/// Register child window.
	virtual void addChild(Window *child);

	/// Remove child window.
	virtual void removeChild(Window *child);

	/// Mark this window and all parents as dirty.
	void setDirty() { if (dirty) return; dirty = true; if (parent != NULL) parent->setDirty(); };

	/// Replace clipboard content.
	virtual void setClipboard(const String &s) { parent->setClipboard(s); };

	/// Get clipboard content.
	virtual const String& getClipboard() { return parent->getClipboard(); };

	/// Default constructor. Only used in class Screen. Do not use.
	Window();

	/// Called whenever the focus changes.
	virtual void focusChanged(bool gained) {
		if (children.size() > 0) children.back()->focusChanged(gained);
	}

public:

	/// Add an event handler.
	void addWindowHandler(Window_Callback *handler) { movehandlers.push_back(handler); }

	/// Remove an event handler.
	void removeWindowHandler(Window_Callback *handler) { movehandlers.remove(handler); }

	/// Create a subwindow of the given parent window.
	Window(Window *parent, int x, int y, int w, int h);

	/// Destructor.
	virtual ~Window();

	/// Resize this window to the given dimensions.
	/** If that would move part of the window outside of this window's area,
	 *  the outside area will silently be clipped while drawing. */
	virtual void resize(int w, int h);
	/// Return this window's width
	virtual int getWidth() const { return width; }
	/// Return this window's height
	virtual int getHeight() const { return height; }

	/// Move this window to the given position relative to the parent window's origin.
	/** If that would move part of the window outside of this window's area,
	 *  the outside area will silently be clipped while drawing. */
	virtual void move(int x, int y);
	/// Return this window's X position relative to the parent's top left corner
	virtual int getX() const { return x; }
	/// Return this window's Y position relative to the parent's top left corner
	virtual int getY() const { return y; }
	/// Return this window's contents' X position relative to the screen's top left corner
	virtual int getScreenX() const { return (parent == NULL?0:parent->getScreenX()+x); }
	/// Return this window's contents' Y position relative to the screen's top left corner
	virtual int getScreenY() const { return (parent == NULL?0:parent->getScreenY()+y); }

	/// Draw this window's content including all children.
	virtual void paintAll(Drawable &d) const;

	/// Draw this window's content.
	virtual void paint(Drawable &d) const { (void)d; };

	/// Show or hide this window.
	/** By default, most windows are shown when created. Hidden windows do not receive any events. */
	virtual void setVisible(bool v) { visible = !!v; parent->setDirty(); if (!visible) mouse_in_window = false; }

	/// Returns \c true if this window is visible.
	virtual bool isVisible() const { return (!parent || parent->isVisible()) && visible; }

	/// Return parent window.
	/** May return NULL if this is the topmost window (the Screen). */
	Window *getParent() const { return parent; }

	/// Get the topmost window (the Screen)
	Screen *getScreen();

	/// Returns \c true if this window has currently the keyboard focus.
	virtual bool hasFocus() const { return parent->hasFocus() && *parent->children.rbegin() == this; }

	/// Mouse was moved. Returns true if event was handled.
	virtual bool mouseMoved(int x, int y);
	/// Mouse was moved outside the window (when it was once inside the window)
	virtual void mouseMovedOutside(void);
	/// Mouse was moved while a button was pressed. Returns true if event was handled.
	virtual bool mouseDragged(int x, int y, MouseButton button);
	/// Mouse was pressed. Returns true if event was handled.
	virtual bool mouseDown(int x, int y, MouseButton button);
	/// Mouse was released. Returns true if event was handled.
	virtual bool mouseUp(int x, int y, MouseButton button);
	/// Mouse was clicked. Returns true if event was handled.
	/** Clicking means pressing and releasing the mouse button while not moving it. */
	virtual bool mouseClicked(int x, int y, MouseButton button);
	/// Mouse was double-clicked. Returns true if event was handled.
	virtual bool mouseDoubleClicked(int x, int y, MouseButton button);
	/// Mouse was pressed outside the bounds of the window, if this window has focus (for transient windows). Returns true if event was handled.
    /// Transient windows by default should disappear.
	virtual bool mouseDownOutside(MouseButton button);

	/// Key was pressed. Returns true if event was handled.
	virtual bool keyDown(const Key &key);
	/// Key was released. Returns true if event was handled.
	virtual bool keyUp(const Key &key);

	/// Put this window on top of all it's siblings. Preserves relative order.
	/** Returns true if the window accepts the raise request. */
	virtual bool raise() {
		Window *last = parent->children.back();
		for (Window *cur = parent->children.back(); cur != this; cur = parent->children.back()) {
			parent->children.remove(cur);
			parent->children.push_front(cur);
		}
		if (last != this) {
			focusChanged(true);
			last->focusChanged(false);
		}
		parent->setDirty();
		return true;
	}

	/// Put this window below all of it's siblings. Does not preserve relative order.
	virtual void lower() {
		parent->children.remove(this);
		parent->children.push_front(this);
		if (this != parent->children.back()) {
			parent->children.back()->focusChanged(true);
			focusChanged(false);
		}
		parent->setDirty();
	}

	/// Return the \p n th child
	Window *getChild(int n) {
		for (std::list<Window *>::const_iterator i = children.begin(); i != children.end(); ++i) {
			if ((n--) == 0) return *i;
		}
		return NULL;
	}

    unsigned int getChildCount(void) {
        return (unsigned int)children.size();
    }

};

class Timer;

/// Timer callback type
struct Timer_Callback {
public:
		/// The timer has expired.
		/** Callbacks for timers take one parameter, the number of ticks since
		 *  application start. Note that this value may wrap after a little less
		 *  than 500 days. If you want callbacks to be called again, return the
		 *  delay in ticks relative to the scheduled time of this
		 *  callback (which may be earlier than now() ). Otherwise return 0. */
		virtual Ticks timerExpired(Ticks time) = 0;
		virtual ~Timer_Callback() {}
};

/** \brief Timing service.
 *  Time is measured in ticks. A tick is about 10 msec.
 *
 *  Note that this is not suitable as a high-accuracy timing service. Timer
 *  events can be off by many ticks, and a tick may be slightly more or
 *  slightly less than 10 msec. Because of that, Timers are only intended for
 *  simple animation effects, input timeouts and similar things.
 *
 */
class Timer {
protected:
	/// Number of ticks since application start.
	static Ticks ticks;

	/// Compare two integers for 'less-than'.
	struct ltuint { bool operator()(Ticks i1, Ticks i2) const {
		return (i1 < i2);
	} };

	/// Active timers.
	static std::multimap<Ticks,Timer_Callback*,ltuint> timers;

public:
	/// Advance time and check for expired timers.
	static void check(Ticks ticks);
	static void check_to(Ticks ticks);

	/// Add a timed callback. \p ticks is a value relative to now().
	/** \p cb is not copied. */
	static void add(Timer_Callback *cb, const Ticks ticks) { timers.insert(std::pair<const Ticks,Timer_Callback *>(ticks+Timer::ticks,cb)); }

	static void remove(const Timer_Callback *const timer);

	/// Return current time (ticks since application start)
	static Ticks now() { return ticks; }

	/// Return number of ticks until next scheduled timer or 0 if no timers
	static Ticks next();
};

struct vscrollbarlayout {
    SDL_Rect scrollthumbRegion = {0,0,0,0};
    SDL_Rect scrollbarRegion = {0,0,0,0};
    int thumbwidth = 0;
    int thumbheight = 0;
    int thumbtravel = 0;
    bool drawthumb = false;
    bool disabled = true;
    bool draw = false;
    int xleft = -1;
    int ytop = -1;
};

/* Window wrapper to make scrollable regions */
class WindowInWindow : public Window {
protected:
    void scrollToWindow(Window *child);
public:
	WindowInWindow(Window *parent, int x, int y, int w, int h) :
		Window(parent,x,y,w,h) {}

    /// Mouse was moved while a button was pressed. Returns true if event was handled.
	virtual bool mouseDragged(int x, int y, MouseButton button);
	/// Mouse was pressed. Returns true if event was handled.
	virtual bool mouseDown(int x, int y, MouseButton button);
	/// Mouse was released. Returns true if event was handled.
	virtual bool mouseUp(int x, int y, MouseButton button);

	/// Mouse was moved. Returns true if event was handled.
	virtual bool mouseMoved(int x, int y);
	/// Mouse was clicked. Returns true if event was handled.
	/** Clicking means pressing and releasing the mouse button while not moving it. */
	virtual bool mouseClicked(int x, int y, MouseButton button);
	/// Mouse was double-clicked. Returns true if event was handled.
	virtual bool mouseDoubleClicked(int x, int y, MouseButton button);

	/// Key was pressed. Returns true if event was handled.
	virtual bool keyDown(const Key &key);

	virtual void getVScrollInfo(vscrollbarlayout &vsl) const;
	virtual void paintScrollBarArrowInBox(Drawable &dscroll,const int x,const int y,const int w,const int h,bool downArrow,bool disabled) const;
	virtual void paintScrollBarThumbDragOutline(Drawable &dscroll,const vscrollbarlayout &vsl) const;
	virtual void paintScrollBarBackground(Drawable &dscroll,const vscrollbarlayout &vsl) const;
	virtual void paintScrollBarThumb(Drawable &dscroll, vscrollbarlayout &vsl) const;
	virtual void paintScrollBar3DOutset(Drawable &dscroll, int x, int y, int w, int h) const;
	virtual void paintScrollBar3DInset(Drawable &dscroll, int x, int y, int w, int h) const;
	virtual void paintAll(Drawable &d) const;

	virtual void resize(int w, int h);

    virtual void enableScrollBars(bool hs,bool vs);
    virtual void enableBorder(bool en);

    bool    hscroll_dragging = false;
    bool    vscroll_dragging = false;
    bool    vscroll_uparrowhold = false;
    bool    vscroll_downarrowhold = false;
    bool    vscroll_uparrowdown = false;
    bool    vscroll_downarrowdown = false;

    /// Timer callback type
    struct DragTimer_Callback : public Timer_Callback {
        public:
            /// The timer has expired.
            /** Callbacks for timers take one parameter, the number of ticks since
             *  application start. Note that this value may wrap after a little less
             *  than 500 days. If you want callbacks to be called again, return the
             *  delay in ticks relative to the scheduled time of this
             *  callback (which may be earlier than now() ). Otherwise return 0. */
            virtual Ticks timerExpired(Ticks time);
            virtual ~DragTimer_Callback() {}
        public:
            WindowInWindow *wnd = NULL;
    };

    bool    dragging = false;
    int     drag_x = 0, drag_y = 0;
    Ticks   drag_start = 0;
    int     drag_start_pos = 0;
    Timer   drag_timer;
    DragTimer_Callback drag_timer_cb;

    int     scroll_pos_x = 0;
    int     scroll_pos_y = 0;
    int     scroll_pos_w = 0;
    int     scroll_pos_h = 0;

    bool    hscroll = false;
    bool    vscroll = false;

    int     hscroll_display_width = 16;
    int     vscroll_display_width = 16;

    bool    border = false;
};

/** \brief A Screen represents the framebuffer that is the final destination of the GUI.
 *
 *  It's main purpose is to manage the current contents of the surface and to combine
 *  it with all GUI elements. You can't resize and move the screen. Requests to do so
 *  will be ignored.
 *
 *  This is an abstract base class. You need to use a subclass which implements rgbToSurface
 *  and surfaceToRGB.
 *
 *  To make things work, Screen needs events. Call the mouse and key event functions (see Window)
 *  whenever such an event occurs. Call update(void *surface, int ticks) regularly, if possible about
 *  every 40msec (25 fps). If nothing has changed, screen updates are quite fast.
 */
class Screen : public Window {
protected:
	/// Screen buffer.
	Drawable *const buffer;
    bool buffer_i_alloc;

	/// Clipboard.
	String clipboard;

	/// Currently pressed mouse button.
	MouseButton button = (MouseButton)0;

	/// Store a single RGB triple (8 bit each) as a native pixel value and advance pointer.
	virtual void rgbToSurface(RGB color, void **pixel) = 0;

	/// Map a single framebuffer pixel to an RGB value.
	virtual RGB surfaceToRGB(void *pixel) = 0;

	/// Create a new screen with the given characteristics.
	Screen(Size width, Size height);

	/// Create a new screen from the given GUI::Drawable.
	Screen(Drawable *d);

public:
	/// Destructor.
	virtual ~Screen();

	/// Set clipboard content.
	template <typename STR> void setClipboard(const STR s) { this->setClipboard(String(s)); }

	/// Set clipboard content.
	virtual void setClipboard(const String &s) { clipboard = s; }

	/// Get clipboard content.
	virtual const String& getClipboard() { return clipboard; }

	/// Do nothing.
	virtual void resize(int w, int h);

	/// Do nothing.
	virtual void move(int x, int y);

	/// Screen has always focus.
	virtual bool hasFocus() const { return true; }

	/// Update the given surface with this screen's content, fully honouring the alpha channel.
	/** \p ticks can be set to a different value depending on how much time has passed. Timing
	 *  doesn't need to be perfect, but try to call this at least every 40 msec. Each tick
	 *  amounts to about 10 msec. Returns the number of ticks until the next event is scheduled, or
	 *  0 if none. */
	Ticks update(void *surface, Ticks ticks = 1);

	/// Default: clear screen.
	virtual void paint(Drawable &d) const;
};


/** \brief A 24 bit per pixel RGB framebuffer aligned to 32 bit per pixel.
 *
 *  Warning: This framebuffer type varies with CPU endiannes. It is meant as
 *  a testing/debugging tool.
 */
class ScreenRGB32le : public Screen {
protected:
	/// Map a single RGB triple (8 bit each) to a native pixel value.
	virtual void rgbToSurface(RGB color, void **pixel) { RGB **p = (RGB **)pixel; **p = color; (*p)++; };

	/// Map a single surface pixel to an RGB value.
	virtual RGB surfaceToRGB(void *pixel) { return *(RGB*)pixel; };
public:
	ScreenRGB32le(Size width, Size height) : Screen(width,height) {};

	virtual void paint(Drawable &d) const;
};


#ifdef SDL_MAJOR_VERSION
/** \brief An SDL surface adapter.
 *
 *  This screen type will draw on an SDL surface (honouring transparency if the
 *  surface is already filled) and react on SDL events. It does not provide an
 *  application event loop, you have to call update() and event() regularly until
 *  you decide to quit the application.
 *
 *  The implementation of this class could as well have been integrated into
 *  GUI::Screen. That might have been a little more efficient, but this way
 *  this class serves as an example for custom screen classes.
 *
 * Note that you have to include \c SDL.h \em before \c gui_tk.h for this class
 * to become available.
 */
class ScreenSDL : public Screen {
protected:
	/// not used.
	virtual void rgbToSurface(RGB color, void **pixel) { (void)color; (void)pixel; };

	/// not used.
	virtual RGB surfaceToRGB(void *pixel) { (void)pixel; return 0; };

	/// The SDL surface being drawn to.
	SDL_Surface *surface;
	Uint32 start_abs_time,current_abs_time,current_time;

	/// Position of last mouse down.
	int downx, downy;

	/// time of last click for double-click detection.
	Ticks lastclick,lastdown;

    /// Integer scaling factor.
	int scale;

public:

	/** Initialize SDL screen with a surface
	 *
	 *  The dimensions of this surface will define the screen dimensions. Changing the surface
	 *  later on will not change the available area.
	 */
	ScreenSDL(SDL_Surface *surface, int scale);
    virtual ~ScreenSDL();

	/** Change current surface
	 *
	 *  The new surface may have different dimensions than the current one, but
	 *  the screen size will not change. This means that either the screen will
	 *  not be displayed fully, or there will be borders around the screen.
	 */
	void setSurface(SDL_Surface *surface) { this->surface = surface; }

	/// Retrieve current surface.
	SDL_Surface *getSurface() { return surface; }

	/// Overridden: makes background transparent by default.
	virtual void paint(Drawable &d) const;

	/// Use this to update the SDL surface. The screen must not be locked.
	Ticks update(Ticks ticks);

	/// Process an SDL event. Returns \c true if event was handled.
	bool event(SDL_Event *ev) { return event(*ev); }

	void watchTime();
	Uint32 getTime() { return current_time; }

	/// Process an SDL event. Returns \c true if event was handled.
	bool event(SDL_Event& event);
};
#endif

class Font;

/** \brief A drawable represents a rectangular off-screen drawing area.
 *
 *  It is an off-screen buffer which can be copied to other drawables or
 *  to a Screen's framebuffer. The drawable's origin is at the top left. All
 *  operations are silently clipped to the available area. The alpha channel
 *  is honoured while copying one drawable to another, but not during other
 *  drawing operations.
 *
 *  Drawables have a current font, color and (x,y) position. Drawing takes place
 *  at the given point using the given font and color. The current position is
 *  then moved to the final point of the drawing primitive unless otherwise
 *  noted. Pixel width of all lines is selectable, but be aware that visual
 *  appearance of lines with width > 1 is not as sophisticated as in well-known
 *  toolkits like Java or Qt.
 *
 *  Some drawing primitives are overloaded to take full coordinates. These ignore
 *  the current position, but update it just like their regular brethren.
 */
class Drawable {
protected:
	friend Ticks Screen::update(void *, Ticks);
	/// The actual pixel buffer.
	RGB *const buffer;
	/// Total width of buffer.
	const int width;
	/// Total height of buffer.
	const int height;
	/// \c true if \a buffer must be freed by this instance.
	const bool owner;

	/// Current color.
	RGB color;
	/// Current font.
	const Font *font;
	/// Current line width.
	int lineWidth;

	/// X translation.
	const int tx;
	/// Y translation.
	const int ty;
	/// clip x.
	const int cx;
	/// clip y.
	const int cy;
	/// clip width.
	const int cw;
	/// clip height.
	const int ch;
	/// functional width.
	const int fw;
	/// functional height.
	const int fh;

	/// Current position x coordinate.
	int x;
	/// Current position y coordinate.
	int y;

public:
	/// Create an empty drawable object with given dimensions.
	/** Optionally, the area is cleared with a given color (default: fully transparent). */
	Drawable(int w, int h, RGB clear = Color::Transparent);

	/// Deep-copying copy constructor.
	/** It honours clip and translate so that the resulting drawable, if drawn
	 *  to the source drawable at (0,0), yields the same result as drawing
	 *  directly to the source surface. If \p fill is not explicitly set, will
	 *  copy the original surface's contents */
	Drawable(Drawable &src, RGB clear = 0);

	/// Shallow-copying copy constructor with clip & translate. See setClipTranslate(int x, int y, int w, int h).
	Drawable(Drawable &src, int x, int y, int w, int h);

	/// Destructor.
	virtual ~Drawable();

	/// Clears the surface.
	void clear(RGB clear = Color::Transparent);

	/// Change current drawing color.
	/** The alpha part is honoured in all drawing primitives like this:
	    All drawing operations in this window will unconditionally overwrite
	    earlier content of this window. Only when combining this window with
	    it's parent, the alpha channel is fully honoured. */
	void setColor(RGB c) { color = c; };
	/// Return the currently selected drawing color.
	RGB getColor() { return color; };

	/// Change current drawing font.
	void setFont(const Font *f) { font = f; };
	/// Return the currently selected drawing font.
	const Font *getFont() { return font; };

	/// Change current line width.
	void setLineWidth(int w) { lineWidth = w; };
	/// Return the current line width.
	int getLineWidth() { return lineWidth; };

	/// Move the current position to the given coordinates.
	void gotoXY(int x, int y) { this->x = x; this->y = y; };
	/// Return current position X.
	int getX() { return x; }
	/// Return current position Y.
	int getY() { return y; }

	/// Return clip width
	int getClipX() { return cx; }
	/// Return clip height
	int getClipY() { return cy; }
	/// Return clip width
	int getClipWidth() { return cw; }
	/// Return clip height
	int getClipHeight() { return ch; }

	/// Paint a single pixel at the current position.
	void drawPixel() { if (x >= cx && x < cw && y >= cy && y < ch) buffer[x+tx+(y+ty)*width] = color; };
	/// Paint a single pixel at the given coordinates.
	void drawPixel(int x, int y) { gotoXY(x,y); drawPixel(); };

	/// Return the pixel color at the current position.
	RGB getPixel() { if (x >= cx && x < cw && y >= cy && y < ch) return buffer[x+tx+(y+ty)*width]; return Color::Transparent; };
	/// Return the pixel color at the given coordinates.
	RGB getPixel(int x, int y) { gotoXY(x,y); return getPixel(); };

	/// Draw a straight line from the current position to the given coordinates.
	void drawLine(int x2, int y2);
	/// Draw a straight line from (\p x1,\p y1) to (\p x2,\p y2).
	void drawLine(int x1, int y1, int x2, int y2) { gotoXY(x1,y1); drawLine(x2,y2); };

	/// Draw a straight line from the current position to the given coordinates.
	void drawDotLine(int x2, int y2);
	/// Draw a straight line from (\p x1,\p y1) to (\p x2,\p y2).
	void drawDotLine(int x1, int y1, int x2, int y2) { gotoXY(x1,y1); drawDotLine(x2,y2); };

	/// Draw a circle centered at the current position with diameter \p d.
	/** The current position is not changed. */
	void drawCircle(int d);
	/// Draw a circle centered at the given coordinates with diameter \p d.
	/** The current position is set to the center. */
	void drawCircle(int x, int y, int d) { gotoXY(x, y); drawCircle(d); };

	/// Draw a rectangle with top left at the current position and size \p w, \p h.
	/** The current position is not changed. */
	void drawRect(int w, int h);
	/// Draw a rectangle with top left at the given coordinates and size \p w, \p h.
	/** The current position is set to the top left corner. */
	void drawRect(int x, int y, int w, int h) { gotoXY(x, y); drawRect(w, h); };

	/// Draw a rectangle with top left at the current position and size \p w, \p h.
	/** The current position is not changed. */
	void drawDotRect(int w, int h);
	/// Draw a rectangle with top left at the given coordinates and size \p w, \p h.
	/** The current position is set to the top left corner. */
	void drawDotRect(int x, int y, int w, int h) { gotoXY(x, y); drawDotRect(w, h); };

	/// Flood-fill an area at the current position.
	/** A continuous area with the same RGB value as the selected pixel is
	    changed to the current color. The current position is not changed. */
	void fill();
	/// Flood-fill an area at a given position.
	/** see fill(), but moves current position to the given coordinates */
	void fill(int x, int y) { gotoXY(x,y); fill(); };

	/// Draw a filled circle centered at the current position with diameter \p d.
	/** The current position is not changed. */
	void fillCircle(int d);
	/// Draw a filled circle centered at the given coordinates with diameter \p d.
	/** The current position is set to the center. */
	void fillCircle(int x, int y, int d) { gotoXY(x, y); fillCircle(d); };

	/// Draw a filled rectangle with top left at the current position and size \p w, \p h.
	/** The current position is not changed. */
	void fillRect(int w, int h);
	/// Draw a filled rectangle with top left at the given coordinates and size \p w, \p h.
	/** The current position is set to the top left corner. */
	void fillRect(int x, int y, int w, int h) { gotoXY(x, y); fillRect(w, h); };

	/// Draw a text string.
	/** Current position is the leftmost pixel on the baseline of the character.
	    The current position is moved to the next character. Background is not
	    cleared. If \p interpret is \c true, some ASCII control characters like
	    backspace, line-feed, tab and ANSI colors are interpreted, and text is word-wrapped at
	    the window borders. You can optionally pass start and length of a substring
	    to print */
	void drawText(const String& text, bool interpret = true, Size start = 0, Size len = (Size)-1);

	/// Draw a text string at a fixed position.
	/** see drawText(const String& text, bool interpret, Size start, Size len) */
	template <typename STR> void drawText(int x, int y, const STR text, bool interpret, Size start, Size len = (Size)-1) { gotoXY(x,y); drawText(String(text), interpret, start, len); }
	/// Draw a single character.
	/** see drawText(const STR text, bool interpret), except wrapping is
	    performed on this character only */
	void drawText(const Char c, bool interpret = false);
	/// Draw a single character at a fixed position.
	/** see drawText(const Char c, bool interpret). */
	void drawText(int x, int y, const Char c, bool interpret = false) { gotoXY(x,y); drawText(c, interpret); }

	/// Copy the contents of another Drawable to this one.
	/** The top left corner of the source Drawable is put at the current
	    position. The alpha channel is fully honoured. Additionally, an extra
	    \p alpha value may be given which is multiplied with the source
	    alpha channel. The current position remains unchanged. */
	void drawDrawable(Drawable &d, unsigned char alpha = 1);
	/// Copy the contents of another Drawable to this one at a given position.
	/** see drawDrawable(Drawable &d, unsigned char alpha). The current position
	    is moved to the given coordinates. */
	void drawDrawable(int x, int y, Drawable &d, unsigned char alpha = 1) { gotoXY(x,y); drawDrawable(d, alpha); }

};

/** \brief A variable- or fixed-width fixed-size Font.
 *
 *  These Fonts can't be scaled once instantiated, but it is possible
 *  to subclass this abstract font class to encapsulate scalable
 *  fonts. Fonts can be anti-aliasing and multi-coloured, depending on
 *  subclass. There is no encoding enforced. The font object implicitly
 *  knows about it's encoding. Because of that, there is a utility
 *  function for string examination as well.
 *
 *  You can't instantiate objects of this class directly, use one of the
 *  subclasses like BitmapFont.
 *
 *  This class provides a font registry which allows you to register
 *  and retrieve font objects using a name of your choice. It is recommended
 *  to use a naming scheme like "FontName-variant-pixelsize-encoding" where
 *  variant is "normal", "bold", "italic" or "bolditalic". No one enforces
 *  this, however.
 *
 *  The GUI uses some special font names. You must add them before creating
 *  the relevant GUI item.
 *  \li \c default - used if a requested font was not found
 *  \li \c title - GUI::ToplevelWindow title
 *  \li \c input - GUI::Input
 *
 */
class Font {
protected:
	friend void Drawable::drawText(const Char c, bool interpret);
	friend void Drawable::drawText(const String& s, bool interpret, Size start, Size len);

	/// Compare two strings for 'less-than'.
	struct ltstr { bool operator()(const char* s1, const char* s2) const {
		return strcmp(s1, s2) < 0;
	} };
	/// Font registry. Contains all known font objects indexed by name.
	static std::map<const char *,Font *,ltstr> registry;

	/// Default constructor.
	Font() {};

	/// Draw character to a drawable at the current position.
	/** \p d's current position is advanced to the position of the next character.
	 *  The y coordinate is located at the baseline before and after the call. */
	virtual void drawChar(Drawable *d, const Char c) const = 0;

	/// Draw \p len characters to a drawable at the current position, starting at string position \p start.
	/** This can be used to provide kerning. */
	virtual void drawString(Drawable *d, const String &s, Size start, Size len) const {
		if (len > s.size()-start) len = (Size)(s.size()-start);
		len += start;
		while (start < len) drawChar(d,s[start++]);
	}

public:
	/// Return a font with the given name (case-sensitive).
	/** If no font was registered with that name, returns NULL. */
	static const Font *getFont(const char *name) {
		std::map<const char *,Font *,ltstr>::iterator i = registry.find(name);
		if (i == registry.end()) return(strcmp(name,"default")?getFont("default"):NULL);
		return (*i).second;
	}

	static void registry_freeall() {
		std::map<const char *,Font *,ltstr>::iterator it;

		while ((it=registry.begin()) != registry.end()) {
			delete it->second;
			it->second = NULL;
			registry.erase(it);
		}
	}

	/// Add a font with a given name.
	/** Don't delete this font once added. This class will do that for you.
	 *  If a font with that name already exists, it will be replaced. */
	static void addFont(const char *name, Font *font) {
		std::map<const char *,Font *,ltstr>::iterator i = registry.find(name);
		if (i != registry.end()) delete (*i).second;
		registry[name] = font;
	}

	virtual ~Font() {};

	/// Retrieve total height of font in pixels.
	virtual int getHeight() const = 0;

	/// Retrieve the ascent, i.e. the number of pixels above the base line.
	virtual int getAscent() const = 0;

	/// Return width of a string.
	template <typename STR> int getWidth(const STR s, Size start = 0, Size len = (Size)-1) const {
		return this->getWidth(String(s), start, len);
	}

	/// Retrieve width of a character.
	virtual int getWidth(Char c = 'M') const = 0;

	/// Retrieve width of a string of characters.
	/** Can be used to provide kerning. */
	virtual int getWidth(const String &s, Size start = 0, Size len = (Size)-1) const {
		int width = 0;
		if ((size_t)start+len > s.size()) len = (Size)(s.size()-start);
		while (len--) width += getWidth(s[start++]);
		return width;
	}

	/// Characters with special appearance or meaning.
	enum SpecialChar { CR = '\r', LF = '\n', BS = '\b', Tab = '\t', Space = ' ', ESC = 27 };

	/// Convert a character to an equivalent SpecialChar. May return values not in SpecialChar.
	virtual SpecialChar toSpecial(Char c) const { return (SpecialChar)(c<255?c:255); }

	/// Convert a SpecialChar to an equivalent character.
	virtual Char fromSpecial(SpecialChar c) const { return c; }
};

/** \brief A bitmap font.
 *  This is a font which is defined by a binary bit map. Each bit in the bitmap
 *  defines one pixel. Bits may be arranged in various common patterns.
 *
 *  Encoding free, character size depends on number of characters in the font.
 */
class BitmapFont : public Font {
protected:
	/// The actual font data.
	const unsigned char *const bitmap;

	/// Width of a character cell.
	const int width;

	/// Height of all characters.
	const int height;

	/// Ascent of all characters.
	const int ascent;

	/// Array of character widths. If font is fixed-width, this is NULL and \a width is used.
	const int *const widths;

	/// Array of character ascents. If this is NULL, \a ascent is used for all characters.
	/** This allows character data to be flush to the top or bottom of it's bitmap area. */
	const int *const ascents;

	/// True if set bits are background, false otherwise.
	const bool background_set;

	/// Number of bits added to get from a column to the next column.
	const int col_step;

	/// Distance between 2 rows of a character, or 0 for variable-width rows.
	const int row_step;

	/// Distance of two characters in the bitmap in bits.
	/** This is calculated as abs(row_step*height) unless explicitly specified. */
	const int character_step;

	/// Array of pointers to font data. If set, neither \a bitmap nor \a character_step are used.
	const unsigned char *const*const char_position;

	/// Array of SpecialChar equivalents.
	/**  If unset, encoding is assumed ASCII-like for the first 32 characters */
	const SpecialChar *const special;

	/// If \c true, then all arrays are freed on destruction.
	const bool owner;

	/// Last defined character. Characters above this will be ignored.
	const Char last;

	/// Draw character to a drawable at the current position.
	/** \p d's current position is advanced to the position of the next character.
	 *  The y coordinate is located at the baseline before and after the call. */
	virtual void drawChar(Drawable *d, const Char c) const;

public:
	/// Constructor.
	/** The default values provide an 8 bit wide fixed-width pixel layout with each byte a row,
	 *  arranged top-to-bottom just like a PC's VGA font. See the individual member documentation
	 *  for details on the parameters. */
	BitmapFont(const unsigned char *data, int height, int ascent, bool owner = false,
		int width = 8, bool background_set = false,
		int col_step = -1, int row_step = 8, int character_step = 0, Char last = 256,
		const int *widths = NULL, const int *ascents = NULL,
		const unsigned char *const* char_position = NULL,
		const SpecialChar *special = NULL);

	virtual ~BitmapFont();

	/// Retrieve total height of font in pixels.
	virtual int getHeight() const { return height; };

	/// Retrieve the ascent, i.e. the number of pixels above the base line.
	virtual int getAscent() const { return ascent; };

	/// Retrieve width of a character.
	virtual int getWidth(Char c = 'M') const { return (widths != NULL?widths[c]:width); };

	/// Convert a character to an equivalent SpecialChar. See Font::toSpecial(Char c)
	virtual SpecialChar toSpecial(Char c) const { return (special != NULL?special[c]:Font::toSpecial(c)); }

	/// Convert a character to an equivalent character. See Font::fromSpecial(SpecialChar c).
	virtual Char fromSpecial(SpecialChar c) const { if (special == NULL) return Font::fromSpecial(c); Char i = 0; while(special[i] != c) i++; return i; }

};

class ActionEventSource;
/// Callback for action events.
struct ActionEventSource_Callback {
public:
		/// Handler with optional String argument.
		/** If the event source doesn't provide an additional argument, the name will be used. */
		virtual void actionExecuted(ActionEventSource *source, const String &arg) = 0;
		virtual ~ActionEventSource_Callback() {}
};

/// Event class for action events.
/** Action events are events generated by GUI elements like Buttons, Menus and by pressing Enter in
 *  an Input. All of these are handled similarly: The source of such an event has a name, and the
 *  Event may also be connected with a String describing what was executed, like the name of the
 *  Menu entry or the contents of the Input.
 */
class ActionEventSource {
protected:
	/// List of registered action handlers.
	std::list<ActionEventSource_Callback *> actionHandlers;

	/// This event source's name.
	/** The name is primarily meant for your own purposes, for example identification of the activated
	 *  element. One exception are Menubars, which display the name of their Menus. */
	String name;

public:
	/// Create a named event source.
	template <typename STR> ActionEventSource(const STR name) : name(String(name)) { }

	/// Dummy destructor.
	virtual ~ActionEventSource() {}

	/// Add a button press event handler.
	void addActionHandler(ActionEventSource_Callback *handler) { actionHandlers.push_back(handler); }

	/// Remove a button press event handler.
	void removeActionHandler(ActionEventSource_Callback *handler) { actionHandlers.remove(handler); }

	/// Set the name of this event source.
	template <typename STR> void setName(const STR name) { this->name = String(name); }

	/// Get the name of this event source.
	const String &getName() const { return name; }

	/// Execute handlers.
	void executeAction(const String &arg) {
		std::list<ActionEventSource_Callback*>::iterator i = actionHandlers.begin();
		bool end = (i == actionHandlers.end());
		while (!end) {
			ActionEventSource_Callback *c = *i;
			++i;
			end = (i == actionHandlers.end());
			c->actionExecuted(this,arg);
		}
	}

	/// Execute handlers.
	void executeAction() { executeAction(name); }
};

/// Internal class for windows whose child content should not span the entire area.
class BorderedWindow : public Window {
protected:
	/// Borders.
	int border_left, border_top, border_right, border_bottom;

	/// Create a bordered window.
	BorderedWindow(Window *parent, int x, int y, int w, int h, int bl, int bt, int br, int bb) :
		Window(parent,x,y,w,h), border_left(bl), border_top(bt), border_right(br), border_bottom(bb) {}

public:
	virtual void paintAll(Drawable &d) const;
	virtual bool mouseMoved(int x, int y);
	virtual bool mouseDown(int x, int y, MouseButton button);
	virtual bool mouseDragged(int x, int y, MouseButton button);
	virtual bool mouseUp(int x, int y, MouseButton button);
	virtual int getScreenX() const { return Window::getScreenX()+border_left; }
	virtual int getScreenY() const { return Window::getScreenY()+border_top; }
};

/// A text label
/** Text labels are positioned relative to the top left corner of their bounding box.
 *  They size themselves automatically and display their text in non-interpreted mode.
 */

class Label : public Window {
	/// The Font used
	const Font *font;

	/// Text color
	RGB color;

	/// Text
	String text;

	/// multiline text?
	bool interpret;

public:

    bool allow_focus = false;

	/// Create a text label with given position, \p text, \p font and \p color.
	/** If \p width is given, the resulting label is a word-wrapped multiline label */
	template <typename STR> Label(Window *parent, int x, int y, const STR text, int width = 0, const Font *font = Font::getFont("default"), RGB color = Color::Text) :
		Window(parent, x, y, (width?width:1), 1), font(font), color(color), text(text), interpret(width != 0)
	{ Label::resize(); tabbable = false; }

	/// Set a new text. Size of the label is adjusted accordingly.
	template <typename STR> void setText(const STR text) { this->text = text; resize(); }
	/// Retrieve current text
	const String& getText() { return text; }

	/// Set a new font. Size of the label is adjusted accordingly.
	void setFont(const Font *font) { this->font = font; resize(); }
	/// Retrieve current font
	const Font *getFont() { return font; }

	/// Set a new text color.
	void setColor(const RGB color) { this->color = color; resize(); }
	/// Retrieve current color
	RGB getColor() { return color; }

	/// Calculate label size. Parameters are ignored.
	virtual void resize(int w = -1, int h = -1) {
        (void)h;//UNUSED
		if (w == -1) w = (interpret?getWidth():0);
		else interpret = (w != 0);
		Drawable d((w?w:1), 1);
		d.setFont(font);
		d.drawText(0, font->getAscent(), text, interpret, 0);
		if (interpret) Window::resize(w, d.getY()-font->getAscent()+font->getHeight());
		else Window::resize(d.getX(), font->getHeight());
	}

	/// Returns \c true if this window has currently the keyboard focus.
	virtual bool hasFocus() const { return allow_focus && Window::hasFocus(); }

	/// Paint label
	virtual void paint(Drawable &d) const { d.setColor(color); d.drawText(0, font->getAscent(), text, interpret, 0); if (hasFocus()) d.drawDotRect(0,0,width-1,height-1); }

	virtual bool raise() { return false; }
};


/// A single-line text input
/** It uses Font::getFont("input") to display content.
 *  It supports selection, the clipboard and all well-known key bindings (except undo).
 */
class Input : public Window, public Timer_Callback, public ActionEventSource {
protected:
	/// The text entered.
	String text;

	/// Current position in \a text.
	Size pos;

	/// Last updated position in \a text.
	Size lastpos;

	/// Coordinates according to pos.
	int posx, posy;

	/// Selection in \a text.
	Size start_sel, end_sel;

	/// Is cursor visible at the moment?
	bool blink;

	/// Insert mode?
	bool insert;

	/// Multiline?
	bool multi;

	/// Horizontal scrolling offset.
	int offset;

    /// Allow user to type Tab into multiline
    bool enable_tab_input;

	/// Ensure that pos is visible.
	void checkOffset() {
		if (lastpos == pos) return;
		const Font *f = Font::getFont("input");
		if (multi) {
			Drawable d(width-6,1);
			d.setFont(f);
			d.drawText(0, 0, text, multi, 0, pos);
			posy = d.getY();
			posx = d.getX();
			if (posy-offset > height-8-f->getHeight()) offset = posy-height+8+f->getHeight();
			if (posy-offset < 0) offset = posy;
		} else {
			posy = 0;
			posx = f->getWidth(text,0,pos);
			if (f->getWidth(text,0,pos+1)-offset > width-10) offset = f->getWidth(text,0,pos+1)-width+10;
			if (f->getWidth(text,0,(pos>0?pos-1:0))-offset < 0) offset = f->getWidth(text,0,(pos>0?pos-1:0));
		}
		lastpos = pos;
		setDirty();
	}

public:
	/// Create an input with given position and width. If not set, height is calculated from the font and input is single-line.
	Input(Window *parent, int x, int y, int w, int h = 0) :
		Window(parent,x,y,w,(h?h:Font::getFont("input")->getHeight()+10)), ActionEventSource("GUI::Input"),
		text(""), pos(0), lastpos(0), posx(0), posy(0), start_sel(0), end_sel(0), blink(true), insert(true), multi(h != 0), offset(0), enable_tab_input(false)
	{ Timer::add(this,30); }

	~Input() {
		Timer::remove(this);
	}

	/// Paint input.
	virtual void paint(Drawable &d) const;

	/// Clear selected area.
	void clearSelection() {
		text.erase(text.begin()+int(pos = imin(start_sel,end_sel)),text.begin()+int(imax(start_sel,end_sel)));
		start_sel = end_sel = pos;
	}

	/// Copy selection to clipboard.
	void copySelection() {
		setClipboard(String(text.begin()+int(imin(start_sel,end_sel)),text.begin()+int(imax(start_sel,end_sel))));
	}

	/// Cut selection to clipboard.
	void cutSelection() {
		setClipboard(String(text.begin()+int(imin(start_sel,end_sel)),text.begin()+int(imax(start_sel,end_sel))));
		clearSelection();
	}

	/// Paste from clipboard.
	void pasteSelection() {
		String c = getClipboard();
		clearSelection();
		text.insert(text.begin()+int(pos),c.begin(),c.end());
		start_sel = end_sel = pos += (Size)c.size();
	}

	/// get character position corresponding to coordinates
	Size findPos(int x, int y);

	/// Set text to be edited.
	template <typename STR> void setText(const STR text) { this->text = text; setDirty(); }
	/// Get the entered text. If you need it longer, copy it immediately.
	const String& getText() { return text; };

	/// Handle text input.
	virtual bool keyDown(const Key &key);

	/// Handle mouse input.
	virtual bool mouseDown(int x, int y, MouseButton button);

	/// Handle mouse input.
	virtual bool mouseDragged(int x, int y, MouseButton button);

	/// Timer callback function
	virtual Ticks timerExpired(Ticks time)
	{ (void)time; blink = !blink; setDirty(); return 30; }

	/// Move the cursor to the end of the text field
	virtual void posToEnd(void);
};

class ToplevelWindow;
/// Callbacks for window events.
struct ToplevelWindow_Callback {
public:
		/// The window has been asked to be closed.
		/** Return \c false in order to block the requested action. Do not do any
		*  deallocation here, as other callbacks may abort the close process.
		*/
		virtual bool windowClosing(ToplevelWindow *win) = 0;

		/// The window will been closed.
		/** Now it is safe to deallocate all external resources that applications
		*  may have associated with this window, like registering this window
		*  with external callbacks.
		*/
		virtual void windowClosed(ToplevelWindow *win) = 0;
		virtual ~ToplevelWindow_Callback() {}
};

/// An actual decorated window.
class ToplevelWindow : public BorderedWindow, public ActionEventSource_Callback {
protected:
	/// Title text
	String title;

	/// Drag base
	int dragx, dragy;

	/// List of registered event handlers.
	std::list<ToplevelWindow_Callback *> closehandlers;

	/// System menu (top left)
	Menu *systemMenu;

public:
	/// Create a new GUI Frame with title bar, border and close button
	template <typename STR> ToplevelWindow(Screen *parent, int x, int y, int w, int h, const STR title);

	/// Call cleanup handlers
	~ToplevelWindow() {
		std::list<ToplevelWindow_Callback*>::iterator i = closehandlers.begin();
		bool end = (i == closehandlers.end());
		while (!end) {
			ToplevelWindow_Callback *c = *i;
			++i;
			end = (i == closehandlers.end());
			c->windowClosed(this);
		}
	}

	/// Menu callback function
	virtual void actionExecuted(ActionEventSource *src, const String &item) {
        (void)src;
		if (item == String("Close")) close();
	}

	/// Add a window event handler.
	void addCloseHandler(ToplevelWindow_Callback *handler) { closehandlers.push_back(handler); }

	/// Remove a window event handler.
	void removeCloseHandler(ToplevelWindow_Callback *handler) { closehandlers.remove(handler); }

	virtual void paint(Drawable &d) const;
	virtual bool mouseDown(int x, int y, MouseButton button);
	virtual bool mouseDoubleClicked(int x, int y, MouseButton button);
	virtual bool mouseUp(int x, int y, MouseButton button) {
		if (button == Left && dragx >= 0 && dragy >= 0) {
			dragx = dragy = -1;
			return true;
		}
		BorderedWindow::mouseUp(x,y,button);
		return true;
	}
	virtual bool mouseDragged(int x, int y, MouseButton button) {
		if (button == Left && dragx >= 0 && dragy >= 0) {
			move(x-dragx+this->x,y-dragy+this->y);
			return true;
		}
		BorderedWindow::mouseDragged(x,y,button);
		return true;
	}
	virtual bool mouseMoved(int x, int y) {
		BorderedWindow::mouseMoved(x,y);
		return true;
	}

	/// Put window on top of all other windows without changing their relative order
	virtual bool raise() {
		Window *last = parent->children.back();
		parent->children.remove(this);
		parent->children.push_back(this);
		if (last != this) {
			focusChanged(true);
			last->focusChanged(false);
		}
		return true;
	}

	/// Set a new title.
	template <typename STR> void setTitle(const STR title) { this->title = title; setDirty(); }
	/// Retrieve current title.
	const String& getTitle() { return title; }

	/// Close window.
	void close() {
		bool doit = true;
		std::list<ToplevelWindow_Callback*>::iterator i = closehandlers.begin();
		bool end = (i == closehandlers.end());
		while (!end) {
			ToplevelWindow_Callback *c = *i;
			++i;
			end = (i == closehandlers.end());
			doit = doit && c->windowClosing(this);
		}
		if (doit) delete this;
	}
};

/// A floating temporary window that is not restricted by it's parent's area.
/** They have a parent which displays them,
 *  but that parent is not their real parent in the window hierarchy: They are always top-level elements, thus
 *  they are not clipped to their logical parent's area, they can freely overlay any part of the screen.
 *  As another consequence, they are not automatically deleted when their logical parent is deleted.
 *
 *  You should observe the following points:
 *    \li TransientWindows behave mostly as if their logical parent was their true parent, but are not
 *	restricted to it's area
 *    \li TransientWindows are deleted automatically when the ToplevelWindow is deleted in which their logical
 *	parent resides; do NOT delete them in your destructor or bad things will happen
 *    \li only the logical parent object can show/hide a TransientWindow at will
 *    \li it will close automatically upon clicking other GUI elements, \em except for the
 *	logical parent and it's children
 */
class TransientWindow : public Window, Window_Callback, ToplevelWindow_Callback {
protected:
	/// The true parent window.
	Window *realparent;

	/// User selected position relative to logical parent.
	int relx, rely;

public:
	/// Handle automatic hiding
	virtual void focusChanged(bool gained) {
		Window::focusChanged(gained);
		if (isVisible() && !gained) {
			if (realparent->hasFocus()) raise();
			else setVisible(false);
		}
	}

	/// Handle automatic delete
	void windowClosed(ToplevelWindow *win) {
        (void)win;
		delete this;
	}

	/// No-op
	bool windowClosing(ToplevelWindow *win) { (void)win; return true; }

	/// Create a transient window with given position and size
	/** \a parent is the logical parent. The true parent object is
	 *  always the screen the logical parent resides on. */
	TransientWindow(Window *parent, int x, int y, int w, int h) :
		Window(parent->getScreen(),x+parent->getScreenX(),y+parent->getScreenY(),w,h),
		realparent(parent), relx(x), rely(y) {
		Window *p = realparent, *last = NULL, *last2 = NULL;
		while (p != NULL) {
			p->addWindowHandler(this);
			last2 = last;
			last = p;
			p = p->getParent();
		}
        transient = true;
		dynamic_cast<ToplevelWindow *>(last2)->addCloseHandler(this);
	}

	~TransientWindow() {
		Window *p = realparent, *last = NULL, *last2 = NULL;
		while (p != NULL) {
			p->removeWindowHandler(this);
			last2 = last;
			last = p;
			p = p->getParent();
		}
		dynamic_cast<ToplevelWindow *>(last2)->removeCloseHandler(this);
	 }

	virtual void move(int x, int y) { relx = x; rely = y;
		Window::move(x+realparent->getScreenX(),y+realparent->getScreenY()); }
	virtual int getX() const { return x-realparent->getScreenX(); }
	virtual int getY() const { return y-realparent->getScreenY(); }
	virtual void setVisible(bool v) { if (v) raise(); Window::setVisible(v); }
	virtual void windowMoved(Window *src, int x, int y) { (void)src; (void)x; (void)y; move(relx,rely); }
    virtual bool mouseDownOutside(MouseButton button) {
        (void)button;

        if (visible) {
            setVisible(false);
            return true;
        }

        return false;
    }

	/// Put window on top of all other windows without changing their relative order
	virtual bool raise() {
		Window *last = parent->children.back();
		parent->children.remove(this);
		parent->children.push_back(this);
		if (last != this) {
			focusChanged(true);
			last->focusChanged(false);
		}
		return true;
	}

};

/// A popup menu.
/** Popup menus are used as context menus or as part of a menu bar. Menus are not visible when created.
 *  Menus have a name which can be used in event handlers to identify it, and it is used in Menubars as well.
 *
 *  Currently, menu entries are not interpreted in any way. This may change in the future. To ensure upwards
 *  compatibility, avoid the \c '&' character in menu entries, since it may have a special meaning in future
 *  versions. Menus use the Font named "menu".
 *
 *  Please note the general remarks in GUI::TransientWindow
 */
class Menu : public TransientWindow, public ActionEventSource {
protected:
	/// List of menu items (displayed text)
	std::vector<String> items;

    /// column support
    unsigned int rows = 0;
    unsigned int columns = 0;
    std::vector<unsigned int> colx;

	/// Currently selected menu item.
	/** Can be -1 if no item is currently active. */
	int selected;

	/// Flag to skip the first mouseUp-event
	bool firstMouseUp;

	/// Where we grabbed the mouse from.
	Window *mouseTakenFrom;

	/// Selects menu item at position (x,y).
	/** \a selected is set to -1 if there is no active item at that location. */
	virtual void selectItem(int x, int y) {
        unsigned int coli = 0;
        int xmin,xmax,ypos = 2;

		selected = -1;

        xmin = 0;
        xmax = width;
        if (coli < colx.size()) {
            xmin = colx[coli++];
            if (coli < colx.size())
                xmax = colx[coli];
        }

        // mouse input should select nothing if outside the bounds of this menu
        if (x < 3 || x >= (width-3) || y < 2 || y >= (height-2)) return;
        selected = 0;

		const int fheight = Font::getFont("menu")->getHeight()+2;
		std::vector<String>::iterator i;
		for (i = items.begin(); i != items.end(); ++i) {
            if ((*i).size() > 0) {
                if (*i == "|") {
                    ypos = 2;
                    xmin = xmax = width;
                    if (coli < colx.size()) {
                        xmin = colx[coli++];
                        if (coli < colx.size())
                            xmax = colx[coli];
                    }
                }
                else {
                    if (x >= xmin && x < xmax) {
                        if (y >= ypos && y < (ypos+fheight))
                            break;
                    }

                    ypos += fheight;
                }
            }
            else {
                if (x >= xmin && x < xmax) {
                    if (y >= ypos && y < (ypos+12))
                        break;
                }

                ypos += 12;
            }

            selected++;
		}

		if (selected >= 0 && items[(unsigned int)selected].size() == 0) selected = -1;
	}

	virtual Size getPreferredWidth() {
		Size width = 0,px = 0;
		const Font *f = Font::getFont("menu");
		std::vector<String>::iterator i;
        columns = 1;
        colx.clear();
        colx.push_back(3+width);
        for (i = items.begin(); i != items.end() && y > 0; ++i) {
            if (*i == "|") {
                colx.push_back(3+width);
                columns++;
                px = width;
            }
            else {
                Size newwidth = (unsigned int)f->getWidth(*i) + px + 33;
                if (newwidth > width) width = newwidth;
            }
		}
		return width+6;
	}

	virtual Size getPreferredHeight() {
		Size height = 0,py = 0,row = 0;
		const Size h = (unsigned int)Font::getFont("menu")->getHeight()+2u;
		std::vector<String>::iterator i;
        rows = 0;
		for (i = items.begin(); i != items.end() && y > 0; ++i) {
            if (*i == "|") {
                py = 0;
                row = 0;
            }
            else {
                row++;
                if (rows < row) rows = row;
                py += ((*i).size() > 0?h:12);
                if (height < py) height = py;
            }
		}
		return height+6;
	}

public:
	/// Create a menu with given position
	/** Size is determined dynamically. \a parent is the logical parent. The true parent object is
	 *  always the screen the logical parent resides on. */
	template <typename STR> Menu(Window *parent, int x, int y, const STR name) :
		TransientWindow(parent,x,y,4,4), ActionEventSource(name), selected(-1)
		{ Menu::setVisible(false); tabbable = false; }

	~Menu() {
		Menu::setVisible(false);
	}

	/// Paint button.
	virtual void paint(Drawable &d) const;

	/// Highlight current item.
	virtual bool mouseMoved(int x, int y)  {
        if (visible) {
            firstMouseUp = false;
    		selectItem(x,y);
	    	return true;
        }

        return false;
	}

    void mouseMovedOutside(void) {
        if (visible && selected >= 0) {
            firstMouseUp = false;
            selected = -1;
            setDirty();
        }
    }

	/// Highlight current item.
	virtual bool mouseDragged(int x, int y, MouseButton button)  {
        (void)button;//UNUSED	

        if (visible) {
            if (x >= 0 && x < width && y >= 0 && y < height)
                firstMouseUp = false;

            selectItem(x,y);
            return true;
        }

        return false;
	}

	virtual bool mouseDown(int x, int y, MouseButton button)  {
        (void)button;//UNUSED
        (void)x;//UNUSED
        (void)y;//UNUSED

        if (visible)
            return true;

        return false;
    }

	virtual bool mouseDownOutside(MouseButton button) {
        (void)button;//UNUSED

        if (visible) {
            setVisible(false);
            selected = -1;
            return true;
        }

        return false;
    }

	/// Possibly select item.
	virtual bool mouseUp(int x, int y, MouseButton button)  {
        (void)button;//UNUSED

        if (visible) {
            selectItem(x,y);
            if (firstMouseUp) firstMouseUp = false;
            else setVisible(false);
            execute();
            return true;
        }

        return false;
    }

	/// Handle keyboard input.
	virtual bool keyDown(const Key &key) {
        if (visible) {
            if (key.special == Key::Up) {
                if (selected == 0)
                    selected = (int)items.size() - 1;
                else
                    selected--;
            }
            else if (key.special == Key::Down) {
                if ((size_t)(++selected) == items.size())
                    selected = 0;
            }
            else if (key.special == Key::Enter) { execute(); return true; }
            else if (key.special == Key::Escape) { setVisible(false); selected = -1; return true; }
            else return true;
            if (items[(unsigned int)selected].size() == 0 && items.size() > 1) return keyDown(key);
            if (selected < 0) selected = (int)(items.size()-1);
            if (selected >= (int)items.size()) selected = 0;
            return true;
        }

        return false;
	}


	/// Add a menu item at end. An empty string denotes a separator.
	template <typename T> void addItem(const T item) {
		items.push_back(String(item));
		resize((int)getPreferredWidth(),(int)getPreferredHeight());
	}

	/// Remove an existing menu item.
	template <typename T> void removeItem(const T item) {
		const String s(item);
		std::vector<String>::iterator i = items.begin();
		while (i != items.end() && s != (*i)) ++i;
		if (i != items.end()) items.erase(i);
		resize(getPreferredWidth(),getPreferredHeight());
	}

	virtual void setVisible(bool v) {
        if (!visible && v)
            firstMouseUp = true;

        TransientWindow::setVisible(v);
		if (v) {
			parent->mouseChild = this;
			raise();
		}

        // NTS: Do not set selected = -1 here on hide, other code in this C++
        //      class relies on calling setVisible() then acting on selection.
        //      Unless of course you want to hunt down random and sporadic
        //      segfaults. --J.C.
	}

	/// Execute menu item.
	void execute() {
		if (selected >= 0) {
			setVisible(false);

            // FIXME: Some action callbacks including the "Close" command in
            //        the system menu will delete this window object before
            //        returning to this level in the call stack. Therefore,
            //        copy selection index and clear it BEFORE calling the
            //        callbacks.
            unsigned int sel = (unsigned int)selected;
            selected = -1;

			executeAction(items[sel]);

            // WARNING: Do not access C++ class methods or variables here,
            //          the "Close" callback may have deleted this window
            //          out from under us! It may happen to work but
            //          understand it becomes a use-after-free bug!
		}
	}
};

/// A push button
/** Buttons have 3D appearance and can have any child widget as content.
 *  There are convenience constructors for common forms of buttons.
 */
class Button : public BorderedWindow, public ActionEventSource {
protected:
	/// \c true, if button is currently pressed down.
	bool pressed;

public:
	/// Create a button with given position and size
	Button(Window *parent, int x, int y, int w, int h) : BorderedWindow(parent,x,y,w,h,6,5,6,5), ActionEventSource("GUI::Button"), pressed(0) {}

	/// Create a text button.
	/** If a size is specified, text is centered. Otherwise, button size is adjusted for the text. */
	template <typename T> Button(Window *parent, int x, int y, const T text, int w = -1, int h = -1);

	/// Paint button.
	virtual void paint(Drawable &d) const;

	/// Press button.
	virtual bool mouseDown(int x, int y, MouseButton button) {
        (void)button;//UNUSED
        (void)x;//UNUSED
        (void)y;//UNUSED
	
		if (button == Left) {
			border_left = 7; border_right = 5; border_top = 7; border_bottom = 3;
			pressed = true;
		}
		return true;
	}

	/// Release button.
	virtual bool mouseUp(int x, int y, MouseButton button)  {
        (void)button;//UNUSED
        (void)x;//UNUSED
        (void)y;//UNUSED
	
		if (button == Left) {
			border_left = 6; border_right = 6; border_top = 5; border_bottom = 5;
			pressed = false;
		}
		return true;
	}

	/// Handle mouse activation.
	virtual bool mouseClicked(int x, int y, MouseButton button) {
        (void)button;//UNUSED
        (void)x;//UNUSED
        (void)y;//UNUSED
	
		if (button == Left) {
			executeAction();
			return true;
		}
		return false;
	}

	/// Handle keyboard input.
	virtual bool keyDown(const Key &key);

	/// Handle keyboard input.
	virtual bool keyUp(const Key &key);

};

/// A menu bar at the top of a ToplevelWindow
/** Menu bars aggregate several Menus. They have a simple border at the bottom. If your application
 *  has a work area with 3D sunken look, place it so that it overlaps the menu by one row.
 *
 *  As with Menus, avoid the character \c '&' in Menu names as it may
 *  have a special meaning in future versions. Menubars use the Font named "menu".
 */
class Menubar : public Window, public ActionEventSource, ActionEventSource_Callback {
protected:
	/// Currently activated menu index.
	int selected;

	/// Horizontal position of next menu.
	int lastx;

	/// List of Menus.
	std::vector<Menu*> menus;

public:
	/// Create a menubar with given position and size
	/** Height is autocalculated from font size */
	Menubar(Window *parent, int x, int y, int w) : Window(parent,x,y,w,Font::getFont("menu")->getHeight()+5), ActionEventSource("GUI::Menubar"), selected(-1), lastx(0) { tabbable = false; }

	/// Add a Menu.
	template <typename STR> void addMenu(const STR name) {
		const String n(name);
		menus.push_back(new Menu(this,lastx,height-2,n));
		menus.back()->addActionHandler(this);
		lastx += Font::getFont("menu")->getWidth(n)+14;
	}

	/// Add a Menuitem.
	template <typename STR> void addItem(int index, const STR name) { menus[(unsigned int)index]->addItem(name); }

	/// Remove a Menuitem.
	template <typename STR> void removeItem(int index, const STR name) { menus[(unsigned int)index]->removeItem(name); }

	/// Paint menubar.
	virtual void paint(Drawable &d) const;

	/// Open menu.
	virtual bool mouseDown(int x, int y, MouseButton button) {
        (void)button;//UNUSED
        (void)y;//UNUSED
		int oldselected = selected;
		if (selected >= 0 && !menus[(unsigned int)selected]->isVisible()) oldselected = -1;
		if (selected >= 0) menus[(unsigned int)selected]->setVisible(false);
		if (x < 0 || x >= lastx) return true;
		for (selected = (int)(menus.size()-1); menus[(unsigned int)selected]->getX() > x; selected--) {};
		if (oldselected == selected) selected = -1;
		else menus[(unsigned int)selected]->setVisible(true);
		return true;
	}

	/// Handle keyboard input.
	virtual bool keyDown(const Key &key) {
        if (key.special == Key::Tab)
            return false;

        return true;
    }

    /// Handle keyboard input.
    virtual bool keyUp(const Key &key) {
        if (key.special == Key::Tab)
            return false;

        return true;
    }

	virtual void actionExecuted(ActionEventSource *src, const String &arg) {
		std::list<ActionEventSource_Callback*>::iterator i = actionHandlers.begin();
		bool end = (i == actionHandlers.end());
		while (!end) {
			ActionEventSource_Callback *c = *i;
			++i;
			end = (i == actionHandlers.end());
			c->actionExecuted(src,arg);
		}
	}
};

/// A checkbox
/** Checkboxes can have any child widget as content.
 *  There are convenience constructors for common forms of checkboxes.
 */
class Checkbox : public BorderedWindow, public ActionEventSource {
protected:
	/// \c true, if checkbox is currently selected.
	bool checked;

public:
	/// Create a checkbox with given position and size
	Checkbox(Window *parent, int x, int y, int w, int h) : BorderedWindow(parent,x,y,w,h,16,0,0,0), ActionEventSource("GUI::Checkbox"), checked(0) {}

	/// Create a checkbox with text label.
	/** If a size is specified, text is centered. Otherwise, checkbox size is adjusted for the text. */
	template <typename T> Checkbox(Window *parent, int x, int y, const T text, int w = -1, int h = -1);

	/// Paint checkbox.
	virtual void paint(Drawable &d) const;

	/// Change checkbox state.
	virtual void setChecked(bool checked) { this->checked = checked; }

	/// Get checkbox state.
	virtual bool isChecked() { return checked; }

	/// Press checkbox.
	virtual bool mouseDown(int x, int y, MouseButton button) {
        (void)button;//UNUSED
        (void)x;//UNUSED
        (void)y;//UNUSED	
		checked = !checked;
		return true;
	}

	/// Release checkbox.
	virtual bool mouseUp(int x, int y, MouseButton button)  {
        (void)button;//UNUSED
        (void)x;//UNUSED
        (void)y;//UNUSED	
		execute();
		return true;
	}

	/// Handle keyboard input.
	virtual bool keyDown(const Key &key);

	/// Handle keyboard input.
	virtual bool keyUp(const Key &key);

	/// Execute handlers.
	virtual void execute() {
		String arg(name);
		if (!checked) arg.insert(arg.begin(),'!');
		executeAction(arg);
	}
};

class Frame;

/// A radio box.
/** Radio boxes can have any child widget as content.
 *  There are convenience constructors for common forms of radio boxes.
 */
class Radiobox : public BorderedWindow, public ActionEventSource {
protected:
	/// \c true, if radio box is currently selected.
	bool checked;

public:
	/// Create a radio box with given position and size
	Radiobox(Frame *parent, int x, int y, int w, int h);

	/// Create a radio box with text label.
	/** If a size is specified, text is centered. Otherwise, checkbox size is adjusted for the text. */
	template <typename T> Radiobox(Frame *parent, int x, int y, const T text, int w = -1, int h = -1);

	/// Paint radio box.
	virtual void paint(Drawable &d) const;

	/// Change radio box state.
	virtual void setChecked(bool checked) { this->checked = checked; }

	/// Get radio box state.
	virtual bool isChecked() { return checked; }

	/// Press radio box.
	virtual bool mouseDown(int x, int y, MouseButton button) {
        (void)button;//UNUSED
        (void)x;//UNUSED
        (void)y;//UNUSED
        checked = true;
		return true;
	}

	/// Release checkbox.
	virtual bool mouseUp(int x, int y, MouseButton button)  {
        (void)button;//UNUSED
        (void)x;//UNUSED
        (void)y;//UNUSED
		executeAction();
		return true;
	}

	/// Handle keyboard input.
	virtual bool keyDown(const Key &key);

	/// Handle keyboard input.
	virtual bool keyUp(const Key &key);
};

/// A rectangular 3D sunken frame
/** These can be used as generic grouping elements and also serve as aggregators for RadioBoxes.
 */
class Frame : public BorderedWindow, public ActionEventSource, protected ActionEventSource_Callback {
protected:
	friend class Radiobox;

	/// Currently selected radio box.
	int selected;

	/// Label of frame.
	String label;

	/// Execute handlers.
	virtual void actionExecuted(ActionEventSource *src, const String &arg) {
        // HACK: Attempting to cast a String to void causes "forming reference to void" errors when building with GCC 4.7
        (void)arg.size();//UNUSED
		for (std::list<Window *>::iterator i = children.begin(); i != children.end(); ++i) {
			Radiobox *r = dynamic_cast<Radiobox*>(*i);
			if (r != NULL && src != dynamic_cast<ActionEventSource*>(r)) r->setChecked(false);
		}
		executeAction(src->getName());
	}

public:
	/// Create a non-labeled frame with given position and size
	Frame(Window *parent, int x, int y, int w, int h) : BorderedWindow(parent,x,y,w,h,5,5,5,5), ActionEventSource("GUI::Frame"), selected(0) {}

	/// Create a frame with text label.
	template <typename T> Frame(Window *parent, int x, int y, int w, int h, const T text) :
		BorderedWindow(parent,x,y,w,h,5,Font::getFont("default")->getHeight()+2,5,5),
		ActionEventSource(text), selected(0), label(text) { }

	/// Paint frame.
	virtual void paint(Drawable &d) const;

};

/// A message box with a single "Close" button.
class MessageBox2 : public GUI::ToplevelWindow {
protected:
	Label *message;
	Button *close;
    WindowInWindow *wiw;
public:
	/// Create a new message box
	template <typename STR> MessageBox2(Screen *parent, int x, int y, int width, const STR title, const STR text) :
		ToplevelWindow(parent, x, y, width, 1, title) {
        wiw = new WindowInWindow(this, 5, 5, width-border_left-border_right-10, 70);
		message = new Label(wiw, 0, 0, text, width-border_left-border_right-10);
		close = new GUI::Button(this, (width-border_left-border_right-70)/2, 10, "Close", 70);
		close->addActionHandler(this);
		setText(text);

		close->raise(); /* make sure keyboard focus is on the close button */
		this->raise(); /* make sure THIS WINDOW has the keyboard focus */
	}

	/// Set a new text. Size of the box is adjusted accordingly.
	template <typename STR> void setText(const STR text) {
        int sfh;
        int msgw;
        bool scroll = true;

        msgw = width-border_left-border_right-10;
        message->resize(msgw, message->getHeight());
		message->setText(text);

        {
            Screen *s = getScreen();
            sfh = s->getHeight() - 70 - border_top - border_bottom;
            if (sfh > (15+message->getHeight())) {
                sfh = (15+message->getHeight());
                scroll = false;
            }
        }

        wiw->enableBorder(scroll);
        wiw->enableScrollBars(false/*h*/,scroll/*v*/);
        if (scroll) {
            msgw -= wiw->vscroll_display_width;
            msgw -= 2/*border*/;
            message->resize(msgw, message->getHeight());
        }

		close->move((width-border_left-border_right-70)/2, sfh);
        wiw->resize(width-border_left-border_right-10, sfh-10);
		resize(width, sfh+close->getHeight()+border_bottom+border_top+5);
	}

	virtual bool keyDown(const GUI::Key &key) {
        if (GUI::ToplevelWindow::keyDown(key)) return true;
        return false;
    }

	virtual bool keyUp(const GUI::Key &key) {
        if (GUI::ToplevelWindow::keyUp(key)) return true;

        if (key.special == GUI::Key::Escape) {
            close->executeAction();
            return true;
        }

        return false;
    }
};

extern int titlebar_y_start;
extern int titlebar_y_stop;

extern int titlebox_y_start;
extern int titlebox_y_height;

template <typename STR> ToplevelWindow::ToplevelWindow(Screen *parent, int x, int y, int w, int h, const STR title) :
	BorderedWindow(parent, x, y, w, h, 6, titlebar_y_stop, 6, 3), title(title),
	dragx(-1), dragy(-1), closehandlers(), systemMenu(new Menu(this,-1,-2,"System Menu")) {
/* If these commands don't do anything, then why have them there?? --J.C. */
#if 0 /* TODO: Allow selective enabling these if the Window object wants us to */
	systemMenu->addItem("Move");
	systemMenu->addItem("Resize");
	systemMenu->addItem("");
	systemMenu->addItem("Minimize");
	systemMenu->addItem("Maximize");
	systemMenu->addItem("Restore");
	systemMenu->addItem("");
#endif
	systemMenu->addItem("Close");
	systemMenu->addActionHandler(this);
    toplevel = true;
}

template <typename STR> Button::Button(Window *parent, int x, int y, const STR text, int w, int h) :
	BorderedWindow(parent,x,y,w,h,6,5,6,5), ActionEventSource(text), pressed(0)
{

	Label *l = new Label(this,0,0,text);
    l->allow_focus = true;
	if (width < 0) resize(l->getWidth()+border_left+border_right+10,height);
	if (height < 0) resize(width,l->getHeight()+border_top+border_bottom+6);
	l->move((width-border_left-border_right-l->getWidth())/2,
		(height-border_top-border_bottom-l->getHeight())/2);
}

template <typename STR> Checkbox::Checkbox(Window *parent, int x, int y, const STR text, int w, int h) :
	BorderedWindow(parent,x,y,w,h,16,0,0,0), ActionEventSource(text), checked(0)
{
	Label *l = new Label(this,0,0,text);
	if (width < 0) resize(l->getWidth()+border_left+border_right+4,height);
	if (height < 0) resize(width,l->getHeight()+border_top+border_bottom+4);
	l->move((width-border_left-border_right-l->getWidth())/2,
		(height-border_top-border_bottom-l->getHeight())/2);
}

template <typename STR> Radiobox::Radiobox(Frame *parent, int x, int y, const STR text, int w, int h) :
	BorderedWindow(parent,x,y,w,h,16,0,0,0), ActionEventSource(text), checked(0)
{
	Label *l = new Label(this,0,0,text);
	if (width < 0) resize(l->getWidth()+border_left+border_right+4,height);
	if (height < 0) resize(width,l->getHeight()+border_top+border_bottom+4);
	l->move((width-border_left-border_right-l->getWidth())/2,
		(height-border_top-border_bottom-l->getHeight())/2);
	addActionHandler(parent);
}

}

#endif
