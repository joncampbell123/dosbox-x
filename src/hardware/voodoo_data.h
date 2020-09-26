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


#ifndef DOSBOX_VOODOO_DATA_H
#define DOSBOX_VOODOO_DATA_H


/*************************************
 *
 *  Misc. constants
 *
 *************************************/

/* enumeration specifying which model of Voodoo we are emulating */
enum
{
	VOODOO_1,
	VOODOO_1_DTMU,
	VOODOO_2,
	MAX_VOODOO_TYPES
};


/* maximum number of TMUs */
#define MAX_TMU					2

/* maximum number of rasterizers */
#define MAX_RASTERIZERS			1024

/* size of the rasterizer hash table */
#define RASTER_HASH_SIZE		97

/* flags for LFB writes */
#define LFB_RGB_PRESENT			1
#define LFB_ALPHA_PRESENT		2
#define LFB_DEPTH_PRESENT		4
#define LFB_DEPTH_PRESENT_MSW	8

/* flags for the register access array */
#define REGISTER_READ			0x01		/* reads are allowed */
#define REGISTER_WRITE			0x02		/* writes are allowed */
#define REGISTER_PIPELINED		0x04		/* writes are pipelined */
#define REGISTER_FIFO			0x08		/* writes go to FIFO */
#define REGISTER_WRITETHRU		0x10		/* writes are valid even for CMDFIFO */

/* shorter combinations to make the table smaller */
#define REG_R					(REGISTER_READ)
#define REG_W					(REGISTER_WRITE)
#define REG_WT					(REGISTER_WRITE | REGISTER_WRITETHRU)
#define REG_RW					(REGISTER_READ | REGISTER_WRITE)
#define REG_RWT					(REGISTER_READ | REGISTER_WRITE | REGISTER_WRITETHRU)
#define REG_RP					(REGISTER_READ | REGISTER_PIPELINED)
#define REG_WP					(REGISTER_WRITE | REGISTER_PIPELINED)
#define REG_RWP					(REGISTER_READ | REGISTER_WRITE | REGISTER_PIPELINED)
#define REG_RWPT				(REGISTER_READ | REGISTER_WRITE | REGISTER_PIPELINED | REGISTER_WRITETHRU)
#define REG_RF					(REGISTER_READ | REGISTER_FIFO)
#define REG_WF					(REGISTER_WRITE | REGISTER_FIFO)
#define REG_RWF					(REGISTER_READ | REGISTER_WRITE | REGISTER_FIFO)
#define REG_RPF					(REGISTER_READ | REGISTER_PIPELINED | REGISTER_FIFO)
#define REG_WPF					(REGISTER_WRITE | REGISTER_PIPELINED | REGISTER_FIFO)
#define REG_RWPF				(REGISTER_READ | REGISTER_WRITE | REGISTER_PIPELINED | REGISTER_FIFO)

/* lookup bits is the log2 of the size of the reciprocal/log table */
#define RECIPLOG_LOOKUP_BITS	9

/* input precision is how many fraction bits the input value has; this is a 64-bit number */
#define RECIPLOG_INPUT_PREC		32

/* lookup precision is how many fraction bits each table entry contains */
#define RECIPLOG_LOOKUP_PREC	22

/* output precision is how many fraction bits the result should have */
#define RECIP_OUTPUT_PREC		15
#define LOG_OUTPUT_PREC			8



/*************************************
 *
 *  Dithering tables
 *
 *************************************/

static const UINT8 dither_matrix_4x4[16] =
{
	 0,  8,  2, 10,
	12,  4, 14,  6,
	 3, 11,  1,  9,
	15,  7, 13,  5
};

static const UINT8 dither_matrix_2x2[16] =
{
	 2, 10,  2, 10,
	14,  6, 14,  6,
	 2, 10,  2, 10,
	14,  6, 14,  6
};



/*************************************
 *
 *  Macros for extracting pixels
 *
 *************************************/

#define EXTRACT_565_TO_888(val, a, b, c)					\
	(a) = (((val) >> 8) & 0xf8) | (((val) >> 13) & 0x07);	\
	(b) = (((val) >> 3) & 0xfc) | (((val) >> 9) & 0x03);	\
	(c) = (((val) << 3) & 0xf8) | (((val) >> 2) & 0x07);	\

#define EXTRACT_x555_TO_888(val, a, b, c)					\
	(a) = (((val) >> 7) & 0xf8) | (((val) >> 12) & 0x07);	\
	(b) = (((val) >> 2) & 0xf8) | (((val) >> 7) & 0x07);	\
	(c) = (((val) << 3) & 0xf8) | (((val) >> 2) & 0x07);	\

#define EXTRACT_555x_TO_888(val, a, b, c)					\
	(a) = (((val) >> 8) & 0xf8) | (((val) >> 13) & 0x07);	\
	(b) = (((val) >> 3) & 0xf8) | (((val) >> 8) & 0x07);	\
	(c) = (((val) << 2) & 0xf8) | (((val) >> 3) & 0x07);	\

#define EXTRACT_1555_TO_8888(val, a, b, c, d)				\
	(a) = ((INT16)(val) >> 15) & 0xff;						\
	EXTRACT_x555_TO_888(val, b, c, d)						\

#define EXTRACT_5551_TO_8888(val, a, b, c, d)				\
	EXTRACT_555x_TO_888(val, a, b, c)						\
	(d) = ((val) & 0x0001) ? 0xff : 0x00;					\

#define EXTRACT_x888_TO_888(val, a, b, c)					\
	(a) = ((val) >> 16) & 0xff;								\
	(b) = ((val) >> 8) & 0xff;								\
	(c) = ((val) >> 0) & 0xff;								\

#define EXTRACT_888x_TO_888(val, a, b, c)					\
	(a) = ((val) >> 24) & 0xff;								\
	(b) = ((val) >> 16) & 0xff;								\
	(c) = ((val) >> 8) & 0xff;								\

#define EXTRACT_8888_TO_8888(val, a, b, c, d)				\
	(a) = ((val) >> 24) & 0xff;								\
	(b) = ((val) >> 16) & 0xff;								\
	(c) = ((val) >> 8) & 0xff;								\
	(d) = ((val) >> 0) & 0xff;								\

#define EXTRACT_4444_TO_8888(val, a, b, c, d)				\
	(a) = (((val) >> 8) & 0xf0) | (((val) >> 12) & 0x0f);	\
	(b) = (((val) >> 4) & 0xf0) | (((val) >> 8) & 0x0f);	\
	(c) = (((val) >> 0) & 0xf0) | (((val) >> 4) & 0x0f);	\
	(d) = (((val) << 4) & 0xf0) | (((val) >> 0) & 0x0f);	\

#define EXTRACT_332_TO_888(val, a, b, c)					\
	(a) = (((val) >> 0) & 0xe0) | (((val) >> 3) & 0x1c) | (((val) >> 6) & 0x03); \
	(b) = (((val) << 3) & 0xe0) | (((val) >> 0) & 0x1c) | (((val) >> 3) & 0x03); \
	(c) = (((val) << 6) & 0xc0) | (((val) << 4) & 0x30) | (((val) << 2) & 0xc0) | (((val) << 0) & 0x03); \



/*************************************
 *
 *  Misc. macros
 *
 *************************************/

/* macro for clamping a value between minimum and maximum values */
#define CLAMP(val,min,max)		do { if ((val) < (min)) { (val) = (min); } else if ((val) > (max)) { (val) = (max); } } while (0)

/* macro to compute the base 2 log for LOD calculations */
#define LOGB2(x)				(log((double)(x)) / log(2.0))



/*************************************
 *
 *  Macros for extracting bitfields
 *
 *************************************/

#define INITEN_ENABLE_HW_INIT(val)			(((val) >> 0) & 1)
#define INITEN_ENABLE_PCI_FIFO(val)			(((val) >> 1) & 1)
#define INITEN_REMAP_INIT_TO_DAC(val)		(((val) >> 2) & 1)
#define INITEN_ENABLE_SNOOP0(val)			(((val) >> 4) & 1)
#define INITEN_SNOOP0_MEMORY_MATCH(val)		(((val) >> 5) & 1)
#define INITEN_SNOOP0_READWRITE_MATCH(val)	(((val) >> 6) & 1)
#define INITEN_ENABLE_SNOOP1(val)			(((val) >> 7) & 1)
#define INITEN_SNOOP1_MEMORY_MATCH(val)		(((val) >> 8) & 1)
#define INITEN_SNOOP1_READWRITE_MATCH(val)	(((val) >> 9) & 1)
#define INITEN_SLI_BUS_OWNER(val)			(((val) >> 10) & 1)
#define INITEN_SLI_ODD_EVEN(val)			(((val) >> 11) & 1)
#define INITEN_SECONDARY_REV_ID(val)		(((val) >> 12) & 0xf)	/* voodoo 2 only */
#define INITEN_MFCTR_FAB_ID(val)			(((val) >> 16) & 0xf)	/* voodoo 2 only */
#define INITEN_ENABLE_PCI_INTERRUPT(val)	(((val) >> 20) & 1)		/* voodoo 2 only */
#define INITEN_PCI_INTERRUPT_TIMEOUT(val)	(((val) >> 21) & 1)		/* voodoo 2 only */
#define INITEN_ENABLE_NAND_TREE_TEST(val)	(((val) >> 22) & 1)		/* voodoo 2 only */
#define INITEN_ENABLE_SLI_ADDRESS_SNOOP(val) (((val) >> 23) & 1)	/* voodoo 2 only */
#define INITEN_SLI_SNOOP_ADDRESS(val)		(((val) >> 24) & 0xff)	/* voodoo 2 only */

#define FBZCP_CC_RGBSELECT(val)				(((val) >> 0) & 3)
#define FBZCP_CC_ASELECT(val)				(((val) >> 2) & 3)
#define FBZCP_CC_LOCALSELECT(val)			(((val) >> 4) & 1)
#define FBZCP_CCA_LOCALSELECT(val)			(((val) >> 5) & 3)
#define FBZCP_CC_LOCALSELECT_OVERRIDE(val)	(((val) >> 7) & 1)
#define FBZCP_CC_ZERO_OTHER(val)			(((val) >> 8) & 1)
#define FBZCP_CC_SUB_CLOCAL(val)			(((val) >> 9) & 1)
#define FBZCP_CC_MSELECT(val)				(((val) >> 10) & 7)
#define FBZCP_CC_REVERSE_BLEND(val)			(((val) >> 13) & 1)
#define FBZCP_CC_ADD_ACLOCAL(val)			(((val) >> 14) & 3)
#define FBZCP_CC_INVERT_OUTPUT(val)			(((val) >> 16) & 1)
#define FBZCP_CCA_ZERO_OTHER(val)			(((val) >> 17) & 1)
#define FBZCP_CCA_SUB_CLOCAL(val)			(((val) >> 18) & 1)
#define FBZCP_CCA_MSELECT(val)				(((val) >> 19) & 7)
#define FBZCP_CCA_REVERSE_BLEND(val)		(((val) >> 22) & 1)
#define FBZCP_CCA_ADD_ACLOCAL(val)			(((val) >> 23) & 3)
#define FBZCP_CCA_INVERT_OUTPUT(val)		(((val) >> 25) & 1)
#define FBZCP_CCA_SUBPIXEL_ADJUST(val)		(((val) >> 26) & 1)
#define FBZCP_TEXTURE_ENABLE(val)			(((val) >> 27) & 1)
#define FBZCP_RGBZW_CLAMP(val)				(((val) >> 28) & 1)		/* voodoo 2 only */
#define FBZCP_ANTI_ALIAS(val)				(((val) >> 29) & 1)		/* voodoo 2 only */

#define ALPHAMODE_ALPHATEST(val)			(((val) >> 0) & 1)
#define ALPHAMODE_ALPHAFUNCTION(val)		(((val) >> 1) & 7)
#define ALPHAMODE_ALPHABLEND(val)			(((val) >> 4) & 1)
#define ALPHAMODE_ANTIALIAS(val)			(((val) >> 5) & 1)
#define ALPHAMODE_SRCRGBBLEND(val)			(((val) >> 8) & 15)
#define ALPHAMODE_DSTRGBBLEND(val)			(((val) >> 12) & 15)
#define ALPHAMODE_SRCALPHABLEND(val)		(((val) >> 16) & 15)
#define ALPHAMODE_DSTALPHABLEND(val)		(((val) >> 20) & 15)
#define ALPHAMODE_ALPHAREF(val)				(((val) >> 24) & 0xff)

#define FOGMODE_ENABLE_FOG(val)				(((val) >> 0) & 1)
#define FOGMODE_FOG_ADD(val)				(((val) >> 1) & 1)
#define FOGMODE_FOG_MULT(val)				(((val) >> 2) & 1)
#define FOGMODE_FOG_ZALPHA(val)				(((val) >> 3) & 3)
#define FOGMODE_FOG_CONSTANT(val)			(((val) >> 5) & 1)
#define FOGMODE_FOG_DITHER(val)				(((val) >> 6) & 1)		/* voodoo 2 only */
#define FOGMODE_FOG_ZONES(val)				(((val) >> 7) & 1)		/* voodoo 2 only */

#define FBZMODE_ENABLE_CLIPPING(val)		(((val) >> 0) & 1)
#define FBZMODE_ENABLE_CHROMAKEY(val)		(((val) >> 1) & 1)
#define FBZMODE_ENABLE_STIPPLE(val)			(((val) >> 2) & 1)
#define FBZMODE_WBUFFER_SELECT(val)			(((val) >> 3) & 1)
#define FBZMODE_ENABLE_DEPTHBUF(val)		(((val) >> 4) & 1)
#define FBZMODE_DEPTH_FUNCTION(val)			(((val) >> 5) & 7)
#define FBZMODE_ENABLE_DITHERING(val)		(((val) >> 8) & 1)
#define FBZMODE_RGB_BUFFER_MASK(val)		(((val) >> 9) & 1)
#define FBZMODE_AUX_BUFFER_MASK(val)		(((val) >> 10) & 1)
#define FBZMODE_DITHER_TYPE(val)			(((val) >> 11) & 1)
#define FBZMODE_STIPPLE_PATTERN(val)		(((val) >> 12) & 1)
#define FBZMODE_ENABLE_ALPHA_MASK(val)		(((val) >> 13) & 1)
#define FBZMODE_DRAW_BUFFER(val)			(((val) >> 14) & 3)
#define FBZMODE_ENABLE_DEPTH_BIAS(val)		(((val) >> 16) & 1)
#define FBZMODE_Y_ORIGIN(val)				(((val) >> 17) & 1)
#define FBZMODE_ENABLE_ALPHA_PLANES(val)	(((val) >> 18) & 1)
#define FBZMODE_ALPHA_DITHER_SUBTRACT(val)	(((val) >> 19) & 1)
#define FBZMODE_DEPTH_SOURCE_COMPARE(val)	(((val) >> 20) & 1)
#define FBZMODE_DEPTH_FLOAT_SELECT(val)		(((val) >> 21) & 1)		/* voodoo 2 only */

#define LFBMODE_WRITE_FORMAT(val)			(((val) >> 0) & 0xf)
#define LFBMODE_WRITE_BUFFER_SELECT(val)	(((val) >> 4) & 3)
#define LFBMODE_READ_BUFFER_SELECT(val)		(((val) >> 6) & 3)
#define LFBMODE_ENABLE_PIXEL_PIPELINE(val)	(((val) >> 8) & 1)
#define LFBMODE_RGBA_LANES(val)				(((val) >> 9) & 3)
#define LFBMODE_WORD_SWAP_WRITES(val)		(((val) >> 11) & 1)
#define LFBMODE_BYTE_SWIZZLE_WRITES(val)	(((val) >> 12) & 1)
#define LFBMODE_Y_ORIGIN(val)				(((val) >> 13) & 1)
#define LFBMODE_WRITE_W_SELECT(val)			(((val) >> 14) & 1)
#define LFBMODE_WORD_SWAP_READS(val)		(((val) >> 15) & 1)
#define LFBMODE_BYTE_SWIZZLE_READS(val)		(((val) >> 16) & 1)

#define CHROMARANGE_BLUE_EXCLUSIVE(val)		(((val) >> 24) & 1)
#define CHROMARANGE_GREEN_EXCLUSIVE(val)	(((val) >> 25) & 1)
#define CHROMARANGE_RED_EXCLUSIVE(val)		(((val) >> 26) & 1)
#define CHROMARANGE_UNION_MODE(val)			(((val) >> 27) & 1)
#define CHROMARANGE_ENABLE(val)				(((val) >> 28) & 1)

#define FBIINIT0_VGA_PASSTHRU(val)			(((val) >> 0) & 1)
#define FBIINIT0_GRAPHICS_RESET(val)		(((val) >> 1) & 1)
#define FBIINIT0_FIFO_RESET(val)			(((val) >> 2) & 1)
#define FBIINIT0_SWIZZLE_REG_WRITES(val)	(((val) >> 3) & 1)
#define FBIINIT0_STALL_PCIE_FOR_HWM(val)	(((val) >> 4) & 1)
#define FBIINIT0_PCI_FIFO_LWM(val)			(((val) >> 6) & 0x1f)
#define FBIINIT0_LFB_TO_MEMORY_FIFO(val)	(((val) >> 11) & 1)
#define FBIINIT0_TEXMEM_TO_MEMORY_FIFO(val) (((val) >> 12) & 1)
#define FBIINIT0_ENABLE_MEMORY_FIFO(val)	(((val) >> 13) & 1)
#define FBIINIT0_MEMORY_FIFO_HWM(val)		(((val) >> 14) & 0x7ff)
#define FBIINIT0_MEMORY_FIFO_BURST(val)		(((val) >> 25) & 0x3f)

#define FBIINIT1_PCI_DEV_FUNCTION(val)		(((val) >> 0) & 1)
#define FBIINIT1_PCI_WRITE_WAIT_STATES(val)	(((val) >> 1) & 1)
#define FBIINIT1_MULTI_SST1(val)			(((val) >> 2) & 1)		/* not on voodoo 2 */
#define FBIINIT1_ENABLE_LFB(val)			(((val) >> 3) & 1)
#define FBIINIT1_X_VIDEO_TILES(val)			(((val) >> 4) & 0xf)
#define FBIINIT1_VIDEO_TIMING_RESET(val)	(((val) >> 8) & 1)
#define FBIINIT1_SOFTWARE_OVERRIDE(val)		(((val) >> 9) & 1)
#define FBIINIT1_SOFTWARE_HSYNC(val)		(((val) >> 10) & 1)
#define FBIINIT1_SOFTWARE_VSYNC(val)		(((val) >> 11) & 1)
#define FBIINIT1_SOFTWARE_BLANK(val)		(((val) >> 12) & 1)
#define FBIINIT1_DRIVE_VIDEO_TIMING(val)	(((val) >> 13) & 1)
#define FBIINIT1_DRIVE_VIDEO_BLANK(val)		(((val) >> 14) & 1)
#define FBIINIT1_DRIVE_VIDEO_SYNC(val)		(((val) >> 15) & 1)
#define FBIINIT1_DRIVE_VIDEO_DCLK(val)		(((val) >> 16) & 1)
#define FBIINIT1_VIDEO_TIMING_VCLK(val)		(((val) >> 17) & 1)
#define FBIINIT1_VIDEO_CLK_2X_DELAY(val)	(((val) >> 18) & 3)
#define FBIINIT1_VIDEO_TIMING_SOURCE(val)	(((val) >> 20) & 3)
#define FBIINIT1_ENABLE_24BPP_OUTPUT(val)	(((val) >> 22) & 1)
#define FBIINIT1_ENABLE_SLI(val)			(((val) >> 23) & 1)
#define FBIINIT1_X_VIDEO_TILES_BIT5(val)	(((val) >> 24) & 1)		/* voodoo 2 only */
#define FBIINIT1_ENABLE_EDGE_FILTER(val)	(((val) >> 25) & 1)
#define FBIINIT1_INVERT_VID_CLK_2X(val)		(((val) >> 26) & 1)
#define FBIINIT1_VID_CLK_2X_SEL_DELAY(val)	(((val) >> 27) & 3)
#define FBIINIT1_VID_CLK_DELAY(val)			(((val) >> 29) & 3)
#define FBIINIT1_DISABLE_FAST_READAHEAD(val) (((val) >> 31) & 1)

#define FBIINIT2_DISABLE_DITHER_SUB(val)	(((val) >> 0) & 1)
#define FBIINIT2_DRAM_BANKING(val)			(((val) >> 1) & 1)
#define FBIINIT2_ENABLE_TRIPLE_BUF(val)		(((val) >> 4) & 1)
#define FBIINIT2_ENABLE_FAST_RAS_READ(val)	(((val) >> 5) & 1)
#define FBIINIT2_ENABLE_GEN_DRAM_OE(val)	(((val) >> 6) & 1)
#define FBIINIT2_ENABLE_FAST_READWRITE(val)	(((val) >> 7) & 1)
#define FBIINIT2_ENABLE_PASSTHRU_DITHER(val) (((val) >> 8) & 1)
#define FBIINIT2_SWAP_BUFFER_ALGORITHM(val)	(((val) >> 9) & 3)
#define FBIINIT2_VIDEO_BUFFER_OFFSET(val)	(((val) >> 11) & 0x1ff)
#define FBIINIT2_ENABLE_DRAM_BANKING(val)	(((val) >> 20) & 1)
#define FBIINIT2_ENABLE_DRAM_READ_FIFO(val)	(((val) >> 21) & 1)
#define FBIINIT2_ENABLE_DRAM_REFRESH(val)	(((val) >> 22) & 1)
#define FBIINIT2_REFRESH_LOAD_VALUE(val)	(((val) >> 23) & 0x1ff)

#define FBIINIT3_TRI_REGISTER_REMAP(val)	(((val) >> 0) & 1)
#define FBIINIT3_VIDEO_FIFO_THRESH(val)		(((val) >> 1) & 0x1f)
#define FBIINIT3_DISABLE_TMUS(val)			(((val) >> 6) & 1)
#define FBIINIT3_FBI_MEMORY_TYPE(val)		(((val) >> 8) & 7)
#define FBIINIT3_VGA_PASS_RESET_VAL(val)	(((val) >> 11) & 1)
#define FBIINIT3_HARDCODE_PCI_BASE(val)		(((val) >> 12) & 1)
#define FBIINIT3_FBI2TREX_DELAY(val)		(((val) >> 13) & 0xf)
#define FBIINIT3_TREX2FBI_DELAY(val)		(((val) >> 17) & 0x1f)
#define FBIINIT3_YORIGIN_SUBTRACT(val)		(((val) >> 22) & 0x3ff)

#define FBIINIT4_PCI_READ_WAITS(val)		(((val) >> 0) & 1)
#define FBIINIT4_ENABLE_LFB_READAHEAD(val)	(((val) >> 1) & 1)
#define FBIINIT4_MEMORY_FIFO_LWM(val)		(((val) >> 2) & 0x3f)
#define FBIINIT4_MEMORY_FIFO_START_ROW(val)	(((val) >> 8) & 0x3ff)
#define FBIINIT4_MEMORY_FIFO_STOP_ROW(val)	(((val) >> 18) & 0x3ff)
#define FBIINIT4_VIDEO_CLOCKING_DELAY(val)	(((val) >> 29) & 7)		/* voodoo 2 only */

#define FBIINIT5_DISABLE_PCI_STOP(val)		(((val) >> 0) & 1)		/* voodoo 2 only */
#define FBIINIT5_PCI_SLAVE_SPEED(val)		(((val) >> 1) & 1)		/* voodoo 2 only */
#define FBIINIT5_DAC_DATA_OUTPUT_WIDTH(val)	(((val) >> 2) & 1)		/* voodoo 2 only */
#define FBIINIT5_DAC_DATA_17_OUTPUT(val)	(((val) >> 3) & 1)		/* voodoo 2 only */
#define FBIINIT5_DAC_DATA_18_OUTPUT(val)	(((val) >> 4) & 1)		/* voodoo 2 only */
#define FBIINIT5_GENERIC_STRAPPING(val)		(((val) >> 5) & 0xf)	/* voodoo 2 only */
#define FBIINIT5_BUFFER_ALLOCATION(val)		(((val) >> 9) & 3)		/* voodoo 2 only */
#define FBIINIT5_DRIVE_VID_CLK_SLAVE(val)	(((val) >> 11) & 1)		/* voodoo 2 only */
#define FBIINIT5_DRIVE_DAC_DATA_16(val)		(((val) >> 12) & 1)		/* voodoo 2 only */
#define FBIINIT5_VCLK_INPUT_SELECT(val)		(((val) >> 13) & 1)		/* voodoo 2 only */
#define FBIINIT5_MULTI_CVG_DETECT(val)		(((val) >> 14) & 1)		/* voodoo 2 only */
#define FBIINIT5_SYNC_RETRACE_READS(val)	(((val) >> 15) & 1)		/* voodoo 2 only */
#define FBIINIT5_ENABLE_RHBORDER_COLOR(val)	(((val) >> 16) & 1)		/* voodoo 2 only */
#define FBIINIT5_ENABLE_LHBORDER_COLOR(val)	(((val) >> 17) & 1)		/* voodoo 2 only */
#define FBIINIT5_ENABLE_BVBORDER_COLOR(val)	(((val) >> 18) & 1)		/* voodoo 2 only */
#define FBIINIT5_ENABLE_TVBORDER_COLOR(val)	(((val) >> 19) & 1)		/* voodoo 2 only */
#define FBIINIT5_DOUBLE_HORIZ(val)			(((val) >> 20) & 1)		/* voodoo 2 only */
#define FBIINIT5_DOUBLE_VERT(val)			(((val) >> 21) & 1)		/* voodoo 2 only */
#define FBIINIT5_ENABLE_16BIT_GAMMA(val)	(((val) >> 22) & 1)		/* voodoo 2 only */
#define FBIINIT5_INVERT_DAC_HSYNC(val)		(((val) >> 23) & 1)		/* voodoo 2 only */
#define FBIINIT5_INVERT_DAC_VSYNC(val)		(((val) >> 24) & 1)		/* voodoo 2 only */
#define FBIINIT5_ENABLE_24BIT_DACDATA(val)	(((val) >> 25) & 1)		/* voodoo 2 only */
#define FBIINIT5_ENABLE_INTERLACING(val)	(((val) >> 26) & 1)		/* voodoo 2 only */
#define FBIINIT5_DAC_DATA_18_CONTROL(val)	(((val) >> 27) & 1)		/* voodoo 2 only */
#define FBIINIT5_RASTERIZER_UNIT_MODE(val)	(((val) >> 30) & 3)		/* voodoo 2 only */

#define FBIINIT6_WINDOW_ACTIVE_COUNTER(val)	(((val) >> 0) & 7)		/* voodoo 2 only */
#define FBIINIT6_WINDOW_DRAG_COUNTER(val)	(((val) >> 3) & 0x1f)	/* voodoo 2 only */
#define FBIINIT6_SLI_SYNC_MASTER(val)		(((val) >> 8) & 1)		/* voodoo 2 only */
#define FBIINIT6_DAC_DATA_22_OUTPUT(val)	(((val) >> 9) & 3)		/* voodoo 2 only */
#define FBIINIT6_DAC_DATA_23_OUTPUT(val)	(((val) >> 11) & 3)		/* voodoo 2 only */
#define FBIINIT6_SLI_SYNCIN_OUTPUT(val)		(((val) >> 13) & 3)		/* voodoo 2 only */
#define FBIINIT6_SLI_SYNCOUT_OUTPUT(val)	(((val) >> 15) & 3)		/* voodoo 2 only */
#define FBIINIT6_DAC_RD_OUTPUT(val)			(((val) >> 17) & 3)		/* voodoo 2 only */
#define FBIINIT6_DAC_WR_OUTPUT(val)			(((val) >> 19) & 3)		/* voodoo 2 only */
#define FBIINIT6_PCI_FIFO_LWM_RDY(val)		(((val) >> 21) & 0x7f)	/* voodoo 2 only */
#define FBIINIT6_VGA_PASS_N_OUTPUT(val)		(((val) >> 28) & 3)		/* voodoo 2 only */
#define FBIINIT6_X_VIDEO_TILES_BIT0(val)	(((val) >> 30) & 1)		/* voodoo 2 only */

#define FBIINIT7_GENERIC_STRAPPING(val)		(((val) >> 0) & 0xff)	/* voodoo 2 only */
#define FBIINIT7_CMDFIFO_ENABLE(val)		(((val) >> 8) & 1)		/* voodoo 2 only */
#define FBIINIT7_CMDFIFO_MEMORY_STORE(val)	(((val) >> 9) & 1)		/* voodoo 2 only */
#define FBIINIT7_DISABLE_CMDFIFO_HOLES(val)	(((val) >> 10) & 1)		/* voodoo 2 only */
#define FBIINIT7_CMDFIFO_READ_THRESH(val)	(((val) >> 11) & 0x1f)	/* voodoo 2 only */
#define FBIINIT7_SYNC_CMDFIFO_WRITES(val)	(((val) >> 16) & 1)		/* voodoo 2 only */
#define FBIINIT7_SYNC_CMDFIFO_READS(val)	(((val) >> 17) & 1)		/* voodoo 2 only */
#define FBIINIT7_RESET_PCI_PACKER(val)		(((val) >> 18) & 1)		/* voodoo 2 only */
#define FBIINIT7_ENABLE_CHROMA_STUFF(val)	(((val) >> 19) & 1)		/* voodoo 2 only */
#define FBIINIT7_CMDFIFO_PCI_TIMEOUT(val)	(((val) >> 20) & 0x7f)	/* voodoo 2 only */
#define FBIINIT7_ENABLE_TEXTURE_BURST(val)	(((val) >> 27) & 1)		/* voodoo 2 only */

#define TEXMODE_ENABLE_PERSPECTIVE(val)		(((val) >> 0) & 1)
#define TEXMODE_MINIFICATION_FILTER(val)	(((val) >> 1) & 1)
#define TEXMODE_MAGNIFICATION_FILTER(val)	(((val) >> 2) & 1)
#define TEXMODE_CLAMP_NEG_W(val)			(((val) >> 3) & 1)
#define TEXMODE_ENABLE_LOD_DITHER(val)		(((val) >> 4) & 1)
#define TEXMODE_NCC_TABLE_SELECT(val)		(((val) >> 5) & 1)
#define TEXMODE_CLAMP_S(val)				(((val) >> 6) & 1)
#define TEXMODE_CLAMP_T(val)				(((val) >> 7) & 1)
#define TEXMODE_FORMAT(val)					(((val) >> 8) & 0xf)
#define TEXMODE_TC_ZERO_OTHER(val)			(((val) >> 12) & 1)
#define TEXMODE_TC_SUB_CLOCAL(val)			(((val) >> 13) & 1)
#define TEXMODE_TC_MSELECT(val)				(((val) >> 14) & 7)
#define TEXMODE_TC_REVERSE_BLEND(val)		(((val) >> 17) & 1)
#define TEXMODE_TC_ADD_ACLOCAL(val)			(((val) >> 18) & 3)
#define TEXMODE_TC_INVERT_OUTPUT(val)		(((val) >> 20) & 1)
#define TEXMODE_TCA_ZERO_OTHER(val)			(((val) >> 21) & 1)
#define TEXMODE_TCA_SUB_CLOCAL(val)			(((val) >> 22) & 1)
#define TEXMODE_TCA_MSELECT(val)			(((val) >> 23) & 7)
#define TEXMODE_TCA_REVERSE_BLEND(val)		(((val) >> 26) & 1)
#define TEXMODE_TCA_ADD_ACLOCAL(val)		(((val) >> 27) & 3)
#define TEXMODE_TCA_INVERT_OUTPUT(val)		(((val) >> 29) & 1)
#define TEXMODE_TRILINEAR(val)				(((val) >> 30) & 1)
#define TEXMODE_SEQ_8_DOWNLD(val)			(((val) >> 31) & 1)

#define TEXLOD_LODMIN(val)					(((val) >> 0) & 0x3f)
#define TEXLOD_LODMAX(val)					(((val) >> 6) & 0x3f)
#define TEXLOD_LODBIAS(val)					(((val) >> 12) & 0x3f)
#define TEXLOD_LOD_ODD(val)					(((val) >> 18) & 1)
#define TEXLOD_LOD_TSPLIT(val)				(((val) >> 19) & 1)
#define TEXLOD_LOD_S_IS_WIDER(val)			(((val) >> 20) & 1)
#define TEXLOD_LOD_ASPECT(val)				(((val) >> 21) & 3)
#define TEXLOD_LOD_ZEROFRAC(val)			(((val) >> 23) & 1)
#define TEXLOD_TMULTIBASEADDR(val)			(((val) >> 24) & 1)
#define TEXLOD_TDATA_SWIZZLE(val)			(((val) >> 25) & 1)
#define TEXLOD_TDATA_SWAP(val)				(((val) >> 26) & 1)
#define TEXLOD_TDIRECT_WRITE(val)			(((val) >> 27) & 1)		/* Voodoo 2 only */

#define TEXDETAIL_DETAIL_MAX(val)			(((val) >> 0) & 0xff)
#define TEXDETAIL_DETAIL_BIAS(val)			(((val) >> 8) & 0x3f)
#define TEXDETAIL_DETAIL_SCALE(val)			(((val) >> 14) & 7)
#define TEXDETAIL_RGB_MIN_FILTER(val)		(((val) >> 17) & 1)		/* Voodoo 2 only */
#define TEXDETAIL_RGB_MAG_FILTER(val)		(((val) >> 18) & 1)		/* Voodoo 2 only */
#define TEXDETAIL_ALPHA_MIN_FILTER(val)		(((val) >> 19) & 1)		/* Voodoo 2 only */
#define TEXDETAIL_ALPHA_MAG_FILTER(val)		(((val) >> 20) & 1)		/* Voodoo 2 only */
#define TEXDETAIL_SEPARATE_RGBA_FILTER(val)	(((val) >> 21) & 1)		/* Voodoo 2 only */

#define TREXINIT_SEND_TMU_CONFIG(val)		(((val) >> 18) & 1)


/*************************************
 *
 *  Core types
 *
 *************************************/

typedef struct _voodoo_state voodoo_state;
typedef struct _poly_extra_data poly_extra_data;

typedef UINT32 rgb_t;

typedef struct _rgba rgba;
#define LSB_FIRST
struct _rgba
{
#ifdef LSB_FIRST
	UINT8				b, g, r, a;
#else
	UINT8				a, r, g, b;
#endif
};


typedef union _voodoo_reg voodoo_reg;
union _voodoo_reg
{
	INT32				i;
	UINT32				u;
	float				f;
	rgba				rgb;
};


typedef voodoo_reg rgb_union;


/* note that this structure is an even 64 bytes long */
typedef struct _stats_block stats_block;
struct _stats_block
{
	INT32				pixels_in;				/* pixels in statistic */
	INT32				pixels_out;				/* pixels out statistic */
	INT32				chroma_fail;			/* chroma test fail statistic */
	INT32				zfunc_fail;				/* z function test fail statistic */
	INT32				afunc_fail;				/* alpha function test fail statistic */
	INT32				clip_fail;				/* clipping fail statistic */
	INT32				stipple_count;			/* stipple statistic */
	INT32				filler[64/4 - 7];		/* pad this structure to 64 bytes */
};


typedef struct _fifo_state fifo_state;
struct _fifo_state
{
	INT32				size;					/* size of the FIFO */
};


typedef struct _pci_state pci_state;
struct _pci_state
{
	fifo_state			fifo;					/* PCI FIFO */
	UINT32				init_enable;			/* initEnable value */
	bool				op_pending;				/* true if an operation is pending */
};


typedef struct _ncc_table ncc_table;
struct _ncc_table
{
	bool				dirty;					/* is the texel lookup dirty? */
	voodoo_reg *		reg;					/* pointer to our registers */
	INT32				ir[4], ig[4], ib[4];	/* I values for R,G,B */
	INT32				qr[4], qg[4], qb[4];	/* Q values for R,G,B */
	INT32				y[16];					/* Y values */
	rgb_t *				palette;				/* pointer to associated RGB palette */
	rgb_t *				palettea;				/* pointer to associated ARGB palette */
	rgb_t				texel[256];				/* texel lookup */
};


typedef struct _tmu_state tmu_state;
struct _tmu_state
{
	UINT8 *				ram;					/* pointer to our RAM */
	UINT32				mask;					/* mask to apply to pointers */
	voodoo_reg *		reg;					/* pointer to our register base */
	bool				regdirty;				/* true if the LOD/mode/base registers have changed */

	UINT32				texaddr_mask;			/* mask for texture address */
	UINT8				texaddr_shift;			/* shift for texture address */

	INT64				starts, startt;			/* starting S,T (14.18) */
	INT64				startw;					/* starting W (2.30) */
	INT64				dsdx, dtdx;				/* delta S,T per X */
	INT64				dwdx;					/* delta W per X */
	INT64				dsdy, dtdy;				/* delta S,T per Y */
	INT64				dwdy;					/* delta W per Y */

	INT32				lodmin, lodmax;			/* min, max LOD values */
	INT32				lodbias;				/* LOD bias */
	UINT32				lodmask;				/* mask of available LODs */
	UINT32				lodoffset[9];			/* offset of texture base for each LOD */
	INT32				detailmax;				/* detail clamp */
	INT32				detailbias;				/* detail bias */
	UINT8				detailscale;			/* detail scale */

	UINT32				wmask;					/* mask for the current texture width */
	UINT32				hmask;					/* mask for the current texture height */

	UINT8				bilinear_mask;			/* mask for bilinear resolution (0xf0 for V1, 0xff for V2) */

	ncc_table			ncc[2];					/* two NCC tables */

	rgb_t *				lookup;					/* currently selected lookup */
	rgb_t *				texel[16];				/* texel lookups for each format */

	rgb_t				palette[256];			/* palette lookup table */
	rgb_t				palettea[256];			/* palette+alpha lookup table */
};


typedef struct _tmu_shared_state tmu_shared_state;
struct _tmu_shared_state
{
	rgb_t				rgb332[256];			/* RGB 3-3-2 lookup table */
	rgb_t				alpha8[256];			/* alpha 8-bit lookup table */
	rgb_t				int8[256];				/* intensity 8-bit lookup table */
	rgb_t				ai44[256];				/* alpha, intensity 4-4 lookup table */

	rgb_t				rgb565[65536];			/* RGB 5-6-5 lookup table */
	rgb_t				argb1555[65536];		/* ARGB 1-5-5-5 lookup table */
	rgb_t				argb4444[65536];		/* ARGB 4-4-4-4 lookup table */
};


typedef struct _setup_vertex setup_vertex;
struct _setup_vertex
{
	float				x, y;					/* X, Y coordinates */
	float				a, r, g, b;				/* A, R, G, B values */
	float				z, wb;					/* Z and broadcast W values */
	float				w0, s0, t0;				/* W, S, T for TMU 0 */
	float				w1, s1, t1;				/* W, S, T for TMU 1 */
};


typedef struct _fbi_state fbi_state;
struct _fbi_state
{
	UINT8 *				ram;					/* pointer to frame buffer RAM */
	UINT32				mask;					/* mask to apply to pointers */
	UINT32				rgboffs[3];				/* word offset to 3 RGB buffers */
	UINT32				auxoffs;				/* word offset to 1 aux buffer */

	UINT8				frontbuf;				/* front buffer index */
	UINT8				backbuf;				/* back buffer index */

	UINT32				yorigin;				/* Y origin subtract value */

	UINT32				width;					/* width of current frame buffer */
	UINT32				height;					/* height of current frame buffer */
//	UINT32				xoffs;					/* horizontal offset (back porch) */
//	UINT32				yoffs;					/* vertical offset (back porch) */
//	UINT32				vsyncscan;				/* vertical sync scanline */
	UINT32				rowpixels;				/* pixels per row */
	UINT32				tile_width;				/* width of video tiles */
	UINT32				tile_height;			/* height of video tiles */
	UINT32				x_tiles;				/* number of tiles in the X direction */

	UINT8				vblank;					/* VBLANK state */
	bool				vblank_dont_swap;		/* don't actually swap when we hit this point */
	bool				vblank_flush_pending;

	/* triangle setup info */
	INT16				ax, ay;					/* vertex A x,y (12.4) */
	INT16				bx, by;					/* vertex B x,y (12.4) */
	INT16				cx, cy;					/* vertex C x,y (12.4) */
	INT32				startr, startg, startb, starta; /* starting R,G,B,A (12.12) */
	INT32				startz;					/* starting Z (20.12) */
	INT64				startw;					/* starting W (16.32) */
	INT32				drdx, dgdx, dbdx, dadx;	/* delta R,G,B,A per X */
	INT32				dzdx;					/* delta Z per X */
	INT64				dwdx;					/* delta W per X */
	INT32				drdy, dgdy, dbdy, dady;	/* delta R,G,B,A per Y */
	INT32				dzdy;					/* delta Z per Y */
	INT64				dwdy;					/* delta W per Y */

	stats_block			lfb_stats;				/* LFB-access statistics */

	UINT8				sverts;					/* number of vertices ready */
	setup_vertex		svert[3];				/* 3 setup vertices */

	fifo_state			fifo;					/* framebuffer memory fifo */

	UINT8				fogblend[64];			/* 64-entry fog table */
	UINT8				fogdelta[64];			/* 64-entry fog table */
	UINT8				fogdelta_mask;			/* mask for for delta (0xff for V1, 0xfc for V2) */

//	rgb_t				clut[512];				/* clut gamma data */
};


typedef struct _dac_state dac_state;
struct _dac_state
{
	UINT8				reg[8];					/* 8 registers */
	UINT8				read_result;			/* pending read result */
};


typedef struct _raster_info raster_info;
struct _raster_info
{
	struct _raster_info *next;					/* pointer to next entry with the same hash */
	poly_draw_scanline_func callback;			/* callback pointer */
	bool				is_generic;				/* true if this is one of the generic rasterizers */
	UINT8				display;				/* display index */
	UINT32				hits;					/* how many hits (pixels) we've used this for */
	UINT32				polys;					/* how many polys we've used this for */
	UINT32				eff_color_path;			/* effective fbzColorPath value */
	UINT32				eff_alpha_mode;			/* effective alphaMode value */
	UINT32				eff_fog_mode;			/* effective fogMode value */
	UINT32				eff_fbz_mode;			/* effective fbzMode value */
	UINT32				eff_tex_mode_0;			/* effective textureMode value for TMU #0 */
	UINT32				eff_tex_mode_1;			/* effective textureMode value for TMU #1 */

	bool				shader_ready;
	UINT32				so_shader_program;
	UINT32				so_vertex_shader;
	UINT32				so_fragment_shader;
	INT32*				shader_ulocations;
};


struct _poly_extra_data
{
	voodoo_state *		state;					/* pointer back to the voodoo state */
	raster_info *		info;					/* pointer to rasterizer information */

	INT16				ax, ay;					/* vertex A x,y (12.4) */
	INT32				startr, startg, startb, starta; /* starting R,G,B,A (12.12) */
	INT32				startz;					/* starting Z (20.12) */
	INT64				startw;					/* starting W (16.32) */
	INT32				drdx, dgdx, dbdx, dadx;	/* delta R,G,B,A per X */
	INT32				dzdx;					/* delta Z per X */
	INT64				dwdx;					/* delta W per X */
	INT32				drdy, dgdy, dbdy, dady;	/* delta R,G,B,A per Y */
	INT32				dzdy;					/* delta Z per Y */
	INT64				dwdy;					/* delta W per Y */

	INT64				starts0, startt0;		/* starting S,T (14.18) */
	INT64				startw0;				/* starting W (2.30) */
	INT64				ds0dx, dt0dx;			/* delta S,T per X */
	INT64				dw0dx;					/* delta W per X */
	INT64				ds0dy, dt0dy;			/* delta S,T per Y */
	INT64				dw0dy;					/* delta W per Y */
	INT32				lodbase0;				/* used during rasterization */

	INT64				starts1, startt1;		/* starting S,T (14.18) */
	INT64				startw1;				/* starting W (2.30) */
	INT64				ds1dx, dt1dx;			/* delta S,T per X */
	INT64				dw1dx;					/* delta W per X */
	INT64				ds1dy, dt1dy;			/* delta S,T per Y */
	INT64				dw1dy;					/* delta W per Y */
	INT32				lodbase1;				/* used during rasterization */

	UINT16				dither[16];				/* dither matrix, for fastfill */

	Bitu				texcount;
	Bitu				r_fbzColorPath;
	Bitu				r_fbzMode;
	Bitu				r_alphaMode;
	Bitu				r_fogMode;
	INT32				r_textureMode0;
	INT32				r_textureMode1;
};


/* typedef struct _voodoo_state voodoo_state; -- declared above */
struct _voodoo_state
{
	UINT8				type;					/* type of system */
	UINT8				chipmask;				/* mask for which chips are available */

	voodoo_reg			reg[0x400];				/* raw registers */
	const UINT8 *		regaccess;				/* register access array */
	const char *const *	regnames;				/* register names array */
	bool				alt_regmap;				/* enable alternate register map? */

	pci_state			pci;					/* PCI state */
	dac_state			dac;					/* DAC state */

	fbi_state			fbi;					/* FBI states */
	tmu_state			tmu[MAX_TMU];			/* TMU states */
	tmu_shared_state	tmushare;				/* TMU shared state */

	stats_block	*		thread_stats;			/* per-thread statistics */

	int					next_rasterizer;		/* next rasterizer index */
	raster_info			rasterizer[MAX_RASTERIZERS];	/* array of rasterizers */
	raster_info *		raster_hash[RASTER_HASH_SIZE];	/* hash table of rasterizers */

	bool				send_config;
	UINT32				tmu_config;

	bool				ogl;
	bool				ogl_dimchange;
	bool				clock_enabled;
	bool				output_on;
	bool				active;
};



/*************************************
 *
 *  Inline FIFO management
 *
 *************************************/

INLINE INT32 fifo_space(fifo_state *f)
{
	INT32 items = 0;
	if (items < 0)
		items += f->size;
	return f->size - 1 - items;
}


INLINE UINT8 count_leading_zeros(UINT32 value)
{
	INT32 result;

#if defined(_MSC_VER) && defined(_M_IX86)
    __asm
    {
    	bsr   eax,value
    	jnz   skip
    	mov   eax,63
    skip:
    	xor   eax,31
        mov   result,eax
    }
#else
	result = 32;
	while(value > 0) {
		result--;
		value >>= 1;
	}
#endif

	return (UINT8)result;
}

/*************************************
 *
 *  Computes a fast 16.16 reciprocal
 *  of a 16.32 value; used for
 *  computing 1/w in the rasterizer.
 *
 *  Since it is trivial to also
 *  compute log2(1/w) = -log2(w) at
 *  the same time, we do that as well
 *  to 16.8 precision for LOD
 *  calculations.
 *
 *  On a Pentium M, this routine is
 *  20% faster than a 64-bit integer
 *  divide and also produces the log
 *  for free.
 *
 *************************************/

INLINE INT64 fast_reciplog(INT64 value, INT32 *log2)
{
	extern UINT32 voodoo_reciplog[];
	UINT32 temp, rlog;
	UINT32 interp;
	UINT32 *table;
	UINT64 recip;
	bool neg = false;
	int lz, exp = 0;

	/* always work with unsigned numbers */
	if (value < 0)
	{
		value = -value;
		neg = true;
	}

	/* if we've spilled out of 32 bits, push it down under 32 */
	if (value & LONGTYPE(0xffff00000000))
	{
		temp = (UINT32)(value >> 16);
		exp -= 16;
	}
	else
		temp = (UINT32)value;

	/* if the resulting value is 0, the reciprocal is infinite */
	if (GCC_UNLIKELY(temp == 0))
	{
		*log2 = 1000 << LOG_OUTPUT_PREC;
		return neg ? 0x80000000 : 0x7fffffff;
	}

	/* determine how many leading zeros in the value and shift it up high */
	lz = count_leading_zeros(temp);
	temp <<= lz;
	exp += lz;

	/* compute a pointer to the table entries we want */
	/* math is a bit funny here because we shift one less than we need to in order */
	/* to account for the fact that there are two UINT32's per table entry */
	table = &voodoo_reciplog[(temp >> (31 - RECIPLOG_LOOKUP_BITS - 1)) & ((2 << RECIPLOG_LOOKUP_BITS) - 2)];

	/* compute the interpolation value */
	interp = (temp >> (31 - RECIPLOG_LOOKUP_BITS - 8)) & 0xff;

	/* do a linear interpolatation between the two nearest table values */
	/* for both the log and the reciprocal */
	rlog = (table[1] * (0x100 - interp) + table[3] * interp) >> 8;
	recip = (table[0] * (0x100 - interp) + table[2] * interp) >> 8;

	/* the log result is the fractional part of the log; round it to the output precision */
	rlog = (rlog + (1 << (RECIPLOG_LOOKUP_PREC - LOG_OUTPUT_PREC - 1))) >> (RECIPLOG_LOOKUP_PREC - LOG_OUTPUT_PREC);

	/* the exponent is the non-fractional part of the log; normally, we would subtract it from rlog */
	/* but since we want the log(1/value) = -log(value), we subtract rlog from the exponent */
	*log2 = ((exp - (31 - RECIPLOG_INPUT_PREC)) << LOG_OUTPUT_PREC) - rlog;

	/* adjust the exponent to account for all the reciprocal-related parameters to arrive at a final shift amount */
	exp += (RECIP_OUTPUT_PREC - RECIPLOG_LOOKUP_PREC) - (31 - RECIPLOG_INPUT_PREC);

	/* shift by the exponent */
	if (exp < 0)
		recip >>= -exp;
	else
		recip <<= exp;

	/* on the way out, apply the original sign to the reciprocal */
	return neg ? -(INT64)recip : (INT64)recip;
}



/*************************************
 *
 *  Float-to-int conversions
 *
 *************************************/

INLINE INT32 float_to_int32(UINT32 data, int fixedbits)
{
	int exponent = ((data >> 23) & 0xff) - 127 - 23 + fixedbits;
	INT32 result = (data & 0x7fffff) | 0x800000;
	if (exponent < 0)
	{
		if (exponent > -32)
			result >>= -exponent;
		else
			result = 0;
	}
	else
	{
		if (exponent < 32)
			result <<= exponent;
		else
			result = 0x7fffffff;
	}
	if (data & 0x80000000)
		result = -result;
	return result;
}


INLINE INT64 float_to_int64(UINT32 data, int fixedbits)
{
	int exponent = ((data >> 23) & 0xff) - 127 - 23 + fixedbits;
	INT64 result = (data & 0x7fffff) | 0x800000;
	if (exponent < 0)
	{
		if (exponent > -64)
			result >>= -exponent;
		else
			result = 0;
	}
	else
	{
		if (exponent < 64)
			result <<= exponent;
		else
			result = LONGTYPE(0x7fffffffffffffff);
	}
	if (data & 0x80000000)
		result = -result;
	return result;
}



/*************************************
 *
 *  Rasterizer inlines
 *
 *************************************/

INLINE UINT32 normalize_color_path(UINT32 eff_color_path)
{
	/* ignore the subpixel adjust and texture enable flags */
	eff_color_path &= ~((1 << 26) | (1 << 27));

	return eff_color_path;
}


INLINE UINT32 normalize_alpha_mode(UINT32 eff_alpha_mode)
{
	/* always ignore alpha ref value */
	eff_alpha_mode &= ~(0xff << 24);

	/* if not doing alpha testing, ignore the alpha function and ref value */
	if (!ALPHAMODE_ALPHATEST(eff_alpha_mode))
		eff_alpha_mode &= ~(7 << 1);

	/* if not doing alpha blending, ignore the source and dest blending factors */
	if (!ALPHAMODE_ALPHABLEND(eff_alpha_mode))
		eff_alpha_mode &= ~((15 << 8) | (15 << 12) | (15 << 16) | (15 << 20));

	return eff_alpha_mode;
}


INLINE UINT32 normalize_fog_mode(UINT32 eff_fog_mode)
{
	/* if not doing fogging, ignore all the other fog bits */
	if (!FOGMODE_ENABLE_FOG(eff_fog_mode))
		eff_fog_mode = 0;

	return eff_fog_mode;
}


INLINE UINT32 normalize_fbz_mode(UINT32 eff_fbz_mode)
{
	/* ignore the draw buffer */
	eff_fbz_mode &= ~(3 << 14);

	return eff_fbz_mode;
}


INLINE UINT32 normalize_tex_mode(UINT32 eff_tex_mode)
{
	/* ignore the NCC table and seq_8_downld flags */
	eff_tex_mode &= ~((1 << 5) | (1 << 31));

	/* classify texture formats into 3 format categories */
	if (TEXMODE_FORMAT(eff_tex_mode) < 8)
		eff_tex_mode = (eff_tex_mode & ~(0xf << 8)) | (0 << 8);
	else if (TEXMODE_FORMAT(eff_tex_mode) >= 10 && TEXMODE_FORMAT(eff_tex_mode) <= 12)
		eff_tex_mode = (eff_tex_mode & ~(0xf << 8)) | (10 << 8);
	else
		eff_tex_mode = (eff_tex_mode & ~(0xf << 8)) | (8 << 8);

	return eff_tex_mode;
}


INLINE UINT32 compute_raster_hash(const raster_info *info)
{
	UINT32 hash;

	/* make a hash */
	hash = info->eff_color_path;
	hash = (hash << 1) | (hash >> 31);
	hash ^= info->eff_fbz_mode;
	hash = (hash << 1) | (hash >> 31);
	hash ^= info->eff_alpha_mode;
	hash = (hash << 1) | (hash >> 31);
	hash ^= info->eff_fog_mode;
	hash = (hash << 1) | (hash >> 31);
	hash ^= info->eff_tex_mode_0;
	hash = (hash << 1) | (hash >> 31);
	hash ^= info->eff_tex_mode_1;

	return hash % RASTER_HASH_SIZE;
}



/*************************************
 *
 *  Dithering macros
 *
 *************************************/

/* note that these equations and the dither matrixes have
   been confirmed to be exact matches to the real hardware */
#define DITHER_RB(val,dith)	((((val) << 1) - ((val) >> 4) + ((val) >> 7) + (dith)) >> 1)
#define DITHER_G(val,dith)	((((val) << 2) - ((val) >> 4) + ((val) >> 6) + (dith)) >> 2)

#define DECLARE_DITHER_POINTERS 												\
	const UINT8 *dither_lookup = NULL;											\
	const UINT8 *dither4 = NULL;												\
	const UINT8 *dither = NULL													\

#define COMPUTE_DITHER_POINTERS(FBZMODE, YY)									\
do																				\
{																				\
	/* compute the dithering pointers */										\
	if (FBZMODE_ENABLE_DITHERING(FBZMODE))										\
	{																			\
		dither4 = &dither_matrix_4x4[((YY) & 3) * 4];							\
		if (FBZMODE_DITHER_TYPE(FBZMODE) == 0)									\
		{																		\
			dither = dither4;													\
			dither_lookup = &dither4_lookup[(YY & 3) << 11];					\
		}																		\
		else																	\
		{																		\
			dither = &dither_matrix_2x2[((YY) & 3) * 4];						\
			dither_lookup = &dither2_lookup[(YY & 3) << 11];					\
		}																		\
	}																			\
}																				\
while (0)

#define APPLY_DITHER(FBZMODE, XX, DITHER_LOOKUP, RR, GG, BB)					\
do																				\
{																				\
	/* apply dithering */														\
	if (FBZMODE_ENABLE_DITHERING(FBZMODE))										\
	{																			\
		/* look up the dither value from the appropriate matrix */				\
		const UINT8 *dith = &DITHER_LOOKUP[((XX) & 3) << 1];					\
																				\
		/* apply dithering to R,G,B */											\
		(RR) = dith[((RR) << 3) + 0];											\
		(GG) = dith[((GG) << 3) + 1];											\
		(BB) = dith[((BB) << 3) + 0];											\
	}																			\
	else																		\
	{																			\
		(RR) >>= 3;																\
		(GG) >>= 2;																\
		(BB) >>= 3;																\
	}																			\
}																				\
while (0)



/*************************************
 *
 *  Clamping macros
 *
 *************************************/

#define CLAMPED_ARGB(ITERR, ITERG, ITERB, ITERA, FBZCP, RESULT)					\
do																				\
{																				\
	INT32 r = (INT32)(ITERR) >> 12;												\
	INT32 g = (INT32)(ITERG) >> 12;												\
	INT32 b = (INT32)(ITERB) >> 12;												\
	INT32 a = (INT32)(ITERA) >> 12;												\
																				\
	if (FBZCP_RGBZW_CLAMP(FBZCP) == 0)											\
	{																			\
		r &= 0xfff;																\
		RESULT.rgb.r = r;														\
		if (r == 0xfff)															\
			RESULT.rgb.r = 0;													\
		else if (r == 0x100)													\
			RESULT.rgb.r = 0xff;												\
																				\
		g &= 0xfff;																\
		RESULT.rgb.g = g;														\
		if (g == 0xfff)															\
			RESULT.rgb.g = 0;													\
		else if (g == 0x100)													\
			RESULT.rgb.g = 0xff;												\
																				\
		b &= 0xfff;																\
		RESULT.rgb.b = b;														\
		if (b == 0xfff)															\
			RESULT.rgb.b = 0;													\
		else if (b == 0x100)													\
			RESULT.rgb.b = 0xff;												\
																				\
		a &= 0xfff;																\
		RESULT.rgb.a = a;														\
		if (a == 0xfff)															\
			RESULT.rgb.a = 0;													\
		else if (a == 0x100)													\
			RESULT.rgb.a = 0xff;												\
	}																			\
	else																		\
	{																			\
		RESULT.rgb.r = (r < 0) ? 0 : (r > 0xff) ? 0xff : (UINT8)r;				\
		RESULT.rgb.g = (g < 0) ? 0 : (g > 0xff) ? 0xff : (UINT8)g;				\
		RESULT.rgb.b = (b < 0) ? 0 : (b > 0xff) ? 0xff : (UINT8)b;				\
		RESULT.rgb.a = (a < 0) ? 0 : (a > 0xff) ? 0xff : (UINT8)a;				\
	}																			\
}																				\
while (0)


#define CLAMPED_Z(ITERZ, FBZCP, RESULT)											\
do																				\
{																				\
	(RESULT) = (INT32)(ITERZ) >> 12;											\
	if (FBZCP_RGBZW_CLAMP(FBZCP) == 0)											\
	{																			\
		(RESULT) &= 0xfffff;													\
		if ((RESULT) == 0xfffff)												\
			(RESULT) = 0;														\
		else if ((RESULT) == 0x10000)											\
			(RESULT) = 0xffff;													\
		else																	\
			(RESULT) &= 0xffff;													\
	}																			\
	else																		\
	{																			\
		CLAMP((RESULT), 0, 0xffff);												\
	}																			\
}																				\
while (0)


#define CLAMPED_W(ITERW, FBZCP, RESULT)											\
do																				\
{																				\
	(RESULT) = (INT16)((ITERW) >> 32);											\
	if (FBZCP_RGBZW_CLAMP(FBZCP) == 0)											\
	{																			\
		(RESULT) &= 0xffff;														\
		if ((RESULT) == 0xffff)													\
			(RESULT) = 0;														\
		else if ((RESULT) == 0x100)												\
			(RESULT) = 0xff;													\
		(RESULT) &= 0xff;														\
	}																			\
	else																		\
	{																			\
		CLAMP((RESULT), 0, 0xff);												\
	}																			\
}																				\
while (0)



/*************************************
 *
 *  Chroma keying macro
 *
 *************************************/

#define APPLY_CHROMAKEY(VV, STATS, FBZMODE, COLOR)								\
do																				\
{																				\
	if (FBZMODE_ENABLE_CHROMAKEY(FBZMODE))										\
	{																			\
		/* non-range version */													\
		if (!CHROMARANGE_ENABLE((VV)->reg[chromaRange].u))						\
		{																		\
			if (((COLOR.u ^ (VV)->reg[chromaKey].u) & 0xffffff) == 0)			\
			{																	\
				(STATS)->chroma_fail++;											\
				goto skipdrawdepth;												\
			}																	\
		}																		\
																				\
		/* tricky range version */												\
		else																	\
		{																		\
			INT32 low, high, test;												\
			int results = 0;													\
																				\
			/* check blue */													\
			low = (VV)->reg[chromaKey].rgb.b;									\
			high = (VV)->reg[chromaRange].rgb.b;								\
			test = COLOR.rgb.b;													\
			results = (test >= low && test <= high);							\
			results ^= CHROMARANGE_BLUE_EXCLUSIVE((VV)->reg[chromaRange].u);	\
			results <<= 1;														\
																				\
			/* check green */													\
			low = (VV)->reg[chromaKey].rgb.g;									\
			high = (VV)->reg[chromaRange].rgb.g;								\
			test = COLOR.rgb.g;													\
			results |= (test >= low && test <= high);							\
			results ^= CHROMARANGE_GREEN_EXCLUSIVE((VV)->reg[chromaRange].u);	\
			results <<= 1;														\
																				\
			/* check red */														\
			low = (VV)->reg[chromaKey].rgb.r;									\
			high = (VV)->reg[chromaRange].rgb.r;								\
			test = COLOR.rgb.r;													\
			results |= (test >= low && test <= high);							\
			results ^= CHROMARANGE_RED_EXCLUSIVE((VV)->reg[chromaRange].u);		\
																				\
			/* final result */													\
			if (CHROMARANGE_UNION_MODE((VV)->reg[chromaRange].u))				\
			{																	\
				if (results != 0)												\
				{																\
					(STATS)->chroma_fail++;										\
					goto skipdrawdepth;											\
				}																\
			}																	\
			else																\
			{																	\
				if (results == 7)												\
				{																\
					(STATS)->chroma_fail++;										\
					goto skipdrawdepth;											\
				}																\
			}																	\
		}																		\
	}																			\
}																				\
while (0)



/*************************************
 *
 *  Alpha masking macro
 *
 *************************************/

#define APPLY_ALPHAMASK(VV, STATS, FBZMODE, AA)									\
do																				\
{																				\
	if (FBZMODE_ENABLE_ALPHA_MASK(FBZMODE))										\
	{																			\
		if (((AA) & 1) == 0)													\
		{																		\
			(STATS)->afunc_fail++;												\
			goto skipdrawdepth;													\
		}																		\
	}																			\
}																				\
while (0)



/*************************************
 *
 *  Alpha testing macro
 *
 *************************************/

#define APPLY_ALPHATEST(VV, STATS, ALPHAMODE, AA)								\
do																				\
{																				\
	if (ALPHAMODE_ALPHATEST(ALPHAMODE))											\
	{																			\
		UINT8 alpharef = (VV)->reg[alphaMode].rgb.a;							\
		switch (ALPHAMODE_ALPHAFUNCTION(ALPHAMODE))								\
		{																		\
			case 0:		/* alphaOP = never */									\
				(STATS)->afunc_fail++;											\
				goto skipdrawdepth;												\
																				\
			case 1:		/* alphaOP = less than */								\
				if ((AA) >= alpharef)											\
				{																\
					(STATS)->afunc_fail++;										\
					goto skipdrawdepth;											\
				}																\
				break;															\
																				\
			case 2:		/* alphaOP = equal */									\
				if ((AA) != alpharef)											\
				{																\
					(STATS)->afunc_fail++;										\
					goto skipdrawdepth;											\
				}																\
				break;															\
																				\
			case 3:		/* alphaOP = less than or equal */						\
				if ((AA) > alpharef)											\
				{																\
					(STATS)->afunc_fail++;										\
					goto skipdrawdepth;											\
				}																\
				break;															\
																				\
			case 4:		/* alphaOP = greater than */							\
				if ((AA) <= alpharef)											\
				{																\
					(STATS)->afunc_fail++;										\
					goto skipdrawdepth;											\
				}																\
				break;															\
																				\
			case 5:		/* alphaOP = not equal */								\
				if ((AA) == alpharef)											\
				{																\
					(STATS)->afunc_fail++;										\
					goto skipdrawdepth;											\
				}																\
				break;															\
																				\
			case 6:		/* alphaOP = greater than or equal */					\
				if ((AA) < alpharef)											\
				{																\
					(STATS)->afunc_fail++;										\
					goto skipdrawdepth;											\
				}																\
				break;															\
																				\
			case 7:		/* alphaOP = always */									\
				break;															\
		}																		\
	}																			\
}																				\
while (0)



/*************************************
 *
 *  Alpha blending macro
 *
 *************************************/

#define APPLY_ALPHA_BLEND(FBZMODE, ALPHAMODE, XX, DITHER, RR, GG, BB, AA)		\
do																				\
{																				\
	if (ALPHAMODE_ALPHABLEND(ALPHAMODE))										\
	{																			\
		int dpix = dest[XX];													\
		int dr = (dpix >> 8) & 0xf8;											\
		int dg = (dpix >> 3) & 0xfc;											\
		int db = (dpix << 3) & 0xf8;											\
		int da = (FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) && depth) ? depth[XX] : 0xff;		\
		int sr = (RR);															\
		int sg = (GG);															\
		int sb = (BB);															\
		int sa = (AA);															\
		int ta;																	\
																				\
		/* apply dither subtraction */											\
		if ((FBZMODE_ALPHA_DITHER_SUBTRACT(FBZMODE)) && DITHER)					\
		{																		\
			/* look up the dither value from the appropriate matrix */			\
			int dith = DITHER[(XX) & 3];										\
																				\
			/* subtract the dither value */										\
			dr = ((dr << 1) + 15 - dith) >> 1;									\
			dg = ((dg << 2) + 15 - dith) >> 2;									\
			db = ((db << 1) + 15 - dith) >> 1;									\
		}																		\
																				\
		/* compute source portion */											\
		switch (ALPHAMODE_SRCRGBBLEND(ALPHAMODE))								\
		{																		\
			default:	/* reserved */											\
			case 0:		/* AZERO */												\
				(RR) = (GG) = (BB) = 0;											\
				break;															\
																				\
			case 1:		/* ASRC_ALPHA */										\
				(RR) = (sr * (sa + 1)) >> 8;									\
				(GG) = (sg * (sa + 1)) >> 8;									\
				(BB) = (sb * (sa + 1)) >> 8;									\
				break;															\
																				\
			case 2:		/* A_COLOR */											\
				(RR) = (sr * (dr + 1)) >> 8;									\
				(GG) = (sg * (dg + 1)) >> 8;									\
				(BB) = (sb * (db + 1)) >> 8;									\
				break;															\
																				\
			case 3:		/* ADST_ALPHA */										\
				(RR) = (sr * (da + 1)) >> 8;									\
				(GG) = (sg * (da + 1)) >> 8;									\
				(BB) = (sb * (da + 1)) >> 8;									\
				break;															\
																				\
			case 4:		/* AONE */												\
				break;															\
																				\
			case 5:		/* AOMSRC_ALPHA */										\
				(RR) = (sr * (0x100 - sa)) >> 8;								\
				(GG) = (sg * (0x100 - sa)) >> 8;								\
				(BB) = (sb * (0x100 - sa)) >> 8;								\
				break;															\
																				\
			case 6:		/* AOM_COLOR */											\
				(RR) = (sr * (0x100 - dr)) >> 8;								\
				(GG) = (sg * (0x100 - dg)) >> 8;								\
				(BB) = (sb * (0x100 - db)) >> 8;								\
				break;															\
																				\
			case 7:		/* AOMDST_ALPHA */										\
				(RR) = (sr * (0x100 - da)) >> 8;								\
				(GG) = (sg * (0x100 - da)) >> 8;								\
				(BB) = (sb * (0x100 - da)) >> 8;								\
				break;															\
																				\
			case 15:	/* ASATURATE */											\
				ta = (sa < (0x100 - da)) ? sa : (0x100 - da);					\
				(RR) = (sr * (ta + 1)) >> 8;									\
				(GG) = (sg * (ta + 1)) >> 8;									\
				(BB) = (sb * (ta + 1)) >> 8;									\
				break;															\
		}																		\
																				\
		/* add in dest portion */												\
		switch (ALPHAMODE_DSTRGBBLEND(ALPHAMODE))								\
		{																		\
			default:	/* reserved */											\
			case 0:		/* AZERO */												\
				break;															\
																				\
			case 1:		/* ASRC_ALPHA */										\
				(RR) += (dr * (sa + 1)) >> 8;									\
				(GG) += (dg * (sa + 1)) >> 8;									\
				(BB) += (db * (sa + 1)) >> 8;									\
				break;															\
																				\
			case 2:		/* A_COLOR */											\
				(RR) += (dr * (sr + 1)) >> 8;									\
				(GG) += (dg * (sg + 1)) >> 8;									\
				(BB) += (db * (sb + 1)) >> 8;									\
				break;															\
																				\
			case 3:		/* ADST_ALPHA */										\
				(RR) += (dr * (da + 1)) >> 8;									\
				(GG) += (dg * (da + 1)) >> 8;									\
				(BB) += (db * (da + 1)) >> 8;									\
				break;															\
																				\
			case 4:		/* AONE */												\
				(RR) += dr;														\
				(GG) += dg;														\
				(BB) += db;														\
				break;															\
																				\
			case 5:		/* AOMSRC_ALPHA */										\
				(RR) += (dr * (0x100 - sa)) >> 8;								\
				(GG) += (dg * (0x100 - sa)) >> 8;								\
				(BB) += (db * (0x100 - sa)) >> 8;								\
				break;															\
																				\
			case 6:		/* AOM_COLOR */											\
				(RR) += (dr * (0x100 - sr)) >> 8;								\
				(GG) += (dg * (0x100 - sg)) >> 8;								\
				(BB) += (db * (0x100 - sb)) >> 8;								\
				break;															\
																				\
			case 7:		/* AOMDST_ALPHA */										\
				(RR) += (dr * (0x100 - da)) >> 8;								\
				(GG) += (dg * (0x100 - da)) >> 8;								\
				(BB) += (db * (0x100 - da)) >> 8;								\
				break;															\
																				\
			case 15:	/* A_COLORBEFOREFOG */									\
				(RR) += (dr * (prefogr + 1)) >> 8;								\
				(GG) += (dg * (prefogg + 1)) >> 8;								\
				(BB) += (db * (prefogb + 1)) >> 8;								\
				break;															\
		}																		\
																				\
		/* blend the source alpha */											\
		(AA) = 0;																\
		if (ALPHAMODE_SRCALPHABLEND(ALPHAMODE) == 4)							\
			(AA) = sa;															\
																				\
		/* blend the dest alpha */												\
		if (ALPHAMODE_DSTALPHABLEND(ALPHAMODE) == 4)							\
			(AA) += da;															\
																				\
		/* clamp */																\
		CLAMP((RR), 0x00, 0xff);												\
		CLAMP((GG), 0x00, 0xff);												\
		CLAMP((BB), 0x00, 0xff);												\
		CLAMP((AA), 0x00, 0xff);												\
	}																			\
}																				\
while (0)



/*************************************
 *
 *  Fogging macro
 *
 *************************************/

#define APPLY_FOGGING(VV, FOGMODE, FBZCP, XX, DITHER4, RR, GG, BB, ITERZ, ITERW, ITERAXXX)	\
do																				\
{																				\
	if (FOGMODE_ENABLE_FOG(FOGMODE))											\
	{																			\
		rgb_union fogcolor = (VV)->reg[fogColor];								\
		INT32 fr, fg, fb;														\
																				\
		/* constant fog bypasses everything else */								\
		if (FOGMODE_FOG_CONSTANT(FOGMODE))										\
		{																		\
			fr = fogcolor.rgb.r;												\
			fg = fogcolor.rgb.g;												\
			fb = fogcolor.rgb.b;												\
		}																		\
																				\
		/* non-constant fog comes from several sources */						\
		else																	\
		{																		\
			INT32 fogblend = 0;													\
																				\
			/* if fog_add is zero, we start with the fog color */				\
			if (FOGMODE_FOG_ADD(FOGMODE) == 0)									\
			{																	\
				fr = fogcolor.rgb.r;											\
				fg = fogcolor.rgb.g;											\
				fb = fogcolor.rgb.b;											\
			}																	\
			else																\
				fr = fg = fb = 0;												\
																				\
			/* if fog_mult is zero, we subtract the incoming color */			\
			if (FOGMODE_FOG_MULT(FOGMODE) == 0)									\
			{																	\
				fr -= (RR);														\
				fg -= (GG);														\
				fb -= (BB);														\
			}																	\
																				\
			/* fog blending mode */												\
			switch (FOGMODE_FOG_ZALPHA(FOGMODE))								\
			{																	\
				case 0:		/* fog table */										\
				{																\
					INT32 delta = (VV)->fbi.fogdelta[wfloat >> 10];				\
					INT32 deltaval;												\
																				\
					/* perform the multiply against lower 8 bits of wfloat */	\
					deltaval = (delta & (VV)->fbi.fogdelta_mask) *				\
								((wfloat >> 2) & 0xff);							\
																				\
					/* fog zones allow for negating this value */				\
					if (FOGMODE_FOG_ZONES(FOGMODE) && (delta & 2))				\
						deltaval = -deltaval;									\
					deltaval >>= 6;												\
																				\
					/* apply dither */											\
					if (FOGMODE_FOG_DITHER(FOGMODE))							\
						if (DITHER4)											\
							deltaval += DITHER4[(XX) & 3];						\
					deltaval >>= 4;												\
																				\
					/* add to the blending factor */							\
					fogblend = (VV)->fbi.fogblend[wfloat >> 10] + deltaval;		\
					break;														\
				}																\
																				\
				case 1:		/* iterated A */									\
					fogblend = ITERAXXX.rgb.a;									\
					break;														\
																				\
				case 2:		/* iterated Z */									\
					CLAMPED_Z((ITERZ), FBZCP, fogblend);						\
					fogblend >>= 8;												\
					break;														\
																				\
				case 3:		/* iterated W - Voodoo 2 only */					\
					CLAMPED_W((ITERW), FBZCP, fogblend);						\
					break;														\
			}																	\
																				\
			/* perform the blend */												\
			fogblend++;															\
			fr = (fr * fogblend) >> 8;											\
			fg = (fg * fogblend) >> 8;											\
			fb = (fb * fogblend) >> 8;											\
		}																		\
																				\
		/* if fog_mult is 0, we add this to the original color */				\
		if (FOGMODE_FOG_MULT(FOGMODE) == 0)										\
		{																		\
			(RR) += fr;															\
			(GG) += fg;															\
			(BB) += fb;															\
		}																		\
																				\
		/* otherwise this just becomes the new color */							\
		else																	\
		{																		\
			(RR) = fr;															\
			(GG) = fg;															\
			(BB) = fb;															\
		}																		\
																				\
		/* clamp */																\
		CLAMP((RR), 0x00, 0xff);												\
		CLAMP((GG), 0x00, 0xff);												\
		CLAMP((BB), 0x00, 0xff);												\
	}																			\
}																				\
while (0)



/*************************************
 *
 *  Texture pipeline macro
 *
 *************************************/

#define TEXTURE_PIPELINE(TT, XX, DITHER4, TEXMODE, COTHER, LOOKUP, LODBASE, ITERS, ITERT, ITERW, RESULT) \
do																				\
{																				\
	INT32 blendr, blendg, blendb, blenda;										\
	INT32 tr, tg, tb, ta;														\
	INT32 s, t, lod, ilod;														\
	INT64 oow;																	\
	INT32 smax, tmax;															\
	UINT32 texbase;																\
	rgb_union c_local;															\
																				\
	/* determine the S/T/LOD values for this texture */							\
	if (TEXMODE_ENABLE_PERSPECTIVE(TEXMODE))									\
	{																			\
		oow = fast_reciplog((ITERW), &lod);										\
		s = (INT32)((oow * (ITERS)) >> 29);										\
		t = (INT32)((oow * (ITERT)) >> 29);										\
		lod += (LODBASE);														\
	}																			\
	else																		\
	{																			\
		s = (INT32)((ITERS) >> 14);												\
		t = (INT32)((ITERT) >> 14);												\
		lod = (LODBASE);														\
	}																			\
																				\
	/* clamp W */																\
	if (TEXMODE_CLAMP_NEG_W(TEXMODE) && (ITERW) < 0)							\
		s = t = 0;																\
																				\
	/* clamp the LOD */															\
	lod += (TT)->lodbias;														\
	if (TEXMODE_ENABLE_LOD_DITHER(TEXMODE))										\
		if (DITHER4)															\
			lod += DITHER4[(XX) & 3] << 4;										\
	if (lod < (TT)->lodmin)														\
		lod = (TT)->lodmin;														\
	if (lod > (TT)->lodmax)														\
		lod = (TT)->lodmax;														\
																				\
	/* now the LOD is in range; if we don't own this LOD, take the next one */	\
	ilod = lod >> 8;															\
	if (!(((TT)->lodmask >> ilod) & 1))											\
		ilod++;																	\
																				\
	/* fetch the texture base */												\
	texbase = (TT)->lodoffset[ilod];											\
																				\
	/* compute the maximum s and t values at this LOD */						\
	smax = (TT)->wmask >> ilod;													\
	tmax = (TT)->hmask >> ilod;													\
																				\
	/* determine whether we are point-sampled or bilinear */					\
	if ((lod == (TT)->lodmin && !TEXMODE_MAGNIFICATION_FILTER(TEXMODE)) ||		\
		(lod != (TT)->lodmin && !TEXMODE_MINIFICATION_FILTER(TEXMODE)))			\
	{																			\
		/* point sampled */														\
																				\
		UINT32 texel0;															\
																				\
		/* adjust S/T for the LOD and strip off the fractions */				\
		s >>= ilod + 18;														\
		t >>= ilod + 18;														\
																				\
		/* clamp/wrap S/T if necessary */										\
		if (TEXMODE_CLAMP_S(TEXMODE))											\
			CLAMP(s, 0, smax);													\
		if (TEXMODE_CLAMP_T(TEXMODE))											\
			CLAMP(t, 0, tmax);													\
		s &= smax;																\
		t &= tmax;																\
		t *= smax + 1;															\
																				\
		/* fetch texel data */													\
		if (TEXMODE_FORMAT(TEXMODE) < 8)										\
		{																		\
			texel0 = *(UINT8 *)&(TT)->ram[(texbase + t + s) & (TT)->mask];		\
			c_local.u = (LOOKUP)[texel0];										\
		}																		\
		else																	\
		{																		\
			texel0 = *(UINT16 *)&(TT)->ram[(texbase + 2*(t + s)) & (TT)->mask];	\
			if (TEXMODE_FORMAT(TEXMODE) >= 10 && TEXMODE_FORMAT(TEXMODE) <= 12)	\
				c_local.u = (LOOKUP)[texel0];									\
			else																\
				c_local.u = ((LOOKUP)[texel0 & 0xff] & 0xffffff) |				\
							((texel0 & 0xff00) << 16);							\
		}																		\
	}																			\
	else																		\
	{																			\
		/* bilinear filtered */													\
																				\
		UINT32 texel0, texel1, texel2, texel3;									\
		UINT8 sfrac, tfrac;														\
		INT32 s1, t1;															\
																				\
		/* adjust S/T for the LOD and strip off all but the low 8 bits of */	\
		/* the fraction */														\
		s >>= ilod + 10;														\
		t >>= ilod + 10;														\
																				\
		/* also subtract 1/2 texel so that (0.5,0.5) = a full (0,0) texel */	\
		s -= 0x80;																\
		t -= 0x80;																\
																				\
		/* extract the fractions */												\
		sfrac = (UINT8)(s & (TT)->bilinear_mask);								\
		tfrac = (UINT8)(t & (TT)->bilinear_mask);								\
																				\
		/* now toss the rest */													\
		s >>= 8;																\
		t >>= 8;																\
		s1 = s + 1;																\
		t1 = t + 1;																\
																				\
		/* clamp/wrap S/T if necessary */										\
		if (TEXMODE_CLAMP_S(TEXMODE))											\
		{																		\
			CLAMP(s, 0, smax);													\
			CLAMP(s1, 0, smax);													\
		}																		\
		if (TEXMODE_CLAMP_T(TEXMODE))											\
		{																		\
			CLAMP(t, 0, tmax);													\
			CLAMP(t1, 0, tmax);													\
		}																		\
		s &= smax;																\
		s1 &= smax;																\
		t &= tmax;																\
		t1 &= tmax;																\
		t *= smax + 1;															\
		t1 *= smax + 1;															\
																				\
		/* fetch texel data */													\
		if (TEXMODE_FORMAT(TEXMODE) < 8)										\
		{																		\
			texel0 = *(UINT8 *)&(TT)->ram[(texbase + t + s) & (TT)->mask];		\
			texel1 = *(UINT8 *)&(TT)->ram[(texbase + t + s1) & (TT)->mask];		\
			texel2 = *(UINT8 *)&(TT)->ram[(texbase + t1 + s) & (TT)->mask];		\
			texel3 = *(UINT8 *)&(TT)->ram[(texbase + t1 + s1) & (TT)->mask];	\
			texel0 = (LOOKUP)[texel0];											\
			texel1 = (LOOKUP)[texel1];											\
			texel2 = (LOOKUP)[texel2];											\
			texel3 = (LOOKUP)[texel3];											\
		}																		\
		else																	\
		{																		\
			texel0 = *(UINT16 *)&(TT)->ram[(texbase + 2*(t + s)) & (TT)->mask];	\
			texel1 = *(UINT16 *)&(TT)->ram[(texbase + 2*(t + s1)) & (TT)->mask];\
			texel2 = *(UINT16 *)&(TT)->ram[(texbase + 2*(t1 + s)) & (TT)->mask];\
			texel3 = *(UINT16 *)&(TT)->ram[(texbase + 2*(t1 + s1)) & (TT)->mask];\
			if (TEXMODE_FORMAT(TEXMODE) >= 10 && TEXMODE_FORMAT(TEXMODE) <= 12)	\
			{																	\
				texel0 = (LOOKUP)[texel0];										\
				texel1 = (LOOKUP)[texel1];										\
				texel2 = (LOOKUP)[texel2];										\
				texel3 = (LOOKUP)[texel3];										\
			}																	\
			else																\
			{																	\
				texel0 = ((LOOKUP)[texel0 & 0xff] & 0xffffff) | 				\
							((texel0 & 0xff00) << 16);							\
				texel1 = ((LOOKUP)[texel1 & 0xff] & 0xffffff) | 				\
							((texel1 & 0xff00) << 16);							\
				texel2 = ((LOOKUP)[texel2 & 0xff] & 0xffffff) | 				\
							((texel2 & 0xff00) << 16);							\
				texel3 = ((LOOKUP)[texel3 & 0xff] & 0xffffff) | 				\
							((texel3 & 0xff00) << 16);							\
			}																	\
		}																		\
																				\
		/* weigh in each texel */												\
		c_local.u = rgba_bilinear_filter(texel0, texel1, texel2, texel3, sfrac, tfrac);\
	}																			\
																				\
	/* select zero/other for RGB */												\
	if (!TEXMODE_TC_ZERO_OTHER(TEXMODE))										\
	{																			\
		tr = COTHER.rgb.r;														\
		tg = COTHER.rgb.g;														\
		tb = COTHER.rgb.b;														\
	}																			\
	else																		\
		tr = tg = tb = 0;														\
																				\
	/* select zero/other for alpha */											\
	if (!TEXMODE_TCA_ZERO_OTHER(TEXMODE))										\
		ta = COTHER.rgb.a;														\
	else																		\
		ta = 0;																	\
																				\
	/* potentially subtract c_local */											\
	if (TEXMODE_TC_SUB_CLOCAL(TEXMODE))											\
	{																			\
		tr -= c_local.rgb.r;													\
		tg -= c_local.rgb.g;													\
		tb -= c_local.rgb.b;													\
	}																			\
	if (TEXMODE_TCA_SUB_CLOCAL(TEXMODE))										\
		ta -= c_local.rgb.a;													\
																				\
	/* blend RGB */																\
	switch (TEXMODE_TC_MSELECT(TEXMODE))										\
	{																			\
		default:	/* reserved */												\
		case 0:		/* zero */													\
			blendr = blendg = blendb = 0;										\
			break;																\
																				\
		case 1:		/* c_local */												\
			blendr = c_local.rgb.r;												\
			blendg = c_local.rgb.g;												\
			blendb = c_local.rgb.b;												\
			break;																\
																				\
		case 2:		/* a_other */												\
			blendr = blendg = blendb = COTHER.rgb.a;							\
			break;																\
																				\
		case 3:		/* a_local */												\
			blendr = blendg = blendb = c_local.rgb.a;							\
			break;																\
																				\
		case 4:		/* LOD (detail factor) */									\
			if ((TT)->detailbias <= lod)										\
				blendr = blendg = blendb = 0;									\
			else																\
			{																	\
				blendr = ((((TT)->detailbias - lod) << (TT)->detailscale) >> 8);\
				if (blendr > (TT)->detailmax)									\
					blendr = (TT)->detailmax;									\
				blendg = blendb = blendr;										\
			}																	\
			break;																\
																				\
		case 5:		/* LOD fraction */											\
			blendr = blendg = blendb = lod & 0xff;								\
			break;																\
	}																			\
																				\
	/* blend alpha */															\
	switch (TEXMODE_TCA_MSELECT(TEXMODE))										\
	{																			\
		default:	/* reserved */												\
		case 0:		/* zero */													\
			blenda = 0;															\
			break;																\
																				\
		case 1:		/* c_local */												\
			blenda = c_local.rgb.a;												\
			break;																\
																				\
		case 2:		/* a_other */												\
			blenda = COTHER.rgb.a;												\
			break;																\
																				\
		case 3:		/* a_local */												\
			blenda = c_local.rgb.a;												\
			break;																\
																				\
		case 4:		/* LOD (detail factor) */									\
			if ((TT)->detailbias <= lod)										\
				blenda = 0;														\
			else																\
			{																	\
				blenda = ((((TT)->detailbias - lod) << (TT)->detailscale) >> 8);\
				if (blenda > (TT)->detailmax)									\
					blenda = (TT)->detailmax;									\
			}																	\
			break;																\
																				\
		case 5:		/* LOD fraction */											\
			blenda = lod & 0xff;												\
			break;																\
	}																			\
																				\
	/* reverse the RGB blend */													\
	if (!TEXMODE_TC_REVERSE_BLEND(TEXMODE))										\
	{																			\
		blendr ^= 0xff;															\
		blendg ^= 0xff;															\
		blendb ^= 0xff;															\
	}																			\
																				\
	/* reverse the alpha blend */												\
	if (!TEXMODE_TCA_REVERSE_BLEND(TEXMODE))									\
		blenda ^= 0xff;															\
																				\
	/* do the blend */															\
	tr = (tr * (blendr + 1)) >> 8;												\
	tg = (tg * (blendg + 1)) >> 8;												\
	tb = (tb * (blendb + 1)) >> 8;												\
	ta = (ta * (blenda + 1)) >> 8;												\
																				\
	/* add clocal or alocal to RGB */											\
	switch (TEXMODE_TC_ADD_ACLOCAL(TEXMODE))									\
	{																			\
		case 3:		/* reserved */												\
		case 0:		/* nothing */												\
			break;																\
																				\
		case 1:		/* add c_local */											\
			tr += c_local.rgb.r;												\
			tg += c_local.rgb.g;												\
			tb += c_local.rgb.b;												\
			break;																\
																				\
		case 2:		/* add_alocal */											\
			tr += c_local.rgb.a;												\
			tg += c_local.rgb.a;												\
			tb += c_local.rgb.a;												\
			break;																\
	}																			\
																				\
	/* add clocal or alocal to alpha */											\
	if (TEXMODE_TCA_ADD_ACLOCAL(TEXMODE))										\
		ta += c_local.rgb.a;													\
																				\
	/* clamp */																	\
	RESULT.rgb.r = (tr < 0) ? 0 : (tr > 0xff) ? 0xff : (UINT8)tr;				\
	RESULT.rgb.g = (tg < 0) ? 0 : (tg > 0xff) ? 0xff : (UINT8)tg;				\
	RESULT.rgb.b = (tb < 0) ? 0 : (tb > 0xff) ? 0xff : (UINT8)tb;				\
	RESULT.rgb.a = (ta < 0) ? 0 : (ta > 0xff) ? 0xff : (UINT8)ta;				\
																				\
	/* invert */																\
	if (TEXMODE_TC_INVERT_OUTPUT(TEXMODE))										\
		RESULT.u ^= 0x00ffffff;													\
	if (TEXMODE_TCA_INVERT_OUTPUT(TEXMODE))										\
		RESULT.rgb.a ^= 0xff;													\
}																				\
while (0)



/*************************************
 *
 *  Pixel pipeline macros
 *
 *************************************/

#define PIXEL_PIPELINE_BEGIN(VV, XX, YY, FBZCOLORPATH, FBZMODE, ITERZ, ITERW)	\
do																				\
{																				\
	INT32 depthval, wfloat;														\
	INT32 prefogr, prefogg, prefogb;											\
	INT32 r, g, b, a;															\
																				\
	/* apply clipping */														\
	/* note that for perf reasons, we assume the caller has done clipping */	\
																				\
	/* handle stippling */														\
	if (FBZMODE_ENABLE_STIPPLE(FBZMODE))										\
	{																			\
		/* rotate mode */														\
		if (FBZMODE_STIPPLE_PATTERN(FBZMODE) == 0)								\
		{																		\
			(VV)->reg[stipple].u = ((VV)->reg[stipple].u << 1) | ((VV)->reg[stipple].u >> 31);\
			if (((VV)->reg[stipple].u & 0x80000000) == 0)						\
			{																	\
				goto skipdrawdepth;												\
			}																	\
		}																		\
																				\
		/* pattern mode */														\
		else																	\
		{																		\
			int stipple_index = (((YY) & 3) << 3) | (~(XX) & 7);				\
			if ((((VV)->reg[stipple].u >> stipple_index) & 1) == 0)				\
			{																	\
				goto skipdrawdepth;												\
			}																	\
		}																		\
	}																			\
																				\
	/* compute "floating point" W value (used for depth and fog) */				\
	if ((ITERW) & LONGTYPE(0xffff00000000))											\
		wfloat = 0x0000;														\
	else																		\
	{																			\
		UINT32 temp = (UINT32)(ITERW);											\
		if ((temp & 0xffff0000) == 0)											\
			wfloat = 0xffff;													\
		else																	\
		{																		\
			int exp = count_leading_zeros(temp);								\
			wfloat = ((exp << 12) | ((~temp >> (19 - exp)) & 0xfff));			\
			if (wfloat < 0xffff) wfloat++;										\
		}																		\
	}																			\
																				\
	/* compute depth value (W or Z) for this pixel */							\
	if (FBZMODE_WBUFFER_SELECT(FBZMODE) == 0)									\
		CLAMPED_Z(ITERZ, FBZCOLORPATH, depthval);								\
	else if (FBZMODE_DEPTH_FLOAT_SELECT(FBZMODE) == 0)							\
		depthval = wfloat;														\
	else																		\
	{																			\
		if ((ITERZ) & 0xf0000000)												\
			depthval = 0x0000;													\
		else																	\
		{																		\
			UINT32 temp = (ITERZ) << 4;											\
			if ((temp & 0xffff0000) == 0)										\
				depthval = 0xffff;												\
			else																\
			{																	\
				int exp = count_leading_zeros(temp);							\
				depthval = ((exp << 12) | ((~temp >> (19 - exp)) & 0xfff));		\
				if (depthval < 0xffff) depthval++;								\
			}																	\
		}																		\
	}																			\
																				\
	/* add the bias */															\
	if (FBZMODE_ENABLE_DEPTH_BIAS(FBZMODE))										\
	{																			\
		depthval += (INT16)(VV)->reg[zaColor].u;								\
		CLAMP(depthval, 0, 0xffff);												\
	}																			\
																				\
	/* handle depth buffer testing */											\
	if (FBZMODE_ENABLE_DEPTHBUF(FBZMODE))										\
	{																			\
		INT32 depthsource;														\
																				\
		/* the source depth is either the iterated W/Z+bias or a */				\
		/* constant value */													\
		if (FBZMODE_DEPTH_SOURCE_COMPARE(FBZMODE) == 0)							\
			depthsource = depthval;												\
		else																	\
			depthsource = (UINT16)(VV)->reg[zaColor].u;							\
																				\
		/* test against the depth buffer */										\
		switch (FBZMODE_DEPTH_FUNCTION(FBZMODE))								\
		{																		\
			case 0:		/* depthOP = never */									\
				goto skipdrawdepth;												\
																				\
			case 1:		/* depthOP = less than */								\
				if (depth)														\
					if (depthsource >= depth[XX])								\
					{															\
						goto skipdrawdepth;										\
					}															\
				break;															\
																				\
			case 2:		/* depthOP = equal */									\
				if (depth)														\
					if (depthsource != depth[XX])								\
					{															\
						goto skipdrawdepth;										\
					}															\
				break;															\
																				\
			case 3:		/* depthOP = less than or equal */						\
				if (depth)														\
					if (depthsource > depth[XX])								\
					{															\
						goto skipdrawdepth;										\
					}															\
				break;															\
																				\
			case 4:		/* depthOP = greater than */							\
				if (depth)														\
					if (depthsource <= depth[XX])								\
					{															\
						goto skipdrawdepth;										\
					}															\
				break;															\
																				\
			case 5:		/* depthOP = not equal */								\
				if (depth)														\
					if (depthsource == depth[XX])								\
					{															\
						goto skipdrawdepth;										\
					}															\
				break;															\
																				\
			case 6:		/* depthOP = greater than or equal */					\
				if (depth)														\
					if (depthsource < depth[XX])								\
					{															\
						goto skipdrawdepth;										\
					}															\
				break;															\
																				\
			case 7:		/* depthOP = always */									\
				break;															\
		}																		\
	}


#define PIXEL_PIPELINE_MODIFY(VV, DITHER, DITHER4, XX, FBZMODE, FBZCOLORPATH, ALPHAMODE, FOGMODE, ITERZ, ITERW, ITERAXXX) \
																				\
	/* perform fogging */														\
	prefogr = r;																\
	prefogg = g;																\
	prefogb = b;																\
	APPLY_FOGGING(VV, FOGMODE, FBZCOLORPATH, XX, DITHER4, r, g, b,				\
					ITERZ, ITERW, ITERAXXX);									\
																				\
	/* perform alpha blending */												\
	APPLY_ALPHA_BLEND(FBZMODE, ALPHAMODE, XX, DITHER, r, g, b, a);


#define PIXEL_PIPELINE_FINISH(VV, DITHER_LOOKUP, XX, dest, depth, FBZMODE)		\
																				\
	/* write to framebuffer */													\
	if (FBZMODE_RGB_BUFFER_MASK(FBZMODE))										\
	{																			\
		/* apply dithering */													\
		APPLY_DITHER(FBZMODE, XX, DITHER_LOOKUP, r, g, b);						\
		dest[XX] = (UINT16)((r << 11) | (g << 5) | b);							\
	}																			\
																				\
	/* write to aux buffer */													\
	if (depth && FBZMODE_AUX_BUFFER_MASK(FBZMODE))								\
	{																			\
		if (FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) == 0)							\
			depth[XX] = (UINT16)depthval;										\
		else																	\
			depth[XX] = (UINT16)a;												\
	}

#define PIXEL_PIPELINE_END(STATS)												\
																				\
	/* track pixel writes to the frame buffer regardless of mask */				\
	(STATS)->pixels_out++;														\
																				\
skipdrawdepth:																	\
	;																			\
}																				\
while (0)

#endif
