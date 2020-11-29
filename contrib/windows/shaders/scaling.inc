/*
Copyright (C) 2003 Ryan A. Nunn

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

//
// Matrix Definitions for Scaler Effects
//

matrix World				: WORLD;
matrix View				: VIEW;
matrix Projection			: PROJECTION;
matrix Worldview			: WORLDVIEW;			// world * view
matrix ViewProjection			: VIEWPROJECTION;		// view * projection
matrix WorldViewProjection		: WORLDVIEWPROJECTION;		// world * view * projection

//
// Source Texture Definition for Scaler Effects
//

// Image Dimentions 
float2 SourceDims : SOURCEDIMS = 256.0F;

// Size of one Texel
float2 TexelSize  : TEXELSIZE = 1.0F/256.0F;

//
// Source Texture
//
texture SourceTexture				: SOURCETEXTURE;

sampler	SourceSampler = sampler_state {
	Texture	  = (SourceTexture);
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = NONE;
	AddressU  = Clamp;
	AddressV  = Clamp;
	SRGBTEXTURE = FALSE;
};

sampler	BilinearSourceSampler = sampler_state {
	Texture	  = (SourceTexture);
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = NONE;
	AddressU  = Clamp;
	AddressV  = Clamp;
	SRGBTEXTURE = FALSE;
};

sampler	SRGBSourceSampler = sampler_state {
	Texture	  = (SourceTexture);
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = NONE;
	AddressU  = Clamp;
	AddressV  = Clamp;
	SRGBTEXTURE = TRUE;
};

sampler	SRGBBilinearSourceSampler = sampler_state {
	Texture	  = (SourceTexture);
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = NONE;
	AddressU  = Clamp;
	AddressV  = Clamp;
	SRGBTEXTURE = TRUE;
};

//
// Working Texture and Working RenderTarget
// Same Dims as Source Texture
//
texture WorkingTexture				: WORKINGTEXTURE;

sampler	WorkingSampler = sampler_state {
	Texture	  = (WorkingTexture);
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = NONE;
	AddressU  = Clamp;
	AddressV  = Clamp;
	SRGBTEXTURE = FALSE;
};

texture WorkingTexture1				: WORKINGTEXTURE1;

sampler	WorkingSampler1 = sampler_state {
	Texture	  = (WorkingTexture1);
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = NONE;
	AddressU  = Clamp;
	AddressV  = Clamp;
	SRGBTEXTURE = FALSE;
};

//
// procedural textures that selects which pixel to generate
//
texture OutputSelectTexture
< 
	string function = "GenerateOutputSelect";	// Function to generate from
	int width = 2;
	int height = 2;
>;

// TextureShader for the OutputSelectTexture
float4 GenerateOutputSelect(float3 Pos : POSITION) : COLOR
{
	float4 ret = (float4)1;
	if (Pos.x < 0.5 && Pos.y < 0.5) ret[0] = 0;
	else if (Pos.y < 0.5) ret[1] = 0;
	else if (Pos.x < 0.5) ret[2] = 0;
	else ret[3] = 0;
	return ret;
}

// Sampler for the OutputSelectTexture
sampler	OutputSelectSampler = sampler_state	
{
	Texture	  = (OutputSelectTexture);
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = NONE;
	AddressU  = WRAP;
	AddressV  = WRAP;
	SRGBTEXTURE = FALSE;
};

//
// A Simple Texture that can do a modulo (or frac) by a lookup, rather than by 
// using a arithmetic instruction. Not entirely accurate
//
texture ModuloTexture
< 
	string function = "GenerateModulo";	// Function to generate from
	int width = 16;
	int height = 16;
>;

// TextureShader for the ModuloTexture
float4 GenerateModulo(float3 Pos : POSITION) : COLOR
{
	return float4(Pos.x, Pos.y, 0, 0);
}

// Sampler for the ModuloTexture
sampler	ModuloSampler = sampler_state	
{
	Texture	  = (ModuloTexture);
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = NONE;
	AddressU  = WRAP;
	AddressV  = WRAP;
	SRGBTEXTURE = FALSE;
};
