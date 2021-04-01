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

#include "config.h"

#if (C_SSHOT)

#include <zlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <png.h>

#include "zmbv.h"

#define DBZV_VERSION_HIGH 0
#define DBZV_VERSION_LOW 1

#define COMPRESSION_NONE 0
#define COMPRESSION_ZLIB 1

#define MAX_VECTOR	16

#define Mask_KeyFrame			0x01
#define	Mask_DeltaPalette		0x02

zmbv_format_t BPPFormat( int bpp ) {
	switch (bpp) {
	case 8:
		return ZMBV_FORMAT_8BPP;
	case 15:
		return ZMBV_FORMAT_15BPP;
	case 16:
		return ZMBV_FORMAT_16BPP;
	case 32:
		return ZMBV_FORMAT_32BPP;
	}
	return ZMBV_FORMAT_NONE;
}
int VideoCodec::NeededSize( int _width, int _height, zmbv_format_t _format) {
	int f;
	switch (_format) {
	case ZMBV_FORMAT_8BPP:f = 1;break;
	case ZMBV_FORMAT_15BPP:f = 2;break;
	case ZMBV_FORMAT_16BPP:f = 2;break;
	case ZMBV_FORMAT_32BPP:f = 4;break;
	default:
		return -1;
	}
	f = f*_width*_height + 2*(1+(_width/8)) * (1+(_height/8))+1024;
	return f + f/1000;
}

bool VideoCodec::SetupBuffers(zmbv_format_t _format, int blockwidth, int blockheight) {
	FreeBuffers();
	palsize = 0;
	switch (_format) {
	case ZMBV_FORMAT_8BPP:
		pixelsize = 1;
		palsize = 256;
		break;
	case ZMBV_FORMAT_15BPP:
		pixelsize = 2;
		break;
	case ZMBV_FORMAT_16BPP:
		pixelsize = 2;
		break;
	case ZMBV_FORMAT_32BPP:
		pixelsize = 4;
		break;
	default:
		return false;
	};
	bufsize = (height+2*MAX_VECTOR)*pitch*pixelsize+2048;

	buf1 = new unsigned char[bufsize];
	buf2 = new unsigned char[bufsize];
	work = new unsigned char[bufsize];

	int xblocks = (width/blockwidth);
	int xleft = width % blockwidth;
	if (xleft) xblocks++;
	int yblocks = (height/blockheight);
	int yleft = height % blockheight;
	if (yleft) yblocks++;
	blockcount=yblocks*xblocks;
	blocks=new FrameBlock[blockcount];

	if (!buf1 || !buf2 || !work || !blocks) {
		FreeBuffers();
		return false;
	}
	int y,x,i;
	i=0;
	for (y=0;y<yblocks;y++) {
		for (x=0;x<xblocks;x++) {
			blocks[i].start=((y*blockheight)+MAX_VECTOR)*pitch+
				(x*blockwidth)+MAX_VECTOR;
			if (xleft && x==(xblocks-1)) {
                blocks[i].dx=xleft;
			} else {
				blocks[i].dx=blockwidth;
			}
			if (yleft && y==(yblocks-1)) {
                blocks[i].dy=yleft;
			} else {
				blocks[i].dy=blockheight;
			}
			i++;
		}
	}

	memset(buf1,0,(unsigned int)bufsize);
	memset(buf2,0,(unsigned int)bufsize);
	memset(work,0,(unsigned int)bufsize);
	oldframe=buf1;
	newframe=buf2;
	format = _format;
	return true;
}

void VideoCodec::CreateVectorTable(void) {
	int x,y,s;
	VectorCount=1;

	VectorTable[0].x=VectorTable[0].y=0;
	for (s=1;s<=10;s++) {
		for (y=0-s;y<=0+s;y++) for (x=0-s;x<=0+s;x++) {
			if (abs(x)==s || abs(y)==s) {
				VectorTable[VectorCount].x=x;
				VectorTable[VectorCount].y=y;
				VectorCount++;
			}
		}
	}
}

template<class P>
INLINE int VideoCodec::PossibleBlock(int vx,int vy,FrameBlock * block) {
	int ret=0;
	P * pold=((P*)oldframe)+block->start+(vy*pitch)+vx;
	P * pnew=((P*)newframe)+block->start;;	
	for (int y=0;y<block->dy;y+=4) {
		for (int x=0;x<block->dx;x+=4) {
			int test=0-(int)((pold[x]-pnew[x])&0x00ffffffu);
			ret-=(test>>31);
		}
		pold+=pitch*4;
		pnew+=pitch*4;
	}
	return ret;
}

template<class P>
INLINE int VideoCodec::CompareBlock(int vx,int vy,FrameBlock * block) {
	int ret=0;
	P * pold=((P*)oldframe)+block->start+(vy*pitch)+vx;
	P * pnew=((P*)newframe)+block->start;;	
	for (int y=0;y<block->dy;y++) {
		for (int x=0;x<block->dx;x++) {
			int test=0-(int)((pold[x]-pnew[x])&0x00ffffffu);
			ret-=(test>>31);
		}
		pold+=pitch;
		pnew+=pitch;
	}
	return ret;
}

template<class P>
INLINE void VideoCodec::AddXorBlock(int vx,int vy,FrameBlock * block) {
	P * pold=((P*)oldframe)+block->start+(vy*pitch)+vx;
	P * pnew=((P*)newframe)+block->start;
	for (int y=0;y<block->dy;y++) {
		for (int x=0;x<block->dx;x++) {
			*((P*)&work[workUsed])=pnew[x] ^ pold[x];
			workUsed+=(int)sizeof(P);
		}
		pold+=pitch;
		pnew+=pitch;
	}
}

template<class P>
void VideoCodec::AddXorFrame(void) {
//	int written=0;
//	int lastvector=0;
	signed char * vectors=(signed char*)&work[workUsed];
	/* Align the following xor data on 4 byte boundary*/
	workUsed=(workUsed + blockcount*2 +3) & ~3;
//	int totalx=0;
//	int totaly=0;
	for (int b=0;b<blockcount;b++) {
		FrameBlock * block=&blocks[b];
		int bestvx = 0;
		int bestvy = 0;
		int bestchange=CompareBlock<P>(0,0, block);
		int possibles=64;
		for (int v=0;v<VectorCount && possibles;v++) {
			if (bestchange<4) break;
			int vx = VectorTable[v].x;
			int vy = VectorTable[v].y;
			if (PossibleBlock<P>(vx, vy, block) < 4) {
				possibles--;
//				if (!possibles) Msg("Ran out of possibles, at %d of %d best %d\n",v,VectorCount,bestchange);
				int testchange=CompareBlock<P>(vx,vy, block);
				if (testchange<bestchange) {
					bestchange=testchange;
					bestvx = vx;
					bestvy = vy;
				}
			}
		}
		vectors[b*2+0]=(bestvx << 1);
		vectors[b*2+1]=(bestvy << 1);
		if (bestchange) {
			vectors[b*2+0]|=1;
			AddXorBlock<P>(bestvx, bestvy, block);
		}
	}
}

bool VideoCodec::SetupCompress( int _width, int _height ) {
	width = _width;
	height = _height;
	pitch = _width + 2*MAX_VECTOR;
	format = ZMBV_FORMAT_NONE;
	if (deflateInit (&zstream, 4) != Z_OK)
		return false;
	return true;
}

bool VideoCodec::SetupDecompress( int _width, int _height) {
	width = _width;
	height = _height;
	pitch = _width + 2*MAX_VECTOR;
	format = ZMBV_FORMAT_NONE;
	if (inflateInit (&zstream) != Z_OK)
		return false;
	return true;
}

bool VideoCodec::PrepareCompressFrame(int flags,  zmbv_format_t _format, char * pal, void *writeBuf, int writeSize) {
	int i;
	unsigned char *firstByte;

	if (_format != format) {
		if (!SetupBuffers( _format, 16, 16))
			return false;
		flags|=1;	//Force a keyframe
	}
	/* replace oldframe with new frame */
	unsigned char *copyFrame = newframe;
	newframe = oldframe;
	oldframe = copyFrame;

	compress.linesDone = 0;
	compress.writeSize = writeSize;
	compress.writeDone = 1;
	compress.writeBuf = (unsigned char *)writeBuf;
	/* Set a pointer to the first byte which will contain info about this frame */
	firstByte = compress.writeBuf;
	*firstByte = 0;
	//Reset the work buffer
	workUsed = 0;workPos = 0;
	if (flags & 1) {
		/* Make a keyframe */
		*firstByte |= Mask_KeyFrame;
		KeyframeHeader * header = (KeyframeHeader *)(compress.writeBuf + compress.writeDone);
		header->high_version = DBZV_VERSION_HIGH;
		header->low_version = DBZV_VERSION_LOW;
		header->compression = COMPRESSION_ZLIB;
		header->format = format;
		header->blockwidth = 16;
		header->blockheight = 16;
		compress.writeDone += (int)sizeof(KeyframeHeader);
		/* Copy the new frame directly over */
		if (palsize) {
			if (pal)
				memcpy(&palette, pal, sizeof(palette));
			else 
				memset(&palette,0, sizeof(palette));
			/* keyframes get the full palette */
			for (i=0;i<palsize;i++) {
				work[workUsed++] = (unsigned char)palette[i*4+0];
				work[workUsed++] = (unsigned char)palette[i*4+1];
				work[workUsed++] = (unsigned char)palette[i*4+2];
			}
		}
		/* Restart deflate */
		deflateReset(&zstream);
	} else {
		if (palsize && pal && memcmp(pal, palette, (unsigned int)palsize * 4u)) {
			*firstByte |= Mask_DeltaPalette;
			for(i=0;i<palsize;i++) {
				work[workUsed++]=(unsigned char)palette[i*4+0] ^ (unsigned char)pal[i*4+0];
				work[workUsed++]=(unsigned char)palette[i*4+1] ^ (unsigned char)pal[i*4+1];
				work[workUsed++]=(unsigned char)palette[i*4+2] ^ (unsigned char)pal[i*4+2];
			}
			memcpy(&palette,pal, (unsigned int)palsize * 4u);
		}
	}
	return true;
}

void VideoCodec::CompressLines(int lineCount, void *lineData[]) {
	int linePitch = pitch * pixelsize;
	int lineWidth = width * pixelsize;
	int i = 0;
	unsigned char *destStart = newframe + pixelsize*(MAX_VECTOR+(compress.linesDone+MAX_VECTOR)*pitch);
	while ( i < lineCount && (compress.linesDone < height)) {
		memcpy(destStart, lineData[i], (size_t)lineWidth );
		destStart += linePitch;
		i++;compress.linesDone++;
	}
}

int VideoCodec::FinishCompressFrame( void ) {
	unsigned char firstByte = *compress.writeBuf;
	if (firstByte & Mask_KeyFrame) {
		int i;
		/* Add the full frame data */
		unsigned char * readFrame = newframe + pixelsize*(MAX_VECTOR+MAX_VECTOR*pitch);	
		for (i=0;i<height;i++) {
			memcpy(&work[workUsed], readFrame, (unsigned int)width * (unsigned int)pixelsize);
			readFrame += pitch*pixelsize;
			workUsed += width*pixelsize;
		}
	} else {
		/* Add the delta frame data */
		switch (format) {
		case ZMBV_FORMAT_8BPP:
			AddXorFrame<uint8_t>();
			break;
		case ZMBV_FORMAT_15BPP:
		case ZMBV_FORMAT_16BPP:
			AddXorFrame<uint16_t>();
			break;
		case ZMBV_FORMAT_32BPP:
			AddXorFrame<uint32_t>();
			break;
		default:
			break;
		}
	}
	/* Create the actual frame with compression */
	zstream.next_in = (Bytef *)work;
	zstream.avail_in = (unsigned int)workUsed;
	zstream.total_in = 0;

	zstream.next_out = (Bytef *)(compress.writeBuf + compress.writeDone);
	zstream.avail_out = (unsigned int)compress.writeSize - (unsigned int)compress.writeDone;
	zstream.total_out = 0;
	deflate(&zstream, Z_SYNC_FLUSH);
	return (int)compress.writeDone + (int)zstream.total_out;
}

template<class P>
INLINE void VideoCodec::UnXorBlock(int vx,int vy,FrameBlock * block) {
	P * pold=((P*)oldframe)+block->start+(vy*pitch)+vx;
	P * pnew=((P*)newframe)+block->start;
	for (int y=0;y<block->dy;y++) {
		for (int x=0;x<block->dx;x++) {
			pnew[x]=pold[x]^*((P*)&work[workPos]);
			workPos+=(int)sizeof(P);
		}
		pold+=pitch;
		pnew+=pitch;
	}
}

template<class P>
INLINE void VideoCodec::CopyBlock(int vx,int vy,FrameBlock * block) {
	P * pold=((P*)oldframe)+block->start+(vy*pitch)+vx;
	P * pnew=((P*)newframe)+block->start;
	for (int y=0;y<block->dy;y++) {
		for (int x=0;x<block->dx;x++) {
			pnew[x]=pold[x];
		}
		pold+=pitch;
		pnew+=pitch;
	}
}

template<class P>
void VideoCodec::UnXorFrame(void) {
	signed char * vectors=(signed char *)&work[workPos];
	workPos=(workPos + blockcount*2 + 3) & ~3;
	for (int b=0;b<blockcount;b++) {
		FrameBlock * block=&blocks[b];
		int delta = vectors[b*2+0] & 1;
		int vx = vectors[b*2+0] >> 1;
		int vy = vectors[b*2+1] >> 1;
		if (delta) UnXorBlock<P>(vx,vy,block);
		else CopyBlock<P>(vx,vy,block);
	}
}

bool VideoCodec::DecompressFrame(void * framedata, int size) {
	unsigned char *data=(unsigned char *)framedata;
	unsigned char tag;int i;

	tag = *data++;
	if (--size<=0)
		return false;
	if (tag & Mask_KeyFrame) {
		KeyframeHeader * header = (KeyframeHeader *)data;
		size -= (int)sizeof(KeyframeHeader);data += sizeof(KeyframeHeader);
		if (size<=0)
            return false;
		if (header->low_version != DBZV_VERSION_LOW || header->high_version != DBZV_VERSION_HIGH) 
			return false;
		if (format != (zmbv_format_t)header->format && !SetupBuffers((zmbv_format_t)header->format, header->blockwidth, header->blockheight))
			return false;
		inflateReset(&zstream);
	} 
	zstream.next_in = (Bytef *)data;
	zstream.avail_in = (unsigned int)size;
	zstream.total_in = 0;

	zstream.next_out = (Bytef *)work;
	zstream.avail_out = (unsigned int)bufsize;
	zstream.total_out = 0;
	inflate(&zstream, Z_FINISH);
	workUsed= (int)zstream.total_out;
	workPos = 0;
	if (tag & Mask_KeyFrame) {
		if (palsize) {
			for (i=0;i<palsize;i++) {
				palette[i*4+0] = (char)work[workPos++];
				palette[i*4+1] = (char)work[workPos++];
				palette[i*4+2] = (char)work[workPos++];
			}
		}
		newframe = buf1;
		oldframe = buf2;
		unsigned char * writeframe = newframe + pixelsize*(MAX_VECTOR+MAX_VECTOR*pitch);	
		for (i=0;i<height;i++) {
			memcpy(writeframe,&work[workPos],(unsigned int)width*(unsigned int)pixelsize);
			writeframe+=pitch*pixelsize;
			workPos+=width*pixelsize;
		}
	} else {
		data = oldframe;
		oldframe = newframe;
		newframe = data;
		if (tag & Mask_DeltaPalette) {
			for (i=0;i<palsize;i++) {
				palette[i*4+0] ^= (unsigned char)work[workPos++];
				palette[i*4+1] ^= (unsigned char)work[workPos++];
				palette[i*4+2] ^= (unsigned char)work[workPos++];
			}
		}
		switch (format) {
		case ZMBV_FORMAT_8BPP:
			UnXorFrame<uint8_t>();
			break;
		case ZMBV_FORMAT_15BPP:
		case ZMBV_FORMAT_16BPP:
			UnXorFrame<uint16_t>();
			break;
		case ZMBV_FORMAT_32BPP:
			UnXorFrame<uint32_t>();
			break;
		default:
			break;
		}
	}
	return true;
}

void VideoCodec::Output_UpsideDown_24(void *output) {
	int i;
	unsigned char *r;
	unsigned char *w = (unsigned char *)output;
	unsigned int pad = width & 3;

	for (i=height-1;i>=0;i--) {
		r = newframe + pixelsize*(MAX_VECTOR+(i+MAX_VECTOR)*pitch);
		switch (format) {
		case ZMBV_FORMAT_8BPP:
			for (unsigned int j=0;(int)j<width;j++) {
				unsigned int c=r[j];
				*w++=(unsigned char)palette[c*4u+2u];
				*w++=(unsigned char)palette[c*4u+1u];
				*w++=(unsigned char)palette[c*4u+0u];
			}
			break;
		case ZMBV_FORMAT_15BPP:
			for (unsigned int j=0;(int)j<width;j++) {
				unsigned short c = *(unsigned short *)&r[j*2u];
				*w++ = (unsigned char)(((c & 0x001fu) * 0x21u) >>  2u);
				*w++ = (unsigned char)(((c & 0x03e0u) * 0x21u) >>  7u);
				*w++ = (unsigned char)(((c & 0x7c00u) * 0x21u) >> 12u);
			}
			break;
		case ZMBV_FORMAT_16BPP:
			for (unsigned int j=0;(int)j<width;j++) {
				unsigned short c = *(unsigned short *)&r[j*2u];
				*w++ = (unsigned char)(((c & 0x001fu) * 0x21u) >>  2u);
				*w++ = (unsigned char)(((c & 0x07e0u) * 0x41u) >>  9u);
				*w++ = (unsigned char)(((c & 0xf800u) * 0x21u) >> 13u);
			}
			break;
		case ZMBV_FORMAT_32BPP:
			for (unsigned int j=0;(int)j<width;j++) {
				*w++ = r[j*4u+0u];
				*w++ = r[j*4u+1u];
				*w++ = r[j*4u+2u];
			}
			break;
		default:
			break;
		}

		// Maintain 32-bit alignment for scanlines.
		w += pad;
	}
}

void VideoCodec::FreeBuffers(void) {
	if (blocks) {
		delete[] blocks;blocks=0;
	}
	if (buf1) {
		delete[] buf1;buf1=0;
	}
	if (buf2) {
		delete[] buf2;buf2=0;
	}
	if (work) {
		delete[] work;work=0;
	}
}


VideoCodec::VideoCodec() {
	CreateVectorTable();
	blocks = 0;
	buf1 = 0;
	buf2 = 0;
	work = 0;
	memset( &zstream, 0, sizeof(zstream));
}

#endif //(C_SSHOT)
