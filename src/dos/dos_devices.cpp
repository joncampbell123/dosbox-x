/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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


#include <string.h>
#include "dosbox.h"
#include "callback.h"
#include "regs.h"
#include "mem.h"
#include "bios.h"
#include "dos_inc.h"
#include "support.h"
#include "parport.h"
#include "drives.h" //Wildcmp
/* Include all the devices */

#include "dev_con.h"
#include <fstream>

DOS_Device * Devices[DOS_DEVICES] = {NULL};
extern int dos_clipboard_device_access;
extern char * dos_clipboard_device_name;

class device_NUL : public DOS_Device {
public:
	device_NUL() { SetName("NUL"); };
	virtual bool Read(uint8_t * data,uint16_t * size) {
        (void)data; // UNUSED
		*size = 0; //Return success and no data read. 
//		LOG(LOG_IOCTL,LOG_NORMAL)("%s:READ",GetName());
		return true;
	}
	virtual bool Write(const uint8_t * data,uint16_t * size) {
        (void)data; // UNUSED
        (void)size; // UNUSED
//		LOG(LOG_IOCTL,LOG_NORMAL)("%s:WRITE",GetName());
		return true;
	}
	virtual bool Seek(uint32_t * pos,uint32_t type) {
        (void)type;
        (void)pos;
//		LOG(LOG_IOCTL,LOG_NORMAL)("%s:SEEK",GetName());
		return true;
	}
	virtual bool Close() { return true; }
	virtual uint16_t GetInformation(void) { return 0x8084; }
	virtual bool ReadFromControlChannel(PhysPt bufptr,uint16_t size,uint16_t * retcode) { (void)bufptr; (void)size; (void)retcode; return false; }
	virtual bool WriteToControlChannel(PhysPt bufptr,uint16_t size,uint16_t * retcode) { (void)bufptr; (void)size; (void)retcode; return false; }
};

class device_PRN : public DOS_Device {
public:
	device_PRN() {
		SetName("PRN");
	}
	bool Read(uint8_t * data,uint16_t * size) {
        (void)data; // UNUSED
        (void)size; // UNUSED
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	bool Write(const uint8_t * data,uint16_t * size) {
		for(int i = 0; i < 9; i++) {
			// look up a parallel port
			if(parallelPortObjects[i] != NULL) {
				// send the data
				for (uint16_t j=0; j<*size; j++) {
					if(!parallelPortObjects[i]->Putchar(data[j])) return false;
				}
				return true;
			}
		}
		return false;
	}
	bool Seek(uint32_t * pos,uint32_t type) {
        (void)type; // UNUSED
		*pos = 0;
		return true;
	}
	uint16_t GetInformation(void) {
		return 0x80A0;
	}
	bool Close() {
		return false;
	}
};

uint16_t cpMap[512] = { // Codepage is standard 437
	0x0020, 0x263a, 0x263b, 0x2665, 0x2666, 0x2663, 0x2660, 0x2219, 0x25d8, 0x25cb, 0x25d9, 0x2642, 0x2640, 0x266a, 0x266b, 0x263c,
	0x25ba, 0x25c4, 0x2195, 0x203c, 0x00b6, 0x00a7, 0x25ac, 0x21a8, 0x2191, 0x2193, 0x2192, 0x2190, 0x221f, 0x2194, 0x25b2, 0x25bc,
	0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
	0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
	0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f,
	0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f,
	0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x2302,
	0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7, 0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
	0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9, 0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192,
	0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba, 0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
	0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,		// 176 - 223 line/box drawing
	0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f, 0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
	0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b, 0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
	0x03b1, 0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4, 0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229,
	0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248, 0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0, 0x0020,

	0x211e, 0x2120, 0x2122, 0x00ae, 0x00a9, 0x00a4, 0x2295, 0x2299, 0x2297, 0x2296, 0x2a38, 0x21d5, 0x21d4, 0x21c4, 0x2196, 0x2198,		// Second bank for WP extended charset
	0x2197, 0x2199, 0x22a4, 0x22a5, 0x22a2, 0x22a3, 0x2213, 0x2243, 0x2260, 0x2262, 0x00b3, 0x00be, 0x0153, 0x0152, 0x05d0, 0x05d1,
	0x0000, 0x2111, 0x211c, 0x2200, 0x2207, 0x29cb, 0x0394, 0x039b, 0x03a0, 0x039e, 0x03a8, 0x05d2, 0x03b3, 0x03b7, 0x03b9, 0x03ba,
	0x03bb, 0x03bd, 0x03c1, 0x03a5, 0x03c9, 0x03be, 0x03b6, 0x02bf, 0x03c5, 0x03c8, 0x03c2, 0x2113, 0x03c7, 0x222e, 0x0178, 0x00cf,
	0x00cb, 0x2227, 0x00c1, 0x0107, 0x0106, 0x01f5, 0x013a, 0x00cd, 0x0139, 0x0144, 0x0143, 0x00d3, 0x0155, 0x0154, 0x015b, 0x015a,
	0x00da, 0x00fd, 0x00dd, 0x017a, 0x0179, 0x00c0, 0x00c8, 0x00cc, 0x00d2, 0x00d9, 0x0151, 0x0150, 0x0171, 0x0170, 0x016f, 0x016e,
	0x010b, 0x010a, 0x0117, 0x0116, 0x0121, 0x0120, 0x0130, 0x017c, 0x017b, 0x00c2, 0x0109, 0x0108, 0x00ca, 0x011d, 0x011c, 0x0125,
	0x0124, 0x00ce, 0x0135, 0x0134, 0x00d4, 0x015d, 0x015c, 0x00db, 0x0175, 0x0174, 0x0177, 0x0176, 0x00e3, 0x00c3, 0x0129, 0x0128,
	0x00f5, 0x00d5, 0x0169, 0x0168, 0x0103, 0x0102, 0x011f, 0x011e, 0x016d, 0x016c, 0x010d, 0x010c, 0x010f, 0x010e, 0x011b, 0x011a,
	0x013e, 0x013d, 0x0148, 0x0147, 0x0159, 0x0158, 0x0161, 0x0160, 0x0165, 0x0164, 0x017e, 0x017d, 0x00f0, 0x0122, 0x0137, 0x0136,
	0x013c, 0x013b, 0x0146, 0x0145, 0x0157, 0x0156, 0x015f, 0x015e, 0x0163, 0x0162, 0x00df, 0x0133, 0x0132, 0x00f8, 0x00d8, 0x2218,
	0x0123, 0x0000, 0x01f4, 0x01e6, 0x2202, 0x0000, 0x0020, 0x2309, 0x2308, 0x230b, 0x230a, 0x23ab, 0x2320, 0x2321, 0x23a9, 0x23a8,
	0x23ac, 0x01e7, 0x2020, 0x2021, 0x201e, 0x222b, 0x222a, 0x2282, 0x2283, 0x2288, 0x2289, 0x2286, 0x2287, 0x220d, 0x2209, 0x2203,
	0x21d1, 0x21d3, 0x21d0, 0x21d2, 0x25a1, 0x2228, 0x22bb, 0x2234, 0x2235, 0x2237, 0x201c, 0x201d, 0x2026, 0x03b8, 0x0101, 0x0113,
	0x0100, 0x0112, 0x012b, 0x012a, 0x014d, 0x014c, 0x016b, 0x016a, 0x0105, 0x0104, 0x0119, 0x0118, 0x012f, 0x012e, 0x0173, 0x0172,
	0x0111, 0x0110, 0x0127, 0x0126, 0x0167, 0x0166, 0x0142, 0x0141, 0x0140, 0x013f, 0x00fe, 0x00de, 0x014a, 0x014b, 0x0149, 0x0000
};

bool lastwrite = false;
uint8_t *clipAscii = NULL;
uint32_t clipSize = 0, cPointer = 0, fPointer;

#if defined(WIN32)
void Unicode2Ascii(const uint16_t* unicode) {
	int memNeeded = WideCharToMultiByte(dos.loaded_codepage, WC_NO_BEST_FIT_CHARS, (LPCWSTR)unicode, -1, NULL, 0, "\x07", NULL);
	if (memNeeded <= 1)																// Includes trailing null
		return;
	if (!(clipAscii = (uint8_t *)malloc(memNeeded)))
		return;
	// Untranslated characters will be set to 0x07 (BEL), and later stripped
	if (WideCharToMultiByte(dos.loaded_codepage, WC_NO_BEST_FIT_CHARS, (LPCWSTR)unicode, -1, (LPSTR)clipAscii, memNeeded, "\x07", NULL) != memNeeded)
		{																			// Can't actually happen of course
		free(clipAscii);
		clipAscii = NULL;
		return;
		}
	memNeeded--;																	// Don't include trailing null
	for (int i = 0; i < memNeeded; i++)
		if (clipAscii[i] > 31 || clipAscii[i] == 9 || clipAscii[i] == 10 || clipAscii[i] == 13)	// Space and up, or TAB, CR/LF allowed (others make no sense when pasting)
			clipAscii[clipSize++] = clipAscii[i];
	return;																			// clipAscii dould be downsized, but of no real interest
}
#else
typedef char host_cnv_char_t;
host_cnv_char_t *CodePageGuestToHost(const char *s);
#endif

bool swapad=true;
extern std::string strPasteBuffer;
void PasteClipboard(bool bPressed);
bool getClipboard() {
	if (clipAscii) {
		free(clipAscii);
		clipAscii = NULL;
	}
#if defined(WIN32)
    clipSize = 0;
    if (OpenClipboard(NULL)) {
        if (HANDLE cbText = GetClipboardData(CF_UNICODETEXT)) {
            uint16_t *unicode = (uint16_t *)GlobalLock(cbText);
            Unicode2Ascii(unicode);
            GlobalUnlock(cbText);
        }
        CloseClipboard();
    }
#else
    swapad=false;
    PasteClipboard(true);
    swapad=true;
    clipSize = 0;
    unsigned int extra = 0;
    unsigned char head, last=13;
    for (int i=0; i<strPasteBuffer.length(); i++) if (strPasteBuffer[i]==10||strPasteBuffer[i]==13) extra++;
	if (clipAscii = (uint8_t *)malloc(strPasteBuffer.length()+extra))
        while (strPasteBuffer.length()) {
            head = strPasteBuffer[0];
            if (head == 10 && last != 13) clipAscii[clipSize++] = 13;
            if (head > 31 || head == 9 || head == 10 || head == 13)
                clipAscii[clipSize++] = head;
            if (head == 13 && (strPasteBuffer.length() < 2 || strPasteBuffer[1] != 10)) clipAscii[clipSize++] = 10;
            strPasteBuffer = strPasteBuffer.substr(1, strPasteBuffer.length());
            last = head;
        }
#endif
    return clipSize != 0;
}

class device_CLIP : public DOS_Device {
private:
	char	tmpAscii[20];
	char	tmpUnicode[20];
	std::string rawdata;				// the raw data sent to CLIP$...
	virtual void CommitData() {
		if (cPointer>0) {
			if (!clipSize)
				getClipboard();
			if (cPointer>clipSize)
				cPointer=clipSize;
			if (clipSize) {
				if (rawdata.capacity() < 100000)
					rawdata.reserve(100000);
				std::string temp=std::string((char *)clipAscii).substr(0, cPointer);
				if (temp[temp.length()-1]!='\n') temp+="\n";
				rawdata.insert(0, temp);
				clipSize=0;
			}
			cPointer=0;
		}
		FILE* fh = fopen(tmpAscii, "wb");			// Append or write to ASCII file
		if (fh)
			{
			fwrite(rawdata.c_str(), rawdata.size(), 1, fh);
			fclose(fh);
			fh = fopen(tmpUnicode, "w+b");			// The same for Unicode file (it's eventually read)
			if (fh)
				{
				fprintf(fh, "\xff\xfe");											// It's a Unicode text file
				for (uint32_t i = 0; i < rawdata.size(); i++)
					{
					uint16_t textChar =  (uint8_t)rawdata[i];
					switch (textChar)
						{
					case 9:																// Tab
					case 12:															// Formfeed
						fwrite(&textChar, 1, 2, fh);
						break;
					case 10:															// Linefeed (combination)
					case 13:
						fwrite("\x0d\x00\x0a\x00", 1, 4, fh);
						if (i < rawdata.size() -1 && textChar == 23-rawdata[i+1])
							i++;
						break;
					default:
						if (textChar >= 32)												// Forget about further control characters?
							fwrite(cpMap+textChar, 1, 2, fh);
						break;
						}
					}
				}
			}
		if (!fh)
			{
			rawdata.clear();
			return;
			}
#if defined(WIN32)
		if (OpenClipboard(NULL))
			{
			if (EmptyClipboard())
				{
				int bytes = ftell(fh);
				HGLOBAL hCbData = GlobalAlloc(NULL, bytes);
				uint8_t* pChData = (uint8_t*)GlobalLock(hCbData);
				if (pChData)
					{
					fseek(fh, 2, SEEK_SET);											// Skip Unicode signature
					fread(pChData, 1, bytes-2, fh);
					pChData[bytes-2] = 0;
					pChData[bytes-1] = 0;
					SetClipboardData(CF_UNICODETEXT, hCbData);
					GlobalUnlock(hCbData);
					}
				}
			CloseClipboard();
			}
#elif defined(C_SDL2) || defined(MACOSX)
            std::ifstream in(tmpAscii);
            std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            std::string result="";
            std::istringstream iss(contents.c_str());
            for (std::string token; std::getline(iss, token); ) {
                char* uname = CodePageGuestToHost(token.c_str());
                result+=(uname!=NULL?std::string(uname):token)+std::string(1, 10);
            }
            if (result.size()&&result.back()==10) result.pop_back();
#if defined(C_SDL2)
            SDL_SetClipboardText(result.c_str());
#else
            bool SetClipboard(std::string value);
            SetClipboard(result);
#endif
#endif
		fclose(fh);
		remove(tmpAscii);
		remove(tmpUnicode);
		rawdata.clear();
		return;
	}
public:
	device_CLIP() {
		SetName(*dos_clipboard_device_name?dos_clipboard_device_name:"CLIP$");
		strcpy(tmpAscii, "#clip$.asc");
		strcpy(tmpUnicode, "#clip$.txt");
	}
	virtual bool Read(uint8_t * data,uint16_t * size) {
		if(control->SecureMode()||!(dos_clipboard_device_access==2||dos_clipboard_device_access==4)) {
			*size = 0;
			return true;
		}
		lastwrite=false;
		if (!clipSize)																// If no data, we have to read the Windows CLipboard (clipSize gets reset on device close)
			{
			getClipboard();
			fPointer = 0;
			}
		if (fPointer >= clipSize)
			*size = 0;
		else if (fPointer+*size > clipSize)
			*size = (uint16_t)(clipSize-fPointer);
		if (*size > 0) {
			memmove(data, clipAscii+fPointer, *size);
			fPointer += *size;
		}
		return true;
	}
	virtual bool Write(const uint8_t * data,uint16_t * size) {
		if(control->SecureMode()||!(dos_clipboard_device_access==3||dos_clipboard_device_access==4)) {
			DOS_SetError(DOSERR_ACCESS_DENIED);
			return false;
		}
		lastwrite=true;
        const uint8_t* datasrc = (uint8_t*)data;
		uint8_t * datadst = (uint8_t *) data;

		int numSpaces = 0;
		for (uint16_t idx = *size; idx; idx--)
			{
			if (*datasrc == ' ')														// Put spaces on hold
				numSpaces++;
			else
				{
				if (numSpaces && *datasrc != 0x0a && *datasrc != 0x0d)					// Spaces on hold and not end of line
					while (numSpaces--)
						*(datadst++) = ' ';
				numSpaces = 0;
				*(datadst++) = *datasrc;
				}
			datasrc++;
			}
		while (numSpaces--)
			*(datadst++) = ' ';
		if (uint16_t newsize = (uint16_t)(datadst - data))									// If data
			{
			if (rawdata.capacity() < 100000)											// Prevent repetive size allocations
				rawdata.reserve(100000);
			rawdata.append((char *)data, newsize);
			}
		return true;
	}
	virtual bool Seek(uint32_t * pos,uint32_t type) {
		if(control->SecureMode()||!(dos_clipboard_device_access==2||dos_clipboard_device_access==4)) {
			*pos = 0;
			return true;
		}
		lastwrite=false;
		if (clipSize == 0)																// No data yet
			{
			getClipboard();
			fPointer =0;
			}
		int32_t newPos;
		switch (type)
			{
		case 0:																			// Start of file
			newPos = *pos;
			break;
		case 1:																			// Current file position
			newPos = fPointer+*pos;
			break;
		case 2:																			// End of file
			newPos = clipSize+*pos;
			break;
		default:
			{
			DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
			return false;
			}
			}
		if (newPos > (int32_t)clipSize)													// Different from "real" Files
			newPos = clipSize;
		else if (newPos < 0)
			newPos = 0;
		*pos = newPos;
		cPointer = newPos;
		fPointer = newPos;
		return true;
	}
	virtual bool Close() {
		if(control->SecureMode()||dos_clipboard_device_access<2)
			return false;
		clipSize = 0;																	// Reset clipboard read
		rawdata.erase(rawdata.find_last_not_of(" \n\r\t")+1);							// Remove trailing white space
		if (!rawdata.size()&&!lastwrite)												// Nothing captured/to do
			return false;
		lastwrite=false;
		int len = (int)rawdata.size();
		if (len > 2 && rawdata[len-3] == 0x0c && rawdata[len-2] == 27 && rawdata[len-1] == 64)	// <ESC>@ after last FF?
			rawdata.erase(len-2, 2);
		CommitData();
		return true;
	}
	uint16_t GetInformation(void) {
		return 0x80E0;
	}
};

bool DOS_Device::Read(uint8_t * data,uint16_t * size) {
	return Devices[devnum]->Read(data,size);
}

bool DOS_Device::Write(const uint8_t * data,uint16_t * size) {
	return Devices[devnum]->Write(data,size);
}

bool DOS_Device::Seek(uint32_t * pos,uint32_t type) {
	return Devices[devnum]->Seek(pos,type);
}

bool DOS_Device::Close() {
	return Devices[devnum]->Close();
}

uint16_t DOS_Device::GetInformation(void) { 
	return Devices[devnum]->GetInformation();
}

bool DOS_Device::ReadFromControlChannel(PhysPt bufptr,uint16_t size,uint16_t * retcode) { 
	return Devices[devnum]->ReadFromControlChannel(bufptr,size,retcode);
}

bool DOS_Device::WriteToControlChannel(PhysPt bufptr,uint16_t size,uint16_t * retcode) { 
	return Devices[devnum]->WriteToControlChannel(bufptr,size,retcode);
}

DOS_File::DOS_File(const DOS_File& orig) : flags(orig.flags), open(orig.open), attr(orig.attr),
time(orig.time), date(orig.date), refCtr(orig.refCtr), hdrive(orig.hdrive) {
	if(orig.name) {
		name=new char [strlen(orig.name) + 1];strcpy(name,orig.name);
	}
}

DOS_File& DOS_File::operator= (const DOS_File& orig) {
    if (this != &orig) {
        flags = orig.flags;
        time = orig.time;
        date = orig.date;
        attr = orig.attr;
        refCtr = orig.refCtr;
        open = orig.open;
        hdrive = orig.hdrive;
        drive = orig.drive;
        newtime = orig.newtime;
        if (name) {
            delete[] name; name = 0;
        }
        if (orig.name) {
            name = new char[strlen(orig.name) + 1]; strcpy(name, orig.name);
        }
    }
    return *this;
}

uint8_t DOS_FindDevice(char const * name) {
	/* should only check for the names before the dot and spacepadded */
	char fullname[DOS_PATHLENGTH];uint8_t drive;
//	if(!name || !(*name)) return DOS_DEVICES; //important, but makename does it
	if (!DOS_MakeName(name,fullname,&drive)) return DOS_DEVICES;

	char* name_part = strrchr(fullname,'\\');
	if(name_part) {
		*name_part++ = 0;
		//Check validity of leading directory.
		if(!Drives[drive]->TestDir(fullname)) return DOS_DEVICES;
	} else name_part = fullname;
   
	char* dot = strrchr(name_part,'.');
	if(dot) *dot = 0; //no ext checking

	static char com[5] = { 'C','O','M','1',0 };
	static char lpt[5] = { 'L','P','T','1',0 };
	// AUX is alias for COM1 and PRN for LPT1
	// A bit of a hack. (but less then before).
	// no need for casecmp as makename returns uppercase
	if (strcmp(name_part, "AUX") == 0) name_part = com;
	if (strcmp(name_part, "PRN") == 0) name_part = lpt;

	/* loop through devices */
	for(uint8_t index = 0;index < DOS_DEVICES;index++) {
		if (Devices[index]) {
			if (WildFileCmp(name_part,Devices[index]->name)) return index;
		}
	}
	return DOS_DEVICES;
}


void DOS_AddDevice(DOS_Device * adddev) {
//Caller creates the device. We store a pointer to it
//TODO Give the Device a real handler in low memory that responds to calls
	if (adddev == NULL) E_Exit("DOS_AddDevice with null ptr");
	for(Bitu i = 0; i < DOS_DEVICES;i++) {
		if (Devices[i] == NULL){
//			LOG_MSG("DOS_AddDevice %s (%p)\n",adddev->name,(void*)adddev);
			Devices[i] = adddev;
			Devices[i]->SetDeviceNumber(i);
			return;
		}
	}
	E_Exit("DOS:Too many devices added");
}

void DOS_DelDevice(DOS_Device * dev) {
// We will destroy the device if we find it in our list.
// TODO:The file table is not checked to see the device is opened somewhere!
	if (dev == NULL) return E_Exit("DOS_DelDevice with null ptr");
	for (Bitu i = 0; i <DOS_DEVICES;i++) {
		if (Devices[i] == dev) { /* NTS: The mainline code deleted by matching names??? Why? */
//			LOG_MSG("DOS_DelDevice %s (%p)\n",dev->name,(void*)dev);
			delete Devices[i];
			Devices[i] = 0;
			return;
		}
	}

	/* hm. unfortunately, too much code in DOSBox assumes that we delete the object.
	 * prior to this fix, failure to delete caused a memory leak */
	LOG_MSG("WARNING: DOS_DelDevice() failed to match device object '%s' (%p). Deleting anyway\n",dev->name,(void*)dev);
	delete dev;
}

void DOS_ShutdownDevices(void) {
	for (Bitu i=0;i < DOS_DEVICES;i++) {
		if (Devices[i] != NULL) {
//			LOG_MSG("DOS: Shutting down device %s (%p)\n",Devices[i]->name,(void*)Devices[i]);
			delete Devices[i];
			Devices[i] = NULL;
		}
	}

    /* NTS: CON counts as a device */
    if (IS_PC98_ARCH) update_pc98_function_row(0);
}

// INT 29h emulation needs to keep track of CON
device_CON *DOS_CON = NULL;

bool ANSI_SYS_installed() {
    if (DOS_CON != NULL)
        return DOS_CON->ANSI_SYS_installed();

    return false;
}

void DOS_SetupDevices(void) {
	DOS_Device * newdev;
	DOS_CON=new device_CON(); newdev=DOS_CON;
	DOS_AddDevice(newdev);
	DOS_Device * newdev2;
	newdev2=new device_NUL();
	DOS_AddDevice(newdev2);
	DOS_Device * newdev3;
	newdev3=new device_PRN();
	DOS_AddDevice(newdev3);
	if (dos_clipboard_device_access) {
		DOS_Device * newdev4;
		newdev4=new device_CLIP();
		DOS_AddDevice(newdev4);
	}
}

/* PC-98 INT DC CL=0x10 AH=0x00 DL=cjar */
void PC98_INTDC_WriteChar(unsigned char b) {
    if (DOS_CON != NULL) {
        uint16_t sz = 1;

        DOS_CON->Write(&b,&sz);
    }
}

void INTDC_CL10h_AH03h(uint16_t raw) {
    if (DOS_CON != NULL)
        DOS_CON->INTDC_CL10h_AH03h(raw);
}

void INTDC_CL10h_AH04h(void) {
    if (DOS_CON != NULL)
        DOS_CON->INTDC_CL10h_AH04h();
}

void INTDC_CL10h_AH05h(void) {
    if (DOS_CON != NULL)
        DOS_CON->INTDC_CL10h_AH05h();
}

void INTDC_CL10h_AH06h(uint16_t count) {
    if (DOS_CON != NULL)
        DOS_CON->INTDC_CL10h_AH06h(count);
}

void INTDC_CL10h_AH07h(uint16_t count) {
    if (DOS_CON != NULL)
        DOS_CON->INTDC_CL10h_AH07h(count);
}

void INTDC_CL10h_AH08h(uint16_t count) {
    if (DOS_CON != NULL)
        DOS_CON->INTDC_CL10h_AH08h(count);
}

void INTDC_CL10h_AH09h(uint16_t count) {
    if (DOS_CON != NULL)
        DOS_CON->INTDC_CL10h_AH09h(count);
}

Bitu INT29_HANDLER(void) {
    if (DOS_CON != NULL) {
        unsigned char b = reg_al;
        uint16_t sz = 1;

        DOS_CON->Write(&b,&sz);
    }

    return CBRET_NONE;
}

extern bool dos_kernel_disabled;

// save state support
void POD_Save_DOS_Devices( std::ostream& stream )
{
	if (!dos_kernel_disabled) {
		if( strcmp( Devices[2]->GetName(), "CON" ) == 0 )
			Devices[2]->SaveState(stream);
	}
}

void POD_Load_DOS_Devices( std::istream& stream )
{
	if (!dos_kernel_disabled) {
		if( strcmp( Devices[2]->GetName(), "CON" ) == 0 )
			Devices[2]->LoadState(stream, false);
	}
}
