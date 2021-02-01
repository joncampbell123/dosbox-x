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
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

// Tell Mac OS X to shut up about deprecated OpenGL calls
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif

#include <stdlib.h>
#include <math.h>
#include <map>

#include "dosbox.h"
#include "video.h"

#include "voodoo_emu.h"
#include "voodoo_opengl.h"
#include "sdlmain.h"

#if C_OPENGL

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
#endif

#include "voodoo_def.h"

SDL_Surface* ogl_surface = NULL;

INT32 cached_line_front_y=-1;
INT32 cached_line_front_width = -1;
INT32 cached_line_front_length=-1;
INT32 cached_line_front_pixels=-1;
UINT32* cached_line_front_data=NULL;

INT32 cached_line_back_y=-1;
INT32 cached_line_back_width = -1;
INT32 cached_line_back_length=-1;
INT32 cached_line_back_pixels=-1;
UINT32* cached_line_back_data=NULL;

static INT32 adjust_x=0;
static INT32 adjust_y=0;

static UINT32 last_clear_color=0;

static UINT32 last_width=0;
static UINT32 last_height=0;
static INT32 last_orientation=-1;


static UINT32 ogl_texture_index = 1;


/* texture cache buffer */
UINT32 texrgb[256*256];


/* texture address map */
std::map <const UINT32, ogl_texmap> textures[2];

bool Direct3D_using(void);
void GFX_PreventFullscreen(bool lockout), GFX_SetResizeable(bool enable), change_output(int output);
#if defined(C_SDL2)
SDL_Window* GFX_SetSDLWindowMode(uint16_t width, uint16_t height, SCREEN_TYPES screenType);
#endif

int Voodoo_OGL_GetWidth() {
	if (v != NULL)
		return (int)v->fbi.width;
	else
		return 0;
}

int Voodoo_OGL_GetHeight() {
	if (v != NULL)
		return (int)v->fbi.height;
	else
		return 0;
}

bool Voodoo_OGL_Active() {
	return (v != NULL && v->clock_enabled && v->output_on);
}

static void ogl_get_depth(voodoo_state* VV, INT32 ITERZ, INT64 ITERW, INT32 *depthval, INT32 *out_wfloat)
{
	INT32 wfloat;
	UINT32 FBZMODE = VV->reg[fbzMode].u;
	UINT32 FBZCOLORPATH = VV->reg[fbzColorPath].u;

	/* compute "floating point" W value (used for depth and fog) */
	if ((ITERW) & LONGTYPE(0xffff00000000))
		wfloat = 0x0000;
	else
	{
		UINT32 temp = (UINT32)(ITERW);
		if ((temp & 0xffff0000) == 0)
			wfloat = 0xffff;
		else
		{
			int exp = count_leading_zeros(temp);
			wfloat = ((exp << 12) | (INT32)(((~temp) >> (unsigned int)(19 - exp)) & 0xfffu));
			if (wfloat < 0xffff) wfloat++;
		}
	}

	/* compute depth value (W or Z) for this pixel */
	if (FBZMODE_WBUFFER_SELECT(FBZMODE) == 0)
		CLAMPED_Z(ITERZ, FBZCOLORPATH, *depthval);
	else if (FBZMODE_DEPTH_FLOAT_SELECT(FBZMODE) == 0)
		*depthval = wfloat;
	else
	{
		if ((unsigned int)(ITERZ) & 0xf0000000l)
			*depthval = 0x0000;
		else
		{
			UINT32 temp = (UINT32)((ITERZ) << 4);
			if ((temp & 0xffff0000ul) == 0)
				*depthval = 0xffff;
			else
			{
				int exp = count_leading_zeros(temp);
				*depthval = ((exp << 12) | (INT32)(((~temp) >> (unsigned int)(19 - exp)) & 0xfffu));
				if (*depthval < 0xffff) (*depthval)++;
			}
		}
	}

	/* add the bias */
	if (FBZMODE_ENABLE_DEPTH_BIAS(FBZMODE))
	{
		*depthval += (INT16)(VV)->reg[zaColor].u;
		CLAMP(*depthval, 0, 0xffff);
	}

	*out_wfloat = wfloat;
}

void ogl_get_fog_blend(voodoo_state* v, INT32 wfloat, INT32 ITERZ, INT64 ITERW, INT32 *fogblend)
{
	UINT32 FOGMODE = v->reg[fogMode].u;
	UINT32 FBZCP = v->reg[fbzColorPath].u;

	*fogblend = 0;
	if (FOGMODE_ENABLE_FOG(FOGMODE) && !FOGMODE_FOG_CONSTANT(FOGMODE))
	{
		/* fog blending mode */
		switch (FOGMODE_FOG_ZALPHA(FOGMODE))
		{
		case 0:		/* fog table */
			{
				INT32 delta = v->fbi.fogdelta[wfloat >> 10];
				INT32 deltaval;

				/* perform the multiply against lower 8 bits of wfloat */
				deltaval = (delta & v->fbi.fogdelta_mask) *
					((wfloat >> 2) & 0xff);

				/* fog zones allow for negating this value */
				if (FOGMODE_FOG_ZONES(FOGMODE) && (delta & 2))
					deltaval = -deltaval;
				deltaval >>= 6;

				/* apply dither */
//				if (FOGMODE_FOG_DITHER(FOGMODE))
//					deltaval += DITHER4[(XX) & 3];
				deltaval >>= 4;

				/* add to the blending factor */
				*fogblend = v->fbi.fogblend[wfloat >> 10] + deltaval;
				break;
			}

		case 1:		/* iterated A */
//			fogblend = ITERAXXX.rgb.a;
			break;

		case 2:		/* iterated Z */
			CLAMPED_Z((ITERZ), FBZCP, *fogblend);
			*fogblend >>= 8;
			break;

		case 3:		/* iterated W - Voodoo 2 only */
			CLAMPED_W((ITERW), FBZCP, *fogblend);
			break;
		}

	}
}

void ogl_get_vertex_data(INT32 x, INT32 y, const void *extradata, ogl_vertex_data *vd) {
	const poly_extra_data *extra = (const poly_extra_data *)extradata;
	v=(voodoo_state*)extra->state;
	INT32 iterr, iterg, iterb, itera;
	INT32 iterz;
	INT64 iterw;
	INT32 dx, dy;
	INT32 d;

	dx = x - extra->ax;
	dy = y - extra->ay;
	iterr = extra->startr + ((dy * extra->drdy)>>4) + ((dx * extra->drdx)>>4);
	iterg = extra->startg + ((dy * extra->dgdy)>>4) + ((dx * extra->dgdx)>>4);
	iterb = extra->startb + ((dy * extra->dbdy)>>4) + ((dx * extra->dbdx)>>4);
	itera = extra->starta + ((dy * extra->dady)>>4) + ((dx * extra->dadx)>>4);
	iterz = extra->startz + ((dy * extra->dzdy)>>4) + ((dx * extra->dzdx)>>4);
	iterw = extra->startw + ((dy * extra->dwdy)>>4) + ((dx * extra->dwdx)>>4);

	for (Bitu i=0; i<extra->texcount; i++) {
		INT64 iters,itert,iterw;
		UINT32 smax,tmax;
		INT64 s, t;
		INT32 lod=0;

		UINT32 texmode=v->tmu[i].reg[textureMode].u;
		UINT32 TEXMODE = texmode;
		INT32 LODBASE = (i==0) ? extra->lodbase0 : extra->lodbase1;

		UINT32 ilod = (UINT32)(v->tmu[i].lodmin >> 8);
		if (!((v->tmu[i].lodmask >> ilod) & 1))
			ilod++;

		smax = (v->tmu[i].wmask >> ilod) + 1;
		tmax = (v->tmu[i].hmask >> ilod) + 1;

		iterw = v->tmu[i].startw + ((dy * v->tmu[i].dwdy)>>4) + ((dx * v->tmu[i].dwdx)>>4);
		iters = v->tmu[i].starts + ((dy * v->tmu[i].dsdy)>>4) + ((dx * v->tmu[i].dsdx)>>4);
		itert = v->tmu[i].startt + ((dy * v->tmu[i].dtdy)>>4) + ((dx * v->tmu[i].dtdx)>>4);

		/* determine the S/T/LOD values for this texture */
		if (TEXMODE_ENABLE_PERSPECTIVE(texmode))
		{
			INT64 oow = fast_reciplog((iterw), &lod);
			s = (oow * (iters)) >> 29;
			t = (oow * (itert)) >> 29;
			lod += LODBASE;
		}
		else
		{
			s = (iters) >> 14;
			t = (itert) >> 14;
			lod = LODBASE;
		}

		/* clamp the LOD */
		lod += v->tmu[i].lodbias;
//		if (TEXMODE_ENABLE_LOD_DITHER(TEXMODE))
//			lod += DITHER4[(XX) & 3] << 4;
		if (lod < v->tmu[i].lodmin)
			lod = v->tmu[i].lodmin;
		if (lod > v->tmu[i].lodmax)
			lod = v->tmu[i].lodmax;

		/* clamp W */
		if (TEXMODE_CLAMP_NEG_W(texmode) && (iterw) < 0)
			s = t = 0;

		if (s != 0) vd->m[i].s = (float)((float)s/(float)(smax*(1u<<(18u+ilod))));
		else vd->m[i].s = 0.0f;
		if (t != 0) vd->m[i].t = (float)((float)t/(float)(tmax*(1u<<(18u+ilod))));
		else vd->m[i].t = 0.0f;
		if (iterw != 0) vd->m[i].w = (float)((float)iterw/(float)(0xffffff));
		else vd->m[i].w = 0.0f;

		vd->m[i].sw = vd->m[i].s * vd->m[i].w;
		vd->m[i].tw = vd->m[i].t * vd->m[i].w;
		vd->m[i].z  = 0.0f;

		INT32 lodblend=0;

		if ((TEXMODE_TC_MSELECT(TEXMODE)==4) || (TEXMODE_TCA_MSELECT(TEXMODE)==4))
		{
			if (v->tmu[i].detailbias <= lod)
				lodblend = 0;
			else
			{
				lodblend = (((v->tmu[i].detailbias - lod) << v->tmu[i].detailscale) >> 8);
				if (lodblend > v->tmu[i].detailmax)
					lodblend = v->tmu[i].detailmax;
			}
		} else if ((TEXMODE_TC_MSELECT(TEXMODE)==5) || (TEXMODE_TCA_MSELECT(TEXMODE)==5))
			lodblend = lod & 0xff;

		vd->m[i].lodblend = (float)lodblend/255.0f;
	}

	vd->r = (float)((float)iterr/(float)(1<<20));
	vd->g = (float)((float)iterg/(float)(1<<20));
	vd->b = (float)((float)iterb/(float)(1<<20));
	vd->a = (float)((float)itera/(float)(1<<20));
	vd->z = (float)((float)iterz/(float)(1<<20));
	vd->w = (float)((float)iterw/(float)(0xffffff));

	INT32 wfloat;
	ogl_get_depth(v,iterz,iterw,&d,&wfloat);
	vd->d = (float)((float)d/(float)(0xffff));

	INT32 fogblend;
	ogl_get_fog_blend(v,wfloat,iterz,iterw,&fogblend);
	vd->fogblend = (float)fogblend/255.0f;

//	vd->x = (float)x / (16.0f);
//	vd->y = (float)y / (16.0f);

	// OpenGL-correction for Blood/ShadowWarrior
	vd->x = (((float)x) - (1.0f/16.0f)) / 16.0f;
	vd->y = (((float)y) - (1.0f/16.0f)) / 16.0f;
}

static UINT32 crc_32_tab[] = { /* CRC polynomial 0xedb88320 */
0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

UINT32 calculate_palsum(UINT32 tmunum) {
	UINT32 csum = 0;
	for (Bitu pct=0; pct<256; pct++) {
		UINT32 pval = v->tmu[tmunum].palette[pct];
		csum = crc_32_tab[(csum ^ pval) & 0xff] ^ (csum>>8);
		csum = crc_32_tab[(csum ^ (pval>>8)) & 0xff] ^ (csum>>8);
		csum = crc_32_tab[(csum ^ (pval>>16)) & 0xff] ^ (csum>>8);
		csum = crc_32_tab[(csum ^ (pval>>24)) & 0xff] ^ (csum>>8);
	}
	return csum;
}

void ogl_cache_texture(const poly_extra_data *extra, ogl_texture_data *td) {
	v=(voodoo_state*)extra->state;
	UINT32 texbase;

	INT32 smax, tmax;
	UINT32 *texrgbp;
	GLuint texID;

	UINT32 num_tmus = 1;
	if (v->tmu[1].ram != NULL) num_tmus++;

	for (UINT32 j=0; j<num_tmus; j++) {
		UINT32 TEXMODE = (UINT32)(j==0 ? extra->r_textureMode0 : extra->r_textureMode1);

		UINT32 ilod = (UINT32)(v->tmu[j].lodmin >> 8);
		if (!((v->tmu[j].lodmask >> ilod) & 1))
			ilod++;

		texbase = v->tmu[j].lodoffset[ilod];

		if ( extra->texcount && (extra->texcount >= j) && (v->tmu[j].lodmin < (8 << 8)) ) {
			bool valid_texid = true;
			std::map<const UINT32, ogl_texmap>::iterator t;
			t = textures[j].find(texbase);
			bool reuse_id = false;
			if (t != textures[j].end())  {
				if (t->second.valid_pal) {
					texID = t->second.current_id;
					if (!t->second.valid_data) {
						valid_texid = false;
						reuse_id = true;
						t->second.valid_data = true;
					}
				} else {
					UINT32 psum = calculate_palsum(j);
					std::map<const UINT32, GLuint>::iterator u = t->second.ids->find(psum);
					if (u != t->second.ids->end()) {
						t->second.valid_pal = true;
						t->second.current_id = u->second;
						texID = u->second;
					} else {
						valid_texid = false;
//						LOG_MSG("texture removed... size %d",t->second.ids->size());
						if (t->second.ids->size() > 8) {
							for (u=t->second.ids->begin(); u!=t->second.ids->end(); ++u) {
								glDeleteTextures(1,&u->second);
							}
							t->second.ids->clear();
						}
						t->second.valid_pal = true;
					}
				}
			} else {
				valid_texid = false;
			}
			if (!valid_texid) {
				smax = (INT32)(v->tmu[j].wmask >> ilod) + 1;
				tmax = (INT32)(v->tmu[j].hmask >> ilod) + 1;

				UINT32 texboffset = texbase;
				texrgbp = (UINT32 *)&texrgb[0];
				memset(texrgbp,0,256*256*4);

				if (!reuse_id) {
					texID = ogl_texture_index++;
					glGenTextures(1, &texID);
				}

				if (TEXMODE_FORMAT(v->tmu[j].reg[textureMode].u) < 8) {
					if (TEXMODE_FORMAT(v->tmu[j].reg[textureMode].u) != 5) {
						for (int i=0; i<(smax*tmax); i++) {
							UINT8 *texptr8 = (UINT8 *)&v->tmu[j].ram[(texboffset) & v->tmu[j].mask];
							UINT32 data = v->tmu[j].lookup[*texptr8];
							*texrgbp = data;
							texboffset++;
							texrgbp++;
						}
					} else {
						for (int i=0; i<(smax*tmax); i++) {
							UINT8 *texptr8 = (UINT8 *)&v->tmu[j].ram[(texboffset) & v->tmu[j].mask];
							UINT8 texel = *texptr8;
							UINT32 data = v->tmu[j].lookup[texel];
							*texrgbp = data;
							texboffset++;
							texrgbp++;
						}
					}
				} else if (TEXMODE_FORMAT(v->tmu[j].reg[textureMode].u) >= 10 && TEXMODE_FORMAT(v->tmu[j].reg[textureMode].u) <= 12) {
					for (int i=0; i<(smax*tmax); i++) {
						UINT16 *texptr16 = (UINT16 *)&v->tmu[j].ram[(texboffset) & v->tmu[j].mask];
						UINT32 data = v->tmu[j].lookup[*texptr16];
						*texrgbp = data;
						texboffset+=2;
						texrgbp++;
					}
				} else if (TEXMODE_FORMAT(v->tmu[j].reg[textureMode].u) != 14) {
					for (int i=0; i<(smax*tmax); i++) {
						UINT16 *texptr16 = (UINT16 *)&v->tmu[j].ram[(texboffset) & v->tmu[j].mask];
						UINT32 data = (v->tmu[j].lookup[*texptr16 & 0xFF] & 0xFFFFFF) | ((*texptr16 & 0xff00u) << 16u);
						*texrgbp = data;
						texboffset+=2;
						texrgbp++;
					}
				} else {
					for (int i=0; i<(smax*tmax); i++) {
						UINT16 *texptr16 = (UINT16 *)&v->tmu[j].ram[(texboffset) & v->tmu[j].mask];
						UINT16 texel1 = *texptr16 & 0xFF;
						UINT32 data = (v->tmu[j].lookup[texel1] & 0xFFFFFF) | ((*texptr16 & 0xff00u) << 16u);
						*texrgbp = data;
						texboffset+=2;
						texrgbp++;
					}
				}

				texrgbp = (UINT32 *)&texrgb[0];
//				LOG_MSG("texid %d format %d -- %d x %d",
//					texID,TEXMODE_FORMAT(v->tmu[j].reg[textureMode].u),smax,tmax);

				glBindTexture(GL_TEXTURE_2D, texID);
				glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
				glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,TEXMODE_CLAMP_S(TEXMODE)?GL_CLAMP_TO_EDGE:GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,TEXMODE_CLAMP_T(TEXMODE)?GL_CLAMP_TO_EDGE:GL_REPEAT);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, smax, tmax, 0, GL_BGRA_EXT, GL_UNSIGNED_INT_8_8_8_8_REV, texrgbp);
				extern PFNGLGENERATEMIPMAPEXTPROC glGenerateMipmapEXT;
				glGenerateMipmapEXT(GL_TEXTURE_2D);
				if ((TEXMODE_FORMAT(v->tmu[j].reg[textureMode].u)==0x05) || (TEXMODE_FORMAT(v->tmu[j].reg[textureMode].u)==0x0e)) {
					UINT32 palsum = calculate_palsum(j);
					if (t == textures[j].end()) {
						std::map<const UINT32, GLuint>* ids = new std::map<const UINT32, GLuint>();
						(*ids)[palsum] = texID;
						ogl_texmap tex = { true, true, TEXMODE_FORMAT(v->tmu[j].reg[textureMode].u),
											texID, ids };
						textures[j][texbase] = tex;
					} else {
						(*textures[j][texbase].ids)[palsum] = texID;
						textures[j][texbase].current_id = texID;
					}
				} else {
					ogl_texmap tex = { true, true, TEXMODE_FORMAT(v->tmu[j].reg[textureMode].u),
										texID, NULL };
					textures[j][texbase] = tex;
				}
			}

			td[j].texID = texID;
			td[j].enable = true;
		} else {
			td[j].enable = false;
		}
	}
}

void voodoo_ogl_invalidate_paltex(void) {
	std::map<const UINT32, ogl_texmap>::iterator t;
	for (int j=0; j<2; j++) {
		for (t=textures[j].begin(); t!=textures[j].end(); ++t) {
			if ((t->second.format == 0x05) || (t->second.format == 0x0e)) {
				t->second.valid_pal = false;
			}
		}
	}
}


GLhandleARB m_hProgramObject   = (GLhandleARB)NULL;


void ogl_printInfoLog(GLhandleARB obj)
{
    int infologLength = 0;
    int charsWritten  = 0;

    glGetObjectParameterivARB(obj, GL_OBJECT_INFO_LOG_LENGTH_ARB, &infologLength);

    if (infologLength > 0)
    {
		char *infoLog = (char *)malloc((size_t)infologLength);
		glGetInfoLogARB(obj, infologLength, &charsWritten, infoLog);
		LOG_MSG("%s\n",infoLog);
		free(infoLog);
    }
}

void ogl_sh_tex_combine(std::string *strFShader, const int TMU, const poly_extra_data *extra) {
	v=(voodoo_state*)extra->state;

	UINT32 TEXMODE     = v->tmu[TMU].reg[textureMode].u;

	if (!TEXMODE_TC_ZERO_OTHER(TEXMODE))
		*strFShader += "  tt.rgb = cother.rgb;\n";
	else
		*strFShader += "  tt.rgb = vec3(0.0);\n";

	if (!TEXMODE_TCA_ZERO_OTHER(TEXMODE))
		*strFShader += "  tt.a = cother.a;\n";
	else
		*strFShader += "  tt.a = 0.0;\n";

	if (TEXMODE_TC_SUB_CLOCAL(TEXMODE))
		*strFShader += "  tt.rgb -= clocal.rgb;\n";
	if (TEXMODE_TCA_SUB_CLOCAL(TEXMODE))
		*strFShader += "  tt.a -= clocal.a;\n";

	switch (TEXMODE_TC_MSELECT(TEXMODE))
	{
	default:
	case 0:
		*strFShader += "  blend.rgb = vec3(0.0);\n";
		break;
	case 1:
		*strFShader += "  blend.rgb = clocal.rgb;\n";
		break;
	case 2:
		*strFShader += "  blend.rgb = vec3(cother.a);\n";
		break;
	case 3:
		*strFShader += "  blend.rgb = vec3(clocal.a);\n";
		break;
	case 4:
		// detail //
	case 5:
		// LOD //
		*strFShader += "  blend.rgb = vec3(f_lodblend";
		*strFShader += TMU==0?"0":"1";
		*strFShader += ");\n";
		break;
	}

	switch (TEXMODE_TCA_MSELECT(TEXMODE))
	{
	default:
	case 0:
		*strFShader += "  blend.a = 0.0;\n";
		break;
	case 1:
		*strFShader += "  blend.a = clocal.a;\n";
		break;
	case 2:
		*strFShader += "  blend.a = cother.a;\n";
		break;
	case 3:
		*strFShader += "  blend.a = clocal.a;\n";
		break;
	case 4:
		// detail //
	case 5:
		// LOD //
		*strFShader += "  blend.a = f_lodblend";
		*strFShader += TMU==0?"0":"1";
		*strFShader += ";\n";
		break;
	}

	if (!TEXMODE_TC_REVERSE_BLEND(TEXMODE))
		*strFShader += "  blend.rgb = vec3(1.0) - blend.rgb;\n";

	if (!TEXMODE_TCA_REVERSE_BLEND(TEXMODE))
		*strFShader += "  blend.a = 1.0 - blend.a;\n";

	*strFShader += "  tt *= blend;\n";

	switch (TEXMODE_TC_ADD_ACLOCAL(TEXMODE))
	{
	case 3:
	case 0:
		break;
	case 1:
		*strFShader += "  tt.rgb += clocal.rgb;\n";
		break;
	case 2:
		*strFShader += "  tt.rgb += vec3(clocal.a);\n";
		break;
	}

	if (TEXMODE_TCA_ADD_ACLOCAL(TEXMODE))
		*strFShader += "  tt.a += clocal.a;\n";

	*strFShader += "  clocal = tt;\n";

	if (TEXMODE_TC_INVERT_OUTPUT(TEXMODE))
		*strFShader += "  clocal.rgb = vec3(1.0)-clocal.rgb;\n";

	if (TEXMODE_TCA_INVERT_OUTPUT(TEXMODE))
		*strFShader += "  clocal.a = 1.0 - clocal.a;\n";

}

void ogl_sh_color_path(std::string *strFShader, const poly_extra_data *extra) {
	v=(voodoo_state*)extra->state;

	UINT32 FBZCOLORPATH = v->reg[fbzColorPath].u;
	UINT32 FBZMODE = v->reg[fbzMode].u;
	UINT32 ALPHAMODE = v->reg[alphaMode].u;

	switch (FBZCP_CC_RGBSELECT(FBZCOLORPATH))
	{
	case 0:
		*strFShader += "  cother = gl_Color;\n";
		break;
	case 1:
		*strFShader += "  cother = texel;\n";
		break;
	case 2:
		*strFShader += "  cother = color1;\n";
		break;
	default:
		*strFShader += "  cother = vec4(0.0);\n";
	}

	// TODO fix chroma key //
	if (FBZMODE_ENABLE_CHROMAKEY(FBZMODE)) {
//		if (!CHROMARANGE_ENABLE(v->reg[chromaRange].u))
			*strFShader +=	"  if (distance (cother.rgb , chromaKey.rgb) < 0.0001) discard;\n";
//		else {
//			*strFShader +=	"  if ((cother.rgb >= (chromaKey.rgb-0.01)) && (cother.rgb <= (chromaRange.rgb+0.01))) discard;";
//		}
	}


	switch (FBZCP_CC_ASELECT(FBZCOLORPATH))
	{
	case 0:
		*strFShader += "  cother.a = gl_Color.a;\n";
		break;
	case 1:
		*strFShader += "  cother.a = texel.a;\n";
		break;
	case 2:
		*strFShader += "  cother.a = color1.a;\n";
		break;
	default:
		*strFShader += "  cother.a = 0.0;\n";
		break;
	}

	// TODO check alpha mask //
//	if (FBZMODE_ENABLE_ALPHA_MASK(FBZMODE))
//		*strFShader += "  if (cother.a > 0.0) discard;";

	if (ALPHAMODE_ALPHATEST(ALPHAMODE))
		switch (ALPHAMODE_ALPHAFUNCTION(ALPHAMODE)) {
			case 0:
				*strFShader += "  discard;\n";
				break;
			case 1:
				*strFShader += "  if (cother.a >= alphaRef) discard;\n";
				break;
			case 2:
//				*strFShader += "  if (cother.a != alphaRef) discard;\n";
				*strFShader += "  if (distance(cother.a , alphaRef) > 0.0001) discard;\n";
				break;
			case 3:
				*strFShader += "  if (cother.a >  alphaRef) discard;\n";
				break;
			case 4:
				*strFShader += "  if (cother.a <= alphaRef) discard;\n";
				break;
			case 5:
//				*strFShader += "  if (cother.a == alphaRef) discard;\n";
				*strFShader += "  if (distance(cother.a , alphaRef) < 0.0001) discard;\n";
				break;
			case 6:
				*strFShader += "  if (cother.a <  alphaRef) discard;\n";
				break;
			case 7:
				break;
	}

	if (FBZCP_CC_LOCALSELECT_OVERRIDE(FBZCOLORPATH) == 0)
	{
		if (FBZCP_CC_LOCALSELECT(FBZCOLORPATH) == 0)
			*strFShader += "  clocal = gl_Color;\n";
		else
			*strFShader += "  clocal = color0;\n";
	}
	else
	{
		*strFShader +=	"  if (texel.a < 0.5) {\n"
							"    clocal = gl_Color;\n"
						"  } else {\n"
							"    clocal = color0;\n"
						"  }\n";
	}

	switch (FBZCP_CCA_LOCALSELECT(FBZCOLORPATH))
	{
	default:
	case 0:
		*strFShader += "  clocal.a = gl_Color.a;\n";
		break;
	case 1:
		*strFShader += "  clocal.a = color0.a;\n";
		break;
	case 2:
		*strFShader += "  clocal.a = gl_Color.a;\n"; // TODO CLAMPED_Z
		break;
	case 3:
		// voodoo2 only
		break;
	}

	if (FBZCP_CC_ZERO_OTHER(FBZCOLORPATH) == 0)
		*strFShader += "  tt.rgb = cother.rgb;\n";
	else
		*strFShader += "  tt.rgb = vec3(0.0);\n";

	if (FBZCP_CCA_ZERO_OTHER(FBZCOLORPATH) == 0)
		*strFShader += "  tt.a = cother.a;\n";
	else
		*strFShader += "  tt.a = 0.0;\n";

	if (FBZCP_CC_SUB_CLOCAL(FBZCOLORPATH))
		*strFShader += "  tt.rgb -= clocal.rgb;\n";

	if (FBZCP_CCA_SUB_CLOCAL(FBZCOLORPATH))
		*strFShader += "  tt.a -= clocal.a;\n";

	switch (FBZCP_CC_MSELECT(FBZCOLORPATH))
	{
	default:
	case 0:
		*strFShader += "  blend.rgb = vec3(0.0);\n";
		break;
	case 1:
		*strFShader += "  blend.rgb = clocal.rgb;\n";
		break;
	case 2:
		*strFShader += "  blend.rgb = vec3(cother.a);\n";
		break;
	case 3:
		*strFShader += "  blend.rgb = vec3(clocal.a);\n";
		break;
	case 4:
		*strFShader += "  blend.rgb = vec3(texel.a);\n";
		break;
	case 5:
		// voodoo2 only
		*strFShader += "  blend.rgb = texel.rgb;\n";
		break;
	}

	switch (FBZCP_CCA_MSELECT(FBZCOLORPATH))
	{
	default:
	case 0:
		*strFShader += "  blend.a = 0.0;\n";
		break;
	case 1:
		*strFShader += "  blend.a = clocal.a;\n";
		break;
	case 2:
		*strFShader += "  blend.a = cother.a;\n";
		break;
	case 3:
		*strFShader += "  blend.a = clocal.a;\n";
		break;
	case 4:
		*strFShader += "  blend.a = texel.a;\n";
		break;
	}

	if (!FBZCP_CC_REVERSE_BLEND(FBZCOLORPATH))
		*strFShader += "  blend.rgb = vec3(1.0) - blend.rgb;\n";

	if (!FBZCP_CCA_REVERSE_BLEND(FBZCOLORPATH))
		*strFShader += "  blend.a = 1.0 - blend.a;\n";

	*strFShader += "  tt *= blend;\n";

	switch (FBZCP_CC_ADD_ACLOCAL(FBZCOLORPATH))
	{
	case 3:
	case 0:
		break;
	case 1:
		*strFShader += "  tt.rgb += clocal.rgb;\n";
		break;
	case 2:
		*strFShader += "  tt.rgb += vec3(clocal.a);\n";
		break;
	}

	if (FBZCP_CCA_ADD_ACLOCAL(FBZCOLORPATH))
		*strFShader += "  tt.a += clocal.a;\n";

	// clamp ?? //

	*strFShader += "  pixel = tt;\n";

	if (FBZCP_CC_INVERT_OUTPUT(FBZCOLORPATH))
		*strFShader += "  pixel.rgb = vec3(1.0) - tt.rgb;\n";

	if (FBZCP_CCA_INVERT_OUTPUT(FBZCOLORPATH))
		*strFShader += "  pixel.a = 1.0 - tt.a;\n";
}

void ogl_sh_fog(std::string *strFShader, const poly_extra_data *extra) {
	v=(voodoo_state*)extra->state;

	UINT32 FOGMODE = v->reg[fogMode].u;

	*strFShader += "  vec4 ff;\n";

	/* constant fog bypasses everything else */
	if (FOGMODE_FOG_CONSTANT(FOGMODE))
		*strFShader += "  ff = fogColor;\n";

	/* non-constant fog comes from several sources */
	else {
		/* if fog_add is zero, we start with the fog color */
		if (FOGMODE_FOG_ADD(FOGMODE) == 0)
			*strFShader += "  ff = fogColor;\n";
		else
			*strFShader += "  ff = vec4(0.0);\n";

		/* if fog_mult is zero, we subtract the incoming color */
		if (FOGMODE_FOG_MULT(FOGMODE) == 0)
			*strFShader += "  ff -= pixel;\n";

		*strFShader += "  float fogblend;\n";
		/* fog blending mode */
		switch (FOGMODE_FOG_ZALPHA(FOGMODE))
		{
			case 0:		/* fog table */
				// blend factor calculated in ogl_get_fog_blend //
				*strFShader += "  fogblend = f_fogblend;\n";
				break;
			case 1:		/* iterated A */
				*strFShader += "  fogblend = gl_Color.a;\n";
				break;
			case 2:		/* iterated Z */
				*strFShader += "  fogblend = f_fogblend;\n";
				break;
			case 3:		/* iterated W - Voodoo 2 only */
				*strFShader += "  fogblend = f_fogblend;\n";
				break;
		}

		/* perform the blend */
		*strFShader += "  ff *= fogblend;\n";

		/* if fog_mult is 0, we add this to the original color */
		if (FOGMODE_FOG_MULT(FOGMODE) == 0)
			*strFShader += "  pixel.rgb += ff.rgb;\n";
		/* otherwise this just becomes the new color */
		else
			*strFShader += "  pixel.rgb = ff.rgb;\n";

	}
}


void ogl_shaders(const poly_extra_data *extra) {
	v=(voodoo_state*)extra->state;

	GLint res;
	std::string strVShader, strFShader;

	/* shaders extensions not loaded */
	if (!glCreateShaderObjectARB) return;

//	UINT32 FBZMODE      = extra->r_fbzMode;
	UINT32 FOGMODE      = extra->r_fogMode;
	UINT32 texcount     = extra->texcount;

	/* build a new shader program */
	if (!extra->info->shader_ready) {

		/* create vertex shader */
		int fcount=0;
		while (glGetError()!=0) {
			fcount++;
			if (fcount>1000) E_Exit("opengl error");
		}

		GLhandleARB m_hVertexShader = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);

		strVShader =
			"attribute float v_fogblend;\n"
			"varying   float f_fogblend;\n"
			"attribute float v_lodblend0;\n"
			"varying   float f_lodblend0;\n"
			"attribute float v_lodblend1;\n"
			"varying   float f_lodblend1;\n"
			"\n"
			"void main()"
			"{\n"
				"  gl_TexCoord[0] = gl_MultiTexCoord0;\n"
				"  gl_TexCoord[1] = gl_MultiTexCoord1;\n"
				"  gl_FrontColor = gl_Color;\n"
				"  f_fogblend = v_fogblend;\n"
				"  f_lodblend0 = v_lodblend0;\n"
				"  f_lodblend1 = v_lodblend1;\n"
				"  gl_Position = ftransform();\n"
			"}\n";

		const char *szVShader = strVShader.c_str();
		glShaderSourceARB(m_hVertexShader, 1, &szVShader, NULL);
		glCompileShaderARB(m_hVertexShader);
		glGetObjectParameterivARB(m_hVertexShader, GL_OBJECT_COMPILE_STATUS_ARB, &res);
		if(res == 0) {
			char infobuffer[1000];
			int infobufferlen = 0;
			glGetInfoLogARB(m_hVertexShader, 999, &infobufferlen, infobuffer);
			infobuffer[infobufferlen] = 0;
			ogl_printInfoLog(m_hVertexShader);
			E_Exit("ERROR: Error compiling vertex shader");
			return;
		}

		/* create fragment shader */
		GLhandleARB m_hFragmentShader = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
		strFShader =
			"varying float f_fogblend;\n"
			"varying float f_lodblend0;\n"
			"varying float f_lodblend1;\n"
			"uniform sampler2D tex0;\n"
			"uniform sampler2D tex1;\n"
			"uniform vec4 color0;\n"
			"uniform vec4 color1;\n"
			"uniform vec4 chromaKey;\n"
			"uniform vec4 chromaRange;\n"
			"uniform float alphaRef;\n"
			"uniform float zaColor;\n"
			"uniform vec4 fogColor;\n"
			"\n"
			"void main()"
			"{\n"
				"  vec4 pixel  = vec4(0.0);\n"
				"  vec4 texel  = vec4(1.0);\n"
				"  vec4 clocal = vec4(1.0);\n"
				"  vec4 cother = vec4(0.0);\n"
				"  vec4 tt     = vec4(0.0);\n"
				"  vec4 blend  = vec4(0.0);\n";

				// TODO glsl depth test? //

				if (texcount >= 2 && v->tmu[1].lodmin < (8 << 8))
				{
					strFShader += "  clocal = texture2DProj(tex1,gl_TexCoord[1]);\n";
					ogl_sh_tex_combine(&strFShader, 1, extra);
					strFShader += "  cother = clocal;\n";
					strFShader += "  texel = clocal;\n";
				}

				if (texcount >= 1 && v->tmu[0].lodmin < (8 << 8))
				{
					strFShader += "  clocal = texture2DProj(tex0,gl_TexCoord[0]);\n";
					ogl_sh_tex_combine(&strFShader, 0, extra);
					strFShader += "  texel = clocal;\n";
				}

				// TODO Clamped ARGB //

				ogl_sh_color_path(&strFShader, extra);

				// fogging //
				if (FOGMODE_ENABLE_FOG(FOGMODE))
					ogl_sh_fog(&strFShader, extra);

				strFShader += "  gl_FragColor = pixel;\n";

				strFShader +=
			"}";

		const char *szFShader = strFShader.c_str();
		glShaderSourceARB(m_hFragmentShader, 1, &szFShader, NULL);

		glCompileShaderARB(m_hFragmentShader);
		glGetObjectParameterivARB(m_hFragmentShader, GL_OBJECT_COMPILE_STATUS_ARB, &res);
		if(res == 0) {
			ogl_printInfoLog(m_hFragmentShader);
			E_Exit("ERROR: Error compiling fragment shader");
			return;
		}


		/* create program object */
		m_hProgramObject = glCreateProgramObjectARB();

		glAttachObjectARB(m_hProgramObject, m_hVertexShader);
		glAttachObjectARB(m_hProgramObject, m_hFragmentShader);

		glLinkProgramARB(m_hProgramObject);

		glGetObjectParameterivARB(m_hProgramObject, GL_OBJECT_LINK_STATUS_ARB, &res);
		if(res == 0) {
			ogl_printInfoLog(m_hProgramObject);
			E_Exit("ERROR: Error linking program");
			return;
		}

		/* use this shader */
		glUseProgramObjectARB(m_hProgramObject);
		extra->info->so_shader_program=(uintptr_t)m_hProgramObject;
		extra->info->so_vertex_shader=(uintptr_t)m_hVertexShader;
		extra->info->so_fragment_shader=(uintptr_t)m_hFragmentShader;

		extra->info->shader_ready=true;

		GLenum glerr=glGetError();
		if (glerr!=0) {
			E_Exit("create shader start glError->%x",glerr);
		}

		int* locations=new int[12];
		locations[0]=glGetUniformLocationARB(m_hProgramObject, "chromaKey");
		locations[1]=glGetUniformLocationARB(m_hProgramObject, "chromaRange");
		locations[2]=glGetUniformLocationARB(m_hProgramObject, "color0");
		locations[3]=glGetUniformLocationARB(m_hProgramObject, "color1");
		locations[4]=glGetUniformLocationARB(m_hProgramObject, "alphaRef");
		locations[5]=glGetUniformLocationARB(m_hProgramObject, "zaColor");
		locations[6]=glGetUniformLocationARB(m_hProgramObject, "tex0");
		locations[7]=glGetUniformLocationARB(m_hProgramObject, "tex1");
		locations[8]=glGetUniformLocationARB(m_hProgramObject, "fogColor");

		locations[9] = glGetAttribLocationARB(m_hProgramObject, "v_fogblend");
		locations[10] = glGetAttribLocationARB(m_hProgramObject, "v_lodblend0");
		locations[11] = glGetAttribLocationARB(m_hProgramObject, "v_lodblend1");
		extra->info->shader_ulocations=locations;
	} else {
		/* use existing shader program */
		if (m_hProgramObject != (GLhandleARB)extra->info->so_shader_program) {
			glUseProgramObjectARB((GLhandleARB)extra->info->so_shader_program);
			m_hProgramObject = (GLhandleARB)extra->info->so_shader_program;
		}
	}

	if (extra->info->shader_ulocations[0]>=0) glUniform4fARB(extra->info->shader_ulocations[0], v->reg[chromaKey].rgb.r/255.0f, v->reg[chromaKey].rgb.g/255.0f, v->reg[chromaKey].rgb.b/255.0f,0);
	if (extra->info->shader_ulocations[1]>=0) glUniform4fARB(extra->info->shader_ulocations[1], v->reg[chromaRange].rgb.r/255.0f, v->reg[chromaRange].rgb.g/255.0f, v->reg[chromaRange].rgb.b/255.0f,0);
	if (extra->info->shader_ulocations[2]>=0) glUniform4fARB(extra->info->shader_ulocations[2], v->reg[color0].rgb.r/255.0f, v->reg[color0].rgb.g/255.0f, v->reg[color0].rgb.b/255.0f, v->reg[color0].rgb.a/255.0f);
	if (extra->info->shader_ulocations[3]>=0) glUniform4fARB(extra->info->shader_ulocations[3], v->reg[color1].rgb.r/255.0f, v->reg[color1].rgb.g/255.0f, v->reg[color1].rgb.b/255.0f, v->reg[color1].rgb.a/255.0f);
	if (extra->info->shader_ulocations[4]>=0) glUniform1fARB(extra->info->shader_ulocations[4], v->reg[alphaMode].rgb.a/255.0f);
	if (extra->info->shader_ulocations[5]>=0) glUniform1fARB(extra->info->shader_ulocations[5], (float)((UINT16)v->reg[zaColor].u)/65535.0f);
	if (extra->info->shader_ulocations[8]>=0) glUniform4fARB(extra->info->shader_ulocations[8], v->reg[fogColor].rgb.r/255.0f, v->reg[fogColor].rgb.g/255.0f, v->reg[fogColor].rgb.b/255.0f,1.0f);

}


void voodoo_ogl_draw_triangle(poly_extra_data *extra) {
	v=extra->state;
	ogl_texture_data td[2];
	ogl_vertex_data vd[3];

	VOGL_ClearBeginMode();

	td[0].enable = false;
	td[1].enable = false;

	UINT32 ALPHAMODE = extra->r_alphaMode;
	UINT32 FBZMODE   = extra->r_fbzMode;


	ogl_get_vertex_data(v->fbi.ax, v->fbi.ay, (void*)extra, &vd[0]);
	ogl_get_vertex_data(v->fbi.bx, v->fbi.by, (void*)extra, &vd[1]);
	ogl_get_vertex_data(v->fbi.cx, v->fbi.cy, (void*)extra, &vd[2]);


	if (FBZMODE_DEPTH_SOURCE_COMPARE(FBZMODE) && VOGL_CheckFeature(VOGL_HAS_STENCIL_BUFFER)) {
		if (m_hProgramObject != 0) {
			glUseProgramObjectARB(0);
			m_hProgramObject = 0;
		}

		if ((FBZMODE_ENABLE_DEPTHBUF(FBZMODE)) && (FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) == 0)) {
			VOGL_SetDepthMode(1,FBZMODE_DEPTH_FUNCTION(FBZMODE));
		} else {
			VOGL_SetDepthMode(0,0);
		}

		VOGL_SetDepthMaskMode(false);
		VOGL_SetColorMaskMode(false, false);

		VOGL_SetAlphaMode(0, 0,0,0,0);

		if (FBZMODE_DRAW_BUFFER(v->reg[fbzMode].u)==0) {
			VOGL_SetDrawMode(true);
		} else {
			VOGL_SetDrawMode(false);
		}

		glEnable(GL_STENCIL_TEST);
		glClear(GL_STENCIL_BUFFER_BIT);
		glStencilFunc(GL_ALWAYS, 1, 1);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

		glBegin(GL_TRIANGLES);

		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

		float depth = (float)(v->reg[zaColor].u&0xffff)/(float)0xffff;
		for (int i=0;i<3;i++)
			glVertex3f(vd[i].x, vd[i].y, depth);

		glEnd();
	}

	ogl_cache_texture(extra,td);
	ogl_shaders(extra);

	if (extra->texcount > 0) {
		for (unsigned int t=0; t<2; t++)
		if ( td[t].enable ) {
			UINT32 TEXMODE = v->tmu[t].reg[textureMode].u;
			glActiveTexture(GL_TEXTURE0_ARB+t);
			glBindTexture (GL_TEXTURE_2D, td[t].texID);
			if (!extra->info->shader_ready) {
				glEnable (GL_TEXTURE_2D);
				// TODO proper fixed-pipeline combiners
				glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			} else {
				if (extra->info->shader_ulocations[6+t] >= 0)
					glUniform1iARB(extra->info->shader_ulocations[6+t],(GLint)t);
			}

			GLint minFilter;
			minFilter = (int)GL_NEAREST + (int)TEXMODE_MINIFICATION_FILTER(TEXMODE);
			if (v->tmu[t].lodmin != v->tmu[t].lodmax)
				minFilter += 0x0100 + (int)TEXMODE_TRILINEAR(TEXMODE)*2;
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,minFilter);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,(int)GL_NEAREST+(int)TEXMODE_MAGNIFICATION_FILTER(TEXMODE));
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,TEXMODE_CLAMP_S(TEXMODE)?GL_CLAMP_TO_EDGE:GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,TEXMODE_CLAMP_T(TEXMODE)?GL_CLAMP_TO_EDGE:GL_REPEAT);
		}
	}

	if (FBZMODE_DEPTH_SOURCE_COMPARE(FBZMODE) && VOGL_CheckFeature(VOGL_HAS_STENCIL_BUFFER)) {
		glStencilFunc(GL_EQUAL, 1, 1);
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
		if (FBZMODE_ENABLE_DEPTHBUF(FBZMODE)) {
			VOGL_SetDepthMode(1,GL_ALWAYS-GL_NEVER);
		} else {
			VOGL_SetDepthMode(0,0);
		}
	} else {
		if (FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) == 0) {
			if (FBZMODE_ENABLE_DEPTHBUF(FBZMODE)) {
				VOGL_SetDepthMode(1,FBZMODE_DEPTH_FUNCTION(FBZMODE));
			} else {
				if (FBZMODE_AUX_BUFFER_MASK(FBZMODE) > 0) {
					VOGL_SetDepthMode(1,GL_ALWAYS-GL_NEVER);
				} else {
					VOGL_SetDepthMode(0,0);
				}
			}
		} else {
			VOGL_SetDepthMode(1,GL_ALWAYS-GL_NEVER);
		}
	}

	bool color_mask = (FBZMODE_RGB_BUFFER_MASK(FBZMODE) > 0);

	if (FBZMODE_AUX_BUFFER_MASK(FBZMODE) > 0) {
		VOGL_SetDepthMaskMode(FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) == 0);
		VOGL_SetColorMaskMode(color_mask, true);
	} else {
		VOGL_SetDepthMaskMode(false);
		VOGL_SetColorMaskMode(color_mask, false);
	}

	if (ALPHAMODE_ALPHABLEND(ALPHAMODE)) {
		VOGL_SetAlphaMode(1, ogl_sfactor[ALPHAMODE_SRCRGBBLEND(ALPHAMODE)], ogl_dfactor[ALPHAMODE_DSTRGBBLEND(ALPHAMODE)],
			(ALPHAMODE_SRCALPHABLEND(ALPHAMODE)==4)?GL_ONE:GL_ZERO,
			(ALPHAMODE_DSTALPHABLEND(ALPHAMODE)==4)?GL_ONE:GL_ZERO);
	} else {
		VOGL_SetAlphaMode(0, 0,0,0,0);
	}

	if (FBZMODE_DRAW_BUFFER(v->reg[fbzMode].u)==0) {
		VOGL_SetDrawMode(true);
		v->fbi.vblank_flush_pending=true;
		cached_line_front_y=-1;
	} else {
		VOGL_SetDrawMode(false);
		cached_line_back_y=-1;
	}


	glBegin(GL_TRIANGLES);

	for (unsigned int i=0;i<3;i++) {
		glColor4fv(&vd[i].r);

		for (unsigned int t=0;t<2;t++)
			if (td[t].enable) {
				glMultiTexCoord4fv(GL_TEXTURE0_ARB+t,&vd[i].m[t].sw);
				if (extra->info->shader_ulocations[10u+t] >= 0)
					glVertexAttrib1fARB((GLuint)extra->info->shader_ulocations[10u+t],vd[i].m[t].lodblend);
			}

		if (extra->info->shader_ulocations[9] >= 0)
			glVertexAttrib1fARB((GLuint)extra->info->shader_ulocations[9],vd[i].fogblend);

		glVertex3fv(&vd[i].x);
	}

	glEnd();

	if (FBZMODE_DEPTH_SOURCE_COMPARE(FBZMODE) && VOGL_CheckFeature(VOGL_HAS_STENCIL_BUFFER)) {
		glDisable(GL_STENCIL_TEST);
	}

	if (!extra->info->shader_ready) {
		glDisable (GL_TEXTURE_2D);
	}
}


void voodoo_ogl_swap_buffer() {
	VOGL_ClearBeginMode();

	SDL_GL_SwapBuffers();

	cached_line_front_y=-1;
	cached_line_back_y=-1;
}


void voodoo_ogl_texture_clear(UINT32 texbase, int TMU) {
	std::map<const UINT32, ogl_texmap>::iterator t;
	t=textures[TMU].find(texbase);
	if (t != textures[TMU].end()) {
		VOGL_ClearBeginMode();
		if (t->second.ids != NULL) {
			std::map<const UINT32, GLuint>::iterator u;
			for (u=t->second.ids->begin(); u!=t->second.ids->end(); ++u) {
				glDeleteTextures(1,&u->second);
			}
			delete t->second.ids;
			t->second.ids = NULL;
		} else {
			t->second.valid_data = false;
		}
		textures[TMU].erase(t);
	}
}

void voodoo_ogl_draw_pixel(int x, int y, bool has_rgb, bool has_alpha, int r, int g, int b, int a) {
	GLfloat x2, y2;

	if (m_hProgramObject != 0) {
		glUseProgramObjectARB(0);
		m_hProgramObject = 0;
	}

	if (LFBMODE_WRITE_BUFFER_SELECT(v->reg[lfbMode].u)==0) {
		VOGL_SetDrawMode(true);
		v->fbi.vblank_flush_pending=true;
	} else {
		VOGL_SetDrawMode(false);
	}

	VOGL_SetDepthMode(0,0);

	VOGL_SetDepthMaskMode(false);
	VOGL_SetColorMaskMode(has_rgb, has_alpha);

	VOGL_SetAlphaMode(0, 0,0,0,0);

	x2 = (GLfloat) x + 0.5;
	y2 = (GLfloat) y + 0.5;

	VOGL_BeginMode(GL_POINTS);
	glColor4ub((GLubyte)(r&0xff), (GLubyte)(g&0xff), (GLubyte)(b&0xff), (GLubyte)(a&0xff));
	glVertex2f(x2, y2);
}

void voodoo_ogl_draw_z(int x, int y, int z) {
//	VOGL_ClearBeginMode();

	if (m_hProgramObject != 0) {
		glUseProgramObjectARB(0);
		m_hProgramObject = 0;
	}

	if (LFBMODE_WRITE_BUFFER_SELECT(v->reg[lfbMode].u)==0) {
		VOGL_SetDrawMode(true);
		v->fbi.vblank_flush_pending=true;
	} else {
		VOGL_SetDrawMode(false);
	}

	VOGL_SetDepthMode(1,GL_ALWAYS-GL_NEVER);

	VOGL_SetDepthMaskMode(true);
	VOGL_SetColorMaskMode(false, false);

	VOGL_SetAlphaMode(0, 0,0,0,0);

	VOGL_BeginMode(GL_POINTS);
//	glBegin(GL_POINTS);
	glVertex3i(x, y, z);	// z adjustment??
//	glEnd();
}

void voodoo_ogl_draw_pixel_pipeline(int x, int y, int r, int g, int b) {
//	VOGL_ClearBeginMode();
	GLfloat x2, y2;

	// TODO redo everything //
	if (m_hProgramObject != 0) {
		glUseProgramObjectARB(0);
		m_hProgramObject = 0;
	}

	if (LFBMODE_WRITE_BUFFER_SELECT(v->reg[lfbMode].u)==0) {
		VOGL_SetDrawMode(true);
		v->fbi.vblank_flush_pending=true;
	} else {
		VOGL_SetDrawMode(false);
	}

	VOGL_SetDepthMode(0,0);

	VOGL_SetDepthMaskMode(false);
	if (FBZMODE_AUX_BUFFER_MASK(v->reg[fbzMode].u) > 0) {
		VOGL_SetColorMaskMode(true, true);
	} else {
		VOGL_SetColorMaskMode(true, false);
	}

	if (ALPHAMODE_ALPHABLEND(v->reg[alphaMode].u)) {
		VOGL_SetAlphaMode(1, ogl_sfactor[ALPHAMODE_SRCRGBBLEND(v->reg[alphaMode].u)], ogl_dfactor[ALPHAMODE_DSTRGBBLEND(v->reg[alphaMode].u)],
			(ALPHAMODE_SRCALPHABLEND(v->reg[alphaMode].u)==4)?GL_ONE:GL_ZERO,
			(ALPHAMODE_DSTALPHABLEND(v->reg[alphaMode].u)==4)?GL_ONE:GL_ZERO);
	} else {
		VOGL_SetAlphaMode(0, 0,0,0,0);
	}

	x2 = (GLfloat) x + 0.5;
	y2 = (GLfloat) y + 0.5;

	VOGL_BeginMode(GL_POINTS);
//	glBegin(GL_POINTS);
//	glColor3f((float)r/255.0f, (float)g/255.0f, (float)b/255.0f);
	glColor3ub((GLubyte)(r&0xff), (GLubyte)(g&0xff), (GLubyte)(b&0xff));
	glVertex2f(x2, y2);
//	glEnd();
}


void voodoo_ogl_clip_window(voodoo_state *v) {
    (void)v;//UNUSED
/*	VOGL_ClearBeginMode();

	int sx = (v->reg[clipLeftRight].u >> 16) & 0x3ff;
	int ex = (v->reg[clipLeftRight].u >> 0) & 0x3ff;
	int sy = (v->reg[clipLowYHighY].u >> 16) & 0x3ff;
	int ey = (v->reg[clipLowYHighY].u >> 0) & 0x3ff;

	if (FBZMODE_Y_ORIGIN(v->reg[fbzMode].u)) {
		sy = (v->fbi.yorigin+1 - sy) & 0x3ff;
		ey = (v->fbi.yorigin+1 - ey) & 0x3ff;
	}

	if ((sx>0) || (sy>0) || (ex<v->fbi.width) || (ey<v->fbi.height))
	{
		glEnable(GL_SCISSOR_TEST);
		glScissor(sx,sy,ex-sx,ey-sy);
	} else {
		glScissor(0,0,v->fbi.width,v->fbi.height);
		glDisable(GL_SCISSOR_TEST);
	} */
}


void voodoo_ogl_fastfill(void) {
	VOGL_ClearBeginMode();

	VOGL_SetDepthMaskMode(true);

	int sx = (v->reg[clipLeftRight].u >> 16) & 0x3ff;
	int ex = (v->reg[clipLeftRight].u >> 0) & 0x3ff;
	int sy = (v->reg[clipLowYHighY].u >> 16) & 0x3ff;
	int ey = (v->reg[clipLowYHighY].u >> 0) & 0x3ff;

//	if (FBZMODE_Y_ORIGIN(v->reg[fbzMode].u))
	{
		int tmp = ((int)v->fbi.yorigin+1 - ey) & 0x3ff;
		ey = ((int)v->fbi.yorigin+1 - sy) & 0x3ff;
		sy = tmp;
	}

	bool scissors_needed = true;
	if ((sx == 0) && (sy == 0)) {
		if (((Bitu)ex == (Bitu)v->fbi.width) && ((Bitu)ey == (Bitu)v->fbi.height)) scissors_needed = false;
	}

	if (scissors_needed) {
		glEnable(GL_SCISSOR_TEST);
		glScissor(sx,sy,ex-sx,ey-sy);
	}


	uint32_t clear_mask=0;
	if (FBZMODE_RGB_BUFFER_MASK(v->reg[fbzMode].u)) {
		clear_mask|=GL_COLOR_BUFFER_BIT;

//		if (FBZMODE_AUX_BUFFER_MASK(v->reg[fbzMode].u) && v->fbi.auxoffs != (UINT32)(~0)) ...
		VOGL_SetColorMaskMode(true, true);

		if (last_clear_color!=v->reg[color1].u) {
			glClearColor((float)v->reg[color1].rgb.r/255.0f,
				(float)v->reg[color1].rgb.g/255.0f,
				(float)v->reg[color1].rgb.b/255.0f,
				(float)v->reg[color1].rgb.a/255.0f);
			last_clear_color=v->reg[color1].u;
		}
		if (FBZMODE_DRAW_BUFFER(v->reg[fbzMode].u)==0) {
			VOGL_SetDrawMode(true);
			v->fbi.vblank_flush_pending=true;
			cached_line_front_y=-1;
		} else {
			VOGL_SetDrawMode(false);
			cached_line_back_y=-1;
		}
	}
	if (FBZMODE_AUX_BUFFER_MASK(v->reg[fbzMode].u) && v->fbi.auxoffs != (UINT32)(~0)) {
//	if (FBZMODE_ENABLE_DEPTHBUF(v->reg[fbzMode].u)) {
		clear_mask|=GL_DEPTH_BUFFER_BIT;
		glClearDepth((float)((UINT16)v->reg[zaColor].u)/65535.0f);
	}

	if (clear_mask) glClear(clear_mask);

	if (scissors_needed) {
		glScissor(0,0,(int)v->fbi.width,(int)v->fbi.height);
		glDisable(GL_SCISSOR_TEST);
	}
}

void voodoo_ogl_clear(void) {
	VOGL_ClearBeginMode();

	VOGL_SetDrawMode(false);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	last_clear_color = 0;
	glClearDepth(1.0f);
	glClearStencil(0);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	voodoo_ogl_swap_buffer();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}


UINT32 voodoo_ogl_read_pixel(int x, int y) {
	UINT32 data[2];
	if ((x < 0) || (y < 0) || (x >= (INT32)v->fbi.width) || (y >= (INT32)v->fbi.height)) return 0xffffffff;

	UINT32 mode=GL_RGBA;
	switch (LFBMODE_READ_BUFFER_SELECT(v->reg[lfbMode].u)) {
		case 0:			/* front buffer */
			VOGL_SetReadMode(true);
			if ((cached_line_front_y != y) || (x+1 >= cached_line_front_width)) {
				if (cached_line_front_length<(INT32)v->fbi.width) {
					if (cached_line_front_data!=NULL) free(cached_line_front_data);
					size_t span_length=((v->fbi.width+64u)&(~15u));
					cached_line_front_data=(UINT32*)malloc(sizeof(UINT32)*span_length);
					cached_line_front_length=(INT32)span_length;
				}
				glReadPixels(0,(int)v->fbi.height-y,(int)v->fbi.width,1,mode,GL_UNSIGNED_BYTE,cached_line_front_data);
				cached_line_front_y=y;
				cached_line_front_width = (INT32)v->fbi.width;
			}
			data[0]=cached_line_front_data[x];
			data[1]=cached_line_front_data[x+1];
			break;
		case 1:			/* back buffer */
			VOGL_SetReadMode(false);
			if ((cached_line_back_y != y) || (x+1 >= cached_line_back_width)) {
				if (cached_line_back_length<(INT32)v->fbi.width) {
					if (cached_line_back_data!=NULL) free(cached_line_back_data);
					size_t span_length=((v->fbi.width+64u)&(~15u));
					cached_line_back_data=(UINT32*)malloc(sizeof(UINT32)*span_length);
					cached_line_back_length=(INT32)span_length;
				}
				glReadPixels(0,(int)v->fbi.height-y,(int)v->fbi.width,1,mode,GL_UNSIGNED_BYTE,cached_line_back_data);
				cached_line_back_y=y;
				cached_line_back_width = (INT32)v->fbi.width;
			}
			data[0]=cached_line_back_data[x];
			data[1]=cached_line_back_data[x+1];
			break;
		case 2:			/* aux buffer */
			mode=GL_DEPTH_COMPONENT;
			VOGL_SetReadMode(false);
			glReadPixels(x,(int)v->fbi.height-y,2,1,mode,GL_UNSIGNED_INT,&data);
			return ((data[0]>>16)&0xffff) | (data[1] & 0xffff0000);
		default:
			E_Exit("read from invalid buf %x",LFBMODE_READ_BUFFER_SELECT(v->reg[lfbMode].u));
			break;
	}

	return ((RGB_BLUE(data[0])>>3)<<11) | ((RGB_GREEN(data[0])>>2)<<5) | (RGB_RED(data[0])>>3) |
			((RGB_BLUE(data[1])>>3)<<27) | ((RGB_GREEN(data[1])>>2)<<21) | ((RGB_RED(data[1])>>3)<<16);
}


void voodoo_ogl_vblank_flush(void) {
	VOGL_ClearBeginMode();
	glFlush();
}


void voodoo_ogl_set_window(voodoo_state *v) {
	VOGL_ClearBeginMode();

	// 	matrix mode GL_PROJECTION assumed
	bool size_changed=false;
	if ((v->fbi.width!=last_width) || (v->fbi.height!=last_height)) size_changed=true;
	if (size_changed || (last_orientation != (INT32)FBZMODE_Y_ORIGIN(v->reg[fbzMode].u))) {
		glLoadIdentity( );
		if (FBZMODE_Y_ORIGIN(v->reg[fbzMode].u))
			glOrtho( 0, v->fbi.width, 0, v->fbi.height, 0.0f, -1.0f );
		else
			glOrtho( 0, v->fbi.width, v->fbi.height, 0, 0.0f, -1.0f );
		if (last_orientation != (INT32)FBZMODE_Y_ORIGIN(v->reg[fbzMode].u))
			last_orientation = FBZMODE_Y_ORIGIN(v->reg[fbzMode].u);
	}
	if (size_changed) {
		//correction for viewport if 640x400
		if( v->fbi.height < 480 && GFX_IsFullscreen()) adjust_y=(480-(int)v->fbi.height)/2;
		if( v->fbi.width < 640 && GFX_IsFullscreen()) adjust_x=(640-(int)v->fbi.width)/2;
		glViewport( adjust_x, adjust_y, (int)v->fbi.width, (int)v->fbi.height );
		last_width = v->fbi.width;
		last_height = v->fbi.height;
	}
}

void voodoo_ogl_reset_videomode(void) {
#if defined(WIN32) && !defined(C_SDL2)
    /* always show the menu */
    void DOSBox_SetMenu(void);
    DOSBox_SetMenu();
#endif

    GFX_PreventFullscreen(true);

	last_clear_color=0;

	last_width=0;
	last_height=0;
	last_orientation=-1;

	VOGL_Reset();

	GFX_TearDown();

#if !defined(C_SDL2)
	bool full_sdl_restart = true;	// make dependent on surface=opengl
#endif

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);

	bool has_alpha = true;
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	bool has_stencil = true;
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	/*glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	glOrtho( 0, v->fbi.width, v->fbi.height, 0, -1, 1 );
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();*/
	// END OF FIX

#if defined (WIN32) && SDL_VERSION_ATLEAST(1, 2, 11) && !defined(C_SDL2)
	SDL_GL_SetAttribute( SDL_GL_SWAP_CONTROL, 0 );
#endif

#if !defined (WIN32) && SDL_VERSION_ATLEAST(1, 2, 10)
	// broken on windows (longstanding SDL bug), may help other platforms to force hardware acceleration
	SDL_GL_SetAttribute( SDL_GL_ACCELERATED_VISUAL, 1 );
#endif

    ogl_surface = NULL;

    void GFX_LosingFocus(void), GFX_ReleaseMouse(void), GFX_ForceFullscreenExit(void);

    GFX_LosingFocus();
    GFX_ReleaseMouse();
    GFX_ForceFullscreenExit();

#if defined(C_SDL2)
    GFX_SetResizeable(false);
    sdl.window = GFX_SetSDLWindowMode((int)v->fbi.width, (int)v->fbi.height, SCREEN_OPENGL);
    if (sdl.window != NULL) ogl_surface = SDL_GetWindowSurface(sdl.window);
	if (ogl_surface == NULL)
		E_Exit("VOODOO: opengl init error");
#else
	Uint32 sdl_flags = SDL_OPENGL;

    ogl_surface = SDL_SetVideoMode((int)v->fbi.width, (int)v->fbi.height, 32, sdl_flags);

	if (ogl_surface == NULL) {
		if (full_sdl_restart) {
			SDL_QuitSubSystem(SDL_INIT_VIDEO);
			SDL_InitSubSystem(SDL_INIT_VIDEO);
			ogl_surface = SDL_SetVideoMode((int)v->fbi.width, (int)v->fbi.height, 32, sdl_flags);
		}
		if (ogl_surface == NULL) {
			has_alpha = false;
			SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
			if ((ogl_surface = SDL_SetVideoMode((int)v->fbi.width, (int)v->fbi.height, 32, sdl_flags)) == NULL) {
				has_stencil = false;
				SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
				if ((ogl_surface = SDL_SetVideoMode((int)v->fbi.width, (int)v->fbi.height, 32, sdl_flags)) == NULL) {
					if (sdl_flags & SDL_FULLSCREEN) {
						sdl_flags &= ~(SDL_FULLSCREEN);
						if ((ogl_surface = SDL_SetVideoMode((int)v->fbi.width, (int)v->fbi.height, 32, sdl_flags)) == NULL) {
							E_Exit("VOODOO: opengl init error");
						}
					} else {
						E_Exit("VOODOO: opengl init error");
					}
				}
				LOG_MSG("VOODOO: Graphics mode does not support Stencil/Alpha channels");
			} else {
				LOG_MSG("VOODOO: Graphics mode does not support Alpha channel");
			}
		}
	}
#endif

    v->ogl_dimchange = true;

	glViewport( 0, 0, (int)v->fbi.width, (int)v->fbi.height );
	last_width = v->fbi.width;
	last_height = v->fbi.height;

    /* NTS: Some demoscene 3Dfx stuff looks terrible without this.
     *      This is said to be the initial state of an OpenGL context.
     *      This is in direct contradiction to the OpenGL output setup
     *      in src/gui/sdlmain.cpp that sets up GL_FLAT shading */
    glShadeModel (GL_SMOOTH);

	GFX_PreventFullscreen(true);
	GFX_UpdateSDLCaptureState();

	int value;

	bool few_colors = false;
	if (SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &value) == 0) {
		if (value < 8) few_colors = true;
	}
	if (SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &value) == 0) {
		if (value < 8) few_colors = true;
	}
	if (SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &value) == 0) {
		if (value < 8) few_colors = true;
	}
	if (few_colors) LOG_MSG("opengl: warning: graphics mode with insufficient color depth");

	if (SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &value) == 0) {
		if (value < 24) LOG_MSG("opengl: warning: depth buffer with insufficient resolution");
	}

	if (SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &value) == 0) {
		if (value < 1) has_stencil = false;
	}
	if (SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &value) == 0) {
		if (value < 8) has_alpha = false;
	}

	if (has_stencil) VOGL_FlagFeature(VOGL_HAS_STENCIL_BUFFER);
	if (has_alpha) VOGL_FlagFeature(VOGL_HAS_ALPHA_PLANE);

	GLint depth_csize;
	glGetIntegerv(GL_DEPTH_BITS, &depth_csize);
	if (depth_csize < 16) {
		LOG_MSG("VOODOO: OpenGL: invalid depth size %d",depth_csize);
	}

#if defined(WIN32) && !defined(C_SDL2)
	// Windows SDL 1.x builds may destroy and re-create the window, and lose our menu bar.
	// make sure to put it back.
	bool OpenGL_using(void);
	void DOSBox_SetMenu(void);
	if (OpenGL_using()) DOSBox_SetMenu();
#endif

	/* Something in Windows keeps changing the shade model on us from the last glShadeModel() call. Change it back. */
	glShadeModel(GL_SMOOTH);

	LOG_MSG("VOODOO: OpenGL: mode set, resolution %d:%d", v->fbi.width, v->fbi.height);
}

void voodoo_ogl_update_dimensions(void) {
	voodoo_ogl_leave(false);

	voodoo_ogl_reset_videomode();

	glMatrixMode( GL_PROJECTION );
	voodoo_ogl_set_window(v);

	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity( );
	glMatrixMode( GL_PROJECTION );
}

bool voodoo_ogl_init(voodoo_state *v) {
	extern void CPU_Core_Dyn_X86_SetFPUMode(bool dh_fpu);
//	CPU_Core_Dyn_X86_SetFPUMode(false);

	extern void CPU_Core_Dyn_X86_Cache_Reset(void);
//	CPU_Core_Dyn_X86_Cache_Reset();


	voodoo_ogl_reset_videomode();

	if (!VOGL_Initialize()) {
		VOGL_Reset();
		// reset video mode etc.
		return false;
	}

/*	std::string features = "";
	if (VOGL_CheckFeature(VOGL_HAS_SHADERS)) features += " shader";
	if (VOGL_CheckFeature(VOGL_HAS_ALPHA_PLANE)) features += " alpha-plane";
	if (VOGL_CheckFeature(VOGL_HAS_STENCIL_BUFFER)) features += " stencil-buffer";

	if (features == "") features = " none";

	LOG_MSG("VOODOO: OpenGL: features enabled:%s",features.c_str()); */


	glMatrixMode( GL_PROJECTION );
	voodoo_ogl_set_window(v);

	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity( );
	glMatrixMode( GL_PROJECTION );

	return true;
}

void voodoo_ogl_leave(bool leavemode) {
	VOGL_ClearBeginMode();

	std::map<const UINT32, ogl_texmap>::iterator t;
	for (int j=0; j<2; j++) {
		for (t=textures[j].begin(); t!=textures[j].end(); ++t) {
			if (t->second.ids != NULL) {
				std::map<const UINT32, GLuint>::iterator u;
				for (u=t->second.ids->begin(); u!=t->second.ids->end(); ++u) {
					glDeleteTextures(1,&u->second);
				}
				if (!t->second.ids->empty()) t->second.ids->clear();
				delete t->second.ids;
				t->second.ids = NULL;
			} else {
				glDeleteTextures(1,&t->second.current_id);
			}
		}
		if (!textures[j].empty()) textures[j].clear();
	}


	if (m_hProgramObject != 0) {
		glUseProgramObjectARB(0);
		m_hProgramObject = 0;
	}

	for (int hct=0; hct<RASTER_HASH_SIZE; hct++) {
		raster_info *info = v->raster_hash[hct];
		for (; info; info = info->next) {
			if (info->shader_ready) {
				delete[] info->shader_ulocations;
				info->shader_ulocations=NULL;

				if (info->so_shader_program > 0) {
					if (info->so_vertex_shader > 0) glDetachObjectARB((GLhandleARB)info->so_shader_program, (GLhandleARB)info->so_vertex_shader);
					if (info->so_fragment_shader > 0) glDetachObjectARB((GLhandleARB)info->so_shader_program, (GLhandleARB)info->so_fragment_shader);
					if (info->so_vertex_shader > 0) glDeleteObjectARB((GLhandleARB)info->so_vertex_shader);
					if (info->so_fragment_shader > 0) glDeleteObjectARB((GLhandleARB)info->so_fragment_shader);
					glDeleteObjectARB((GLhandleARB)info->so_shader_program);
				}

				info->shader_ready=false;
			}
		}
	}


	cached_line_front_y=-1;
	cached_line_back_y=-1;

	if (leavemode) {
		LOG_MSG("VOODOO: OpenGL: quit");

        ogl_surface = NULL;
        GFX_PreventFullscreen(false);
        GFX_RestoreMode();
#if defined(C_SDL2)
        if (Direct3D_using()) {
            GFX_SetSDLWindowMode(currentWindowWidth, currentWindowHeight, SCREEN_SURFACE);
            change_output(6);
        }
#endif
    }
}

void voodoo_ogl_shutdown(voodoo_state *v) {
	// TODO revert to previous video mode //

	voodoo_ogl_leave(false);

	v->active = false;
}

#else


bool voodoo_ogl_init(voodoo_state *v) {
    (void)v;
	return false;
}

void voodoo_ogl_leave(void) {
}

void voodoo_ogl_shutdown(voodoo_state *v) {
    (void)v;
}

void voodoo_ogl_set_window(voodoo_state *v) {
    (void)v;
	E_Exit("invalid call to voodoo_ogl_set_window");
}

void voodoo_ogl_swap_buffer(void) {
	E_Exit("invalid call to voodoo_ogl_swap_buffer");
}

void voodoo_ogl_vblank_flush(void) {
	E_Exit("invalid call to voodoo_ogl_vblank_flush");
}

void voodoo_ogl_clear(void) {
	E_Exit("invalid call to voodoo_ogl_clear");
}

void voodoo_ogl_fastfill(void) {
	E_Exit("invalid call to voodoo_ogl_fastfill");
}

void voodoo_ogl_clip_window(voodoo_state *v) {
    (void)v;
	E_Exit("invalid call to voodoo_ogl_clip_window");
}

void voodoo_ogl_texture_clear(UINT32 texbase, int TMU) {
    (void)texbase;
    (void)TMU;
	E_Exit("invalid call to voodoo_ogl_texture_clear");
}

void voodoo_ogl_invalidate_paltex(void) {
	E_Exit("invalid call to voodoo_ogl_invalidate_paltex");
}

void voodoo_ogl_draw_pixel(int x, int y, bool has_rgb, bool has_alpha, int r, int g, int b, int a) {
    (void)has_alpha;
    (void)has_rgb;
    (void)x;
    (void)y;
    (void)r;
    (void)g;
    (void)b;
    (void)a;
	E_Exit("invalid call to voodoo_ogl_draw_pixel");
}

void voodoo_ogl_draw_z(int x, int y, int z1, int z2) {
    (void)z1;
    (void)z2;
    (void)x;
    (void)y;
	E_Exit("invalid call to voodoo_ogl_draw_z");
}

void voodoo_ogl_draw_pixel_pipeline(int x, int y, int r, int g, int b) {
    (void)x;
    (void)y;
    (void)r;
    (void)g;
    (void)b;
	E_Exit("invalid call to voodoo_ogl_draw_pixel_pipeline");
}

UINT32 voodoo_ogl_read_pixel(int x, int y) {
    (void)x;
    (void)y;
	E_Exit("invalid call to voodoo_ogl_read_pixel");
	return 0;
}

void voodoo_ogl_draw_triangle(poly_extra_data *extra) {
    (void)extra;
	E_Exit("invalid call to voodoo_ogl_draw_triangle");
}

#endif
