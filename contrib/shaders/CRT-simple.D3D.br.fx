/*
 *  CRT-simple shader
 *
 *  Copyright (C) 2011 DOLLS. Based on cgwg's CRT shader.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  Direct3D port by gulikoza at users.sourceforge.net
 *
 */

        #include "Scaling.inc"

        // The name of this effect
        string name : NAME = "CRTFX";
        float scaling : SCALING = 1.0;

        // Enable screen curvature.
        #define CURVATURE

        // Controls the intensity of the barrel distortion used to emulate the
        // curvature of a CRT. 0.0 is perfectly flat, 1.0 is annoyingly
        // distorted, higher values are increasingly ridiculous.
        #define distortion 0.05

        // Simulate a CRT gamma of 2.4.
        #define inputGamma  2.4

        // Compensate for the standard sRGB gamma of 2.2.
        // Fixed 2.2 to 2.6
        #define outputGamma 2.6

        // Macros.
        #define TEX2D(c) pow(tex2D(SourceBorderSampler, (c)), inputGamma)
        #define PI 3.141592653589

        //
        // Techniques
        //

        // combineTechnique: Final combine steps. Outputs to destination frame buffer
        string combineTechique : COMBINETECHNIQUE = "CRTFX";

        // preprocessTechnique: PreProcessing steps. Outputs to WorkingTexture
        //string preprocessTechique : PREPROCESSTECHNIQUE = "";

        struct VS_OUTPUT_PRODUCT
        {
		float4 Position		: POSITION;
		float2 pixel0		: TEXCOORD0;
		float2 abspos		: TEXCOORD1;
	};

        sampler SourceBorderSampler = sampler_state {
		Texture	  = (SourceTexture);
		MinFilter = POINT;
		MagFilter = POINT;
		MipFilter = NONE;
		AddressU  = Border;
		AddressV  = Border;
		SRGBTEXTURE = FALSE;
        };

        // vertex shader
        VS_OUTPUT_PRODUCT VS_Product(
			float3 Position : POSITION,
			float2 TexCoord : TEXCOORD0)
        {
                VS_OUTPUT_PRODUCT Out = (VS_OUTPUT_PRODUCT)0;

                // Do the standard vertex processing.
                Out.Position = mul(half4(Position, 1), WorldViewProjection);

                // Precalculate a bunch of useful values we'll need in the fragment
                // shader.

                // Texture coords.
                Out.pixel0 = TexCoord;

                // Resulting X pixel-coordinate of the pixel we're drawing.
                // Assumes (-0.5, 0.5) quad and output size in World matrix
                // as currently done in DOSBox D3D patch
                Out.abspos = float2((Position.x + 0.5) * World._11, (Position.y - 0.5) * (-World._22));

                return Out;
        }

        // Apply radial distortion to the given coordinate.
        float2 radialDistortion(float2 coord, float2 pos)
        {
                pos /= float2(World._11, World._22);
                float2 cc = pos - 0.5;
                float dist = dot(cc, cc) * distortion;
                return coord * (pos + cc * (1.0 + dist) * dist) / pos;
        }

        // Calculate the influence of a scanline on the current pixel.
        //
        // 'distance' is the distance in texture coordinates from the current
        // pixel to the scanline in question.
        // 'color' is the colour of the scanline at the horizontal location of
        // the current pixel.
        float4 scanlineWeights(float distance, float4 color)
        {
                // The "width" of the scanline beam is set as 2*(1 + x^4) for
                // each RGB channel.
                float4 wid = 2.0 + 2.0 * pow(color, 4.0);

                // The "weights" lines basically specify the formula that gives
                // you the profile of the beam, i.e. the intensity as
                // a function of distance from the vertical center of the
                // scanline. In this case, it is gaussian if width=2, and
                // becomes nongaussian for larger widths. Ideally this should
                // be normalized so that the integral across the beam is
                // independent of its width. That is, for a narrower beam
                // "weights" should have a higher peak at the center of the
                // scanline than for a wider beam.
                // Fixed 0.3 to 0.4
                float4 weights = distance / 0.4;
                return 1.4 * exp(-pow(weights * rsqrt(0.5 * wid), wid)) / (0.6 + 0.2 * wid);
        }

        half4 PS_Product ( in VS_OUTPUT_PRODUCT input ) : COLOR
        {
                // Here's a helpful diagram to keep in mind while trying to
                // understand the code:
                //
                //  |      |      |      |      |
                // -------------------------------
                //  |      |      |      |      |
                //  |  01  |  11  |  21  |  31  | <-- current scanline
                //  |      | @    |      |      |
                // -------------------------------
                //  |      |      |      |      |
                //  |  02  |  12  |  22  |  32  | <-- next scanline
                //  |      |      |      |      |
                // -------------------------------
                //  |      |      |      |      |
                //
                // Each character-cell represents a pixel on the output
                // surface, "@" represents the current pixel (always somewhere
                // in the bottom half of the current scan-line, or the top-half
                // of the next scanline). The grid of lines represents the
                // edges of the texels of the underlying texture.

                // Texture coordinates of the texel containing the active pixel.
        #ifdef CURVATURE
                float2 xy = radialDistortion(input.pixel0, input.abspos);
        #else
                float2 xy = input.pixel0;
        #endif

                // Of all the pixels that are mapped onto the texel we are
                // currently rendering, which pixel are we currently rendering?
                float2 ratio_scale = xy * SourceDims - 0.5;
                float2 uv_ratio = frac(ratio_scale);

                // Snap to the center of the underlying texel.
                xy = (floor(ratio_scale) + 0.5) / SourceDims;

                // Calculate the effective colour of the current and next
                // scanlines at the horizontal location of the current pixel.
                float4 col  = TEX2D(xy);
                float4 col2 = TEX2D(xy + float2(0.0, TexelSize.y));

                // Calculate the influence of the current and next scanlines on
                // the current pixel.
                float4 weights  = scanlineWeights(uv_ratio.y, col);
                float4 weights2 = scanlineWeights(1.0 - uv_ratio.y, col2);
                float3 mul_res  = (col * weights + col2 * weights2).rgb;

                // dot-mask emulation:
                // Output pixels are alternately tinted green and magenta.
                float3 dotMaskWeights = lerp(
                        float3(1.0, 0.7, 1.0),
                        float3(0.7, 1.0, 0.7),
                        floor(input.abspos.x % 2.0)
                    );

                mul_res *= dotMaskWeights;

                return half4(pow(mul_res, 1.0 / outputGamma), 1.0);
        }


technique CRTFX
{
	pass P0
	{
		// shaders
		VertexShader = compile vs_3_0 VS_Product();
		PixelShader  = compile ps_3_0 PS_Product();
		AlphaBlendEnable = FALSE;
		ColorWriteEnable = RED|GREEN|BLUE|ALPHA;
		SRGBWRITEENABLE = FALSE;
	}
}
