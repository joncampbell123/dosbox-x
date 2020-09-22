/*
 *  Hq2x scaler pixel shader version by Mitja Gros (Mitja.Gros@gmail.com)
 *
 *  Port of OpenGL-HQ Copyright (C) 2004-2005 Jörg Walter <jwalt@garni.ch>
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

#include "scaling.inc"

string name : NAME = "hq2x";
float scaling : SCALING = 1.00;
float staticTreshold : HQ2XSTATICTRESHOLD = 10.0 / 255.0;
float dynamicTreshold : HQ2XDYNAMICTRESHOLD = 33.0 / 100.0;

// combineTechnique: Final combine steps. Outputs to destination frame buffer
string combineTechique : COMBINETECHNIQUE = "Combine_hq2x";

// sourcetexture -> workingtexture
string preprocessTechique : PREPROCESSTECHNIQUE = "Preprocess0_hq2x";
// workingtexture -> workingtexture1
string preprocessTechique1 : PREPROCESSTECHNIQUE1 = "Preprocess1_hq2x";

texture Hq2xLookupTexture		: HQ2XLOOKUPTEXTURE;

sampler	Hq2xLookupSampler = sampler_state {
	Texture	  = (Hq2xLookupTexture);
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = NONE;
	AddressU  = Clamp;
	AddressV  = Clamp;
	AddressW  = Clamp;
	SRGBTEXTURE = FALSE;
};

struct VS_OUTPUT_PRODUCT0
{
	float4 Position		: POSITION;
	float2 pixel0		: TEXCOORD0;
	float2 pixel1		: TEXCOORD1;
	float2 pixel2		: TEXCOORD2;
	float2 pixel3		: TEXCOORD3;
};

struct VS_OUTPUT_PRODUCT1
{
	float4 Position		: POSITION;
	float2 pixel0		: TEXCOORD0;
	float2 pixel1		: TEXCOORD1;
	float2 pixel2		: TEXCOORD2;
	float2 pixel3		: TEXCOORD3;
};

struct VS_OUTPUT_PRODUCT2
{
	float4 Position		: POSITION;
	float2 pixel0		: TEXCOORD0;
};

// vertex shaders
VS_OUTPUT_PRODUCT0 VS_Product0(
			float3 Position : POSITION,
			float2 TexCoord : TEXCOORD0)
{
	VS_OUTPUT_PRODUCT0 Out = (VS_OUTPUT_PRODUCT0)0;

	const float4 coordmask = { 1, 0, 1, 0 };

	Out.Position = mul(half4(Position, 1), WorldViewProjection);   // Matrix multipliy
	Out.pixel0 = TexCoord;
	Out.pixel1 = TexelSize * coordmask + Out.pixel0;
	Out.pixel2 = TexelSize * coordmask.gbra + Out.pixel0;
	Out.pixel3 = TexelSize + Out.pixel0;

	return Out;
}

VS_OUTPUT_PRODUCT1 VS_Product1(
			float3 Position : POSITION,
			float2 TexCoord : TEXCOORD0)
{
	VS_OUTPUT_PRODUCT1 Out = (VS_OUTPUT_PRODUCT1)0;

	const float4 coordmask = { 0, -1, 0, 0.0625 };

	Out.Position = mul(half4(Position, 1), WorldViewProjection);   // Matrix multipliy
	Out.pixel3 = TexCoord;
	Out.pixel0 = Out.pixel3 - TexelSize;
	Out.pixel1 = TexelSize.x * coordmask + Out.pixel3;
	Out.pixel2 = TexelSize.x * coordmask.gbra + Out.pixel3;	

	return Out;
}

VS_OUTPUT_PRODUCT2 VS_Product2(
			float3 Position : POSITION,
			float2 TexCoord : TEXCOORD0)
{
	VS_OUTPUT_PRODUCT2 Out = (VS_OUTPUT_PRODUCT2)0;

	Out.Position = mul(half4(Position, 1), WorldViewProjection);   // Matrix multipliy

	Out.pixel0 = TexCoord;

	return Out;
}

// pixel shaders
half PS_Product0_Diff(half4 pixel1, half4 pixel2)
{
	const half4 weight = { 1.884313725490196078431372549, 3.768627450980392156862745098, 2.822790287990196078431372548, 0 };
	const half4 mask = { 0.4710784313725490196078431372, 0, -0.4710784313725490196078431372, 0 };

	half4 diff = 0, rmean = 0;

	diff.rgb = pixel1 - pixel2;
	rmean.a = pixel1.r + pixel2.r;
	rmean.rgb = rmean.a * mask + weight;
	diff.rgb = diff * diff;
	return clamp(dot(rmean, diff), 0 , 1);
}

half4 PS_Product0 ( in VS_OUTPUT_PRODUCT0 input ) : COLOR
{
	half4 pixel0 = tex2D(SourceSampler, input.pixel0);
	half4 pixel1 = tex2D(SourceSampler, input.pixel1);
	half4 pixel2 = tex2D(SourceSampler, input.pixel2);
	half4 pixel3 = tex2D(SourceSampler, input.pixel3);

	half4 result = 0;

	result.b = PS_Product0_Diff(pixel0, pixel3);
	result.r = PS_Product0_Diff(pixel0, pixel1);
	result.g = PS_Product0_Diff(pixel0, pixel2);
	result.a = PS_Product0_Diff(pixel1, pixel2);

	return result;
}

half4 PS_Product1 ( in VS_OUTPUT_PRODUCT1 input ) : COLOR
{
	const half4 constant = { 65536, 0.1666666666666666, 1, 0 };	
	const half4 factors = { 0.0627450980392157,  0.125490196078431,  0.250980392156863,   0.501960784313725 };
	const half4 oneSixteenth = { 0, -1, 0, 0.0625 };

	half4 pixel0 = tex2D(WorkingSampler, input.pixel0);
	half4 pixel1 = tex2D(WorkingSampler, input.pixel1);
	half4 pixel2 = tex2D(WorkingSampler, input.pixel2);
	half4 pixel3 = tex2D(WorkingSampler, input.pixel3);

	half4 minimum = 0, currentTrigger = 0, result = 0;

	pixel1.r = pixel0.b;
	pixel2.g = pixel0.a;

	minimum = pixel1 + pixel2 + pixel3;
	currentTrigger.a = dot(minimum, constant.g);
	currentTrigger.a = clamp(currentTrigger.a * dynamicTreshold, 0, 1);
	currentTrigger.a = max(currentTrigger.a, staticTreshold);

	currentTrigger.a = currentTrigger.a * constant.r;
	pixel1 = clamp(pixel1 * constant.r - currentTrigger.a, 0, 1);
	pixel2 = clamp(pixel2 * constant.r - currentTrigger.a, 0, 1);
	pixel3 = clamp(pixel3 * constant.r - currentTrigger.a, 0, 1);

	result.a = dot(pixel3, factors);
	pixel2 = pixel2 * oneSixteenth.a + pixel1;
	result.rgb = dot(pixel2, factors);

	return result;
}

half4 PS_Product2 ( in VS_OUTPUT_PRODUCT2 input ) : COLOR
{
	const half4 cap = { 1.142857142857142857127368540393064222371, -0.07142857142857143002735720305196309709572, 0.83825, 0.080875 };
	const half4 constant = { 0.0625, 0.5, 0.99609375, 0.001953125 };

	half2 coord1 = 0.0f;
	half2 coord2 = 0.0f;
	half2 coord3 = 0.0f;
	half3 centerOffset = 0.0f;

	half4 diff = tex2D(WorkingSampler1, input.pixel0);

	centerOffset.xy = input.pixel0.xy * SourceDims.xy;
	centerOffset.xy = frac(centerOffset.xy);

	coord3.xy = centerOffset.xy - constant.g;
	coord3 = (coord3 < 0) ? -TexelSize : TexelSize;

	coord3.xy = input.pixel0 + coord3;
	coord1.x = coord3.x;
	coord1.y = input.pixel0.y;
	coord2.x = input.pixel0.x;
	coord2.y = coord3.y;

	centerOffset.xy = clamp(centerOffset.xy * cap.r + cap.g, 0, 1);
	centerOffset.xy = centerOffset.xy * cap.b + cap.a;

	centerOffset.x = centerOffset.x * constant.x + diff.a;
	centerOffset.z = diff.z * constant.b + constant.a;

	half4 pixel0 = tex2D(SourceSampler, input.pixel0);
	half4 pixel1 = tex2D(SourceSampler, coord1);
	half4 pixel2 = tex2D(SourceSampler, coord2);
	half4 pixel3 = tex2D(SourceSampler, coord3);
	half4 factors = tex3D(Hq2xLookupSampler, centerOffset);

	pixel0 = pixel0 * factors.b;
	pixel0 = pixel1 * factors.g + pixel0;
	pixel0 = pixel2 * factors.r + pixel0;
	half4 result = pixel3 * factors.a + pixel0;

	return result;
}

technique Preprocess0_hq2x
{
    pass P0
    {
		// shaders
		VertexShader = compile vs_2_0 VS_Product0();
		PixelShader  = compile ps_2_0 PS_Product0();
		AlphaBlendEnable = FALSE;
		ColorWriteEnable = RED|GREEN|BLUE|ALPHA;
		SRGBWRITEENABLE = FALSE;
    }
}

technique Preprocess1_hq2x
{
    pass P0
    {
		// shaders
		VertexShader = compile vs_2_0 VS_Product1();
		PixelShader  = compile ps_2_0 PS_Product1();
		AlphaBlendEnable = FALSE;
		ColorWriteEnable = RED|GREEN|BLUE|ALPHA;
		SRGBWRITEENABLE = FALSE;
    }
}

technique Combine_hq2x
{
    pass P0
    {
		// shaders
		VertexShader = compile vs_2_0 VS_Product2();
		PixelShader  = compile ps_2_0 PS_Product2();
		AlphaBlendEnable = FALSE;
		ColorWriteEnable = RED|GREEN|BLUE|ALPHA;
		SRGBWRITEENABLE = FALSE;
    }
}
