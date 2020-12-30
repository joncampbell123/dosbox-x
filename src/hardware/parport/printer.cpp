/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "printer.h"

#if C_PRINTER

#include <math.h>
#include "setup.h"
#include "mapper.h"
#include "printer_if.h"
#include "support.h"
#include "cross.h"
#include "printer_charmaps.h"
#include "control.h"
#include "pic.h" // for timeout
#include "render.h"

#if defined(USE_TTF)
extern unsigned char DOSBoxTTFbi[48868];
#endif
extern void GFX_CaptureMouse(void);
extern bool mouselocked;

static CPrinter* defaultPrinter = NULL;

#define PARAM16(I) (params[I+1]*256+params[I])
#define PIXX ((Bitu)floor(curX*dpi+0.5))
#define PIXY ((Bitu)floor(curY*dpi+0.5))

static uint16_t confdpi, confwidth, confheight;
static Bitu printer_timout;
static bool timeout_dirty;
static const char* document_path;
static const char* font_path;
static char confoutputDevice[50];
static bool confmultipageOutput, shellhide;
static std::string actstd, acterr;

void CPrinter::FillPalette(uint8_t redmax, uint8_t greenmax, uint8_t bluemax, uint8_t colorID, SDL_Palette* pal)
{
	float red = (float)redmax / (float)30.9;
    float green = (float)greenmax / (float)30.9;
    float blue = (float)bluemax / (float)30.9;

	uint8_t colormask = colorID<<=5;

	for(int i = 0; i < 32; i++) {
		pal->colors[i+colormask].r = 255 - (uint8_t)floor(red * (float)i);
		pal->colors[i+colormask].g = 255 - (uint8_t)floor(green * (float)i);
		pal->colors[i+colormask].b = 255 - (uint8_t)floor(blue * (float)i);
	}
}

CPrinter::CPrinter(uint16_t dpi, uint16_t width, uint16_t height, char* output, bool multipageOutput) 
{
	if (FT_Init_FreeType(&FTlib))
	{
		LOG(LOG_MISC,LOG_ERROR)("PRINTER: Unable to init Freetype2. Printing disabled");
		page = NULL;
	}
	else
	{
		this->dpi = dpi;
		this->output = output;
		this->multipageOutput = multipageOutput;

		defaultPageWidth = (double)width / (double)10;
		defaultPageHeight = (double)height / (double)10;

		// Create page
		page = SDL_CreateRGBSurface(SDL_SWSURFACE, (int)(defaultPageWidth*dpi), (int)(defaultPageHeight*dpi), 8, 0, 0, 0, 0);

		// Set a grey palette
		SDL_Palette* palette = page->format->palette;
		
		for (Bitu i = 0; i < 32; i++)
		{
			palette->colors[i].r = 255;
			palette->colors[i].g = 255;
			palette->colors[i].b = 255;
		}
		// 0 = all white needed for logic 000
		FillPalette(  0,   0,   0, 1, palette);
		// 1 = magenta* 001
		FillPalette(  0, 255,   0, 1, palette);
		// 2 = cyan*    010
		FillPalette(255,   0,   0, 2, palette);
		// 3 = "violet" 011
		FillPalette(255, 255,   0, 3, palette);
		// 4 = yellow*  100
		FillPalette(  0,   0, 255, 4, palette);
		// 5 = red      101
		FillPalette(  0, 255, 255, 5, palette);
		// 6 = green    110
		FillPalette(255,   0, 255, 6, palette);
		// 7 = black    111
		FillPalette(255, 255, 255, 7, palette);

		// yyyxxxxx bit pattern: yyy=color xxxxx = intensity: 31=max
		// Printing colors on top of each other ORs them and gets the
		// correct resulting color.
		// i.e. magenta on blank page yyy=001
		// then yellow on magenta 001 | 100 = 101 = red
		
		color = COLOR_BLACK;
		
		curFont = NULL;
		charRead = false;
		autoFeed = false;
		outputHandle = NULL;

		resetPrinter();

		if (strcasecmp(output, "printer") == 0)
		{
#if defined (WIN32)
			// Show Print dialog to obtain a printer device context
			PRINTDLG pd;
			pd.lStructSize = sizeof(PRINTDLG); 
			pd.hDevMode = (HANDLE) NULL; 
			pd.hDevNames = (HANDLE) NULL; 
			pd.Flags = PD_RETURNDC; 
			pd.hwndOwner = NULL; 
			pd.hDC = (HDC) NULL; 
			pd.nFromPage = 1; 
			pd.nToPage = 1; 
			pd.nMinPage = 0; 
			pd.nMaxPage = 0; 
			pd.nCopies = 1; 
			pd.hInstance = NULL; 
			pd.lCustData = 0L; 
			pd.lpfnPrintHook = (LPPRINTHOOKPROC) NULL; 
			pd.lpfnSetupHook = (LPSETUPHOOKPROC) NULL; 
			pd.lpPrintTemplateName = (LPSTR) NULL; 
			pd.lpSetupTemplateName = (LPSTR)  NULL; 
			pd.hPrintTemplate = (HANDLE) NULL; 
			pd.hSetupTemplate = (HANDLE) NULL; 
			PrintDlg(&pd);
			// TODO: what if user presses cancel?
			printerDC = pd.hDC;
#endif
		}
		LOG(LOG_MISC,LOG_NORMAL)("PRINTER: Enabled");
	}
}

void CPrinter::resetPrinterHard()
{
	charRead = false;
	resetPrinter();
}

void CPrinter::resetPrinter()
{
		color = COLOR_BLACK;
		curX = curY = 0.0;
		ESCSeen = false;
		FSSeen = false;
		ESCCmd = 0;
		numParam = neededParam = 0;
		topMargin = leftMargin = 0.0;
		rightMargin = pageWidth = defaultPageWidth;
		bottomMargin = pageHeight = defaultPageHeight;
		lineSpacing = (double)1 / 6;
		cpi = 10.0;
		curCharTable = 1;
		style = 0;
		extraIntraSpace = 0.0;
		printUpperContr = true;
		bitGraph.remBytes = 0;
		densk = 0;
		densl = 1;
		densy = 2;
		densz = 3;
		charTables[0] = 0; // Italics
		charTables[1] = charTables[2] = charTables[3] = 437;
		definedUnit = -1;
		multipoint = false;
		multiPointSize = 0.0;
		multicpi = 0.0;
		hmi = -1.0;
		msb = 255;
		numPrintAsChar = 0;
		LQtypeFace = courier;

		selectCodepage(charTables[curCharTable]);

		updateFont();

		newPage(false,true);

		// Default tabs => Each eight characters
		for (Bitu i = 0; i < 32; i++)
			horiztabs[i] = i * 8 * (1 / (double)cpi);
		numHorizTabs = 32;
		numVertTabs = 255;
}


CPrinter::~CPrinter(void)
{
	finishMultipage();
	if (page != NULL)
	{
		SDL_FreeSurface(page);
		page = NULL;
		FT_Done_FreeType(FTlib);
	}
#if defined (WIN32)
	DeleteDC(printerDC);
#endif
}

void CPrinter::selectCodepage(uint16_t cp)
{
	const uint16_t* mapToUse = NULL;

	Bitu i = 0;
	while(charmap[i].codepage!=0)
    {
		if(charmap[i].codepage==cp)
        {
			mapToUse = charmap[i].map;
			break;
		}
		i++;
	}
	if (mapToUse == NULL)
    {
		LOG(LOG_MISC,LOG_WARN)("Unsupported codepage %i. Using CP437 instead.", cp);
		selectCodepage(437);
		return;
	}
	for (int i = 0; i < 256; i++)
		curMap[i] = mapToUse[i];
}

void CPrinter::updateFont()
{
	if (curFont != NULL)
		FT_Done_Face(curFont);

	std::string fontName, basedir;
#if defined(WIN32)
    basedir = font_path;
#else
    basedir = font_path;
#endif
    if (basedir.back()!='\\' && basedir.back()!='/')
        basedir += CROSS_FILESPLIT;

	switch (LQtypeFace)
	{
	    case roman:
		    fontName = basedir + "roman.ttf";
		    break;
	    case sansserif:
		    fontName = basedir + "sansserif.ttf";
		    break;
	    case courier:
		    fontName = basedir + "courier.ttf";
		    break;
	    case script:
		    fontName = basedir + "script.ttf";
		    break;
	    case ocra:
	    case ocrb:;
		    fontName = basedir + "ocra.ttf";
		    break;
	    default:
		    fontName = basedir + "roman.ttf";
	}
	
#ifndef WIN32
	std::string configfont;
	Cross::GetPlatformConfigDir(configfont);
	configfont += fontName;
	fontName = configfont;
#endif
	
	if (FT_New_Face(FTlib, fontName.c_str(), 0, &curFont))
	{
        std::string oldfont=fontName;
        struct stat wstat;
#if defined(WIN32)
        const char* windir = "C:\\WINDOWS";
        if(stat(windir,&wstat) || !(wstat.st_mode & S_IFDIR)) {
            TCHAR dir[MAX_PATH];
            if (GetWindowsDirectory(dir, MAX_PATH))
                windir=dir;
        }
        basedir = std::string(windir) + "\\fonts\\";
#else
        basedir = "/usr/share/fonts/";
#endif
        switch (LQtypeFace)
        {
            case roman:
#if defined(WIN32)
                fontName = basedir + "times.ttf";
#else
                fontName = basedir + "liberation-serif/LiberationSerif-Regular.ttf";
                if(stat(fontName.c_str(),&wstat)) fontName = basedir + "truetype/liberation/LiberationSerif-Regular.ttf";
#endif
                break;
            case sansserif:
#if defined(WIN32)
                fontName = basedir + "arial.ttf";
#else
                fontName = basedir + "liberation-sans/LiberationSans-Regular.ttf";
                if(stat(fontName.c_str(),&wstat)) fontName = basedir + "truetype/liberation/LiberationSans-Regular.ttf";
#endif
                break;
            case courier:
#if defined(WIN32)
                fontName = basedir + "cour.ttf";
#else
                fontName = basedir + "liberation-mono/LiberationMono-Regular.ttf";
                if(stat(fontName.c_str(),&wstat)) fontName = basedir + "truetype/liberation/LiberationMono-Regular.ttf";
#endif
                break;
            case script:
                fontName = basedir + "freescpt.ttf";
                break;
            case ocra:
            case ocrb:
                fontName = basedir + "ocraext.ttf";
                break;
            default:
#if defined(WIN32)
                fontName = basedir + "times.ttf";
#else
                fontName = basedir + "liberation-serif/LiberationSerif-Regular.ttf";
                if(stat(fontName.c_str(),&wstat)) fontName = basedir + "truetype/liberation/LiberationSerif-Regular.ttf";
#endif
        }
        if (FT_New_Face(FTlib, fontName.c_str(), 0, &curFont)
#if defined(USE_TTF)
        && FT_New_Memory_Face(FTlib, DOSBoxTTFbi, sizeof(DOSBoxTTFbi), 0, &curFont)
#endif
        ) {
            //LOG(LOG_MISC,LOG_ERROR)("Unable to load font %s", fontName);
            LOG_MSG("Unable to load font %s (or %s)", oldfont.c_str(), fontName.c_str());
            curFont = NULL;
        }
	}

	double horizPoints = 10.5;
	double vertPoints = 10.5;

	if (!multipoint)
    {
		actcpi = cpi;
/*
		switch(style & (STYLE_CONDENSED|STYLE_PROP)) {
			case STYLE_CONDENSED: // only condensed
				if (cpi == 10.0) {
					actcpi = 17.14;
					horizPoints *= 10.0/17.14;
				} else if(cpi == 12.0) {
					actcpi = 20.0;
					horizPoints *= 10.0/20.0;
					vertPoints *= 10.0/12.0;
				} else {
					// ignored
				}
				break;
			case STYLE_PROP|STYLE_CONDENSED:
				horizPoints /= 2.0;
				break;
			case 0: // neither
			case STYLE_PROP: // only proportional
				horizPoints *= 10.0/cpi;
				vertPoints *= 10.0/cpi;
				break;
		}
*/
		if (!(style & STYLE_CONDENSED))
        {
			horizPoints *= 10.0 / cpi;
			vertPoints *= 10.0 / cpi;
		}

		if (!(style & STYLE_PROP))
        {
			if ((cpi == 10.0) && (style & STYLE_CONDENSED))
            {
				actcpi = 17.14;
				horizPoints *= 10.0 / 17.14;
			}
			if ((cpi == 12.0) && (style & STYLE_CONDENSED))
            {
				actcpi = 20.0;
				horizPoints *= 10.0 / 20.0;
				vertPoints *= 10.0 / 12.0;
			}	
		} else if (style & STYLE_CONDENSED) horizPoints /= 2.0;


		if ((style & STYLE_DOUBLEWIDTH) || (style & STYLE_DOUBLEWIDTHONELINE))
        {
			actcpi /= 2.0;
			horizPoints *= 2.0;
		}

		if (style & STYLE_DOUBLEHEIGHT)	vertPoints *= 2.0;
	}
    else
    {
        // multipoint true
		actcpi = multicpi;
		horizPoints = vertPoints = multiPointSize;
	}

	if ((style & STYLE_SUPERSCRIPT) || (style & STYLE_SUBSCRIPT)) {
		horizPoints *= 2.0/3.0;
		vertPoints *= 2.0/3.0;
		actcpi /= 2.0/3.0;
	}

	FT_Set_Char_Size(curFont, (uint16_t)horizPoints*64, (uint16_t)vertPoints*64, dpi, dpi);
	
	if (style & STYLE_ITALICS || charTables[curCharTable] == 0)
	{
		FT_Matrix  matrix;
		matrix.xx = 0x10000L;
		matrix.xy = (FT_Fixed)(0.20 * 0x10000L);
		matrix.yx = 0;
		matrix.yy = 0x10000L;
		FT_Set_Transform(curFont, &matrix, 0);
	}
}

bool CPrinter::processCommandChar(uint8_t ch)
{
	if (ESCSeen || FSSeen)
	{
		ESCCmd = ch;
		if(FSSeen) ESCCmd |= 0x800;
		ESCSeen = FSSeen = false;
		numParam = 0;

		switch (ESCCmd) {
		    case 0x02: // Undocumented
		    case 0x0a: // Reverse line feed											(ESC LF)
		    case 0x0c: // Return to top of current page								(ESC FF)
		    case 0x0e: // Select double-width printing (one line)					(ESC SO)		
		    case 0x0f: // Select condensed printing									(ESC SI)
		    case 0x23: // Cancel MSB control										(ESC #)
		    case 0x30: // Select 1/8-inch line spacing								(ESC 0)
		    case 0x31: // Select 7/60-inch line spacing								(ESC 1)
		    case 0x32: // Select 1/6-inch line spacing								(ESC 2)
		    case 0x34: // Select italic font										(ESC 4)
		    case 0x35: // Cancel italic font										(ESC 5)
		    case 0x36: // Enable printing of upper control codes					(ESC 6)
		    case 0x37: // Enable upper control codes								(ESC 7)
		    case 0x38: // Disable paper-out detector								(ESC 8)
		    case 0x39: // Enable paper-out detector									(ESC 9)
		    case 0x3c: // Unidirectional mode (one line)							(ESC <)
		    case 0x3d: // Set MSB to 0												(ESC =)
		    case 0x3e: // Set MSB to 1												(ESC >)
		    case 0x40: // Initialize printer										(ESC @)
		    case 0x45: // Select bold font											(ESC E)
		    case 0x46: // Cancel bold font											(ESC F)
		    case 0x47: // Select double-strike printing								(ESC G)
		    case 0x48: // Cancel double-strike printing								(ESC H)
		    case 0x4d: // Select 10.5-point, 12-cpi									(ESC M)
		    case 0x4f: // Cancel bottom margin [conflict]							(ESC O)
		    case 0x50: // Select 10.5-point, 10-cpi									(ESC P)
		    case 0x54: // Cancel superscript/subscript printing						(ESC T)
		    case 0x5e: // Enable printing of all character codes on next character	(ESC ^)
		    case 0x67: // Select 10.5-point, 15-cpi									(ESC g)

		    case 0x834: // Select italic font								(FS 4)	(= ESC 4)
		    case 0x835: // Cancel italic font								(FS 5)	(= ESC 5)
		    case 0x846: // Select forward feed mode							(FS F)
		    case 0x852: // Select reverse feed mode							(FS R)
			    neededParam = 0;
			    break;
		    case 0x19: // Control paper loading/ejecting							(ESC EM)
		    case 0x20: // Set intercharacter space									(ESC SP)
		    case 0x21: // Master select												(ESC !)
		    case 0x2b: // Set n/360-inch line spacing								(ESC +)
		    case 0x2d: // Turn underline on/off										(ESC -)
		    case 0x2f: // Select vertical tab channel								(ESC /)
		    case 0x33: // Set n/180-inch line spacing								(ESC 3)
		    case 0x41: // Set n/60-inch line spacing								(ESC A)
		    case 0x43: // Set page length in lines									(ESC C)
		    case 0x49: // Select character type and print pitch						(ESC I)
		    case 0x4a: // Advance print position vertically							(ESC J)
		    case 0x4e: // Set bottom margin											(ESC N)
		    case 0x51: // Set right margin											(ESC Q)
		    case 0x52: // Select an international character set						(ESC R)
		    case 0x53: // Select superscript/subscript printing						(ESC S)
		    case 0x55: // Turn unidirectional mode on/off							(ESC U)
		    //case 0x56: // Repeat data												(ESC V)
		    case 0x57: // Turn double-width printing on/off							(ESC W)
		    case 0x61: // Select justification										(ESC a)
		    case 0x66: // Absolute horizontal tab in columns [conflict]				(ESC f)
		    case 0x68: // Select double or quadruple size							(ESC h)
		    case 0x69: // Immediate print											(ESC i)
		    case 0x6a: // Reverse paper feed										(ESC j)
		    case 0x6b: // Select typeface											(ESC k)
		    case 0x6c: // Set left margin											(ESC 1)
		    case 0x70: // Turn proportional mode on/off								(ESC p)
		    case 0x72: // Select printing color										(ESC r)
		    case 0x73: // Low-speed mode on/off										(ESC s)
		    case 0x74: // Select character table									(ESC t)
		    case 0x77: // Turn double-height printing on/off						(ESC w)
		    case 0x78: // Select LQ or draft										(ESC x)
		    case 0x7e: // Select/Deselect slash zero								(ESC ~)

		    case 0x832: // Select 1/6-inch line spacing						(FS 2)	(= ESC 2)
		    case 0x833: // Set n/360-inch line spacing						(FS 3)	(= ESC +)
		    case 0x841: // Set n/60-inch line spacing						(FS A)	(= ESC A)
		    case 0x843:	// Select LQ type style								(FS C)	(= ESC k)
		    case 0x845: // Select character width							(FS E)
		    case 0x849: // Select character table							(FS I)	(= ESC t)
		    case 0x853: // Select High Speed/High Density elite pitch		(FS S)
		    case 0x856: // Turn double-height printing on/off				(FS V)	(= ESC w)
			    neededParam = 1;
			    break;
		    case 0x24: // Set absolute horizontal print position					(ESC $)
		    case 0x3f: // Reassign bit-image mode									(ESC ?)
		    case 0x4b: // Select 60-dpi graphics									(ESC K)
		    case 0x4c: // Select 120-dpi graphics									(ESC L)
		    case 0x59: // Select 120-dpi, double-speed graphics						(ESC Y)
		    case 0x5a: // Select 240-dpi graphics									(ESC Z)
		    case 0x5c: // Set relative horizontal print position					(ESC \)
		    case 0x63: // Set horizontal motion index (HMI)	[conflict]				(ESC c)
		    case 0x65: // Set vertical tab stops every n lines						(ESC e)
		    case 0x85a: // Print 24-bit hex-density graphics						(FS Z)
			    neededParam = 2;
			    break;
		    case 0x2a: // Select bit image											(ESC *)
		    case 0x58: // Select font by pitch and point [conflict]					(ESC X)
			    neededParam = 3;
			    break;
		    case 0x5b: // Select character height, width, line spacing
			    neededParam = 7;
			    break;
		    case 0x62: // Set vertical tabs in VFU channels							(ESC b) 
		    case 0x42: // Set vertical tabs											(ESC B)
			    numVertTabs = 0;
			    return true;
		    case 0x44: // Set horizontal tabs										(ESC D)
			    numHorizTabs = 0;
			    return true;
		    case 0x25: // Select user-defined set									(ESC %)
		    case 0x26: // Define user-defined characters							(ESC &)
		    case 0x3a: // Copy ROM to RAM											(ESC :)
			    LOG(LOG_MISC,LOG_ERROR)("User-defined characters not supported!");
			    return true;
		    case 0x28: // Two bytes sequence
			    return true;
		    default:
			    LOG_MSG("PRINTER: Unknown command %s (%02Xh) %c , unable to skip parameters.",
				    (ESCCmd & 0x800)?"FS":"ESC",ESCCmd, ESCCmd);
			
			    neededParam = 0;
			    ESCCmd = 0;
			    return true;
		}

		if (neededParam > 0)
			return true;
	}

	// Two bytes sequence
	if (ESCCmd == '(')
	{
		ESCCmd = 0x200 + ch;

		switch (ESCCmd)
		{
		    case 0x242: // Bar code setup and print (ESC (B)
		    case 0x25e: // Print data as characters (ESC (^)
			    neededParam = 2;
			    break;
		    case 0x255: // Set unit (ESC (U)
			    neededParam = 3;
			    break;
		    case 0x243: // Set page length in defined unit (ESC (C)
		    case 0x256: // Set absolute vertical print position (ESC (V)
		    case 0x276: // Set relative vertical print position (ESC (v)
			    neededParam = 4;
			    break;
		    case 0x274: // Assign character table (ESC (t)
		    case 0x22d: // Select line/score (ESC (-)
			    neededParam = 5;
			    break;
		    case 0x263: // Set page format (ESC (c)
			    neededParam = 6;
			    break;

            default:
			    // ESC ( commands are always followed by a "number of parameters" word parameter
			    //LOG(LOG_MISC,LOG_ERROR)
				    LOG_MSG("PRINTER: Skipping unsupported command ESC ( %c (%02X).", ESCCmd, ESCCmd);
			    neededParam = 2;
			    ESCCmd = 0x101;
			    return true;
		}

		if (neededParam > 0)
			return true;
	}

	// Ignore VFU channel setting
	if (ESCCmd == 0x62)
    {
		ESCCmd = 0x42;
		return true;
	}

	// Collect vertical tabs
	if (ESCCmd == 0x42)
    {
		if (ch == 0 || (numVertTabs > 0 && verttabs[numVertTabs-1] > (double)ch * lineSpacing)) // Done
			ESCCmd = 0;
		else
			if (numVertTabs < 16)
				verttabs[numVertTabs++] = (double)ch * lineSpacing;
	}

	// Collect horizontal tabs
	if (ESCCmd == 0x44) 
	{
		if (ch == 0 || (numHorizTabs > 0 && horiztabs[numHorizTabs-1] > (double)ch * (1 / (double)cpi))) // Done
			ESCCmd = 0;
		else
			if (numHorizTabs < 32)
				horiztabs[numHorizTabs++] = (double)ch*(1 / (double)cpi);
	}

	if (numParam < neededParam)
	{
		params[numParam++] = ch;

		if (numParam < neededParam)
			return true;
	}

	if (ESCCmd != 0)
	{
		switch (ESCCmd)
		{
		    case 0x02: // Undocumented
			    // Ignore
			    break;
		    case 0x0e: // Select double-width printing (one line) (ESC SO)		
			    if (!multipoint)
			    {
				    hmi = -1;
				    style |= STYLE_DOUBLEWIDTHONELINE;
				    updateFont();
			    }
			    break;
		    case 0x0f: // Select condensed printing (ESC SI)
			    if (!multipoint && (cpi != 15.0))
                {
				    hmi = -1;
				    style |= STYLE_CONDENSED;
				    updateFont();
			    }
			    break;
		    case 0x19: // Control paper loading/ejecting (ESC EM)
			    // We are not really loading paper, so most commands can be ignored
			    if (params[0] == 'R')
				    newPage(true, false); // TODO resetx?
			    break;
		    case 0x20: // Set intercharacter space (ESC SP)
			    if (!multipoint)
			    {
				    extraIntraSpace = (double)params[0] / (printQuality == QUALITY_DRAFT ? 120 : 180);
				    hmi = -1;
				    updateFont();
			    }
			    break;
		    case 0x21: // Master select (ESC !)
			    cpi = params[0] & 0x01 ? 12:10;

			    // Reset first seven bits
			    style &= 0xFF80;
			    if (params[0] & 0x02)
				    style |= STYLE_PROP;
			    if (params[0] & 0x04)
 				    style |= STYLE_CONDENSED;
			    if (params[0] & 0x08)
 				    style |= STYLE_BOLD;
			    if (params[0] & 0x10)
 				    style |= STYLE_DOUBLESTRIKE;
			    if (params[0] & 0x20)
 				    style |= STYLE_DOUBLEWIDTH;
			    if (params[0] & 0x40)
 				    style |= STYLE_ITALICS;
			    if (params[0] & 0x80)
			    {
				    score = SCORE_SINGLE;
 				    style |= STYLE_UNDERLINE;
			    }

			    hmi = -1;
			    multipoint = false;
			    updateFont();
			    break;
		    case 0x23: // Cancel MSB control (ESC #)
			    msb = 255;
			    break;
		    case 0x24: // Set absolute horizontal print position (ESC $)
			    {
				    double unitSize = definedUnit;
				    if (unitSize < 0)
					    unitSize = (double)60.0;

				    double newX = leftMargin + ((double)PARAM16(0) / unitSize);
				    if (newX <= rightMargin)
					    curX = newX;
			    }
			    break;
		    case 0x85a: // Print 24-bit hex-density graphics (FS Z)
			    setupBitImage(40, PARAM16(0));
			    break;
		    case 0x2a: // Select bit image (ESC *)
			    setupBitImage(params[0], PARAM16(1));
			    break;
		    case 0x2b: // Set n/360-inch line spacing (ESC +)
		    case 0x833: // Set n/360-inch line spacing (FS 3)
			    lineSpacing = (double)params[0] / 360;
			    break;
		    case 0x2d: // Turn underline on/off (ESC -)
			    if (params[0] == 0 || params[0] == 48)
				    style &= ~STYLE_UNDERLINE;
			    if (params[0] == 1 || params[0] == 49)
			    {
				    style |= STYLE_UNDERLINE;
				    score = SCORE_SINGLE;
			    }
			    updateFont();
			    break;
		    case 0x2f: // Select vertical tab channel (ESC /)
			    // Ignore
			    break;
		    case 0x30: // Select 1/8-inch line spacing (ESC 0)
			    lineSpacing = (double)1 / 8;
			    break;
		    case 0x32: // Select 1/6-inch line spacing (ESC 2)
			    lineSpacing = (double)1 / 6;
			    break;
		    case 0x33: // Set n/180-inch line spacing (ESC 3)
			    lineSpacing = (double)params[0] / 180;
			    break;
		    case 0x34: // Select italic font (ESC 4)
			    style |= STYLE_ITALICS;
			    updateFont();
			    break;
		    case 0x35: // Cancel italic font (ESC 5)
			    style &= ~STYLE_ITALICS;
			    updateFont();
			    break;
		    case 0x36: // Enable printing of upper control codes (ESC 6)
			    printUpperContr = true;
			    break;
		    case 0x37: // Enable upper control codes (ESC 7)
			    printUpperContr = false;
			    break;
		    case 0x3c: // Unidirectional mode (one line) (ESC <)
			    // We don't have a print head, so just ignore this
			    break;
		    case 0x3d: // Set MSB to 0 (ESC =)
			    msb = 0;
			    break;
		    case 0x3e: // Set MSB to 1 (ESC >)
			    msb = 1;
			    break;
		    case 0x3f: // Reassign bit-image mode (ESC ?)
			    if (params[0] == 75)
				    densk = params[1];
			    if (params[0] == 76)
				    densl = params[1];
			    if (params[0] == 89)
				    densy = params[1];
			    if (params[0] == 90)
				    densz = params[1];
			    break;
		    case 0x40: // Initialize printer (ESC @)
			    resetPrinter();
			    break;
		    case 0x41: // Set n/60-inch line spacing
		    case 0x841:
			    lineSpacing = (double)params[0] / 60;
			    break;
		    case 0x43: // Set page length in lines (ESC C)
			    if (params[0] != 0)
				    pageHeight = bottomMargin = (double)params[0] * lineSpacing;
			    else // == 0 => Set page length in inches
			    {
				    neededParam = 1;
				    numParam = 0;
				    ESCCmd = 0x100;
				    return true;
			    }
			    break;
		    case 0x45: // Select bold font (ESC E)
			    style |= STYLE_BOLD;
			    updateFont();
			    break;
		    case 0x46: // Cancel bold font (ESC F)
			    style &= ~STYLE_BOLD;
			    updateFont();
			    break;
		    case 0x47: // Select dobule-strike printing (ESC G)
			    style |= STYLE_DOUBLESTRIKE;
			    break;
		    case 0x48: // Cancel double-strike printing (ESC H)
			    style &= ~STYLE_DOUBLESTRIKE;
			    break;
		    case 0x4a: // Advance print position vertically (ESC J n)
			    curY += (double)((double)params[0] / 180);
			    if (curY > bottomMargin)
				    newPage(true, false);
			    break;
		    case 0x4b: // Select 60-dpi graphics (ESC K)
			    setupBitImage(densk, PARAM16(0));
			    break;
		    case 0x4c: // Select 120-dpi graphics (ESC L)
			    setupBitImage(densl, PARAM16(0));
			    break;
		    case 0x4d: // Select 10.5-point, 12-cpi (ESC M)
			    cpi = 12;
			    hmi = -1;
			    multipoint = false;
			    updateFont();
			    break;
		    case 0x4e: // Set bottom margin (ESC N)
			    topMargin = 0.0;
			    bottomMargin = (double)params[0] * lineSpacing; 
			    break;
		    case 0x4f: // Cancel bottom (and top) margin
			    topMargin = 0.0;
			    bottomMargin = pageHeight;
			    break;
		    case 0x50: // Select 10.5-point, 10-cpi (ESC P)
			    cpi = 10;
			    hmi = -1;
			    multipoint = false;
			    updateFont();
			    break;
		    case 0x51: // Set right margin
			    rightMargin = (double)(params[0] - 1.0) / (double)cpi;
			    break;
		    case 0x52: // Select an international character set (ESC R)
			    if (params[0] <= 13 || params[0] == 64)
			    {
				    if (params[0] == 64)
					    params[0] = 14;

				    curMap[0x23] = intCharSets[params[0]][0];
				    curMap[0x24] = intCharSets[params[0]][1];
				    curMap[0x40] = intCharSets[params[0]][2];
				    curMap[0x5b] = intCharSets[params[0]][3];
				    curMap[0x5c] = intCharSets[params[0]][4];
				    curMap[0x5d] = intCharSets[params[0]][5];
				    curMap[0x5e] = intCharSets[params[0]][6];
				    curMap[0x60] = intCharSets[params[0]][7];
				    curMap[0x7b] = intCharSets[params[0]][8];
				    curMap[0x7c] = intCharSets[params[0]][9];
				    curMap[0x7d] = intCharSets[params[0]][10];
				    curMap[0x7e] = intCharSets[params[0]][11];
			    }
			    break;
		    case 0x53: // Select superscript/subscript printing (ESC S)
			    if (params[0] == 0 || params[0] == 48)
				    style |= STYLE_SUBSCRIPT;
			    if (params[0] == 1 || params[1] == 49)
				    style |= STYLE_SUPERSCRIPT;
			    updateFont();
			    break;
		    case 0x54: // Cancel superscript/subscript printing (ESC T)
			    style &= 0xFFFF - STYLE_SUPERSCRIPT - STYLE_SUBSCRIPT;
			    updateFont();
			    break;
		    case 0x55: // Turn unidirectional mode on/off (ESC U)
			    // We don't have a print head, so just ignore this
			    break;
		    case 0x57: // Turn double-width printing on/off (ESC W)
			    if (!multipoint)
			    {
				    hmi = -1;
				    if (params[0] == 0 || params[0] == 48)
					    style &= ~STYLE_DOUBLEWIDTH;
				    if (params[0] == 1 || params[0] == 49)
					    style |= STYLE_DOUBLEWIDTH;
				    updateFont();
			    }
			    break;
		    case 0x58: // Select font by pitch and point (ESC X)
			    multipoint = true;
			    // Copy currently non-multipoint CPI if no value was set so far
			    if (multicpi == 0)
				    multicpi = cpi;
			    if (params[0] > 0)  // Set CPI
			    {
				    if (params[0] == 1) // Proportional spacing
					    style |= STYLE_PROP;
				    else if (params[0] >= 5)
					    multicpi = (double)360 / (double)params[0];
			    }
			    if (multiPointSize == 0)
				    multiPointSize = (double)10.5;
			    if (PARAM16(1) > 0) // Set points
				    multiPointSize = ((double)PARAM16(1)) / 2;			
			    updateFont();
			    break;
		    case 0x59: // Select 120-dpi, double-speed graphics (ESC Y)
			    setupBitImage(densy, PARAM16(0));
			    break;
		    case 0x5a: // Select 240-dpi graphics (ESC Z)
			    setupBitImage(densz, PARAM16(0));
			    break;
		    case 0x5c: // Set relative horizontal print position (ESC \)
			    {
				    int16_t toMove = PARAM16(0);
				    double unitSize = definedUnit;
				    if (unitSize < 0)
					    unitSize = (double)(printQuality == QUALITY_DRAFT ? 120.0 : 180.0);
				    curX += (double)((double)toMove / unitSize);
			    }
			    break;
		    case 0x61: // Select justification (ESC a)
			    // Ignore
			    break;
		    case 0x63: // Set horizontal motion index (HMI) (ESC c)
			    hmi = (double)PARAM16(0) / (double)360.0;
			    extraIntraSpace = 0.0;
			    break;
		    case 0x67: // Select 10.5-point, 15-cpi (ESC g)
			    cpi = 15;
			    hmi = -1;
			    multipoint = false;
			    updateFont();
			    break;
		    case 0x846: // Select forward feed mode (FS F) - set reverse not implemented yet
			    if (lineSpacing < 0) lineSpacing *= -1;
			    break;
		    case 0x6a: // Reverse paper feed (ESC j)
			    {
				    double reverse = (double)PARAM16(0) / (double)216.0;
				    reverse = curY - reverse;
				    if (reverse < leftMargin)
                        curY = leftMargin;
				    else
                        curY = reverse;
				    break;
			    }
		    case 0x6b: // Select typeface (ESC k)
			    if (params[0] <= 11 || params[0] == 30 || params[0] == 31) 
				    LQtypeFace = (Typeface)params[0];
			    updateFont();
			    break;
		    case 0x6c: // Set left margin (ESC l)
			    leftMargin =  (double)(params[0] - 1.0) / (double)cpi;
			    if (curX < leftMargin)
				    curX = leftMargin;
			    break;
		    case 0x70: // Turn proportional mode on/off (ESC p)
			    if (params[0] == 0 || params[0] == 48)
				    style &= (0xffff - STYLE_PROP);
			    if (params[0] == 1 || params[0] == 49)
			    {
				    style |= STYLE_PROP;
				    printQuality = QUALITY_LQ;
			    }
			    multipoint = false;
			    hmi = -1;
			    updateFont();
			    break;
		    case 0x72: // Select printing color (ESC r)
			    if (params[0] == 0 || params[0] > 6)
                    color = COLOR_BLACK;
			    else
                    color = params[0] << 5;       
			    break;
		    case 0x73: // Select low-speed mode (ESC s)
			    // Ignore
			    break;
		    case 0x74: // Select character table (ESC t)
		    case 0x849: // Select character table (FS I)
			    if (params[0] < 4)
				    curCharTable = params[0];
			    if (params[0] >= 48 && params[0] <= 51)
				    curCharTable = params[0] - 48;
			    selectCodepage(charTables[curCharTable]);
			    updateFont();
			    break;
		    case 0x77: // Turn double-height printing on/off (ESC w)
			    if (!multipoint)
			    {
				    if (params[0] == 0 || params[0] == 48)
					    style &= ~STYLE_DOUBLEHEIGHT;
				    if (params[0] == 1 || params[0] == 49)
					    style |= STYLE_DOUBLEHEIGHT;
				    updateFont();
			    }
			    break;
		    case 0x78: // Select LQ or draft (ESC x)
			    if (params[0] == 0 || params[0] == 48) {
				    printQuality = QUALITY_DRAFT;
				    style |= STYLE_CONDENSED;
			    }
			    if (params[0] == 1 || params[0] == 49) {
				    printQuality = QUALITY_LQ;
				    style &= ~STYLE_CONDENSED;
			    }
			    hmi = -1;
			    updateFont();
			    break;
		    case 0x100: // Set page length in inches (ESC C NUL)
			    pageHeight = (double)params[0];
			    bottomMargin = pageHeight;
			    topMargin = 0.0;
			    break;
		    case 0x101: // Skip unsupported ESC ( command
			    neededParam = PARAM16(0);
			    numParam = 0;
			    break;
		    case 0x274: // Assign character table (ESC (t)
			    if (params[2] < 4 && params[3] < 16)
			    {
				    charTables[params[2]] = codepages[params[3]];
				    //LOG_MSG("curr table: %d, p2: %d, p3: %d",curCharTable,params[2],params[3]);
				    if (params[2] == curCharTable)
					    selectCodepage(charTables[curCharTable]);
			    }
			    break;
		    case 0x22d: // Select line/score (ESC (-) 
			    style &= ~(STYLE_UNDERLINE | STYLE_STRIKETHROUGH | STYLE_OVERSCORE);
			    score = params[4];
			    if (score)
			    {
				    if (params[3] == 1)
					    style |= STYLE_UNDERLINE;
				    if (params[3] == 2)
					    style |= STYLE_STRIKETHROUGH;
				    if (params[3] == 3)
					    style |= STYLE_OVERSCORE;
			    }
			    updateFont();
			    break;
		    case 0x242: // Bar code setup and print (ESC (B)
			    LOG(LOG_MISC,LOG_ERROR)("PRINTER: Bardcode printing not supported");
			    // Find out how many bytes to skip
			    neededParam = PARAM16(0);
			    numParam = 0;
			    break;
		    case 0x243: // Set page length in defined unit (ESC (C)
			    if (params[0] != 0 && definedUnit > 0)
			    {
				    pageHeight = bottomMargin = ((double)PARAM16(2)) * definedUnit;
				    topMargin = 0.0;
			    }
			    break;
		    case 0x255: // Set unit (ESC (U)
			    definedUnit = (double)params[2] / (double)3600;
			    break;
		    case 0x256: // Set absolute vertical print position (ESC (V)
			    {
				    double unitSize = definedUnit;
				    if (unitSize < 0)
					    unitSize = (double)360.0;
				    double newPos = topMargin + (((double)PARAM16(2)) * unitSize);
				    if (newPos > bottomMargin)
					    newPage(true, false);
				    else
					    curY = newPos;
			    }
			    break;
		    case 0x25e: // Print data as characters (ESC (^)
			    numPrintAsChar = PARAM16(0);
			    break;
		    case 0x263: // Set page format (ESC (c)
			    if (definedUnit > 0)
			    {
				    double newTop, newBottom;
				    newTop = ((double)PARAM16(2)) * definedUnit;
				    newBottom = ((double)PARAM16(4)) * definedUnit;
				    if(newTop >= newBottom) break;
				    if(newTop < pageHeight) topMargin = newTop;
				    if(newBottom < pageHeight) bottomMargin = newBottom;
				    if(topMargin > curY) curY = topMargin;
				    //LOG_MSG("du %d, p1 %d, p2 %d, newtop %f, newbott %f, nt %f, nb %f, ph %f",
				    //	(Bitu)definedUnit,PARAM16(2),PARAM16(4),topMargin,bottomMargin,
				    //	newTop,newBottom,pageHeight);
			    }
			    break;
		    case 0x276: // Set relative vertical print position (ESC (v)
			    {
				    double unitSize = definedUnit;
				    if (unitSize < 0)
					    unitSize = (double)360.0;
				    double newPos = curY + ((double)((int16_t)PARAM16(2)) * unitSize);
				    if (newPos > topMargin)
				    {
					    if (newPos > bottomMargin)
						    newPage(true, false);
					    else
						    curY = newPos;	
				    }
			    }
			    break;
		    default:
			    if (ESCCmd < 0x100)
				    //LOG(LOG_MISC,LOG_WARN)
				    LOG_MSG("PRINTER: Skipped unsupported command ESC %c (%02X)", ESCCmd, ESCCmd);
			    else
				    //LOG(LOG_MISC,LOG_WARN)
				    LOG_MSG("PRINTER: Skipped unsupported command ESC ( %c (%02X)", ESCCmd-0x200, ESCCmd-0x200);
		}

		ESCCmd = 0;
		return true;
	}

	switch (ch)
	{
	    case 0x00:  // NUL is ignored by the printer
		    return true;
	    case 0x07:  // Beeper (BEL)
		    // BEEEP!
		    return true;
	    case 0x08:	// Backspace (BS)
		    {
			    double newX = curX - (1/(double)actcpi);
			    if (hmi > 0)
				    newX = curX - hmi;
			    if (newX >= leftMargin)
				    curX = newX;
		    }
		    return true;
	    case 0x09:	// Tab horizontally (HT)
		    {
			    // Find tab right to current pos
			    double moveTo = -1;
			    for (uint8_t i = 0; i < numHorizTabs; i++)
				    if (horiztabs[i] > curX)
					    moveTo = horiztabs[i];
			    // Nothing found => Ignore
			    if (moveTo > 0 && moveTo < rightMargin)
				    curX = moveTo;
		    }
		    return true;
	    case 0x0b:	// Tab vertically (VT)
		    if (numVertTabs == 0) // All tabs cancelled => Act like CR
			    curX = leftMargin;
		    else if (numVertTabs == 255) // No tabs set since reset => Act like LF
		    {
			    curX = leftMargin;
			    curY += lineSpacing;
			    if (curY > bottomMargin)
				    newPage(true, false);
		    }
		    else
		    {
			    // Find tab below current pos
			    double moveTo = -1;
			    for (uint8_t i = 0; i < numVertTabs; i++)
				    if (verttabs[i] > curY)
					    moveTo = verttabs[i];

			    // Nothing found => Act like FF
			    if (moveTo > bottomMargin || moveTo < 0)
				    newPage(true, false);
			    else
				    curY = moveTo;
		    }
		    if (style & STYLE_DOUBLEWIDTHONELINE)
		    {
			    style &= 0xFFFF - STYLE_DOUBLEWIDTHONELINE;
			    updateFont();
		    }
		    return true;
	    case 0x0c:		// Form feed (FF)
		    if (style & STYLE_DOUBLEWIDTHONELINE)
		    {
			    style &= 0xFFFF - STYLE_DOUBLEWIDTHONELINE;
			    updateFont();
		    }
		    newPage(true, true);
		    return true;
	    case 0x0d:		// Carriage Return (CR)
		    curX = leftMargin;
		    if (!autoFeed)
			    return true;
	    case 0x0a:		// Line feed
		    if (style & STYLE_DOUBLEWIDTHONELINE)
		    {
			    style &= 0xFFFF - STYLE_DOUBLEWIDTHONELINE;
			    updateFont();
		    }
		    curX = leftMargin;
		    curY += lineSpacing;
		    if (curY > bottomMargin)
			    newPage(true, false);
		    return true;
	    case 0x0e:		//Select double-width printing (one line) (SO)
		    if (!multipoint)
		    {
			    hmi = -1;
			    style |= STYLE_DOUBLEWIDTHONELINE;
			    updateFont();
		    }
		    return true;
	    case 0x0f:		// Select condensed printing (SI)
		    if (!multipoint && (cpi != 15.0))
            {
			    hmi = -1;
			    style |= STYLE_CONDENSED;
			    updateFont();
		    }
		    return true;
	    case 0x11:		// Select printer (DC1)
		    // Ignore
		    return true;
	    case 0x12:		// Cancel condensed printing (DC2)
		    hmi = -1;
		    style &= ~STYLE_CONDENSED;
		    updateFont();
		    return true;
	    case 0x13:		// Deselect printer (DC3)
		    // Ignore
		    return true;
	    case 0x14:		// Cancel double-width printing (one line) (DC4)
		    hmi = -1;
		    style &= ~STYLE_DOUBLEWIDTHONELINE;
		    updateFont();
		    return true;
	    case 0x18:		// Cancel line (CAN)
		    return true;
	    case 0x1b:		// ESC
		    ESCSeen = true;
		    return true;
	    case 0x1c:		// FS (IBM commands)
		    FSSeen = true;
		    return true;
	    default:
		    return false;
	}

	return false;
}

static void PRINTER_EventHandler(Bitu param);

void CPrinter::newPage(bool save, bool resetx)
{
	PIC_RemoveEvents(PRINTER_EventHandler);
	if (printer_timout)
        timeout_dirty=false;
	if (save)
		outputPage();
	if (resetx)
        curX = leftMargin;
	curY = topMargin;

	SDL_Rect rect;
	rect.x = 0;
	rect.y = 0;
	rect.w = page->w;
	rect.h = page->h;
	SDL_FillRect(page, &rect, SDL_MapRGB(page->format, 255, 255, 255));

	/*for(int i = 0; i < 256; i++)
	{
        *((uint8_t*)page->pixels+i)=i;
	}*/
}

void CPrinter::printChar(uint8_t ch)
{
	charRead = true;
	if (page == NULL) return;

	// Don't think that DOS programs uses this but well: Apply MSB if desired
	if (msb != 255) {
		if (msb == 0) ch &= 0x7F;
		if (msb == 1) ch |= 0x80;
	}

	// Are we currently printing a bit graphic?
	if (bitGraph.remBytes > 0) {
		printBitGraph(ch);
		return;
	}

	// Print everything?
	if (numPrintAsChar > 0) numPrintAsChar--;
	else if (processCommandChar(ch)) return;

	// Do not print if no font is available
	if (!curFont) return;

	if(ch == 0x1) ch = 0x20;
	
	// Find the glyph for the char to render
	FT_UInt index = FT_Get_Char_Index(curFont, curMap[ch]);
	
	// Load the glyph 
	FT_Load_Glyph(curFont, index, FT_LOAD_DEFAULT);

	// Render a high-quality bitmap
	FT_Render_Glyph(curFont->glyph, FT_RENDER_MODE_NORMAL);

	uint16_t penX = (uint16_t)(PIXX + curFont->glyph->bitmap_left);
	uint16_t penY = (uint16_t)(PIXY - curFont->glyph->bitmap_top + curFont->size->metrics.ascender / 64);

	if (style & STYLE_SUBSCRIPT) penY += curFont->glyph->bitmap.rows / 2;

	// Copy bitmap into page
	SDL_LockSurface(page);

	blitGlyph(curFont->glyph->bitmap, penX, penY, false);
	blitGlyph(curFont->glyph->bitmap, penX+1, penY, true);

	// Doublestrike => Print the glyph a second time one pixel below
	if (style & STYLE_DOUBLESTRIKE)
    {
		blitGlyph(curFont->glyph->bitmap, penX, penY+1, true);
		blitGlyph(curFont->glyph->bitmap, penX+1, penY+1, true);
	}

	// Bold => Print the glyph a second time one pixel to the right
	// or be a bit more bold...
	if (style & STYLE_BOLD)
    {
		blitGlyph(curFont->glyph->bitmap, penX+1, penY, true);
		blitGlyph(curFont->glyph->bitmap, penX+2, penY, true);
		blitGlyph(curFont->glyph->bitmap, penX+3, penY, true);
	}
	SDL_UnlockSurface(page);

	// For line printing
	uint16_t lineStart = (uint16_t)PIXX;

	// advance the cursor to the right
	double x_advance;
	if (style &	STYLE_PROP)
		x_advance = (double)((double)(curFont->glyph->advance.x) / (double)(dpi * 64));
	else
    {
		if (hmi < 0)
            x_advance = 1 / (double)actcpi;
		else
            x_advance = hmi;
	}
	x_advance += extraIntraSpace;
    curX += x_advance;

	// Draw lines if desired
	if ((score != SCORE_NONE) && (style & (STYLE_UNDERLINE|STYLE_STRIKETHROUGH|STYLE_OVERSCORE)))
	{
		// Find out where to put the line
		uint16_t lineY = (uint16_t)PIXY;
		double height = (curFont->size->metrics.height >> 6); // TODO height is fixed point madness...

		if (style & STYLE_UNDERLINE)
            lineY = (uint16_t)PIXY + (uint16_t)(height * 0.9);
		else if (style & STYLE_STRIKETHROUGH)
            lineY = (uint16_t)PIXY + (uint16_t)(height * 0.45);
		else if (style & STYLE_OVERSCORE)
			lineY = (uint16_t)PIXY - (uint16_t)(((score == SCORE_DOUBLE) || (score == SCORE_DOUBLEBROKEN)) ? 5 : 0);

		drawLine(lineStart, PIXX, lineY, score == SCORE_SINGLEBROKEN || score == SCORE_DOUBLEBROKEN);

		// draw second line if needed
		if ((score == SCORE_DOUBLE)||(score == SCORE_DOUBLEBROKEN))
			drawLine(lineStart, PIXX, lineY + 5, score == SCORE_SINGLEBROKEN || score == SCORE_DOUBLEBROKEN);
	}
	// If the next character would go beyond the right margin, line-wrap.
	if((curX + x_advance) > rightMargin)
    {
		curX = leftMargin;
		curY += lineSpacing;
		if (curY > bottomMargin) newPage(true, false);
	}
}

void CPrinter::blitGlyph(FT_Bitmap bitmap, uint16_t destx, uint16_t desty, bool add) {
	for (Bitu y = 0; y < bitmap.rows; y++)
    {
		for (Bitu x = 0; x < bitmap.width; x++)
        {
			// Read pixel from glyph bitmap
			uint8_t source = *(bitmap.buffer + x + y * bitmap.pitch);

			// Ignore background and don't go over the border
			if (source > 0 && (destx + x < (Bitu)page->w) && (desty + y < (Bitu)page->h))
            {
				uint8_t* target = (uint8_t*)page->pixels + (x + destx) + (y + desty) * page->pitch;
				source>>=3;
				
				if (add)
                {
					if (((*target) & 0x1f) + source > 31)
                        *target |= (color | 0x1f);
					else
                    {
						*target += source;
						*target |= color;
					}
				}
				else
                    *target = source|color;
			}
		}
	}
}

void CPrinter::drawLine(Bitu fromx, Bitu tox, Bitu y, bool broken)
{
	SDL_LockSurface(page);

	Bitu breakmod = dpi / 15;
	Bitu gapstart = (breakmod * 4) / 5;

	// Draw anti-aliased line
	for (Bitu x = fromx; x <= tox; x++)
	{
		// Skip parts if broken line or going over the border
		if ((!broken || (x % breakmod <= gapstart)) && (x < (Bitu)page->w))
		{
			if (y > 0 && (y - 1) < (Bitu)page->h)
				*((uint8_t*)page->pixels + x + (y - 1) * (Bitu)page->pitch) = 240;
			if (y < (Bitu)page->h)
				*((uint8_t*)page->pixels + x + y * (Bitu)page->pitch) = !broken ? 255 : 240;
			if (y + 1 < (Bitu)page->h)
				*((uint8_t*)page->pixels + x + (y + 1) * (Bitu)page->pitch) = 240;
		}
	}
	SDL_UnlockSurface(page);
}

void CPrinter::setAutofeed(bool feed)
{
	autoFeed = feed;
}

bool CPrinter::getAutofeed()
{
	return autoFeed;
}

bool CPrinter::isBusy()
{
	// We're never busy
	return false;
}

bool CPrinter::ack()
{
	// Acknowledge last char read
	if(charRead)
    {
		charRead=false;
		return true;
	}
	return false;
}

void CPrinter::setupBitImage(uint8_t dens, uint16_t numCols)
{
	switch (dens)
	{
	    case 0:
		    bitGraph.horizDens = 60;
		    bitGraph.vertDens = 60;
		    bitGraph.adjacent = true;
		    bitGraph.bytesColumn = 1;
		    break;
	    case 1:
		    bitGraph.horizDens = 120;
		    bitGraph.vertDens = 60;
		    bitGraph.adjacent = true;
		    bitGraph.bytesColumn = 1;
		    break;
	    case 2:
		    bitGraph.horizDens = 120;
		    bitGraph.vertDens = 60;
		    bitGraph.adjacent = false;
		    bitGraph.bytesColumn = 1;
		    break;
	    case 3:
		    bitGraph.horizDens = 60;
		    bitGraph.vertDens = 240;
		    bitGraph.adjacent = false;
		    bitGraph.bytesColumn = 1;
		    break;
	    case 4:
		    bitGraph.horizDens = 80;
		    bitGraph.vertDens = 60;
		    bitGraph.adjacent = true;
		    bitGraph.bytesColumn = 1;
		    break;
	    case 6:
		    bitGraph.horizDens = 90;
		    bitGraph.vertDens = 60;
		    bitGraph.adjacent = true;
		    bitGraph.bytesColumn = 1;
		    break;
	    case 32:
		    bitGraph.horizDens = 60;
		    bitGraph.vertDens = 180;
		    bitGraph.adjacent = true;
		    bitGraph.bytesColumn = 3;
		    break;
	    case 33:
		    bitGraph.horizDens = 120;
		    bitGraph.vertDens = 180;
		    bitGraph.adjacent = true;
		    bitGraph.bytesColumn = 3;
		    break;
	    case 38:
		    bitGraph.horizDens = 90;
		    bitGraph.vertDens = 180;
		    bitGraph.adjacent = true;
		    bitGraph.bytesColumn = 3;
		    break;
	    case 39:
		    bitGraph.horizDens = 180;
		    bitGraph.vertDens = 180;
		    bitGraph.adjacent = true;
		    bitGraph.bytesColumn = 3;
		    break;
	    case 40:
		    bitGraph.horizDens = 360;
		    bitGraph.vertDens = 180;
		    bitGraph.adjacent = false;
		    bitGraph.bytesColumn = 3;
		    break;
	    case 71:
		    bitGraph.horizDens = 180;
		    bitGraph.vertDens = 360;
		    bitGraph.adjacent = true;
		    bitGraph.bytesColumn = 6;
		    break;
	    case 72:
		    bitGraph.horizDens = 360;
		    bitGraph.vertDens = 360;
		    bitGraph.adjacent = false;
		    bitGraph.bytesColumn = 6;
		    break;
	    case 73:
		    bitGraph.horizDens = 360;
		    bitGraph.vertDens = 360;
		    bitGraph.adjacent = true;
		    bitGraph.bytesColumn = 6;
		    break;
	    default:
		    LOG(LOG_MISC,LOG_ERROR)("PRINTER: Unsupported bit image density %i", dens);
	}

	bitGraph.remBytes = numCols * bitGraph.bytesColumn;
	bitGraph.readBytesColumn = 0;
}

void CPrinter::printBitGraph(uint8_t ch)
{
	bitGraph.column[bitGraph.readBytesColumn++] = ch;
	bitGraph.remBytes--;

	// Only print after reading a full column
	if (bitGraph.readBytesColumn < bitGraph.bytesColumn)
		return;

	double oldY = curY;

	SDL_LockSurface(page);

	// When page dpi is greater than graphics dpi, the drawn pixels get "bigger"
	Bitu pixsizeX = 1; 
	Bitu pixsizeY = 1;
	if(bitGraph.adjacent)
    {
		pixsizeX = dpi / bitGraph.horizDens > 0 ? dpi / bitGraph.horizDens : 1;
		pixsizeY = dpi / bitGraph.vertDens > 0 ? dpi / bitGraph.vertDens : 1;
	}
	// TODO figure this out for 360dpi mode in windows

//	Bitu pixsizeX = dpi/bitGraph.horizDens > 0? dpi/bitGraph.horizDens : 1;
//	Bitu pixsizeY = dpi/bitGraph.vertDens > 0? dpi/bitGraph.vertDens : 1;

	for (Bitu i = 0; i < bitGraph.bytesColumn; i++)
	{
        // for each byte
		for (Bitu j = 128; j != 0; j >>= 1)
        {
            // for each bit
			if (bitGraph.column[i] & j)
            {
				for (Bitu xx=0; xx<pixsizeX; xx++)
					for (Bitu yy=0; yy<pixsizeY; yy++)
                    {
						if (((PIXX + xx) < (Bitu)page->w) && ((PIXY + yy) < (Bitu)page->h))
							*((uint8_t*)page->pixels + (PIXX + xx) + (PIXY + yy)*page->pitch) |= (color | 0x1F);
					}
			} // else white pixel
			curY += (double)1 / (double)bitGraph.vertDens; // TODO line wrap?
		}
	}

	SDL_UnlockSurface(page);

	curY = oldY;

	bitGraph.readBytesColumn = 0;

	// Advance to the left
	curX += (double)1 / (double)bitGraph.horizDens;
}

void CPrinter::formFeed()
{
	// Don't output blank pages
	newPage(!isBlank(), true);
	finishMultipage();
}

static void findNextName(const char* front, const char* ext, char* fname)
{
	int i = 1;
	Bitu slen = (Bitu)strlen(document_path);
	if(slen > (200 - 15))
    {
		fname[0] = 0;
		return;
	}

    FILE *test = NULL;
	do
	{
		strcpy(fname, document_path);
#ifdef WIN32
		const char* const pathstring = "\\%s%d%s";
#else 
		const char* const pathstring = "/%s%d%s";
#endif
		sprintf(fname + strlen(fname), pathstring, front, i++, ext);
		test = fopen(fname, "rb");
		if (test != NULL) fclose(test);
	}
	while (test != NULL);
}

void CPrinter::doAction(const char *fname) {
    std::string action;
    if (actstd.size()) {
        action=actstd;
        bool fail=false;
#if defined(WIN32)
        bool q=false;
        int pos=-1;
        std::string para=fname;
        for (int i=0; i<action.size(); i++) {
            if (action[i]=='"') q=!q;
            else if (action[i]==' ' && !q) {
                pos=i;
                break;
            }
        }
        if (pos>-1) {
            para=action.substr(pos+1)+" "+fname;
            action=action.substr(0, pos);
        }
        fail=(INT_PTR)ShellExecute(NULL, "open", action.c_str(), para.c_str(), NULL, shellhide?SW_HIDE:SW_NORMAL)<=32;
#else
        fail=system((action+" "+fname).c_str())!=0;
#endif
        if (acterr.size()&&fail) {
            action=acterr;
#if defined(WIN32)
            q=false;
            pos=-1;
            para=fname;
            for (int i=0; i<action.size(); i++) {
                if (action[i]=='"') q=!q;
                else if (action[i]==' ' && !q) {
                    pos=i;
                    break;
                }
            }
            if (pos>-1) {
                para=action.substr(pos+1)+" "+fname;
                action=action.substr(0, pos);
            }
            fail=(INT_PTR)ShellExecute(NULL, "open", action.c_str(), para.c_str(), NULL, shellhide?SW_HIDE:SW_NORMAL)<=32;
#else
            fail=system((action+" "+fname).c_str())!=0;
#endif
        }
        bool systemmessagebox(char const * aTitle, char const * aMessage, char const * aDialogType, char const * aIconType, int aDefaultButton);
        if (fail) systemmessagebox("Error", "The requested file handler failed to complete.", "ok","error", 1);
    }
}

void CPrinter::outputPage() 
{
	char fname[200];

	if (strcasecmp(output, "printer") == 0)
	{
#if defined (WIN32)
		// You'll need the mouse for the print dialog
		if(mouselocked)
			 GFX_CaptureMouse();

		uint16_t physW = GetDeviceCaps(printerDC, PHYSICALWIDTH);
		uint16_t physH = GetDeviceCaps(printerDC, PHYSICALHEIGHT);

		double scaleW, scaleH;

		if (page->w > physW) 
	        scaleW = (double)page->w / (double)physW;
	    else 
			scaleW = (double)physW / (double)page->w; 
 
		if (page->h > physH) 
	        scaleH = (double)page->h / (double)physH;
	    else 
			scaleH = (double)physH / (double)page->h; 

		HDC memHDC = CreateCompatibleDC(printerDC);
		HBITMAP bitmap = CreateCompatibleBitmap(memHDC, page->w, page->h);
		SelectObject(memHDC, bitmap);

		// Start new printer job?
		if (outputHandle == NULL)
		{
			DOCINFO docinfo;
			memset(&docinfo, 0, sizeof(docinfo));
			docinfo.cbSize = sizeof(docinfo);
			docinfo.lpszDocName = "DOSBox-X Printer";
			docinfo.lpszOutput = NULL;
			docinfo.lpszDatatype = NULL;
			docinfo.fwType = 0;

			StartDoc(printerDC, &docinfo);
			multiPageCounter = 1;
		}

		if (StartPage(printerDC) < 0)
        {
			LOG_MSG("PRINTER: Cannot start page.");
			DeleteObject(bitmap);
			DeleteDC(memHDC);
			return;
		}
		SDL_LockSurface(page);

		SDL_Palette* sdlpal = page->format->palette;

		for (uint16_t y = 0; y < page->h; y++)
		{
			for (uint16_t x = 0; x < page->w; x++)
			{
				uint8_t pixel = *((uint8_t*)page->pixels + x + (y*page->pitch));
				uint32_t color = 0;
				color |= sdlpal->colors[pixel].r;
				color |= ((uint32_t)sdlpal->colors[pixel].g) << 8;
				color |= ((uint32_t)sdlpal->colors[pixel].b) << 16;
				SetPixel(memHDC, x, y, color);
			}
		}

		SDL_UnlockSurface(page);
	
		StretchBlt(printerDC, 0, 0, physW, physH, memHDC, 0, 0, page->w, page->h, SRCCOPY);

		EndPage(printerDC);

		if (multipageOutput)
		{
			multiPageCounter++;
			outputHandle = printerDC;
		}
		else
		{
			EndDoc(printerDC);
			outputHandle = NULL;
		}
		DeleteObject(bitmap);
		DeleteDC(memHDC);
#else
		LOG_MSG("PRINTER: Direct printing not supported under this OS");
#endif
	}
#ifdef C_LIBPNG
	else if (strcasecmp(output, "png") == 0)
	{
		// Find a page that does not exists
		findNextName("page", ".png", &fname[0]);
	
		png_structp png_ptr;
		png_infop info_ptr;
		png_bytep* row_pointers;
		png_color palette[256];
		Bitu i;

		/* Open the actual file */
		FILE* fp = fopen(fname,"wb");
		if (!fp) 
		{
			LOG(LOG_MISC,LOG_ERROR)("PRINTER: Can't open file %s for printer output", fname);
			return;
		}

		/* First try to alloacte the png structures */
		png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		if (!png_ptr) return;
		info_ptr = png_create_info_struct(png_ptr);
		if (!info_ptr)
        {
			png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
			return;
		}

		/* Finalize the initing of png library */
		png_init_io(png_ptr, fp);
		png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
		
		/* set other zlib parameters */
		png_set_compression_mem_level(png_ptr, 8);
		png_set_compression_strategy(png_ptr, Z_DEFAULT_STRATEGY);
		png_set_compression_window_bits(png_ptr, 15);
		png_set_compression_method(png_ptr, 8);
		png_set_compression_buffer_size(png_ptr, 8192);
		
		png_set_IHDR(png_ptr, info_ptr, page->w, page->h,
			8, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		for (i = 0; i < 256; i++) 
		{
			palette[i].red = page->format->palette->colors[i].r;
			palette[i].green = page->format->palette->colors[i].g;
			palette[i].blue = page->format->palette->colors[i].b;
		}
		png_set_PLTE(png_ptr, info_ptr, palette,256);
		
		SDL_LockSurface(page);

		// Allocate an array of scanline pointers
		row_pointers = (png_bytep*)malloc(page->h * sizeof(png_bytep));
		for (i = 0; i < (Bitu)page->h; i++) 
			row_pointers[i] = ((uint8_t*)page->pixels + (i * page->pitch));

		// tell the png library what to encode.
		png_set_rows(png_ptr, info_ptr, row_pointers);
		
		// Write image to file
		png_write_png(png_ptr, info_ptr, 0, NULL);

		SDL_UnlockSurface(page);
		
		/*close file*/
		fclose(fp);
	
		/*Destroy PNG structs*/
		png_destroy_write_struct(&png_ptr, &info_ptr);
		
		/*clean up dynamically allocated RAM.*/
		free(row_pointers);
		doAction(fname);
	}
#endif
	else if (strcasecmp(output, "ps") == 0)
	{
		FILE* psfile = NULL;
		
		// Continue postscript file?
		if (outputHandle != NULL)
			psfile = (FILE*)outputHandle;

		// Create new file?
		if (psfile == NULL)
		{
			if (!multipageOutput)
				findNextName("page", ".ps", &fname[0]);
			else
				findNextName("doc", ".ps", &fname[0]);

			psfile = fopen(fname, "wb");
			if (!psfile) 
			{
				LOG(LOG_MISC,LOG_ERROR)("PRINTER: Can't open file %s for printer output", fname);
				return;
			}

			// Print header
			fprintf(psfile, "%%!PS-Adobe-3.0\n");
			fprintf(psfile, "%%%%Pages: (atend)\n");
			fprintf(psfile, "%%%%BoundingBox: 0 0 %i %i\n", (uint16_t)(defaultPageWidth * 72), (uint16_t)(defaultPageHeight * 72));
			fprintf(psfile, "%%%%Creator: DOSBox-X Virtual Printer\n");
			fprintf(psfile, "%%%%DocumentData: Clean7Bit\n");
			fprintf(psfile, "%%%%LanguageLevel: 2\n");
			fprintf(psfile, "%%%%EndComments\n");
			multiPageCounter = 1;
		}

		fprintf(psfile, "%%%%Page: %i %i\n", multiPageCounter, multiPageCounter);
		fprintf(psfile, "%i %i scale\n", (uint16_t)(defaultPageWidth * 72), (uint16_t)(defaultPageHeight * 72));
		fprintf(psfile, "%i %i 8 [%i 0 0 -%i 0 %i]\n", page->w, page->h, page->w, page->h, page->h);
		fprintf(psfile, "currentfile\n");
		fprintf(psfile, "/ASCII85Decode filter\n");
		fprintf(psfile, "/RunLengthDecode filter\n");
		fprintf(psfile, "image\n");

		SDL_LockSurface(page);

		uint32_t pix = 0;
		uint32_t numpix = page->h * page->w;
		ASCII85BufferPos = ASCII85CurCol = 0;

		while (pix < numpix)
		{
			// Compress data using RLE
			if ((pix < numpix - 2) && (getPixel(pix) == getPixel(pix + 1)) && (getPixel(pix) == getPixel(pix + 2)))
			{
				// Found three or more pixels with the same color
				uint8_t sameCount = 3;
				uint8_t col = getPixel(pix);
				while (sameCount < 128 && sameCount + pix < numpix && col == getPixel(pix + sameCount))
					sameCount++;

				fprintASCII85(psfile, 257 - sameCount);
				fprintASCII85(psfile, 255 - col);

				// Skip ahead
				pix += sameCount;
			}
			else
			{
				// Find end of heterogenous area
				uint8_t diffCount = 1;
				while (
                    diffCount < 128 && diffCount + pix < numpix && 
					(
				        (diffCount + pix < numpix - 2) || (getPixel(pix + diffCount) != getPixel(pix + diffCount+1)) || (getPixel(pix + diffCount) != getPixel(pix + diffCount+2))
					)
                ) diffCount++;

				fprintASCII85(psfile, diffCount-1);
				for (uint8_t i = 0; i < diffCount; i++)
					fprintASCII85(psfile, 255 - getPixel(pix++));
			}
		}

		// Write EOD for RLE and ASCII85
		fprintASCII85(psfile, 128);
		fprintASCII85(psfile, 256);

		SDL_UnlockSurface(page);

		fprintf(psfile, "showpage\n");

		if (multipageOutput)
		{
			multiPageCounter++;
			outputHandle = psfile;
		}
		else
		{
			fprintf(psfile, "%%%%Pages: 1\n");
			fprintf(psfile, "%%%%EOF\n");
			fclose(psfile);
			outputHandle = NULL;
			doAction(fname);
		}
	}
	else
	{	
		// Find a page that does not exists
		findNextName("page", ".bmp", &fname[0]);
		SDL_SaveBMP(page, fname);
		doAction(fname);
	}
}

void CPrinter::fprintASCII85(FILE* f, uint16_t b)
{
	if (b != 256)
	{
		if (b < 256)
			ASCII85Buffer[ASCII85BufferPos++] = (uint8_t)b;

		if (ASCII85BufferPos == 4 || b == 257)
		{
			uint32_t num = (uint32_t)ASCII85Buffer[0] << 24 | (uint32_t)ASCII85Buffer[1] << 16 | (uint32_t)ASCII85Buffer[2] << 8 | (uint32_t)ASCII85Buffer[3];

			// Deal with special case
			if (num == 0 && b != 257)
			{
				fprintf(f, "z");
				if (++ASCII85CurCol >= 79)
				{
					ASCII85CurCol = 0;
					fprintf(f, "\n");
				}
			}
			else
			{
				char buffer[5];
				for (int8_t i = 4; i >= 0; i--)
				{
					buffer[i] = (uint8_t)((uint32_t)num % (uint32_t)85);
					buffer[i] += 33;
					num /= (uint32_t)85;
				}

				// Make sure a line never starts with a % (which may be mistaken as start of a comment)
				if (ASCII85CurCol == 0 && buffer[0] == '%')
					fprintf(f, " ");
				
				for (int i = 0; i < ((b != 257) ? 5 : ASCII85BufferPos + 1); i++)
				{
					fprintf(f, "%c", buffer[i]);
					if (++ASCII85CurCol >= 79)
					{
						ASCII85CurCol = 0;
						fprintf(f, "\n");
					}
				}
			}

			ASCII85BufferPos = 0;
		}

	}
	else // Close string
	{
		// Partial tupel if there are still bytes in the buffer
		if (ASCII85BufferPos > 0)
		{
			for (uint8_t i = ASCII85BufferPos; i < 4; i++)
				ASCII85Buffer[i] = 0;

			fprintASCII85(f, 257);
		}

		fprintf(f, "~");
		fprintf(f, ">\n");
	}
}

void CPrinter::finishMultipage()
{
	if (outputHandle != NULL)
	{
		if (strcasecmp(output, "ps") == 0)
		{
			FILE* psfile = (FILE*)outputHandle;
			fprintf(psfile, "%%%%Pages: %i\n", multiPageCounter);
			fprintf(psfile, "%%%%EOF\n");
			fclose(psfile);
		}
		else if (strcasecmp(output, "printer") == 0)
		{
#if defined (WIN32)
			EndDoc(printerDC);
#endif
		}
		outputHandle = NULL;
	}
}

bool CPrinter::isBlank()
{
	bool blank = true;

	SDL_LockSurface(page);

	for (uint16_t y = 0; y < page->h; y++)
		for (uint16_t x = 0; x < page->w; x++)
			if (*((uint8_t*)page->pixels + x + (y * page->pitch)) != 0)
				blank = false;

	SDL_UnlockSurface(page);
	return blank;
}

uint8_t CPrinter::getPixel(uint32_t num)
{
	// Respect the pitch
	return *((uint8_t*)page->pixels + (num % page->w) + ((num / page->w) * page->pitch));
}

static uint8_t dataregister; // contents of the parallel port data register

Bitu PRINTER_readdata(Bitu port,Bitu iolen)
{
    (void)port;
    (void)iolen;
	return dataregister;
}

void PRINTER_writedata(Bitu port,Bitu val,Bitu iolen)
{
    (void)port;
    (void)iolen;
	dataregister = (uint8_t)val;
}
uint8_t controlreg = 0x04;

Bitu PRINTER_readstatus(Bitu port,Bitu iolen)
{
    (void)port;
    (void)iolen;
	//LOG_MSG("PRINTER_readstatus CS:IP %8x:%8x",SegValue(cs),reg_eip);
	// Don't create a CPrinter unless the program really wants to print
	// Return standard: No error, printer online, no ack and not busy
	if (!defaultPrinter)
		return 0xdf;

	// Printer is always online and never reports an error
	uint8_t status = 0x1f;// 0x18;

//	if (controlreg&0x08==0)
//		status |= 0x10;

	if (!defaultPrinter->isBusy())
		status |= 0x80;

	if (!defaultPrinter->ack())
		status |= 0x40;

	return status;
}

static void FormFeed(bool pressed)
{
	if (pressed)
		if (defaultPrinter)
        {
			PIC_RemoveEvents(PRINTER_EventHandler);
			if (printer_timout) timeout_dirty = false;
			
			defaultPrinter->formFeed();
		}
}


static void PRINTER_EventHandler(Bitu param)
{
    (void)param;
	//LOG_MSG("printerevent");
	if (timeout_dirty)
    {
        // add another
		PIC_AddEvent(PRINTER_EventHandler, (float)printer_timout, 0);
		//LOG_MSG("timeout renew");
		timeout_dirty = false;
	}
    else
    {
		timeout_dirty = false;
		FormFeed(true);
	}
}

void PRINTER_writecontrol(Bitu port,Bitu val,Bitu iolen)
{
    (void)port;
    (void)iolen;
	//LOG_MSG("PRINTER_writecontrol CS:IP %8x:%8x",SegValue(cs),reg_eip);
	// init printer if bit 4 is switched on
	if ((val & 0x04) && defaultPrinter && (!(controlreg & 0x04)))
		defaultPrinter->resetPrinterHard();

	// data is strobed to the parallel printer on the falling edge of strobe bit
	if(!(val & 0x1) && (controlreg & 0x1)) {
		if (!defaultPrinter)
		    defaultPrinter = new CPrinter(confdpi, confwidth, confheight, confoutputDevice, confmultipageOutput);
		defaultPrinter->printChar(dataregister);
		if (!timeout_dirty)
        {
			PIC_AddEvent(PRINTER_EventHandler, (float)printer_timout, 0);
			timeout_dirty = true;
		}
	}

	controlreg = (uint8_t)val;
	if (defaultPrinter)
		defaultPrinter->setAutofeed((val & 0x02) > 0);
}

Bitu PRINTER_readcontrol(Bitu port,Bitu iolen)
{
    (void)port;
    (void)iolen;
	//LOG_MSG("PRINTER_readcontrol CS:IP %8x:%8x",SegValue(cs),reg_eip);
	// Don't create a CPrinter unless the program really wants to print
	if (!defaultPrinter)
		return 0xe0 | controlreg; //0xe8;

	return 0xe0 | (defaultPrinter->getAutofeed() ? 0x02 : 0x00) | (controlreg & 0xfd);
}

void PRINTER_Shutdown(Section* sec)
{
    (void)sec;//UNUSED

	if (defaultPrinter)
	{
		delete defaultPrinter;
		defaultPrinter = NULL;
	}
}

bool inited = false;
bool PRINTER_isInited()
{
	return inited;
}

void PRINTER_Init() 
{
	Section_prop * section = static_cast<Section_prop *>(control->GetSection("printer"));
    AddExitFunction(AddExitFunctionFuncPair(PRINTER_Shutdown), true);

	// Set base address of LPT1 in the BIOS variable segment
	//real_writew(0x0040, 0x0008, LPTPORT);

	if (!section->Get_bool("printer")) return;
	inited = true;
	document_path = section->Get_string("docpath");
	font_path = section->Get_string("fontpath");
	confdpi = section->Get_int("dpi");
	confwidth = section->Get_int("width");
	confheight = section->Get_int("height");
	printer_timout = section->Get_int("timeout");
	if(!printer_timout) timeout_dirty = true; // this will lock up the timeout
	else timeout_dirty = false;
	strcpy(&confoutputDevice[0], section->Get_string("printoutput"));
	confmultipageOutput = section->Get_bool("multipage");
	shellhide = section->Get_bool("shellhide");
	actstd = section->Get_string("openwith");
	acterr = section->Get_string("openerror");

	//IO_RegisterWriteHandler(LPTPORT,PRINTER_writedata,IO_MB);
	//IO_RegisterReadHandler(LPTPORT,PRINTER_readdata,IO_MB);
	
	//IO_RegisterReadHandler(LPTPORT+1,PRINTER_readstatus,IO_MB);
	//IO_RegisterWriteHandler(LPTPORT+2,PRINTER_writecontrol,IO_MB);
	//IO_RegisterReadHandler(LPTPORT+2,PRINTER_readcontrol,IO_MB);

    DOSBoxMenu::item *item;
	MAPPER_AddHandler(FormFeed, MK_f2 , MMOD1, "ejectpage", "Send form-feed", &item);
    item->set_text("Send form-feed");
}

#endif
