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


#ifndef DOSBOX_VOODOO_H
#define DOSBOX_VOODOO_H


#define VOODOO_INITIAL_LFB	0xd0000000
#define VOODOO_LFB_SIZE		0x01000000 /* VOODOO_PAGES == 4096   (4096 << 12) 16MB size must be a power of 2 */
#define VOODOO_LFB_ALIGNMASK	( ~(VOODOO_LFB_SIZE - 1u) ) /* 16MB align */
#define VOODOO_REG_PAGES	1024
#define VOODOO_LFB_PAGES	1024
#define VOODOO_TEX_PAGES	2048
#define VOODOO_PAGES		(VOODOO_REG_PAGES+VOODOO_LFB_PAGES+VOODOO_TEX_PAGES)

static_assert( VOODOO_LFB_ALIGNMASK == 0xFF000000ul, "Voodoo LFB error" );

#define VOODOO_EMU_TYPE_OFF		0
#define VOODOO_EMU_TYPE_SOFTWARE	1
#define VOODOO_EMU_TYPE_ACCELERATED	2	/* OpenGL */
#define VOODOO_EMU_TYPE_VULKAN		3
#define VOODOO_EMU_TYPE_AUTO		4	/* OpenGL, Vulkan, Direct3D or Direct3D 11, for the output in use */
#define VOODOO_EMU_TYPE_DIRECT3D	5
#define VOODOO_EMU_TYPE_DIRECT3D11	6

/* what the hardware emulation renders with */
#define VOODOO_HW_API_OPENGL		0
#define VOODOO_HW_API_VULKAN		1
#define VOODOO_HW_API_DIRECT3D		2
#define VOODOO_HW_API_DIRECT3D11	3


void VOODOO_PCI_InitEnable(Bitu val);
void VOODOO_PCI_Enable(bool enable);
void VOODOO_PCI_SetLFB(uint32_t lfbaddr);
bool VOODOO_PCI_CheckLFBPage(Bitu page);
bool Voodoo_OGL_Active();

/* render resolution of the OpenGL 3dfx emulation ([voodoo] voodoo_resolution) */
#define VOODOO_OGL_RES_NATIVE		0	/* the 3dfx video mode's */
#define VOODOO_OGL_RES_DOUBLE		1	/* twice the 3dfx video mode's, both ways */
#define VOODOO_OGL_RES_DISPLAY		2	/* the size it is shown at on the screen */
void Voodoo_OGL_SetResolutionMode(int mode);
int Voodoo_OGL_GetResolutionMode(void);
int Voodoo_OGL_ParseResolutionMode(const char *s);
void Voodoo_OGL_SetDisplaySize(unsigned int w, unsigned int h);
void Voodoo_Output_Enable(bool enabled);
PageHandler* VOODOO_GetPageHandler();

#endif
