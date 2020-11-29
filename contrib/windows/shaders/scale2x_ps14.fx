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
// ** Scale2x **
//
// ** PIXEL SHADER 1.4 VERSION **
//
// This version of the algorithm for Pixel Shader V 1.4 is significantly different from the
// original 2.0 version. It doesn't use any dependant texture reads
// Understanding the code may be quite difficult.
//
// ** ORIGINAL NOTES **
//
// Homepage: http://scale2x.sourceforge.net/
//
// Adapted by Ryan A. Nunn into Pixel and Vertex Shaders to be used with the 
// Scaling2D entry in the Beyond3D/ATI Shader Competition
//
// Using the following source pixel map
//
// A B C
// D E F
// G H I
//
// and the following dest pixel map (which is the pixel 'E' expanded)
//
// E0 E1
// E2 E3
//
// The Scale2x algorithm works as follows:
// E0 = D == B && B != F && D != H ? D : E;
// E1 = B == F && B != D && F != H ? F : E;
// E2 = D == H && D != B && H != F ? D : E;
// E3 = H == F && D != H && B != F ? F : E;
//
// That can be re-written as this:
// E0 = D == B && B != F && H != D ? D : E;
// E1 = F == B && B != D && H != F ? F : E;
// E2 = D == H && H != F && B != D ? D : E;
// E3 = F == H && H != D && B != F ? F : E;
// 
// As can be seen, there are some fairly obvious patterns
// For the bottom row, pixels B and H are swapped vs the top row
// And for the right column, pixels D and F are swapped vs the left column 
//	
// That means, with dependant textures reads, this algorithm can be converted
// into a fairly compact pixel shader
//
//


#include "Scaling.inc"

// The name of this effect
string name : NAME = "Scale2x PS1.4";
float scaling : SCALING = 2.0;

//
// Techniques
//

// combineTechnique: Final combine steps. Outputs to destination frame buffer
string combineTechique : COMBINETECHNIQUE =  "Scale2x_New_14";

// preprocessTechnique: PreProcessing steps. Outputs to WorkingTexture
//string preprocessTechique : PREPROCESSTECHNIQUE = "";


// 
// Textures and Samplers
//

texture SelectTexture
< 
	string function = "SelectOffset";	// Function to generate from
	int width = 2;
	int height = 2;
>;

sampler	SelectSampler = sampler_state	
{
	Texture	  = (SelectTexture);
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = NONE;
	AddressU  = WRAP;
	AddressV  = WRAP;
	SRGBTEXTURE = FALSE;
};

//
// Texture Gen Shaders
//

float4 SelectOffset(float3 Pos : POSITION) : COLOR
{
	float4 ret = { 0.0, 0.0, 0, 0 };
	
	// 0.00 = Do nothing in shader
	// 0.25 = Invert the logical comparison
	// 1.00 = Ingore this value
	
	if (Pos.x <  0.5 && Pos.y <  0.5) ret = float4(0.00, 0.25, 0.25, 1.00);
	if (Pos.x <  0.5 && Pos.y >= 0.5) ret = float4(0.25, 1.00, 0.00, 0.25);
	if (Pos.x >= 0.5 && Pos.y <  0.5) ret = float4(0.25, 0.00, 1.00, 0.25);
	if (Pos.x >= 0.5 && Pos.y >= 0.5) ret = float4(1.00, 0.25, 0.25, 0.00);
	
	return ret;
}

//
// Vertex Shader Output
//

struct VS_OUTPUT_NEW_14
{
	float4 Position		: POSITION;
	float2 CentreUV		: TEXCOORD0;
	float2 LeftUV		: TEXCOORD1;
	float2 RightUV		: TEXCOORD2;
	float2 AboveUV		: TEXCOORD3;
	float2 BelowUV		: TEXCOORD4;
	float2 Selector		: TEXCOORD5;
};


//
// Vertex Shader
//

VS_OUTPUT_NEW_14 VS_New_14 (
			float3 Position : POSITION,
			float2 TexCoord : TEXCOORD0)
{
	VS_OUTPUT_NEW_14 Out = (VS_OUTPUT_NEW_14)0;

	Out.Position = mul(float4(Position, 1), WorldViewProjection);   // Matrix multipliy
	Out.CentreUV = TexCoord;
	Out.Selector = TexCoord*SourceDims;
	Out.LeftUV = TexCoord + float2(-TexelSize.x, 0);
	Out.RightUV = TexCoord + float2(TexelSize.x, 0);
	Out.AboveUV = TexCoord + float2(0, -TexelSize.y);
	Out.BelowUV = TexCoord + float2(0, TexelSize.y);

	return Out;
}


//
// Pixel Shader
//


/*
TL: A==L && A!=R && B!=L
BL: B==L && B!=R && A!=L
TR: A==R && A!=L && B!=R
BR: B==R && B!=L && A!=R

L TL: A==L && A!=R && B!=L && 1111 --
L BL: A!=L && 1111 && B==L && B!=R -+
R TR: A!=L && A==R && 1111 && B!=R +-
R BR: 1111 && A!=R && B!=L && B==R ++

L TL: !(A!=L || A==R || B==L || 0000) --	0.00, 0.25, 0,25, 1.00
L BL: !(A==L || 0000 || B!=L || B==R) -+	0.25, 1.00, 0.00, 0.25
R TR: !(A==L || A!=R || 0000 || B==R) +-	0.25, 0.00, 1.00, 0.25
R BR: !(0000 || A==R || B==L || B!=R) ++	1.00, 0.25, 0.25, 0.00

// if greater than 0, we invert 
// if greater then 0.5, we zero it out

1 1 0 1

cmp -> r2 if != 0.0
cnd -> r1 if  > 0.5

use left texture if y or w components are 0

*/

// PS1.4 shader
PIXELSHADER PS_New_14 = asm 
{
	ps_1_4
	def c0, 0, 1, 0, 0
	def c1, 1, 0, 0, 0
	
	texld	r1,		t1	// L
	texld	r2,		t2	// R
	texld	r3,		t3	// A
	texld	r4,		t4	// B

	// We do the comparisons here

	sub_x8	r5,		r1,	r3	
	dp4_x8	r0.x,	r5,	r5		// L!=A
	
	sub_x8	r5,		r2,	r3		
	dp4_x8	r0.y,	r5,	r5		// R!=A

	sub_x8	r5,		r1,	r4		
	dp4_x8	r0.z,	r5,	r5		// L!=B

	sub_x8	r5,		r2,	r4		
	dp4_x8	r1.xyz,	r5,	r5		// R!=B
	
	phase
	
	//mov	r0,		r0	// L!=A, R!=A, L!=B
	//mov	r1,		r1	// R!=B
	texld	r2,		t0	// C
	texld	r3,		t1	// L
	texld	r4,		t2	// R
	texld	r5,		t5	// SelectTexture

	// Combine the 4 comparison results into 1 register
	cmp_sat	r0.xyz,	-r0,	c0.x,	c0.y
	+cmp_sat r0.w,	-r1.x,	c0.x,	c0.y

	// if the SelectTexture is greater than 0, we invert that result
	cmp		r0,		-r5,	r0,		1-r0
	// if the SelectTexture is greater than 0.5, we ignore that result (set to 0)
	cnd		r0,		r5,		c0.x,	r0
	// Do logical OR of each result
	dp4		r0,		r0,		r0

	// Choose left/right: Use left texture if y or w components in SelectTexture are 0
	mul		r5.w,	r5.y,	r5.w
	cmp		r1,		-r5.w,	r4,	 r3	
	
	// Output final colour
	cmp		r0,		-r0.w,	r1,	 r2
};

//
// Technique
//

technique Scale2x_New_14
{
	pass P0
	{
		// Shaders
		VertexShader = compile vs_1_1 VS_New_14();
		PixelShader  = (PS_New_14);
		AlphaBlendEnable = FALSE;
		ColorWriteEnable = RED|GREEN|BLUE|ALPHA;
		SRGBWRITEENABLE = FALSE;
		
		Sampler[0] = (SourceSampler);
		Sampler[1] = (SourceSampler);
		Sampler[2] = (SourceSampler);
		Sampler[3] = (SourceSampler);
		Sampler[4] = (SourceSampler);
		Sampler[5] = (SelectSampler);
	}
}
