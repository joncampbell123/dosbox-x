/*    
 *  CRT shader
 *
 *  Copyright (C) 2010, 2011 cgwg, Themaister,DOLLS, gulikoza
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  (cgwg gave their consent to have the original version of this shader
 *  distributed under the GPL in this message:
 *
 *      http://board.byuu.org/viewtopic.php?p=26075#p26075
 *
 *      "Feel free to distribute my shaders under the GPL. After all, the
 *      barrel distortion code was taken from the Curvature shader, which is
 *      under the GPL."
 *
 *  Direct3D port by gulikoza at users.sourceforge.net
 *
 */

        #include "Scaling.inc"

        // The name of this effect
        string name : NAME = "CRTFX";
        float scaling : SCALING = 1.0;

        // Comment the next line to disable interpolation in linear gamma (and gain speed).
        #define LINEAR_PROCESSING

        // Compensate for 16-235 level range as per Rec. 601.
        #define REF_LEVELS

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
        #define FIX(c) max(abs(c), 1e-5);
        #define PI 3.141592653589

        #ifdef REF_LEVELS
        #       define LEVELS(c) max((c - 16.0 / 255.0) * 255.0 / (235.0 - 16.0), 0.0)
        #else
        #       define LEVELS(c) c
        #endif

        #ifdef LINEAR_PROCESSING
        #       define TEX2D(c) pow(LEVELS(tex2D(SourceBorderSampler, (c))), inputGamma)
        #else
        #       define TEX2D(c) LEVELS(tex2D(SourceBorderSampler, (c)))
        #endif

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
		float2 pixel1		: TEXCOORD1;
		float2 abspos		: TEXCOORD2;
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

                // The size of one texel, in texture-coordinates.
                Out.pixel1 = TexelSize;

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

                // Calculate Lanczos scaling coefficients describing the effect
                // of various neighbour texels in a scanline on the current
                // pixel.
                float4 coeffs = PI * float4(1.0 + uv_ratio.x, uv_ratio.x, 1.0 - uv_ratio.x, 2.0 - uv_ratio.x);

                // Prevent division by zero.
                coeffs = FIX(coeffs);

                // Lanczos2 kernel.
                coeffs = 2.0 * sin(coeffs) * sin(coeffs / 2.0) / (coeffs * coeffs);

                // Normalize.
                coeffs /= dot(coeffs, 1.0);

                // Calculate the effective colour of the current and next
                // scanlines at the horizontal location of the current pixel,
                // using the Lanczos coefficients above.
                float4 col  = clamp(
                        mul(coeffs, float4x4(
                                TEX2D(xy + float2(-input.pixel1.r, 0.0)),
                                TEX2D(xy),
                                TEX2D(xy + float2(input.pixel1.x, 0.0)),
                                TEX2D(xy + float2(2.0 * input.pixel1.x, 0.0))
                        )), 0.0, 1.0);
                float4 col2 = clamp(
                        mul(coeffs, float4x4(
                               TEX2D(xy + float2(-input.pixel1.x, input.pixel1.y)),
                               TEX2D(xy + float2(0.0, input.pixel1.y)),
                               TEX2D(xy + input.pixel1),
                               TEX2D(xy + float2(2.0 * input.pixel1.x, input.pixel1.y))
                        )), 0.0, 1.0);

        #ifndef LINEAR_PROCESSING
                col  = pow(col , inputGamma);
                col2 = pow(col2, inputGamma);
        #endif

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

                // Convert the image gamma for display on our output device.
                mul_res = pow(mul_res, 1.0 / outputGamma);

                // Color the texel.
                return half4(mul_res, 1.0);
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
