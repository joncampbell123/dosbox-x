/*
 *  Copyright (C) 2002-2013  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include "SDL.h"
#include "menu.h"
#include "../libs/gui_tk/gui_tk.h"

#include "dosbox.h"
#include "keyboard.h"
#include "video.h"
#include "render.h"
#include "mapper.h"
#include "setup.h"
#include "control.h"
#include "shell.h"
#include "cpu.h"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <assert.h>

/* helper class for command execution */
class VirtualBatch : public BatchFile {
public:
					VirtualBatch(DOS_Shell *host, const std::string& cmds);
	bool				ReadLine(char *line);
protected:
	std::istringstream		lines;
};

extern Bit8u			int10_font_14[256 * 14];
extern Program*			first_shell;

extern bool			MSG_Write(const char *);
extern void			LoadMessageFile(const char * fname);
extern void			GFX_SetTitle(Bit32s cycles,Bits frameskip,Bits timing,bool paused);

static int			cursor;
static bool			running;
static int			saved_bpp;
static bool			shell_idle;
static int			old_unicode;
static bool			mousetoggle;
static bool			shortcut=false;
static SDL_Surface*		screenshot;
static SDL_Surface*		background;

/* Prepare screen for UI */
void UI_Init(void) {
	GUI::Font::addFont("default",new GUI::BitmapFont(int10_font_14,14,10));
}

static void getPixel(Bits x, Bits y, int &r, int &g, int &b, int shift)
{
	if (x >= (Bits)render.src.width) x = (Bits)render.src.width-1;
	if (y >= (Bits)render.src.height) x = (Bits)render.src.height-1;
	if (x < 0) x = 0;
	if (y < 0) y = 0;

	Bit8u* src = (Bit8u *)&scalerSourceCache;
	Bit32u pixel;
	switch (render.scale.inMode) {
	case scalerMode8:
		pixel = *(x+(Bit8u*)(src+y*render.scale.cachePitch));
		r += render.pal.rgb[pixel].red >> shift;
		g += render.pal.rgb[pixel].green >> shift;
		b += render.pal.rgb[pixel].blue >> shift;
		break;
	case scalerMode15:
		pixel = *(x+(Bit16u*)(src+y*render.scale.cachePitch));
		r += (pixel >> (7+shift)) & (0xf8 >> shift);
		g += (pixel >> (2+shift)) & (0xf8 >> shift);
		b += (pixel << (3-shift)) & (0xf8 >> shift);
		break;
	case scalerMode16:
		pixel = *(x+(Bit16u*)(src+y*render.scale.cachePitch));
		r += (pixel >> (8+shift)) & (0xf8 >> shift);
		g += (pixel >> (3+shift)) & (0xfc >> shift);
		b += (pixel << (3-shift)) & (0xf8 >> shift);
		break;
	case scalerMode32:
		pixel = *(x+(Bit32u*)(src+y*render.scale.cachePitch));
		r += (pixel >> (16+shift)) & (0xff >> shift);
		g += (pixel >> (8+shift))  & (0xff >> shift);
		b += (pixel >> shift)      & (0xff >> shift);
		break;
	}
}

extern bool dos_kernel_disabled;

static GUI::ScreenSDL *UI_Startup(GUI::ScreenSDL *screen) {
	GFX_EndUpdate(0);
	GFX_SetTitle(-1,-1,-1,true);
	if(!screen) { //Coming from DOSBox. Clean up the keyboard buffer.
		KEYBOARD_ClrBuffer();//Clear buffer
	}
	GFX_LosingFocus();//Release any keys pressed (buffer gets filled again). (could be in above if, but clearing the mapper input when exiting the mapper is sensible as well
	SDL_Delay(20);

	LoadMessageFile(static_cast<Section_prop*>(control->GetSection("dosbox"))->Get_string("language"));

	// Comparable to the code of intro.com, but not the same! (the code of intro.com is called from within a com file)
	shell_idle = !dos_kernel_disabled && first_shell && (DOS_PSP(dos.psp()).GetSegment() == DOS_PSP(dos.psp()).GetParent());

	int w, h;
	bool fs;
	GFX_GetSize(w, h, fs);
	if (w <= 400) {
		w *=2; h *=2;
	}

	old_unicode = SDL_EnableUNICODE(1);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,SDL_DEFAULT_REPEAT_INTERVAL);
	screenshot = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32, GUI::Color::RedMask, GUI::Color::GreenMask, GUI::Color::BlueMask, 0);

	// create screenshot for fade effect
	int rs = screenshot->format->Rshift, gs = screenshot->format->Gshift, bs = screenshot->format->Bshift, am = GUI::Color::AlphaMask;
	for (int y = 0; y < h; y++) {
		Bit32u *bg = (Bit32u*)(y*screenshot->pitch + (char*)screenshot->pixels);
		for (int x = 0; x < w; x++) {
			int r = 0, g = 0, b = 0;
			getPixel(x    *(int)render.src.width/w, y    *(int)render.src.height/h, r, g, b, 0);
			bg[x] = r << rs | g << gs | b << bs;
		}
	}

	background = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32, GUI::Color::RedMask, GUI::Color::GreenMask, GUI::Color::BlueMask, GUI::Color::AlphaMask);
	// use a blurred and sepia-toned screenshot as menu background
	for (int y = 0; y < h; y++) {
		Bit32u *bg = (Bit32u*)(y*background->pitch + (char*)background->pixels);
		for (int x = 0; x < w; x++) {
			int r = 0, g = 0, b = 0;
#ifdef WIN32
			getPixel(x    *(int)render.src.width/w, y    *(int)render.src.height/h, r, g, b, 0);
			bg[x] = r << rs | g << gs | b << bs | am;
#else
			getPixel(x    *(int)render.src.width/w, y    *(int)render.src.height/h, r, g, b, 3); 
			getPixel((x-1)*(int)render.src.width/w, y    *(int)render.src.height/h, r, g, b, 3); 
			getPixel(x    *(int)render.src.width/w, (y-1)*(int)render.src.height/h, r, g, b, 3); 
			getPixel((x-1)*(int)render.src.width/w, (y-1)*(int)render.src.height/h, r, g, b, 3); 
			getPixel((x+1)*(int)render.src.width/w, y    *(int)render.src.height/h, r, g, b, 3); 
			getPixel(x    *(int)render.src.width/w, (y+1)*(int)render.src.height/h, r, g, b, 3); 
			getPixel((x+1)*(int)render.src.width/w, (y+1)*(int)render.src.height/h, r, g, b, 3); 
			getPixel((x-1)*(int)render.src.width/w, (y+1)*(int)render.src.height/h, r, g, b, 3); 
			int r1 = (int)((r * 393 + g * 769 + b * 189) / 1351); // 1351 -- tweak colors 
			int g1 = (int)((r * 349 + g * 686 + b * 168) / 1503); // 1203 -- for a nice 
			int b1 = (int)((r * 272 + g * 534 + b * 131) / 2340); // 2140 -- golden hue 
			bg[x] = r1 << rs | g1 << gs | b1 << bs | am; 
#endif
		}
	}

	cursor = SDL_ShowCursor(SDL_QUERY);
	SDL_ShowCursor(SDL_ENABLE);

	mousetoggle = mouselocked;
	if (mouselocked) GFX_CaptureMouse();

	SDL_Surface* sdlscreen = SDL_SetVideoMode(w, h, 32, SDL_SWSURFACE|(fs?SDL_FULLSCREEN:SDL_RESIZABLE));
	if (sdlscreen == NULL) E_Exit("Could not initialize video mode %ix%ix32 for UI: %s", w, h, SDL_GetError());

	// fade out
	// Jonathan C: do it FASTER!
	SDL_Event event; 
	for (int i = 0xff; i > 0; i -= 0x30) { 
		SDL_SetAlpha(screenshot, SDL_SRCALPHA, i); 
		SDL_BlitSurface(background, NULL, sdlscreen, NULL); 
		SDL_BlitSurface(screenshot, NULL, sdlscreen, NULL); 
		SDL_UpdateRect(sdlscreen, 0, 0, 0, 0); 
		while (SDL_PollEvent(&event)); 
		SDL_Delay(40); 
	} 
 
	SDL_BlitSurface(background, NULL, sdlscreen, NULL);
	SDL_UpdateRect(sdlscreen, 0, 0, 0, 0);

	if (screen) screen->setSurface(sdlscreen);
	else screen = new GUI::ScreenSDL(sdlscreen);

	saved_bpp = render.src.bpp;
	render.src.bpp = 0;
	running = true;
	return screen;
}

/* Restore screen */
static void UI_Shutdown(GUI::ScreenSDL *screen) {
	SDL_Surface *sdlscreen = screen->getSurface();
	render.src.bpp = saved_bpp;

	// fade in
	// Jonathan C: do it FASTER!
	SDL_Event event;
	for (int i = 0x00; i < 0xff; i += 0x30) {
		SDL_SetAlpha(screenshot, SDL_SRCALPHA, i);
		SDL_BlitSurface(background, NULL, sdlscreen, NULL);
		SDL_BlitSurface(screenshot, NULL, sdlscreen, NULL);
		SDL_UpdateRect(sdlscreen, 0, 0, 0, 0);
		while (SDL_PollEvent(&event)) {};
		SDL_Delay(40); 
	}

	// clean up
	if (mousetoggle) GFX_CaptureMouse();
	SDL_ShowCursor(cursor);
	SDL_FreeSurface(background);
	SDL_FreeSurface(screenshot);
	SDL_FreeSurface(sdlscreen);
	screen->setSurface(NULL);
#ifdef WIN32
	void res_init(void);
	void change_output(int output);
	res_init();
	change_output(7);
#else
#if 1
	GFX_RestoreMode();
#else
	GFX_ResetScreen();
#endif
#endif
	SDL_EnableUNICODE(old_unicode);
	SDL_EnableKeyRepeat(0,0);
	GFX_SetTitle(-1,-1,-1,false);

	void GFX_ForceRedrawScreen(void);
	GFX_ForceRedrawScreen();
}

static void UI_RunCommands(GUI::ScreenSDL *s, const std::string &cmds) {
	DOS_Shell temp;
	temp.call = true;
	UI_Shutdown(s);
	Bit16u n=1; Bit8u c='\n';
	DOS_WriteFile(STDOUT,&c,&n);
	temp.bf = new VirtualBatch(&temp, cmds);
	temp.RunInternal();
	temp.ShowPrompt();
	UI_Startup(s);
}

VirtualBatch::VirtualBatch(DOS_Shell *host, const std::string& cmds) : BatchFile(host, "CON", "", ""), lines(cmds) {
}

bool VirtualBatch::ReadLine(char *line) {
	std::string l;

	if (!std::getline(lines,l)) {
		delete this;
		return false;
	}

	strcpy(line,l.c_str());
	return true;
}

/* stringification and conversion from the c++ FAQ */
class BadConversion : public std::runtime_error {
public: BadConversion(const std::string& s) : std::runtime_error(s) { }
};

template<typename T> inline std::string stringify(const T& x, std::ios_base& ( *pf )(std::ios_base&) = NULL) {
	std::ostringstream o;
	if (pf) o << pf;
	if (!(o << x)) throw BadConversion(std::string("stringify(") + typeid(x).name() + ")");
	return o.str();
}

template<typename T> inline void convert(const std::string& s, T& x, bool failIfLeftoverChars = true, std::ios_base& ( *pf )(std::ios_base&) = NULL) {
	std::istringstream i(s);
	if (pf) i >> pf;
	char c;
	if (!(i >> x) || (failIfLeftoverChars && i.get(c))) throw BadConversion(s);
}

/*****************************************************************************************************************************************/
/* UI classes */

class PropertyEditor : public GUI::Window, public GUI::ActionEventSource_Callback {
protected:
	Section_prop * section;
	Property *prop;
public:
	PropertyEditor(Window *parent, int x, int y, Section_prop *section, Property *prop) :
		Window(parent, x, y, 240, 30), section(section), prop(prop) { }

	virtual bool prepare(std::string &buffer) = 0;

	void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) {
		std::string line;
		if (prepare(line)) {
			if (first_shell) section->ExecuteDestroy(false);
			prop->SetValue(GUI::String(line));
			if (first_shell) section->ExecuteInit(false);
		}
	}
};

class PropertyEditorBool : public PropertyEditor {
	GUI::Checkbox *input;
public:
	PropertyEditorBool(Window *parent, int x, int y, Section_prop *section, Property *prop) :
		PropertyEditor(parent, x, y, section, prop) {
		input = new GUI::Checkbox(this, 0, 3, prop->propname.c_str());
		input->setChecked(static_cast<bool>(prop->GetValue()));
	}

	bool prepare(std::string &buffer) {
		if (input->isChecked() == static_cast<bool>(prop->GetValue())) return false;
		buffer.append(input->isChecked()?"true":"false");
		return true;
	}
};

class PropertyEditorString : public PropertyEditor {
protected:
	GUI::Input *input;
public:
	PropertyEditorString(Window *parent, int x, int y, Section_prop *section, Property *prop) :
		PropertyEditor(parent, x, y, section, prop) {
		new GUI::Label(this, 0, 5, prop->propname);
		input = new GUI::Input(this, 130, 0, 110);
		std::string temps = prop->GetValue().ToString();
		input->setText(stringify(temps));
	}

	bool prepare(std::string &buffer) {
		std::string temps = prop->GetValue().ToString();
		if (input->getText() == GUI::String(temps)) return false;
		buffer.append(static_cast<const std::string&>(input->getText()));
		return true;
	}
};

class PropertyEditorFloat : public PropertyEditor {
protected:
	GUI::Input *input;
public:
	PropertyEditorFloat(Window *parent, int x, int y, Section_prop *section, Property *prop) :
		PropertyEditor(parent, x, y, section, prop) {
		new GUI::Label(this, 0, 5, prop->propname);
		input = new GUI::Input(this, 130, 0, 50);
		input->setText(stringify((double)prop->GetValue()));
	}

	bool prepare(std::string &buffer) {
		double val;
		convert(input->getText(), val, false);
		if (val == (double)prop->GetValue()) return false;
		buffer.append(stringify(val));
		return true;
	}
};

class PropertyEditorHex : public PropertyEditor {
protected:
	GUI::Input *input;
public:
	PropertyEditorHex(Window *parent, int x, int y, Section_prop *section, Property *prop) :
		PropertyEditor(parent, x, y, section, prop) {
		new GUI::Label(this, 0, 5, prop->propname);
		input = new GUI::Input(this, 130, 0, 50);
		std::string temps = prop->GetValue().ToString();
		input->setText(temps.c_str());
	}

	bool prepare(std::string &buffer) {
		int val;
		convert(input->getText(), val, false, std::hex);
		if ((Hex)val ==  prop->GetValue()) return false;
		buffer.append(stringify(val, std::hex));
		return true;
	}
};

class PropertyEditorInt : public PropertyEditor {
protected:
	GUI::Input *input;
public:
	PropertyEditorInt(Window *parent, int x, int y, Section_prop *section, Property *prop) :
		PropertyEditor(parent, x, y, section, prop) {
		new GUI::Label(this, 0, 5, prop->propname);
		input = new GUI::Input(this, 130, 0, 50);
		//Maybe use ToString() of Value
		input->setText(stringify(static_cast<int>(prop->GetValue())));
	};

	bool prepare(std::string &buffer) {
		int val;
		convert(input->getText(), val, false);
		if (val == static_cast<int>(prop->GetValue())) return false;
		buffer.append(stringify(val));
		return true;
	};
};

class HelpWindow : public GUI::MessageBox2 {
public:
	HelpWindow(GUI::Screen *parent, int x, int y, Section *section) :
		MessageBox2(parent, x, y, 580, "", "") { // 740
		if (section == NULL) {
			fprintf(stderr,"BUG: HelpWindow constructor called with section == NULL\n");
			return;
		}

		std::string title(section->GetName());
		title.at(0) = std::toupper(title.at(0));
		setTitle("Help for "+title);

		Section_prop* sec = dynamic_cast<Section_prop*>(section);
		if (sec) {
			std::string msg;
			Property *p;
			int i = 0;
			while ((p = sec->Get_prop(i++))) {
				msg += std::string("\033[34m")+p->propname+":\033[0m "+p->Get_help()+"\n";
			}
			msg.replace(msg.end()-1,msg.end(),"");
			setText(msg);
		} else {
		std::string name = section->GetName();
		std::transform(name.begin(), name.end(), name.begin(), (int(*)(int))std::toupper);
		name += "_CONFIGFILE_HELP";
		setText(MSG_Get(name.c_str()));
		}
	};
};

class SectionEditor : public GUI::ToplevelWindow {
	Section_prop * section;
public:
	SectionEditor(GUI::Screen *parent, int x, int y, Section_prop *section) :
		ToplevelWindow(parent, x, y, 510, 300, ""), section(section) {
		if (section == NULL) {
			fprintf(stderr,"BUG: SectionEditor constructor called with section == NULL\n");
			return;
		}
		std::string title(section->GetName());
		title[0] = std::toupper(title[0]);
		setTitle("Configuration for "+title);
		new GUI::Label(this, 5, 10, "Settings:");
		GUI::Button *b = new GUI::Button(this, 120, 220, "Cancel", 70);
		b->addActionHandler(this);
		b = new GUI::Button(this, 200, 220, "Help", 70);
		b->addActionHandler(this);
		b = new GUI::Button(this, 280, 220, "OK", 70);

		int i = 0;
		Property *prop;
		while ((prop = section->Get_prop(i))) {
			Prop_bool   *pbool   = dynamic_cast<Prop_bool*>(prop);
			Prop_int    *pint    = dynamic_cast<Prop_int*>(prop);
			Prop_double  *pdouble  = dynamic_cast<Prop_double*>(prop);
			Prop_hex    *phex    = dynamic_cast<Prop_hex*>(prop);
			Prop_string *pstring = dynamic_cast<Prop_string*>(prop);
			Prop_multival* pmulti = dynamic_cast<Prop_multival*>(prop);
			Prop_multival_remain* pmulti_remain = dynamic_cast<Prop_multival_remain*>(prop);

			PropertyEditor *p;
			if (pbool) p = new PropertyEditorBool(this, 5+250*(i/6), 40+(i%6)*30, section, prop);
			else if (phex) p = new PropertyEditorHex(this, 5+250*(i/6), 40+(i%6)*30, section, prop);
			else if (pint) p = new PropertyEditorInt(this, 5+250*(i/6), 40+(i%6)*30, section, prop);
			else if (pdouble) p = new PropertyEditorFloat(this, 5+250*(i/6), 40+(i%6)*30, section, prop);
			else if (pstring) p = new PropertyEditorString(this, 5+250*(i/6), 40+(i%6)*30, section, prop);
			else if (pmulti) p = new PropertyEditorString(this, 5+250*(i/6), 40+(i%6)*30, section, prop);
			else if (pmulti_remain) p = new PropertyEditorString(this, 5+250*(i/6), 40+(i%6)*30, section, prop);
			else { i++; continue; }
			b->addActionHandler(p);
			i++;
		}
		b->addActionHandler(this);
	}
	void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) {
		if (arg == "OK" || arg == "Cancel" || arg == "Close") { close(); if(shortcut) running=false; }
		else if (arg == "Help") new HelpWindow(static_cast<GUI::Screen*>(parent), getX()-10, getY()-10, section);
		else ToplevelWindow::actionExecuted(b, arg);
	}
};

class AutoexecEditor : public GUI::ToplevelWindow {
	Section_line * section;
	GUI::Input *content;
public:
	AutoexecEditor(GUI::Screen *parent, int x, int y, Section_line *section) :
		ToplevelWindow(parent, x, y, 450, 300, ""), section(section) {
		if (section == NULL) {
			fprintf(stderr,"BUG: AutoexecEditor constructor called with section == NULL\n");
			return;
		}

		std::string title(section->GetName());
		title[0] = std::toupper(title[0]);
		setTitle("Edit "+title);
		new GUI::Label(this, 5, 10, "Content:");
		content = new GUI::Input(this, 5, 30, 420, 185);
		content->setText(section->data);
		if (first_shell) (new GUI::Button(this, 5, 220, "Append History"))->addActionHandler(this);
		if (shell_idle) (new GUI::Button(this, 180, 220, "Execute Now"))->addActionHandler(this);
		(new GUI::Button(this, 290, 220, "Cancel", 70))->addActionHandler(this);
		(new GUI::Button(this, 360, 220, "OK", 70))->addActionHandler(this);
	}

	void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) {
		if (arg == "OK") section->data = *(std::string*)content->getText();
		if (arg == "OK" || arg == "Cancel" || arg == "Close") { close(); if(shortcut) running=false; }
		else if (arg == "Append Shell Commands") {
			DOS_Shell *s = static_cast<DOS_Shell *>(first_shell);
			std::list<std::string>::reverse_iterator i = s->l_history.rbegin();
			std::string lines = *(std::string*)content->getText();
			while (i != s->l_history.rend()) {
				lines += "\n";
				lines += *i;
				++i;
			}
			content->setText(lines);
		} else if (arg == "Execute Now") {
			UI_RunCommands(dynamic_cast<GUI::ScreenSDL*>(getScreen()), content->getText());
		} else ToplevelWindow::actionExecuted(b, arg);
	}
};

class SaveDialog : public GUI::ToplevelWindow {
protected:
	GUI::Input *name;
public:
	SaveDialog(GUI::Screen *parent, int x, int y, const char *title) :
		ToplevelWindow(parent, x, y, 400, 150, title) {
		new GUI::Label(this, 5, 10, "Enter filename for configuration file:");
		name = new GUI::Input(this, 5, 30, 350);
		extern std::string capturedir;
		std::string fullpath,file;
		Cross::GetPlatformConfigName(file);
		const size_t last_slash_idx = capturedir.find_last_of("\\/");
		if (std::string::npos != last_slash_idx) {
			fullpath = capturedir.substr(0, last_slash_idx);
			fullpath += CROSS_FILESPLIT;
			fullpath += file;
		} else
			fullpath = "dosbox.conf";
		name->setText(fullpath.c_str());
		(new GUI::Button(this, 120, 70, "Cancel", 70))->addActionHandler(this);
		(new GUI::Button(this, 210, 70, "OK", 70))->addActionHandler(this);
	}

	void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) {
		if (arg == "OK") control->PrintConfig(name->getText());
		close();
		if(shortcut) running=false;
	}
};

class SaveLangDialog : public GUI::ToplevelWindow {
protected:
	GUI::Input *name;
public:
	SaveLangDialog(GUI::Screen *parent, int x, int y, const char *title) :
		ToplevelWindow(parent, x, y, 400, 150, title) {
		new GUI::Label(this, 5, 10, "Enter filename for language file:");
		name = new GUI::Input(this, 5, 30, 350);
		name->setText("messages.txt");
		(new GUI::Button(this, 120, 70, "Cancel", 70))->addActionHandler(this);
		(new GUI::Button(this, 210, 70, "OK", 70))->addActionHandler(this);
	}

	void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) {
		if (arg == "OK") MSG_Write(name->getText());
		close();
		if(shortcut) running=false;
	}
};

class SetCycles : public GUI::ToplevelWindow {
protected:
	GUI::Input *name;
public:
	SetCycles(GUI::Screen *parent, int x, int y, const char *title) :
		ToplevelWindow(parent, x, y, 400, 150, title) {
		new GUI::Label(this, 5, 10, "Enter CPU cycles:");
		name = new GUI::Input(this, 5, 30, 350);
	    std::ostringstream str;
		str << "fixed " << CPU_CycleMax;

		std::string cycles=str.str();
		name->setText(cycles.c_str());
		(new GUI::Button(this, 120, 70, "Cancel", 70))->addActionHandler(this);
		(new GUI::Button(this, 210, 70, "OK", 70))->addActionHandler(this);
	}

	void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) {
		if (arg == "OK") {
			Section* sec = control->GetSection("cpu");
			if (sec) {
				std::string tmp("cycles=");
				const char* well = name->getText();
				std::string s(well, 20);
				sec->ExecuteDestroy(false);
				tmp.append(s);
				sec->HandleInputline(tmp);
				sec->ExecuteInit(false);
				delete well;
			}
		}
		close();
		if(shortcut) running=false;
	}
};

class SetVsyncrate : public GUI::ToplevelWindow {
protected:
	GUI::Input *name;
public:
	SetVsyncrate(GUI::Screen *parent, int x, int y, const char *title) :
		ToplevelWindow(parent, x, y, 400, 150, title) {
		new GUI::Label(this, 5, 10, "Enter vertical syncrate (Hz):");
		name = new GUI::Input(this, 5, 30, 350);
		Section_prop * sec = static_cast<Section_prop *>(control->GetSection("vsync"));
		if (sec)
			name->setText(sec->Get_string("vsyncrate"));
		else
			name->setText("");
		(new GUI::Button(this, 120, 70, "Cancel", 70))->addActionHandler(this);
		(new GUI::Button(this, 210, 70, "OK", 70))->addActionHandler(this);
	}

	void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) {
		Section_prop * sec = static_cast<Section_prop *>(control->GetSection("vsync"));
		if (arg == "OK") {
			if (sec) {
				const char* well = name->getText();
				std::string s(well, 20);
				std::string tmp("vsyncrate=");
				sec->ExecuteDestroy(false);
				tmp.append(s);
				sec->HandleInputline(tmp);
				sec->ExecuteInit(false);
				delete well;
			}
		}
		LOG_MSG("GUI: Current Vertical Sync Rate: %s Hz", sec->Get_string("vsyncrate"));
		close();
		if(shortcut) running=false;
	}
};

class SetLocalSize : public GUI::ToplevelWindow {
protected:
	GUI::Input *name;
public:
	SetLocalSize(GUI::Screen *parent, int x, int y, const char *title) :
		ToplevelWindow(parent, x, y, 450, 150, title) {
			new GUI::Label(this, 5, 10, "Enter default local freesize (MB, min=0, max=1024):");
			name = new GUI::Input(this, 5, 30, 400);
			extern unsigned int hdd_defsize;
			int human_readable = 512 * 32 * hdd_defsize / 1024 / 1024;
			char buffer[6];
			sprintf(buffer, "%d", human_readable);
			name->setText(buffer);
			(new GUI::Button(this, 120, 70, "Cancel", 70))->addActionHandler(this);
			(new GUI::Button(this, 210, 70, "OK", 70))->addActionHandler(this);
	}

	void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) {
		if (arg == "OK") {
			char * text=name->getText();
			extern unsigned int hdd_defsize;
			int human_readable = atoi(name->getText());
			if (human_readable < 0)
				hdd_defsize = 0;
			else if (human_readable > 1024)
				hdd_defsize = 256000;
			else
				hdd_defsize = human_readable * 1024 * 1024 / 512 / 32;
			LOG_MSG("GUI: Current default freesize for local disk: %dMB", 512 * 32 * hdd_defsize / 1024 / 1024);
		}
		close();
		if (shortcut) running = false;
	}
};

class ConfigurationWindow : public GUI::ToplevelWindow {
public:
	ConfigurationWindow(GUI::Screen *parent, GUI::Size x, GUI::Size y, GUI::String title) :
		GUI::ToplevelWindow(parent, x, y, 470, 380, title) {

		(new GUI::Button(this, 185, 305, "Close", 80))->addActionHandler(this);

		GUI::Menubar *bar = new GUI::Menubar(this, 0, 0, getWidth());
		bar->addMenu("Configuration");
		bar->addItem(0,"Save...");
		bar->addItem(0,"Save Language File...");
		bar->addItem(0,"");
		bar->addItem(0,"Close");
		bar->addMenu("Settings");
		bar->addMenu("Help");
		bar->addItem(2,"Introduction");
		bar->addItem(2,"Getting Started");
		bar->addItem(2,"CD-ROM Support");
		bar->addItem(2,"Special Keys");
		bar->addItem(2,"");
		bar->addItem(2,"About");
		bar->addActionHandler(this);

		new GUI::Label(this, 10, 30, "Choose a settings group to configure:");

		Section *sec;
		int i = 0;
		while ((sec = control->GetSection(i))) {
			std::string name = sec->GetName();
			name[0] = std::toupper(name[0]);
			GUI::Button *b = new GUI::Button(this, 12+(i/7)*110, 50+(i%7)*35, name, 100);
			b->addActionHandler(this);
			bar->addItem(1, name);
			i++;
		}

		if (first_shell) {
			(new GUI::Button(this, 12+(i/7)*110, 50+(i%7)*35, "Keyboard", 100))->addActionHandler(this);
			bar->addItem(1, "");
			bar->addItem(1, "Keyboard");
		}
	}

	~ConfigurationWindow() { running = false; }

	void actionExecuted(GUI::ActionEventSource *b, const GUI::String &arg) {
		GUI::String sname = arg;
		sname.at(0) = std::tolower(sname.at(0));
		Section *sec;
		if (arg == "Close" || arg == "Cancel" || arg == "Close") {
			running = false;
		} else if (arg == "Keyboard") {
			UI_Shutdown(dynamic_cast<GUI::ScreenSDL*>(getScreen()));
			MAPPER_RunEvent(0);
			UI_Startup(dynamic_cast<GUI::ScreenSDL*>(getScreen()));
		} else if (sname == "autoexec") {
			Section_line *section = static_cast<Section_line *>(control->GetSection((const char *)sname));
			new AutoexecEditor(getScreen(), 50, 30, section);
		} else if ((sec = control->GetSection((const char *)sname))) {
			Section_prop *section = static_cast<Section_prop *>(sec);
			new SectionEditor(getScreen(), 50, 30, section);
		} else if (arg == "About") {
			new GUI::MessageBox2(getScreen(), 200, 150, 280, "About DOSBox", "\nDOSBox-X\nAn emulator for old DOS Games\n\nCopyright 2002-2013\nThe DOSBox Team");
		} else if (arg == "Introduction") {
			new GUI::MessageBox2(getScreen(), 20, 50, 600, "Introduction", MSG_Get("PROGRAM_INTRO"));
		} else if (arg == "Getting Started") {
			std::string msg = MSG_Get("PROGRAM_INTRO_MOUNT_START");
#ifdef WIN32
			msg += MSG_Get("PROGRAM_INTRO_MOUNT_WINDOWS");
#else
			msg += MSG_Get("PROGRAM_INTRO_MOUNT_OTHER");
#endif
			msg += MSG_Get("PROGRAM_INTRO_MOUNT_END");

			new GUI::MessageBox2(getScreen(), 20, 50, 600, std::string("Introduction"), msg);
		} else if (arg == "CD-ROM Support") {
			new GUI::MessageBox2(getScreen(), 20, 50, 600, "Introduction", MSG_Get("PROGRAM_INTRO_CDROM"));
		} else if (arg == "Special Keys") {
			new GUI::MessageBox2(getScreen(), 20, 50, 600, "Introduction", MSG_Get("PROGRAM_INTRO_SPECIAL"));
		} else if (arg == "Save...") {
			new SaveDialog(getScreen(), 90, 100, "Save Configuration...");
		} else if (arg == "Save Language File...") {
			new SaveLangDialog(getScreen(), 90, 100, "Save Language File...");
		} else {
			return ToplevelWindow::actionExecuted(b, arg);
		}
	}
};

/*********************************************************************************************************************/
/* UI control functions */

static void UI_Execute(GUI::ScreenSDL *screen) {
	SDL_Surface *sdlscreen;
	SDL_Event event;

	sdlscreen = screen->getSurface();
	new ConfigurationWindow(screen, 30, 30, "DOSBox Configuration");

	// event loop
	while (running) {
		while (SDL_PollEvent(&event)) {
			if (!screen->event(event)) {
				if (event.type == SDL_QUIT) running = false;
			}
		}

		//Selecting keyboard will create a new surface.
		sdlscreen = screen->getSurface();
		SDL_BlitSurface(background, NULL, sdlscreen, NULL);
		screen->update(4);
		SDL_UpdateRect(sdlscreen, 0, 0, 0, 0);

		SDL_Delay(40);
	}
}

#ifdef WIN32
static void UI_Select(GUI::ScreenSDL *screen, int select) {
	SDL_Surface *sdlscreen = NULL;
	Section_line *section2 = NULL;
	Section_prop *section = NULL;
	Section *sec = NULL;
	SDL_Event event;

	sdlscreen = screen->getSurface();
	switch (select) {
		case 0:
			new GUI::MessageBox2(screen, 200, 150, 280, "", "");
			running=false;
			break;
		case 1:
			new SaveDialog(screen, 90, 100, "Save Configuration...");
			break;
		case 2:
			sec = control->GetSection("sdl");
			section=static_cast<Section_prop *>(sec); 
			new SectionEditor(screen,50,30,section);
			break;
		case 3:
			sec = control->GetSection("dosbox");
			section=static_cast<Section_prop *>(sec); 
			new SectionEditor(screen,50,30,section);
			break;
		case 4:
			sec = control->GetSection("mixer");
			section=static_cast<Section_prop *>(sec); 
			new SectionEditor(screen,50,30,section);
			break;
		case 5:
			sec = control->GetSection("serial");
			section=static_cast<Section_prop *>(sec); 
			new SectionEditor(screen,50,30,section);
			break;
		case 6:
			sec = control->GetSection("ne2000");
			section=static_cast<Section_prop *>(sec); 
			new SectionEditor(screen,50,30,section);
			break;
		case 7:
			section2 = static_cast<Section_line *>(control->GetSection("autoexec"));
			new AutoexecEditor(screen, 50, 30, section2);
			break;
		case 8:
			sec = control->GetSection("glide");
			section=static_cast<Section_prop *>(sec); 
			new SectionEditor(screen,50,30,section);
			break;
		case 9:
			new SaveLangDialog(screen, 90, 100, "Save Language File...");
			break;
		case 10:
			new ConfigurationWindow(screen, 30, 30, "DOSBox Configuration");
			break;
		case 11:
			sec = control->GetSection("parallel");
			section=static_cast<Section_prop *>(sec);
			new SectionEditor(screen,50,30,section);
			break;
		case 12:
			sec = control->GetSection("printer");
			section=static_cast<Section_prop *>(sec);
			new SectionEditor(screen,50,30,section);
			break;
		case 13:
			sec = control->GetSection("cpu");
			section=static_cast<Section_prop *>(sec);
			new SectionEditor(screen,50,30,section);
			break;
		case 14:
			sec = control->GetSection("dos");
			section=static_cast<Section_prop *>(sec);
			new SectionEditor(screen,50,30,section);
			break;
		case 15:
			sec = control->GetSection("midi");
			section=static_cast<Section_prop *>(sec);
			new SectionEditor(screen,50,30,section);
			break;
		case 16:
			new SetCycles(screen, 90, 100, "Set CPU Cycles...");
			break;
		case 17:
			new SetVsyncrate(screen, 90, 100, "Set vertical syncrate...");
			break;
		case 18:
			new SetLocalSize(screen, 90, 100, "Set Default Local Freesize...");
			break;
		default:
			break;
	}

	// event loop
	while (running) {
		while (SDL_PollEvent(&event)) {
			if (!screen->event(event)) {
				if (event.type == SDL_QUIT) running = false;
			}
		}
		SDL_BlitSurface(background, NULL, sdlscreen, NULL);
		screen->update(4);
		SDL_UpdateRect(sdlscreen, 0, 0, 0, 0);
		SDL_Delay(20);
	}
}

void UI_Shortcut(int select) {
	if(running) return;
	if(menu.maxwindow) ShowWindow(GetHWND(), SW_RESTORE);
	shortcut=true;
	GUI::ScreenSDL *screen = UI_Startup(NULL);
	UI_Select(screen,select);
	UI_Shutdown(screen);
	shortcut=false;
	delete screen;
}
#endif

void UI_Run(bool pressed) {
	if (pressed || running) return;
	GUI::ScreenSDL *screen = UI_Startup(NULL);
	UI_Execute(screen);
	UI_Shutdown(screen);
	delete screen;
}
