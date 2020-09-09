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


#include "Scaling.inc"

// The name of this effect
string name : NAME = "Scale2x";
float scaling : SCALING = 2.0;

//
// Techniques
//

// combineTechnique: Final combine steps. Outputs to destination frame buffer
string combineTechique : COMBINETECHNIQUE =  "Scale2x_New";

// preprocessTechnique: PreProcessing steps. Outputs to WorkingTexture
//string preprocessTechique : PREPROCESSTECHNIQUE = "";


// 
// Textures and Samplers
//


//
// Texture Gen Shaders
//


//
// Vertex Shader Output
//

struct VS_OUTPUT_NEW
{
	float4 Position		: POSITION;
	float2 CentreUV		: TEXCOORD0;
	float2 Selector		: TEXCOORD1;
};


//
// Vertex Shader
//

VS_OUTPUT_NEW VS_New(
			float3 Position : POSITION,
			float2 TexCoord : TEXCOORD0)
{
	VS_OUTPUT_NEW Out = (VS_OUTPUT_NEW)0;

	Out.Position = mul(float4(Position, 1), WorldViewProjection);   // Matrix multipliy
	Out.CentreUV = TexCoord;
	Out.Selector = TexCoord*SourceDims;

	return Out;
}


//
// Pixel Shader
//

// Even faster version (Uses dependant texture reads to work)
float4 PS_New ( in VS_OUTPUT_NEW input ) : COLOR
{
//	half2 selector = frac(input.Selector);
	half4 selector = tex2D(ModuloSampler, input.Selector);

	// Invert left and right if we are right
	float2 offset_x = { TexelSize.x, 0 };
	if (selector.x >= 0.5) offset_x = -offset_x;

	// Invert above and below if we are bottom
	float2 offset_y = { 0, TexelSize.y };
	if (selector.y >= 0.5) offset_y = -offset_y;

	half4 L = tex2D(SourceSampler,	input.CentreUV-offset_x);
	half4 R = tex2D(SourceSampler,	input.CentreUV+offset_x);
	half4 A = tex2D(SourceSampler,	input.CentreUV-offset_y);
	half4 B = tex2D(SourceSampler,	input.CentreUV+offset_y);

	bool al	= all(A	== L);
	bool ar	= all(A	== R);
	bool bl	= all(B	== L);

	half4 ret = tex2D(SourceSampler, input.CentreUV);
	if (al && !ar && !bl) ret = L;
	return ret;
}



//
// Technique
//

technique Scale2x_New
{
    pass P0
    {
		// shaders
		VertexShader = compile vs_1_1 VS_New();
		PixelShader  = compile ps_2_0 PS_New();
		AlphaBlendEnable = FALSE;
		ColorWriteEnable = RED|GREEN|BLUE|ALPHA;
		SRGBWRITEENABLE = FALSE;
    }  
}
