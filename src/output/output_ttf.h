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
bool ttf_blinking_cursor_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem);
bool ttf_right_left_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem);
bool ttf_dbcs_sbcs_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem);
bool ttf_auto_boxdraw_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem);
bool ttf_halfwidth_katakana_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem);
bool ttf_extend_charset_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem);
bool ttf_print_font_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem);
bool ttf_reset_colors_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem);
bool ttf_style_change_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem);
bool ttf_wp_change_callback(DOSBoxMenu * const menu,DOSBoxMenu::item * const menuitem);

extern int wpType, lastfontsize, blinkCursor;
extern bool firstset, printfont, lastmenu, showbold, showital, showline, showsout, resetreq, dbcs_sbcs, autoboxdraw, halfwidthkana, enable_dbcs_tables;

#endif /*DOSBOX_OUTPUT_TTF_H*/