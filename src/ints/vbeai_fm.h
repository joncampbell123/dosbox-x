/*
 *  MIDI-to-OPL synthesiser for the DOSBox-X VBE/AI provider.
 *
 *  See docs/vbeai.md. VBE/AI models an FM chip as a MIDI device whose driver
 *  interprets the stream rather than forwarding it, so this is the other half
 *  of the MIDI device class: a voice allocator and register programmer sitting
 *  behind msMIDImsg.
 */

#ifndef DOSBOX_VBEAI_FM_H
#define DOSBOX_VBEAI_FM_H

#include "dosbox.h"

/* Registered VBE/AI patch types (spec 5.4 / 7.2). */
#define VBEAI_PATCH_OPL2        0x0010
#define VBEAI_PATCH_OPL3        0x0011

/* Create the chip and its mixer channel. opl3 selects 18 two-operator voices
 * instead of 9. Safe to call repeatedly; returns false if the mixer refused. */
bool     VBEAI_FM_Init(bool opl3);
void     VBEAI_FM_ShutDown(void);

/* Silence everything and restore the default bank. */
void     VBEAI_FM_Reset(void);

/* Feed one MIDI byte. Running status, sysex and realtime bytes are handled
 * here, because unlike the transmitter path there is no MIDI layer below us
 * to do it. */
void     VBEAI_FM_Byte(uint8_t b);

/* msPreLoadPatch: install a patch for one GM program. Returns false if the
 * patch type is not one we understand or the block is malformed. */
bool     VBEAI_FM_LoadPatch(uint16_t type, uint16_t program, PhysPt data, uint32_t len);

/* msUnloadPatch: put the default patch back -- the one from the loaded bank
 * if there is one, otherwise the built-in. */
void     VBEAI_FM_UnloadPatch(uint16_t program);

/* Replace the default bank from a file. Understands DMX GENMIDI ("#OPL_II#")
 * and Ad Lib .BNK ("ADLIB-"), telling them apart by their signature. Returns
 * false and leaves the previous bank in place if the file cannot be used. */
bool     VBEAI_FM_LoadBank(const char *path);

/* Short description of the bank in use, for logging. */
const char *VBEAI_FM_BankName(void);

/* MIDITONES wants the count of tones *not* currently in use. */
unsigned VBEAI_FM_FreeVoices(void);
unsigned VBEAI_FM_TotalVoices(void);

/* MIDIVOICESTEAL, as a 16-bit per-channel mask; bit 0 is MIDI channel 0. */
void     VBEAI_FM_SetVoiceSteal(uint16_t mask);
uint16_t VBEAI_FM_GetVoiceSteal(void);

/* True once Init has succeeded. */
bool     VBEAI_FM_Active(void);

#endif
