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
// ** 2xSaI **
//
// 2xSaI algorithm is Copyright (c) 1999-2001 by Derek Liauw Kie Fa. 
//
// 2xSaI is free under GPL
//
// Adapted by Ryan A. Nunn into Pixel and Vertex Shaders to be used with the 
// Scaling2D entry in the Beyond3D/ATI Shader Competition
//
// Download original C/ASM code here: http://elektron.its.tudelft.nl/~dalikifa/
//
// Notes about the creation of the shaders
//
// The differences from the C/ASM code are quite substantial, which would be 
// plainly obvious to anyone who has worked with the C/ASM versions of these 
// algorithms.
//
// The reasons for this are as follows:
// 1) Can only output a single pixel at a time (Note MRT wouldn't be useful)
// 2) 'Small' instruction count limits
// 3) No program flow control
//
// As such, the algorithm couldn't be simply converted into pixel shaders 
// because multiple nested if statements were used. While this may be fairly
// fast on a CPU, it is very unsuitable for a pixel shader. So I decided to do
// things differently. I decided that I should instead write boolean 
// expressions for each of the possible values for each product, and convert 
// those into pixel shaders with a final combining stage
//
// An example of one of the expressions is:
//   useColA = (AD && BC && BD) || (((AC && AF && BJ && !BE) || (AD && AE && BL)) && !BC);
//
// That expression represents which colours much and must not be the same in 
// order for product0 to be colorA
//
// After doing all that, I had managed to convert the algorithm from working 
// out all the products at the same time, to working out each product 
// individually. The algorithm would now be suitable for conversion into pixel 
// shaders.
//

#include "Scaling.inc"

// The name of this effect
string name : NAME = "2xSaI (Gamma Corrected)";
float scaling : SCALING = 2.0;

//
// Techniques
//

// combineTechnique: Final combine steps. Outputs to destination frame buffer
string combineTechique : COMBINETECHNIQUE =  "Combine_2xSaI";

// preprocessTechnique: PreProcessing steps. Outputs to WorkingTexture
string preprocessTechique : PREPROCESSTECHNIQUE = "Preprocess_2xSaI";


// 
// Textures and Samplers
//


//
// Texture Gen Shaders
//


//
// Vertex Shader Output
//

// Texel Locations
//
//     -1  0  1  2
//  -1  I  E  F  J
//  0   G  A  B  K
//  1   H  C  D  L
//  2   M  N  O  P

struct VS_OUTPUT_PRODUCT0
{
	float4 Position		: POSITION;
	float2 colorA		: TEXCOORD0;
	float2 colorB		: TEXCOORD1;
	float2 colorC		: TEXCOORD2;
	float2 colorD		: TEXCOORD3;
	float2 colorE		: TEXCOORD4;
	float2 colorF		: TEXCOORD5;
	float2 colorH		: TEXCOORD6;
	float2 colorJ		: TEXCOORD7;
};

struct VS_OUTPUT_PRODUCT1
{
	float4 Position		: POSITION;
	float2 colorA		: TEXCOORD0;
	float2 colorC		: TEXCOORD1;
	float2 colorB		: TEXCOORD2;
	float2 colorD		: TEXCOORD3;
	float2 colorG		: TEXCOORD4;
	float2 colorH		: TEXCOORD5;
	float2 colorF		: TEXCOORD6;
	float2 colorM		: TEXCOORD7;
};

struct VS_OUTPUT_PRODUCT2_PASS0
{
	float4 Position		: POSITION;
	float2 colorA		: TEXCOORD0;
	float2 colorB		: TEXCOORD1;
	float2 colorC		: TEXCOORD2;
	float2 colorD		: TEXCOORD3;
};

struct VS_OUTPUT_PRODUCT2_PASS1
{
	float4 Position		: POSITION;
	float2 colorA		: TEXCOORD0;
	float2 colorB		: TEXCOORD1;
	float2 colorG		: TEXCOORD2;
	float2 colorE		: TEXCOORD3;
	float2 colorK		: TEXCOORD4;
	float2 colorF		: TEXCOORD5;
};

struct VS_OUTPUT_PRODUCT2_PASS2
{
	float4 Position		: POSITION;
	float2 colorA		: TEXCOORD0;
	float2 colorB		: TEXCOORD1;
	float2 colorL		: TEXCOORD2;
	float2 colorO		: TEXCOORD3;
	float2 colorH		: TEXCOORD4;
	float2 colorN		: TEXCOORD5;
};

struct VS_OUTPUT_COMBINE
{
	float4 Position		: POSITION;
	float2 colorA		: TEXCOORD0;
	float2 colorB		: TEXCOORD1;
	float2 colorC		: TEXCOORD2;
	float2 colorD		: TEXCOORD3;
	float2 Selector		: TEXCOORD4;
};

//
// Vertex Shaders
//

VS_OUTPUT_PRODUCT0 VS_Product0(
			float3 Position : POSITION,
			float2 TexCoord : TEXCOORD0)
{
	VS_OUTPUT_PRODUCT0 Out = (VS_OUTPUT_PRODUCT0)0;

	Out.Position = mul(float4(Position, 1), WorldViewProjection);   // Matrix multipliy
	Out.colorA = TexCoord;
	Out.colorB = TexCoord + TexelSize * float2( 1, 0);
	Out.colorC = TexCoord + TexelSize * float2( 0, 1);
	Out.colorD = TexCoord + TexelSize * float2( 1, 1);
	Out.colorF = TexCoord + TexelSize * float2( 1,-1);
	Out.colorH = TexCoord + TexelSize * float2(-1, 1);

	Out.colorE = TexCoord + TexelSize * float2( 0,-1);
	Out.colorJ = TexCoord + TexelSize * float2( 2,-1);

	return Out;
}

VS_OUTPUT_PRODUCT1 VS_Product1(
			float3 Position : POSITION,
			float2 TexCoord : TEXCOORD0)
{
	VS_OUTPUT_PRODUCT1 Out = (VS_OUTPUT_PRODUCT1)0;

	Out.Position = mul(float4(Position, 1), WorldViewProjection);   // Matrix multipliy
	Out.colorA = TexCoord;
	Out.colorB = TexCoord + TexelSize * float2( 1, 0);
	Out.colorC = TexCoord + TexelSize * float2( 0, 1);
	Out.colorD = TexCoord + TexelSize * float2( 1, 1);
	Out.colorF = TexCoord + TexelSize * float2( 1,-1);
	Out.colorH = TexCoord + TexelSize * float2(-1, 1);

	Out.colorG = TexCoord + TexelSize * float2(-1, 0);
	Out.colorM = TexCoord + TexelSize * float2(-1, 2);

	return Out;
}

VS_OUTPUT_PRODUCT2_PASS0 VS_Product2_Pass0(
			float3 Position : POSITION,
			float2 TexCoord : TEXCOORD0)
{
	VS_OUTPUT_PRODUCT2_PASS0 Out = (VS_OUTPUT_PRODUCT2_PASS0)0;

	Out.Position = mul(float4(Position, 1), WorldViewProjection);   // Matrix multipliy
	Out.colorA = TexCoord;
	Out.colorB = TexCoord + TexelSize * float2( 1, 0);
	Out.colorC = TexCoord + TexelSize * float2( 0, 1);
	Out.colorD = TexCoord + TexelSize * float2( 1, 1);

	return Out;
}

VS_OUTPUT_PRODUCT2_PASS1 VS_Product2_Pass1 (
			float3 Position : POSITION,
			float2 TexCoord : TEXCOORD0)
{
	VS_OUTPUT_PRODUCT2_PASS1 Out = (VS_OUTPUT_PRODUCT2_PASS1)0;

	Out.Position = mul(float4(Position, 1), WorldViewProjection);   // Matrix multipliy
	Out.colorA = TexCoord;
	Out.colorB = TexCoord + TexelSize * float2( 1, 0);
	Out.colorG = TexCoord + TexelSize * float2(-1, 0);
	Out.colorE = TexCoord + TexelSize * float2( 0,-1);
	Out.colorK = TexCoord + TexelSize * float2( 2, 0);
	Out.colorF = TexCoord + TexelSize * float2( 1,-1);

	return Out;
}

VS_OUTPUT_PRODUCT2_PASS2 VS_Product2_Pass2 (
			float3 Position : POSITION,
			float2 TexCoord : TEXCOORD0)
{
	VS_OUTPUT_PRODUCT2_PASS2 Out = (VS_OUTPUT_PRODUCT2_PASS2)0;

	Out.Position = mul(float4(Position, 1), WorldViewProjection);   // Matrix multipliy
	Out.colorA = TexCoord;
	Out.colorB = TexCoord + TexelSize * float2( 1, 0);
	Out.colorL = TexCoord + TexelSize * float2( 2, 1);
	Out.colorO = TexCoord + TexelSize * float2( 1, 2);
	Out.colorH = TexCoord + TexelSize * float2(-1, 1);
	Out.colorN = TexCoord + TexelSize * float2( 0, 2);

	return Out;
}

VS_OUTPUT_COMBINE VS_Combine(
			float3 Position : POSITION,
			float2 TexCoord : TEXCOORD0)
{
	VS_OUTPUT_COMBINE Out = (VS_OUTPUT_COMBINE)0;

	Out.Position = mul(float4(Position, 1), WorldViewProjection);   // Matrix multipliy
	Out.colorA = TexCoord;
	Out.colorB = TexCoord + TexelSize * float2( 1, 0);
	Out.colorC = TexCoord + TexelSize * float2( 0, 1);
	Out.colorD = TexCoord + TexelSize * float2( 1, 1);
	Out.Selector = TexCoord*SourceDims;

	return Out;
}


//
// Pixel Shaders
//

//
// This pixel shader is used for the product0 and product1 preprocesses steps.
// By purely changing the vertex shader, does the effect of this pixel shader change.
// What happens is the texture coords become mirrored across the xy diagonal.
//
// This has been hand coded to reduce instruction counts so this can run in 1 pass, per 
// product. This code is likely to run very poorly on NV3x hardware.
//
PIXELSHADER PS_Product0_1 = asm 
{
	ps_2_0

	// Some opcode 'aliases' for bools
	#define	or		add_sat
	#define	and		mul
	#define	and_or		mad_sat
	
	// Same thing, but partial precision (using mad_sat_pp is causing 'issues')
	#define or_pp		add_sat_pp
	#define and_pp		mul_pp
	#define and_or_pp	mad_sat

	//
	// Constants
	//
	
	def		c0,	0,	1,	0.35,	0.7
	#define		zero	c0.r
	#define		one	c0.g
	#define		ret_B	c0.b
	#define		ret_A	c0.a

	//def		c2,	TexelSize	// Set by the effect
	#define		TexelSize	c2
	
	

	//
	// Samplers
	//
	
	// The only sampler we need. It is set by the effect
	dcl_2d		s0					// SourceSampler
	
	
	//
	// Texture coords. These match VS_OUTPUT_PRODUCT0
	//
	
	dcl		t0.xy					// input.colorA (-TexelSize -> colorI)
	dcl		t1.xy					// input.colorB (+TexelSize -> colorL)
	dcl		t2.xy					// input.colorC
	dcl		t3.xy					// input.colorD
	dcl		t4.xy					// input.colorE
	dcl		t5.xy					// input.colorF
	dcl		t6.xy					// input.colorH
	dcl		t7.xy					// input.colorJ


	//
	// The code
	//


	// Sample the first 6 textures (ABCDEF)
	texld_pp	r5,	t0,	s0			// colorA
	texld_pp	r6,	t1,	s0			// colorB
	texld_pp	r7,	t2,	s0			// colorC
	texld_pp	r8,	t3,	s0			// colorD
	texld_pp	r9,	t4,	s0			// colorE
	texld_pp	r10,	t5,	s0			// colorF

	//
	// Note that when we do the initial comparisons, we actually check to see 
	// if any texels are different, rather than all same. This allows for an 
	// optimization, since it requires less instructions (3 less per 4 comparisons)
	//
	// The sub finds the differences
	// the dp3 will make the diffs all positive, and then combine them into a 
	// single component of the register.
	//


	//
	// R2 Register. Contains A==C, B==C, A==D, B==D
	// 
	#define AC r2.a
	#define BC r2.r
	#define AD r2.g
	#define BD r2.b
	
	// Calc A!=C
	sub_pp		r0,	r5,	r7						// 0
	dp4_pp		AC,	r0,	r0						// 1

	// Calc B!=C
	sub_pp		r0,	r6,	r7						// 2
	dp4_pp		BC,	r0,	r0						// 3

	// Calc A!=D
	sub_pp		r0,	r5,	r8						// 4
	dp4_pp		AD,	r0,	r0						// 5

	// Calc B!=D
	sub_pp		r0,	r6,	r8						// 6
	dp4_pp		BD,	r0,	r0						// 7

	// Invert register: r2 = !r2
	cmp_pp		r2,	-r2,	one,	zero					// 8
	

	//
	// R3 Register. Contains A==E, B==E, A==F, B==F
	// 
	#define AE r3.a
	#define BE r3.r
	#define AF r3.g
	#define BF r3.b
	
	// Calc A!=E
	sub_pp		r0,	r5,	r9						// 9
	dp4_pp		AE,	r0,	r0						// 10

	// Calc B!=E
	sub_pp		r0,	r6,	r9						// 11
	dp4_pp		BE,	r0,	r0						// 12

	// Calc A!=F
	sub_pp		r0,	r5,	r10						// 13
	dp4_pp		AF,	r0,	r0						// 14

	// Calc B!=F
	sub_pp		r0,	r6,	r10						// 15
	dp4_pp		BF,	r0,	r0						// 16

	// Invert register: r3 = !r3
	cmp_pp		r3,	-r3,	one,	zero					// 17



	// Sample final 4 textures (colorH, colorJ, colorI, and colorL)
	sub		r0.xy,	t0,	TexelSize					// 18
	add		r1.xy,	t1,	TexelSize					// 19
	texld_pp	r7,	t6,	s0			// colorH
	texld_pp	r8,	t7,	s0			// colorJ
	texld_pp	r9,	r0,	s0			// colorI -> input.colorA-texelSize
	texld_pp	r10,	r1,	s0			// colorL -> input.colorB+texelSize


	//
	// R4 Register. Contains A==H, B==J, A==I, B==L
	// 

	#define AH r4.a
	#define BJ r4.r
	#define AI r4.g
	#define BL r4.b
	
	// Calc A!=H
	sub_pp		r0,	r5,	r7						// 20
	dp4_pp		AH,	r0,	r0						// 21

	// Calc B!=J
	sub_pp		r0,	r6,	r8						// 22
	dp4_pp		BJ,	r0,	r0						// 23

	// Calc A!=I
	sub_pp		r0,	r5,	r9						// 24
	dp4_pp		AI,	r0,	r0						// 25

	// Calc B!=L
	sub_pp		r0,	r6,	r10						// 26
	dp4_pp		BL,	r0,	r0						// 27

	// Invert register: r4 = !r4
	cmp_pp		r4,	-r4,	one,	zero					// 28
	

	//
	// useColB = r5.rgb = 
	//                         ((BE && BD && AI && !AF) || (BC && BF && AH)) && !AD
	//
	// useColA = r5.a = 
	//    (AD && BC && BD) || (((AC && AF && BJ && !BE) || (AD && AE && BL)) && !BC)
	//
	// As can easily be seen, mostly the same operations need to be done for
	// calculating useColA and useColB, so the following code 'might' co-issue.
	// So, I'll be nice, and keep useColB in the vector pipe, and usecolA in the
	// scaler pipe. It would only get rid of 5 instructions, but that is a lot if
	// this is repeating hundreds of thousands of times
	//

	// B:                                                   BC && BF
	// A:                                                   AD && AE
	and_pp		r6.rgb,	BC,	BF						// 29
	and_pp		r6.a,	AD,	AE						// 30

	// B:                                                  (BC && BF && AH)
	// A:                                                  (AD && AE && BL)
	and_pp		r6.rgb,	r6,	AH						// 31
	and_pp		r6.a,	r6,	BL						// 32

	// B:                        BE && BD
	// A:                        AC && AF
	and_pp		r5.rgb,	BE,	BD						// 33
	and_pp		r5.a,	AC,	AF						// 34

	// B:                                    AI && !AF 
	// A:                                    BJ && !BE
	cmp_pp		r7.rgb,	-AF,	AI,	zero					// 35
	cmp_pp		r7.a,	-BE,	BJ,	zero					// 36

	// B:                      ((BE && BD && AI && !AF) || (BC && BF && AH))
	// A:                      ((AC && AF && BJ && !BE) || (AD && AE && BL))
	and_or_pp	r5	,r5,	r7,	r6					// 37

	// B:                      ((BE && BD && AI && !AF) || (BC && BF && AH)) && !AD
	// A:                     (((AC && AF && BJ && !BE) || (AD && AE && BL)) && !BC)
	cmp_pp		r5.rgb,	-AD,	r5,	zero					// 38
	cmp_pp		r5.a,	-BC,	r5,	zero					// 39

	// A:  AD && BC
	and_pp		r6.a,	AD,	BC						// 40

	// A: (AD && BC && BD) || (((AC && AF && BJ && !BE) || (AD && AE && BL)) && !BC)
	and_or_pp	r5.a,	r6.a,	BD,	r5.a					// 41

	//
	// We return ret_B, if only useColB is set.
	// We return ret_A, if useColA is set.
	// We return zero, if neither is set.
	//

	mul_pp		r5.rgb,	r5,	ret_B						// 42
	cmp_pp		r5,	-r5.a,	r5.b,	ret_A					// 43
	mov_pp		oC0,	r5							// 44

#undef AC
#undef BC
#undef AD
#undef BD
#undef AE
#undef BE
#undef AF
#undef BF
#undef AH
#undef BJ
#undef AI
#undef BL
#undef one
#undef zero
#undef ret_A
#undef ret_B
#undef TexelSize
#undef or
#undef and
#undef and_or
#undef or_pp
#undef and_pp
#undef and_or_pp
};


//
// Calcuating product2 is quite complex. This function does basic calculations
// to see if r is required, and if not, then what should be done instead
//
half4 PS_Product2_Pass0 ( in VS_OUTPUT_PRODUCT2_PASS0 input ) : COLOR
{
	half4 colorA = tex2D(SourceSampler, input.colorA);
	half4 colorB = tex2D(SourceSampler, input.colorB);
	half4 colorC = tex2D(SourceSampler, input.colorC);
	half4 colorD = tex2D(SourceSampler, input.colorD);

	bool AD = all(colorA == colorD);
	bool BC = all(colorB == colorC);
	bool AB = all(colorA == colorB);

	half4 ret = 0;					// Q
	if (AD && !BC) ret += 1.0;			// A
	if (BC && !AD) ret += 0.3;			// B
	if (AD && BC)
	{
		ret = 0.6;				// R
		if (AB) ret = 1.0;			// A
	}

	return ret;
}


//
// Modified GetResult code (see normal 2xSaI code for original).
//
// Difference is so we end up outputting a number in the range 0 to 1
// instead of -4 to +4 like normal 2xSaI code calculates. Each GetResult
// function outputs a number from 0 to 0.25. These are all added together
//
inline half GetResult1(half4 A, half4 B, half4 C, half4 D)
{
	half x = 0; 
	half y = 0;
	half r = 0.125;

	bool AC = all(A==C);
	bool AD = all(A==D);
	bool BC = all(B==C);
	bool BD = all(B==D);

	x  = AC;
	x += AD;
	y  = BC && !AC;
	y += BD && !AD;

	if (x < 1.5) r+=0.125; 
	if (y < 1.5) r-=0.125;
	return r;
}

//
// Same as above, except returns -r
//
inline half GetResult2(half4 A, half4 B, half4 C, half4 D)
{
	half x = 0; 
	half y = 0;
	half r = 0.125;

	bool AC = all(A==C);
	bool AD = all(A==D);
	bool BC = all(B==C);
	bool BD = all(B==D);

	x  = AC;
	x += AD;
	y  = BC && !AC;
	y += BD && !AD;

	if (x < 1.5) r-=0.125; 
	if (y < 1.5) r+=0.125;
	return r;
}

//
// We calculate 'r' in 2 passes. This is the first pass.
//
// Both passes output a number from 0 to 0.5. Additive blending is used
// for the second pass, creating a number in the 0 to 1 range
//
half4 PS_Product2_Pass1 ( in VS_OUTPUT_PRODUCT2_PASS1 input ) : COLOR
{
	half4 colorA = tex2D(SourceSampler, input.colorA);
	half4 colorB = tex2D(SourceSampler, input.colorB);
	half4 colorG = tex2D(SourceSampler, input.colorG);
	half4 colorE = tex2D(SourceSampler, input.colorE);
	half4 colorK = tex2D(SourceSampler, input.colorK);
	half4 colorF = tex2D(SourceSampler, input.colorF);

	half r = 0;
	r += GetResult1 (colorA, colorB, colorG, colorE);
	r += GetResult2 (colorB, colorA, colorK, colorF);

	return r;
}

//
// and this is the second pass
//
half4 PS_Product2_Pass2 ( in VS_OUTPUT_PRODUCT2_PASS2 input ) : COLOR
{
	half4 colorA = tex2D(SourceSampler, input.colorA);
	half4 colorB = tex2D(SourceSampler, input.colorB);
	half4 colorL = tex2D(SourceSampler, input.colorL);
	half4 colorO = tex2D(SourceSampler, input.colorO);
	half4 colorH = tex2D(SourceSampler, input.colorH);
	half4 colorN = tex2D(SourceSampler, input.colorN);

	half r = 0;
	r += GetResult2 (colorB, colorA, colorH, colorN);
	r += GetResult1 (colorA, colorB, colorL, colorO);

	return r;
}

//
// Final combine stage. Outputs to framebuffer.
//
// Uses preprocess information, and information about what pixel to output
// to to various interpolations of the colours where required.
//
half4 PS_Combine ( in VS_OUTPUT_COMBINE input ) : COLOR
{
	half4 selector = tex2D(ModuloSampler, input.Selector);
	half4 working = tex2D(WorkingSampler, input.colorA);
	half4 colorA = tex2D(SRGBSourceSampler, input.colorA);
	half4 colorB = tex2D(SRGBSourceSampler, input.colorB);
	half4 colorC = tex2D(SRGBSourceSampler, input.colorC);
	half4 colorD = tex2D(SRGBSourceSampler, input.colorD);
	half4 ABCD = (colorA+colorB+colorC+colorD)/4;

	half4 product0 = colorA;
	if (working.r < 0.6) product0 = colorB; 
	if (working.r < 0.3) product0 = (colorA+colorB)/2;

	half4 product1 = colorA;
	if (working.g < 0.6) product1 = colorC;
	if (working.g < 0.3) product1 = (colorA+colorC)/2;

	bool AD = all(colorA == colorD);
	bool BC = all(colorB == colorC);
	bool AB = all(colorA == colorB);

	half4 product2 = ABCD;
	if (working.a > 0.55) product2 = colorA;
	if (working.a < 0.45) product2 = colorB;
	if (working.b < 0.5) product2 = colorB;
	if (working.b < 0.25) product2 = ABCD;
	if (working.b > 0.75) product2 = colorA;

	half4 ret = colorA;
	if (selector.x >= 0.5) ret = product0;
	if (selector.y >= 0.5) ret = product1;
	if (selector.x >= 0.5 && selector.y >= 0.5) ret = product2;

	return ret;
}

technique Preprocess_2xSaI
{
    pass prod0
    {
		// shaders
		VertexShader = compile vs_1_1 VS_Product0();
		PixelShader  = (PS_Product0_1);

		Sampler[0] = (SourceSampler);
		PixelShaderConstant[2] = (TexelSize);
		AlphaBlendEnable = FALSE;
		ColorWriteEnable = RED;
		SRGBWRITEENABLE = FALSE;
    }  
    pass prod1
    {
		// shaders
		VertexShader = compile vs_1_1 VS_Product1();
		PixelShader  = (PS_Product0_1);

		Sampler[0] = (SourceSampler);
		PixelShaderConstant[2] = (TexelSize);
		AlphaBlendEnable = FALSE;
		ColorWriteEnable = GREEN;
		SRGBWRITEENABLE = FALSE;
    } 
    pass prod2_pass0
    {
		// shaders
		VertexShader = compile vs_1_1 VS_Product2_Pass0();
		PixelShader  = compile ps_2_0 PS_Product2_Pass0();
		ColorWriteEnable = BLUE;
		AlphaBlendEnable = FALSE;
		SRGBWRITEENABLE = FALSE;
    }  
    pass prod2_pass1
    {
		// shaders
		VertexShader = compile vs_1_1 VS_Product2_Pass1();
		PixelShader  = compile ps_2_0 PS_Product2_Pass1();
		ColorWriteEnable = ALPHA;
		AlphaBlendEnable = FALSE;
		SRGBWRITEENABLE = FALSE;
    }  
    pass prod2_pass2
    {
		// shaders
		VertexShader = compile vs_1_1 VS_Product2_Pass2();
		PixelShader  = compile ps_2_0 PS_Product2_Pass2();
		ColorWriteEnable = ALPHA;
		AlphaBlendEnable = TRUE;
		SrcBlend = ONE;
		DestBlend = ONE;
		SRGBWRITEENABLE = FALSE;
    }  
}

technique Combine_2xSaI
{
    pass P0
    {
		// shaders
		VertexShader = compile vs_1_1 VS_Combine();
		PixelShader  = compile ps_2_0 PS_Combine();
		AlphaBlendEnable = FALSE;
		ColorWriteEnable = RED|GREEN|BLUE|ALPHA;
		SRGBWRITEENABLE = TRUE;
    }  
}
