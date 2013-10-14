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
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

//#include <dosbox.h>
#include "config.h"

#if C_PRINTER

#if !defined __PRINTER_H
#define __PRINTER_H

#ifdef C_LIBPNG
#include <png.h>
#endif

#include "SDL.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#if defined (WIN32)
#include <windows.h>
#include <winspool.h>
#endif

#define Z_BEST_COMPRESSION 9
#define Z_DEFAULT_STRATEGY 0

#define STYLE_PROP 0x01
#define STYLE_CONDENSED 0x02
#define STYLE_BOLD 0x04
#define STYLE_DOUBLESTRIKE 0x08
#define STYLE_DOUBLEWIDTH 0x10
#define STYLE_ITALICS 0x20
#define STYLE_UNDERLINE 0x40
#define STYLE_SUPERSCRIPT 0x80
#define STYLE_SUBSCRIPT 0x100
#define STYLE_STRIKETHROUGH 0x200
#define STYLE_OVERSCORE 0x400
#define STYLE_DOUBLEWIDTHONELINE 0x800
#define STYLE_DOUBLEHEIGHT 0x1000

#define SCORE_NONE 0x00
#define SCORE_SINGLE 0x01
#define SCORE_DOUBLE 0x02
#define SCORE_SINGLEBROKEN 0x05
#define SCORE_DOUBLEBROKEN 0x06

#define QUALITY_DRAFT 0x01
#define QUALITY_LQ 0x02

#define COLOR_BLACK 7<<5

enum Typeface
{
	roman = 0,
	sansserif,
	courier,
	prestige,
	script,
	ocrb,
	ocra,
	orator,
	orators,
	scriptc,
	romant,
	sansserifh,
	svbusaba = 30,
	svjittra = 31
};

class CPrinter {
public:

	CPrinter (Bit16u dpi, Bit16u width, Bit16u height, char* output, bool multipageOutput);
	virtual ~CPrinter();

	// Process one character sent to virtual printer
	void printChar(Bit8u ch);

	// Hard Reset (like switching printer off and on)
	void resetPrinterHard();

	// Set Autofeed value 
	void setAutofeed(bool feed);

	// Get Autofeed value
	bool getAutofeed();

	// True if printer is unable to process more data right now (do not use printChar)
	bool isBusy();

	// True if the last sent character was received 
	bool ack();

	// Manual formfeed
	void formFeed();

	// Returns true if the current page is blank
	bool isBlank();

private:

	// used to fill the color "sub-pallettes"
	void FillPalette(Bit8u redmax, Bit8u greenmax, Bit8u bluemax, Bit8u colorID,
							SDL_Palette* pal);

    // Checks if given char belongs to a command and process it. If false, the character
	// should be printed
	bool processCommandChar(Bit8u ch);

	// Resets the printer to the factory settings
	void resetPrinter();

	// Reload font. Must be called after changing dpi, style or cpi
	void updateFont();

	// Clears page. If save is true, saves the current page to a bitmap
	void newPage(bool save, bool resetx);

	// Blits the given glyph on the page surface. If add is true, the values of bitmap are
	// added to the values of the pixels in the page
	void blitGlyph(FT_Bitmap bitmap, Bit16u destx, Bit16u desty, bool add);

	// Draws an anti-aliased line from (fromx, y) to (tox, y). If broken is true, gaps are included
	void drawLine(Bitu fromx, Bitu tox, Bitu y, bool broken);

	// Setup the bitGraph structure
	void setupBitImage(Bit8u dens, Bit16u numCols);

	// Process a character that is part of bit image. Must be called iff bitGraph.remBytes > 0.
	void printBitGraph(Bit8u ch);

	// Copies the codepage mapping from the constant array to CurMap
	void selectCodepage(Bit16u cp);

	// Output current page 
	void outputPage();

	// Prints out a byte using ASCII85 encoding (only outputs something every four bytes). When b>255, closes the ASCII85 string
	void fprintASCII85(FILE* f, Bit16u b);

	// Closes a multipage document
	void finishMultipage();

	// Returns value of the num-th pixel (couting left-right, top-down) in a safe way
	Bit8u getPixel(Bit32u num);

	FT_Library FTlib;					// FreeType2 library used to render the characters

	SDL_Surface* page;					// Surface representing the current page
	FT_Face curFont;					// The font currently used to render characters
	Bit8u color;

	Real64 curX, curY;					// Position of the print head (in inch)

	Bit16u dpi;							// dpi of the page
	Bit16u ESCCmd;						// ESC-command that is currently processed
	bool ESCSeen;						// True if last read character was an ESC (0x1B)
	bool FSSeen;						// True if last read character was an FS (0x1C) (IBM commands)

	Bit8u numParam, neededParam;		// Numbers of parameters already read/needed to process command

	Bit8u params[20];					// Buffer for the read params
	Bit16u style;						// Style of font (see STYLE_* constants)
	Real64 cpi, actcpi;					// CPI value set by program and the actual one (taking in account font types)
	Bit8u score;						// Score for lines (see SCORE_* constants)

	Real64 topMargin, bottomMargin, rightMargin, leftMargin;	// Margins of the page (in inch)
	Real64 pageWidth, pageHeight;								// Size of page (in inch)
	Real64 defaultPageWidth, defaultPageHeight;					// Default size of page (in inch)
	Real64 lineSpacing;											// Size of one line (in inch)

	Real64 horiztabs[32];				// Stores the set horizontal tabs (in inch)
	Bit8u numHorizTabs;					// Number of configured tabs

	Real64 verttabs[16];				// Stores the set vertical tabs (in inch)
	Bit8u numVertTabs;					// Number of configured tabs

	Bit8u curCharTable;					// Currently used char table und charset
	Bit8u printQuality;					// Print quality (see QUALITY_* constants)

	Typeface LQtypeFace;				// Typeface used in LQ printing mode

	Real64 extraIntraSpace;				// Extra space between two characters (set by program, in inch)

	bool charRead;						// True if a character was read since the printer was last initialized
	bool autoFeed;						// True if a LF should automatically added after a CR
	bool printUpperContr;				// True if the upper command characters should be printed

	struct bitGraphicParams				// Holds information about printing bit images
	{
		Bit16u horizDens, vertDens;		// Density of image to print (in dpi)
		bool adjacent;					// Print adjacent pixels? (ignored)
		Bit8u bytesColumn;				// Bytes per column
		Bit16u remBytes;				// Bytes left to read before image is done
		Bit8u column[6];				// Bytes of the current and last column
		Bit8u readBytesColumn;			// Bytes read so far for the current column
	} bitGraph;

	Bit8u densk, densl, densy, densz;	// Image density modes used in ESC K/L/Y/Z commands

	Bit16u curMap[256];					// Currently used ASCII => Unicode mapping
	Bit16u charTables[4];				// Charactertables

	Real64 definedUnit;					// Unit used by some ESC/P2 commands (negative => use default)

	bool multipoint;					// If multipoint mode is enabled
	Real64 multiPointSize;				// Point size of font in multipoint mode
	Real64 multicpi;					// CPI used in multipoint mode

	Real64 hmi;							// Horizontal motion index (in inch; overrides CPI settings)

	Bit8u msb;							// MSB mode
	Bit16u numPrintAsChar;				// Number of bytes to print as characters (even when normally control codes)

#if defined (WIN32)
	HDC printerDC;						// Win32 printer device
#endif

	char* output;						// Output method selected by user
	void* outputHandle;					// If not null, additional pages will be appended to the given handle
	bool multipageOutput;				// If true, all pages are combined to one file/print job etc. until the "eject page" button is pressed
	Bit16u multiPageCounter;			// Current page (when printing multipages)

	Bit8u ASCII85Buffer[4];				// Buffer used in ASCII85 encoding
	Bit8u ASCII85BufferPos;				// Position in ASCII85 encode buffer
	Bit8u ASCII85CurCol;				// Columns printed so far in the current lines
};

#endif

#endif
