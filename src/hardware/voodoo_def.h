/***************************************************************************/
/*        Portion of this software comes with the following license:       */
/***************************************************************************/
/*

    Copyright Aaron Giles
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

        * Redistributions of source code must retain the above copyright
          notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
          notice, this list of conditions and the following disclaimer in
          the documentation and/or other materials provided with the
          distribution.
        * Neither the name 'MAME' nor the names of its contributors may be
          used to endorse or promote products derived from this software
          without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY AARON GILES ''AS IS'' AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL AARON GILES BE LIABLE FOR ANY DIRECT,
    INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
    STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
    IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

/*************************************************************************

    3dfx Voodoo Graphics SST-1/2 emulator

    emulator by Aaron Giles

**************************************************************************/


#ifndef DOSBOX_VOODOO_DEF_H
#define DOSBOX_VOODOO_DEF_H


/*************************************
 *
 *  Register constants
 *
 *************************************/

/* Codes to the right:
    R = readable
    W = writeable
    P = pipelined
    F = goes to FIFO
*/

/* 0x000 */
#define status			(0x000/4)	/* R  P  */
#define intrCtrl		(0x004/4)	/* RW P   -- Voodoo2/Banshee only */
#define vertexAx		(0x008/4)	/*  W PF */
#define vertexAy		(0x00c/4)	/*  W PF */
#define vertexBx		(0x010/4)	/*  W PF */
#define vertexBy		(0x014/4)	/*  W PF */
#define vertexCx		(0x018/4)	/*  W PF */
#define vertexCy		(0x01c/4)	/*  W PF */
#define startR			(0x020/4)	/*  W PF */
#define startG			(0x024/4)	/*  W PF */
#define startB			(0x028/4)	/*  W PF */
#define startZ			(0x02c/4)	/*  W PF */
#define startA			(0x030/4)	/*  W PF */
#define startS			(0x034/4)	/*  W PF */
#define startT			(0x038/4)	/*  W PF */
#define startW			(0x03c/4)	/*  W PF */

/* 0x040 */
#define dRdX			(0x040/4)	/*  W PF */
#define dGdX			(0x044/4)	/*  W PF */
#define dBdX			(0x048/4)	/*  W PF */
#define dZdX			(0x04c/4)	/*  W PF */
#define dAdX			(0x050/4)	/*  W PF */
#define dSdX			(0x054/4)	/*  W PF */
#define dTdX			(0x058/4)	/*  W PF */
#define dWdX			(0x05c/4)	/*  W PF */
#define dRdY			(0x060/4)	/*  W PF */
#define dGdY			(0x064/4)	/*  W PF */
#define dBdY			(0x068/4)	/*  W PF */
#define dZdY			(0x06c/4)	/*  W PF */
#define dAdY			(0x070/4)	/*  W PF */
#define dSdY			(0x074/4)	/*  W PF */
#define dTdY			(0x078/4)	/*  W PF */
#define dWdY			(0x07c/4)	/*  W PF */

/* 0x080 */
#define triangleCMD		(0x080/4)	/*  W PF */
#define fvertexAx		(0x088/4)	/*  W PF */
#define fvertexAy		(0x08c/4)	/*  W PF */
#define fvertexBx		(0x090/4)	/*  W PF */
#define fvertexBy		(0x094/4)	/*  W PF */
#define fvertexCx		(0x098/4)	/*  W PF */
#define fvertexCy		(0x09c/4)	/*  W PF */
#define fstartR			(0x0a0/4)	/*  W PF */
#define fstartG			(0x0a4/4)	/*  W PF */
#define fstartB			(0x0a8/4)	/*  W PF */
#define fstartZ			(0x0ac/4)	/*  W PF */
#define fstartA			(0x0b0/4)	/*  W PF */
#define fstartS			(0x0b4/4)	/*  W PF */
#define fstartT			(0x0b8/4)	/*  W PF */
#define fstartW			(0x0bc/4)	/*  W PF */

/* 0x0c0 */
#define fdRdX			(0x0c0/4)	/*  W PF */
#define fdGdX			(0x0c4/4)	/*  W PF */
#define fdBdX			(0x0c8/4)	/*  W PF */
#define fdZdX			(0x0cc/4)	/*  W PF */
#define fdAdX			(0x0d0/4)	/*  W PF */
#define fdSdX			(0x0d4/4)	/*  W PF */
#define fdTdX			(0x0d8/4)	/*  W PF */
#define fdWdX			(0x0dc/4)	/*  W PF */
#define fdRdY			(0x0e0/4)	/*  W PF */
#define fdGdY			(0x0e4/4)	/*  W PF */
#define fdBdY			(0x0e8/4)	/*  W PF */
#define fdZdY			(0x0ec/4)	/*  W PF */
#define fdAdY			(0x0f0/4)	/*  W PF */
#define fdSdY			(0x0f4/4)	/*  W PF */
#define fdTdY			(0x0f8/4)	/*  W PF */
#define fdWdY			(0x0fc/4)	/*  W PF */

/* 0x100 */
#define ftriangleCMD	(0x100/4)	/*  W PF */
#define fbzColorPath	(0x104/4)	/* RW PF */
#define fogMode			(0x108/4)	/* RW PF */
#define alphaMode		(0x10c/4)	/* RW PF */
#define fbzMode			(0x110/4)	/* RW  F */
#define lfbMode			(0x114/4)	/* RW  F */
#define clipLeftRight	(0x118/4)	/* RW  F */
#define clipLowYHighY	(0x11c/4)	/* RW  F */
#define nopCMD			(0x120/4)	/*  W  F */
#define fastfillCMD		(0x124/4)	/*  W  F */
#define swapbufferCMD	(0x128/4)	/*  W  F */
#define fogColor		(0x12c/4)	/*  W  F */
#define zaColor			(0x130/4)	/*  W  F */
#define chromaKey		(0x134/4)	/*  W  F */
#define chromaRange		(0x138/4)	/*  W  F  -- Voodoo2/Banshee only */
#define userIntrCMD		(0x13c/4)	/*  W  F  -- Voodoo2/Banshee only */

/* 0x140 */
#define stipple			(0x140/4)	/* RW  F */
#define color0			(0x144/4)	/* RW  F */
#define color1			(0x148/4)	/* RW  F */
#define fbiPixelsIn		(0x14c/4)	/* R     */
#define fbiChromaFail	(0x150/4)	/* R     */
#define fbiZfuncFail	(0x154/4)	/* R     */
#define fbiAfuncFail	(0x158/4)	/* R     */
#define fbiPixelsOut	(0x15c/4)	/* R     */
#define fogTable		(0x160/4)	/*  W  F */

/* 0x1c0 */
#define cmdFifoBaseAddr	(0x1e0/4)	/* RW     -- Voodoo2 only */
#define cmdFifoBump		(0x1e4/4)	/* RW     -- Voodoo2 only */
#define cmdFifoRdPtr	(0x1e8/4)	/* RW     -- Voodoo2 only */
#define cmdFifoAMin		(0x1ec/4)	/* RW     -- Voodoo2 only */
#define colBufferAddr	(0x1ec/4)	/* RW     -- Banshee only */
#define cmdFifoAMax		(0x1f0/4)	/* RW     -- Voodoo2 only */
#define colBufferStride	(0x1f0/4)	/* RW     -- Banshee only */
#define cmdFifoDepth	(0x1f4/4)	/* RW     -- Voodoo2 only */
#define auxBufferAddr	(0x1f4/4)	/* RW     -- Banshee only */
#define cmdFifoHoles	(0x1f8/4)	/* RW     -- Voodoo2 only */
#define auxBufferStride	(0x1f8/4)	/* RW     -- Banshee only */

/* 0x200 */
#define fbiInit4		(0x200/4)	/* RW     -- Voodoo/Voodoo2 only */
#define clipLeftRight1	(0x200/4)	/* RW     -- Banshee only */
#define vRetrace		(0x204/4)	/* R      -- Voodoo/Voodoo2 only */
#define clipTopBottom1	(0x204/4)	/* RW     -- Banshee only */
#define backPorch		(0x208/4)	/* RW     -- Voodoo/Voodoo2 only */
#define videoDimensions	(0x20c/4)	/* RW     -- Voodoo/Voodoo2 only */
#define fbiInit0		(0x210/4)	/* RW     -- Voodoo/Voodoo2 only */
#define fbiInit1		(0x214/4)	/* RW     -- Voodoo/Voodoo2 only */
#define fbiInit2		(0x218/4)	/* RW     -- Voodoo/Voodoo2 only */
#define fbiInit3		(0x21c/4)	/* RW     -- Voodoo/Voodoo2 only */
#define hSync			(0x220/4)	/*  W     -- Voodoo/Voodoo2 only */
#define vSync			(0x224/4)	/*  W     -- Voodoo/Voodoo2 only */
#define clutData		(0x228/4)	/*  W  F  -- Voodoo/Voodoo2 only */
#define dacData			(0x22c/4)	/*  W     -- Voodoo/Voodoo2 only */
#define maxRgbDelta		(0x230/4)	/*  W     -- Voodoo/Voodoo2 only */
#define hBorder			(0x234/4)	/*  W     -- Voodoo2 only */
#define vBorder			(0x238/4)	/*  W     -- Voodoo2 only */
#define borderColor		(0x23c/4)	/*  W     -- Voodoo2 only */

/* 0x240 */
#define hvRetrace		(0x240/4)	/* R      -- Voodoo2 only */
#define fbiInit5		(0x244/4)	/* RW     -- Voodoo2 only */
#define fbiInit6		(0x248/4)	/* RW     -- Voodoo2 only */
#define fbiInit7		(0x24c/4)	/* RW     -- Voodoo2 only */
#define swapPending		(0x24c/4)	/*  W     -- Banshee only */
#define leftOverlayBuf	(0x250/4)	/*  W     -- Banshee only */
#define rightOverlayBuf	(0x254/4)	/*  W     -- Banshee only */
#define fbiSwapHistory	(0x258/4)	/* R      -- Voodoo2/Banshee only */
#define fbiTrianglesOut	(0x25c/4)	/* R      -- Voodoo2/Banshee only */
#define sSetupMode		(0x260/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sVx				(0x264/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sVy				(0x268/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sARGB			(0x26c/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sRed			(0x270/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sGreen			(0x274/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sBlue			(0x278/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sAlpha			(0x27c/4)	/*  W PF  -- Voodoo2/Banshee only */

/* 0x280 */
#define sVz				(0x280/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sWb				(0x284/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sWtmu0			(0x288/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sS_W0			(0x28c/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sT_W0			(0x290/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sWtmu1			(0x294/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sS_Wtmu1		(0x298/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sT_Wtmu1		(0x29c/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sDrawTriCMD		(0x2a0/4)	/*  W PF  -- Voodoo2/Banshee only */
#define sBeginTriCMD	(0x2a4/4)	/*  W PF  -- Voodoo2/Banshee only */

/* 0x2c0 */
#define bltSrcBaseAddr	(0x2c0/4)	/* RW PF  -- Voodoo2 only */
#define bltDstBaseAddr	(0x2c4/4)	/* RW PF  -- Voodoo2 only */
#define bltXYStrides	(0x2c8/4)	/* RW PF  -- Voodoo2 only */
#define bltSrcChromaRange (0x2cc/4)	/* RW PF  -- Voodoo2 only */
#define bltDstChromaRange (0x2d0/4)	/* RW PF  -- Voodoo2 only */
#define bltClipX		(0x2d4/4)	/* RW PF  -- Voodoo2 only */
#define bltClipY		(0x2d8/4)	/* RW PF  -- Voodoo2 only */
#define bltSrcXY		(0x2e0/4)	/* RW PF  -- Voodoo2 only */
#define bltDstXY		(0x2e4/4)	/* RW PF  -- Voodoo2 only */
#define bltSize			(0x2e8/4)	/* RW PF  -- Voodoo2 only */
#define bltRop			(0x2ec/4)	/* RW PF  -- Voodoo2 only */
#define bltColor		(0x2f0/4)	/* RW PF  -- Voodoo2 only */
#define bltCommand		(0x2f8/4)	/* RW PF  -- Voodoo2 only */
#define bltData			(0x2fc/4)	/*  W PF  -- Voodoo2 only */

/* 0x300 */
#define textureMode		(0x300/4)	/*  W PF */
#define tLOD			(0x304/4)	/*  W PF */
#define tDetail			(0x308/4)	/*  W PF */
#define texBaseAddr		(0x30c/4)	/*  W PF */
#define texBaseAddr_1	(0x310/4)	/*  W PF */
#define texBaseAddr_2	(0x314/4)	/*  W PF */
#define texBaseAddr_3_8	(0x318/4)	/*  W PF */
#define trexInit0		(0x31c/4)	/*  W  F  -- Voodoo/Voodoo2 only */
#define trexInit1		(0x320/4)	/*  W  F */
#define nccTable		(0x324/4)	/*  W  F */



/*************************************
 *
 *  Alias map of the first 64
 *  registers when remapped
 *
 *************************************/

static const UINT8 register_alias_map[0x40] =
{
	status,		0x004/4,	vertexAx,	vertexAy,
	vertexBx,	vertexBy,	vertexCx,	vertexCy,
	startR,		dRdX,		dRdY,		startG,
	dGdX,		dGdY,		startB,		dBdX,
	dBdY,		startZ,		dZdX,		dZdY,
	startA,		dAdX,		dAdY,		startS,
	dSdX,		dSdY,		startT,		dTdX,
	dTdY,		startW,		dWdX,		dWdY,

	triangleCMD,0x084/4,	fvertexAx,	fvertexAy,
	fvertexBx,	fvertexBy,	fvertexCx,	fvertexCy,
	fstartR,	fdRdX,		fdRdY,		fstartG,
	fdGdX,		fdGdY,		fstartB,	fdBdX,
	fdBdY,		fstartZ,	fdZdX,		fdZdY,
	fstartA,	fdAdX,		fdAdY,		fstartS,
	fdSdX,		fdSdY,		fstartT,	fdTdX,
	fdTdY,		fstartW,	fdWdX,		fdWdY
};



/*************************************
 *
 *  Table of per-register access rights
 *
 *************************************/

static const UINT8 voodoo_register_access[0x100] =
{
	/* 0x000 */
	REG_RP,		0,			REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,

	/* 0x040 */
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,

	/* 0x080 */
	REG_WPF,	0,			REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,

	/* 0x0c0 */
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,

	/* 0x100 */
	REG_WPF,	REG_RWPF,	REG_RWPF,	REG_RWPF,
	REG_RWF,	REG_RWF,	REG_RWF,	REG_RWF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		0,			0,

	/* 0x140 */
	REG_RWF,	REG_RWF,	REG_RWF,	REG_R,
	REG_R,		REG_R,		REG_R,		REG_R,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,

	/* 0x180 */
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,

	/* 0x1c0 */
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	0,			0,			0,			0,
	0,			0,			0,			0,

	/* 0x200 */
	REG_RW,		REG_R,		REG_RW,		REG_RW,
	REG_RW,		REG_RW,		REG_RW,		REG_RW,
	REG_W,		REG_W,		REG_W,		REG_W,
	REG_W,		0,			0,			0,

	/* 0x240 */
	0,			0,			0,			0,
	0,			0,			0,			0,
	0,			0,			0,			0,
	0,			0,			0,			0,

	/* 0x280 */
	0,			0,			0,			0,
	0,			0,			0,			0,
	0,			0,			0,			0,
	0,			0,			0,			0,

	/* 0x2c0 */
	0,			0,			0,			0,
	0,			0,			0,			0,
	0,			0,			0,			0,
	0,			0,			0,			0,

	/* 0x300 */
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,

	/* 0x340 */
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,

	/* 0x380 */
	REG_WF
};


static const UINT8 voodoo2_register_access[0x100] =
{
	/* 0x000 */
	REG_RP,		REG_RWPT,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,

	/* 0x040 */
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,

	/* 0x080 */
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,

	/* 0x0c0 */
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,

	/* 0x100 */
	REG_WPF,	REG_RWPF,	REG_RWPF,	REG_RWPF,
	REG_RWF,	REG_RWF,	REG_RWF,	REG_RWF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,

	/* 0x140 */
	REG_RWF,	REG_RWF,	REG_RWF,	REG_R,
	REG_R,		REG_R,		REG_R,		REG_R,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,

	/* 0x180 */
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,

	/* 0x1c0 */
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_RWT,	REG_RWT,	REG_RWT,	REG_RWT,
	REG_RWT,	REG_RWT,	REG_RWT,	REG_RW,

	/* 0x200 */
	REG_RWT,	REG_R,		REG_RWT,	REG_RWT,
	REG_RWT,	REG_RWT,	REG_RWT,	REG_RWT,
	REG_WT,		REG_WT,		REG_WF,		REG_WT,
	REG_WT,		REG_WT,		REG_WT,		REG_WT,

	/* 0x240 */
	REG_R,		REG_RWT,	REG_RWT,	REG_RWT,
	0,			0,			REG_R,		REG_R,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,

	/* 0x280 */
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	0,			0,
	0,			0,			0,			0,

	/* 0x2c0 */
	REG_RWPF,	REG_RWPF,	REG_RWPF,	REG_RWPF,
	REG_RWPF,	REG_RWPF,	REG_RWPF,	REG_RWPF,
	REG_RWPF,	REG_RWPF,	REG_RWPF,	REG_RWPF,
	REG_RWPF,	REG_RWPF,	REG_RWPF,	REG_WPF,

	/* 0x300 */
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WPF,
	REG_WPF,	REG_WPF,	REG_WPF,	REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,

	/* 0x340 */
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,
	REG_WF,		REG_WF,		REG_WF,		REG_WF,

	/* 0x380 */
	REG_WF
};



/*************************************
 *
 *  Register string table for debug
 *
 *************************************/

static const char *const voodoo_reg_name[] =
{
	/* 0x000 */
	"status",		"{intrCtrl}",	"vertexAx",		"vertexAy",
	"vertexBx",		"vertexBy",		"vertexCx",		"vertexCy",
	"startR",		"startG",		"startB",		"startZ",
	"startA",		"startS",		"startT",		"startW",
	/* 0x040 */
	"dRdX",			"dGdX",			"dBdX",			"dZdX",
	"dAdX",			"dSdX",			"dTdX",			"dWdX",
	"dRdY",			"dGdY",			"dBdY",			"dZdY",
	"dAdY",			"dSdY",			"dTdY",			"dWdY",
	/* 0x080 */
	"triangleCMD",	"reserved084",	"fvertexAx",	"fvertexAy",
	"fvertexBx",	"fvertexBy",	"fvertexCx",	"fvertexCy",
	"fstartR",		"fstartG",		"fstartB",		"fstartZ",
	"fstartA",		"fstartS",		"fstartT",		"fstartW",
	/* 0x0c0 */
	"fdRdX",		"fdGdX",		"fdBdX",		"fdZdX",
	"fdAdX",		"fdSdX",		"fdTdX",		"fdWdX",
	"fdRdY",		"fdGdY",		"fdBdY",		"fdZdY",
	"fdAdY",		"fdSdY",		"fdTdY",		"fdWdY",
	/* 0x100 */
	"ftriangleCMD",	"fbzColorPath",	"fogMode",		"alphaMode",
	"fbzMode",		"lfbMode",		"clipLeftRight","clipLowYHighY",
	"nopCMD",		"fastfillCMD",	"swapbufferCMD","fogColor",
	"zaColor",		"chromaKey",	"{chromaRange}","{userIntrCMD}",
	/* 0x140 */
	"stipple",		"color0",		"color1",		"fbiPixelsIn",
	"fbiChromaFail","fbiZfuncFail",	"fbiAfuncFail",	"fbiPixelsOut",
	"fogTable160",	"fogTable164",	"fogTable168",	"fogTable16c",
	"fogTable170",	"fogTable174",	"fogTable178",	"fogTable17c",
	/* 0x180 */
	"fogTable180",	"fogTable184",	"fogTable188",	"fogTable18c",
	"fogTable190",	"fogTable194",	"fogTable198",	"fogTable19c",
	"fogTable1a0",	"fogTable1a4",	"fogTable1a8",	"fogTable1ac",
	"fogTable1b0",	"fogTable1b4",	"fogTable1b8",	"fogTable1bc",
	/* 0x1c0 */
	"fogTable1c0",	"fogTable1c4",	"fogTable1c8",	"fogTable1cc",
	"fogTable1d0",	"fogTable1d4",	"fogTable1d8",	"fogTable1dc",
	"{cmdFifoBaseAddr}","{cmdFifoBump}","{cmdFifoRdPtr}","{cmdFifoAMin}",
	"{cmdFifoAMax}","{cmdFifoDepth}","{cmdFifoHoles}","reserved1fc",
	/* 0x200 */
	"fbiInit4",		"vRetrace",		"backPorch",	"videoDimensions",
	"fbiInit0",		"fbiInit1",		"fbiInit2",		"fbiInit3",
	"hSync",		"vSync",		"clutData",		"dacData",
	"maxRgbDelta",	"{hBorder}",	"{vBorder}",	"{borderColor}",
	/* 0x240 */
	"{hvRetrace}",	"{fbiInit5}",	"{fbiInit6}",	"{fbiInit7}",
	"reserved250",	"reserved254",	"{fbiSwapHistory}","{fbiTrianglesOut}",
	"{sSetupMode}",	"{sVx}",		"{sVy}",		"{sARGB}",
	"{sRed}",		"{sGreen}",		"{sBlue}",		"{sAlpha}",
	/* 0x280 */
	"{sVz}",		"{sWb}",		"{sWtmu0}",		"{sS/Wtmu0}",
	"{sT/Wtmu0}",	"{sWtmu1}",		"{sS/Wtmu1}",	"{sT/Wtmu1}",
	"{sDrawTriCMD}","{sBeginTriCMD}","reserved2a8",	"reserved2ac",
	"reserved2b0",	"reserved2b4",	"reserved2b8",	"reserved2bc",
	/* 0x2c0 */
	"{bltSrcBaseAddr}","{bltDstBaseAddr}","{bltXYStrides}","{bltSrcChromaRange}",
	"{bltDstChromaRange}","{bltClipX}","{bltClipY}","reserved2dc",
	"{bltSrcXY}",	"{bltDstXY}",	"{bltSize}",	"{bltRop}",
	"{bltColor}",	"reserved2f4",	"{bltCommand}",	"{bltData}",
	/* 0x300 */
	"textureMode",	"tLOD",			"tDetail",		"texBaseAddr",
	"texBaseAddr_1","texBaseAddr_2","texBaseAddr_3_8","trexInit0",
	"trexInit1",	"nccTable0.0",	"nccTable0.1",	"nccTable0.2",
	"nccTable0.3",	"nccTable0.4",	"nccTable0.5",	"nccTable0.6",
	/* 0x340 */
	"nccTable0.7",	"nccTable0.8",	"nccTable0.9",	"nccTable0.A",
	"nccTable0.B",	"nccTable1.0",	"nccTable1.1",	"nccTable1.2",
	"nccTable1.3",	"nccTable1.4",	"nccTable1.5",	"nccTable1.6",
	"nccTable1.7",	"nccTable1.8",	"nccTable1.9",	"nccTable1.A",
	/* 0x380 */
	"nccTable1.B"
};


#endif
