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
// 2D Scale Effect File
//
// Point Sampling
//

#include "Scaling.inc"

// The name of this effect
string name : NAME = "Point Sampling";
float scaling : SCALING = 2.0;

//
// Techniques
//

// combineTechnique: Final combine steps. Outputs to destination frame buffer
string combineTechique : COMBINETECHNIQUE = "Point";

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

struct VS_OUTPUT
{
	float4 Position		: POSITION;
	float2 UV			: TEXCOORD0;
};


//
// Vertex Shader
//

VS_OUTPUT VS(
			float3 Position : POSITION,
			float2 TexCoord : TEXCOORD0)
{
	VS_OUTPUT Out =	(VS_OUTPUT)0;

	Out.Position = mul(float4(Position, 1), WorldViewProjection);   // Matrix multipliy
	Out.UV = TexCoord;

	return Out;
}


//
// Pixel Shader
//

float4 PS (
	in VS_OUTPUT input
) : COLOR
{
	return tex2D(SourceSampler, input.UV);
}


//
// Technique
//

technique Point
{
    pass P0
    {
        // shaders
        VertexShader = compile vs_1_1 VS();
        PixelShader  = compile ps_2_0 PS();
	AlphaBlendEnable = FALSE;
	ColorWriteEnable = RED|GREEN|BLUE|ALPHA;
	SRGBWRITEENABLE = FALSE;
    }  
}
