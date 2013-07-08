/*
 *  Copyright (C) 2002-2010  The DOSBox Team
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

#ifndef DOSBOX_DOSBOX_H
#ifdef _MSC_VER
#define INLINE __forceinline
#else
#define INLINE inline
#endif
#endif

#define CODEC_4CC "ZMBV"

typedef enum {
	ZMBV_FORMAT_NONE		= 0x00,
	ZMBV_FORMAT_1BPP		= 0x01,
	ZMBV_FORMAT_2BPP		= 0x02,
	ZMBV_FORMAT_4BPP		= 0x03,
	ZMBV_FORMAT_8BPP		= 0x04,
	ZMBV_FORMAT_15BPP	= 0x05,
	ZMBV_FORMAT_16BPP	= 0x06,
	ZMBV_FORMAT_24BPP	= 0x07,
	ZMBV_FORMAT_32BPP	= 0x08
} zmbv_format_t;

void Msg(const char fmt[], ...);
class VideoCodec {
private:
	struct FrameBlock {
		int start;
		int dx,dy;
	};
	struct CodecVector {
		int x,y;
		int slot;
	};
	struct KeyframeHeader {
		unsigned char high_version;
		unsigned char low_version;
		unsigned char compression;
		unsigned char format;
		unsigned char blockwidth,blockheight;
	};

	struct {
		int		linesDone;
		int		writeSize;
		int		writeDone;
		unsigned char	*writeBuf;
	} compress;

	CodecVector VectorTable[512];
	int VectorCount;

	unsigned char *oldframe, *newframe;
	unsigned char *buf1, *buf2, *work;
	int bufsize;

	int blockcount; 
	FrameBlock * blocks;

	int workUsed, workPos;

	int palsize;
	char palette[256*4];
	int height, width, pitch;
	zmbv_format_t format;
	int pixelsize;

	z_stream zstream;

	// methods
	void FreeBuffers(void);
	void CreateVectorTable(void);
	bool SetupBuffers(zmbv_format_t format, int blockwidth, int blockheight);

	template<class P>
		void AddXorFrame(void);
	template<class P>
		void UnXorFrame(void);
	template<class P>
		INLINE int PossibleBlock(int vx,int vy,FrameBlock * block);
	template<class P>
		INLINE int CompareBlock(int vx,int vy,FrameBlock * block);
	template<class P>
		INLINE void AddXorBlock(int vx,int vy,FrameBlock * block);
	template<class P>
		INLINE void UnXorBlock(int vx,int vy,FrameBlock * block);
	template<class P>
		INLINE void CopyBlock(int vx, int vy,FrameBlock * block);
public:
	VideoCodec();
	bool SetupCompress( int _width, int _height);
	bool SetupDecompress( int _width, int _height);
	zmbv_format_t BPPFormat( int bpp );
	int NeededSize( int _width, int _height, zmbv_format_t _format);

	void CompressLines(int lineCount, void *lineData[]);
	bool PrepareCompressFrame(int flags,  zmbv_format_t _format, char * pal, void *writeBuf, int writeSize);
	int FinishCompressFrame( void );
	bool DecompressFrame(void * framedata, int size);
	void Output_UpsideDown_24(void * output);
};
