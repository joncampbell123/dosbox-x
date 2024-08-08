/*
   Author: RibShark, rsn8887, (based on TheMaister)
   License: Public domain
   
   Version 6
   
   This is an integer prescale filter that achieves a smooth scaling result with minimum blur. 
   This is good for pixelgraphics that are scaled by non-integer factors.
   
   Changelog:
   Version 6: Adapted for DOSBox-X
   Version 5: Fix broken prescale factor calculation (again)
   Version 4: Fix completely broken prescale factor calculation 
   Version 3: fix small shift and missing pixels
   Version 2: Automatic prescale factor	
   Version 1: First Release, manual prescale factor
*/

matrix World				: WORLD;
matrix View				: VIEW;
matrix Projection			: PROJECTION;
matrix Worldview			: WORLDVIEW;			// world * view
matrix ViewProjection			: VIEWPROJECTION;		// view * projection
matrix WorldViewProjection		: WORLDVIEWPROJECTION;		// world * view * projection

float2 sourceSize : SOURCEDIMS;
float2 inputSize : INPUTDIMS;
float2 outputSize : OUTPUTDIMS;

texture sourceTex 	: SOURCETEXTURE;

sampler	sourceSampler = sampler_state
{
	Texture   = (sourceTex);
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	AddressU  = Clamp;
	AddressV  = Clamp;
};

string combineTechnique : COMBINETECHNIQUE =  "Sharp_Bilinear_Simple";

struct VS_OUTPUT
{
	float4 oPosition 	: POSITION;
	float2 oTexCoord 	: TEXCOORD0;
	float2 texelScaled : TEXCOORD1;
	float2 prescaleFactor : TEXCOORD2;
};

// Vertex Shader
VS_OUTPUT vShader (float3 pos : POSITION, float2 texCoord : TEXCOORD0)
{
	VS_OUTPUT OUT = (VS_OUTPUT)0;

	OUT.oPosition = mul(float4(pos,1.0f),WorldViewProjection);
	OUT.oTexCoord = texCoord;
	
	//This fixes the missing pixels on the top of the screen in WinUAE
	OUT.texelScaled = (texCoord*sourceSize)-0.5f;
	
	//find optimum integer prescale and pass it to the pixelShader
	OUT.prescaleFactor = floor(outputSize / inputSize);	

	return OUT;
}

// Pixel Shader
float4 pShader ( in VS_OUTPUT inp ) : COLOR
{  
	//inp.texelScaled has already been multiplied by texture_size inside vertex shader
	float2 texelFloored = floor(inp.texelScaled);
	float2 s = frac(inp.texelScaled);

	float2 regionRange = 0.5f - 0.5f / inp.prescaleFactor;

	// Figure out where in the texel to sample to get correct pre-scaled bilinear.
	// Uses the hardware bilinear interpolator to avoid having to sample 4 times manually. 
	float2 centerDist = s - 0.5f;
	float2 f = (centerDist - clamp(centerDist, -regionRange, regionRange)) * inp.prescaleFactor + 0.5f;
	float2 modTexel = texelFloored + f;
   
	return float4(tex2D(sourceSampler, modTexel/sourceSize).rgb, 1.0f);   
}

technique Sharp_Bilinear_Simple
{
   pass P0
   {
     VertexShader = compile vs_1_0 vShader();
     PixelShader  = compile ps_2_0 pShader();
   }  
}