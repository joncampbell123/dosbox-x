#include "dosbox.h"

#ifndef DOSBOX_OUTPUT_TTF_H
#define DOSBOX_OUTPUT_TTF_H

// output API
Bitu OUTPUT_TTF_SetSize();
void OUTPUT_TTF_Select(int fsize=-1);
void TTF_IncreaseSize(bool pressed);
void TTF_DecreaseSize(bool pressed);
void ResetTTFSize(Bitu /*val*/);
void ttf_setlines(int cols, int lins);
void GFX_EndTextLines(bool force=false);
void UpdateDefaultPrinterFont(void);
void ttf_reset(void);
int setTTFCodePage(void);
bool TTF_using(void);
bool setColors(const char *colorArray, int n);

extern int wpType, lastfontsize, blinkCursor;
extern bool firstset, printfont, lastmenu, showbold, showital, showline, showsout, resetreq, dbcs_sbcs, autoboxdraw, halfwidthkana, enable_dbcs_tables;

#endif /*DOSBOX_OUTPUT_TTF_H*/