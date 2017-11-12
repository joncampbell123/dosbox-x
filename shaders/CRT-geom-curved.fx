/*
 *  CRT shader
 *
 *  Copyright (C) 2010-2012 cgwg, Themaister, DOLLS, gulikoza
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  Direct3D port by gulikoza at users.sourceforge.net
 *   Source: crt-geom-interlaced-curved.shader (2012/02/06)
 *
 */

  #include "Scaling.inc"

  // The name of this effect
  string name : NAME = "CRTFX";
  float scaling : SCALING = 1.0;

  float2 InputDims : INPUTDIMS = 256.0F;

  // Comment the next line to disable interpolation in linear gamma (and gain speed).
  #define LINEAR_PROCESSING

  // Enable screen curvature.
  #define CURVATURE

  // Enable 3x oversampling of the beam profile
  #define OVERSAMPLE

  // Use the older, purely gaussian beam profile
  //#define USEGAUSSIAN

  // START of parameters

  // gamma of simulated CRT
  float CRTgamma = 2.4;

  // gamma of display monitor (typically 2.2 is correct)
  float monitorgamma = 2.2;

  // overscan (e.g. 1.02 for 2% overscan)
  float2 overscan = { 0.99, 0.99 };

  // aspect ratio
  float2 aspect = { 1.0, 0.75 };

  // lengths are measured in units of (approximately) the width of the monitor
  // simulated distance from viewer to monitor
  float d = 2.0;

  // radius of curvature
  float R = 2.0;

  // tilt angle in radians
  // (behavior might be a bit wrong if both components are nonzero)
  float2 angle = { 0.0, -0.0 };

  // size of curved corners
  float cornersize = 0.03;

  // border smoothness parameter
  // decrease if borders are too aliased
  float cornersmooth = 80.0;

  // END of parameters

  // Macros.
  #define FIX(c) max(abs(c), 1e-5);
  #define PI 3.141592653589

  #ifdef LINEAR_PROCESSING
  #       define TEX2D(c) pow(tex2D(SourceBorderSampler, (c)), CRTgamma)
  #else
  #       define TEX2D(c) tex2D(SourceBorderSampler, (c))
  #endif

  static float2 sinangle = sin(angle);
  static float2 cosangle = cos(angle);

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
	float3 stretch		: TEXCOORD1;
	float4 abspos		: TEXCOORD2;	// original size xy, scaled size zw
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


  // Helper functions
  float intersect(float2 xy)
  {
    float A = dot(xy,xy)+d*d;
    float B = 2.0*(R*(dot(xy,sinangle)-d*cosangle.x*cosangle.y)-d*d);
    float C = d*d + 2.0*R*d*cosangle.x*cosangle.y;
    return (-B-sqrt(B*B-4.0*A*C))/(2.0*A);
  }


  float2 bkwtrans(float2 xy)
  {
    float c = intersect(xy);
    float2 point = c*xy;
    point -= -R*sinangle;
    point /= R;
    float2 tang = sinangle/cosangle;
    float2 poc = point/cosangle;
    float A = dot(tang,tang)+1.0;
    float B = -2.0*dot(poc,tang);
    float C = dot(poc,poc)-1.0;
    float a = (-B+sqrt(B*B-4.0*A*C))/(2.0*A);
    float2 uv = (point-a*sinangle)/cosangle;
    float r = R*acos(a);
    return uv*r/sin(r/R);
  }

  float2 fwtrans(float2 uv)
  {
    float r = FIX(sqrt(dot(uv,uv)));
    uv *= sin(r/R)/r;
    float x = 1.0-cos(r/R);
    float D = d/R + x*cosangle.x*cosangle.y+dot(uv,sinangle);
    return d*(uv*cosangle-x*sinangle)/D;
  }

  float3 maxscale()
  {
    float2 c = bkwtrans(-R * sinangle / (1.0 + R/d*cosangle.x*cosangle.y));
    float2 a = float2(0.5,0.5)*aspect;
    float2 lo = float2(fwtrans(float2(-a.x,c.y)).x,
		 fwtrans(float2(c.x,-a.y)).y)/aspect;
    float2 hi = float2(fwtrans(float2(+a.x,c.y)).x,
		 fwtrans(float2(c.x,+a.y)).y)/aspect;
    return float3((hi+lo)*aspect*0.5,max(hi.x-lo.x,hi.y-lo.y));
  }


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
  
	Out.stretch = maxscale();

	// Resulting X pixel-coordinate of the pixel we're drawing.
        // Assumes (-0.5, 0.5) quad and output size in World matrix
        // as currently done in DOSBox D3D patch
        Out.abspos = float4(
		Position.x + 0.5, Position.y + 0.5,
		(Position.x + 0.5) * World._11, (Position.y - 0.5) * (-World._22));

	return Out;
  }

  float2 transform(float2 coord, float3 stretch)
  {
    coord *= SourceDims / InputDims;
    coord = (coord - 0.5) * aspect * stretch.z + stretch.xy;
    return (bkwtrans(coord) / overscan / aspect + 0.5) * InputDims / SourceDims;
  }

  float corner(float2 coord)
  {
    coord *= SourceDims / InputDims;
    coord = (coord - 0.5) * overscan + 0.5;
    coord = min(coord, 1.0-coord) * aspect;
    float2 cdist = cornersize;
    coord = (cdist - min(coord,cdist));
    float dist = sqrt(dot(coord,coord));
    return clamp((cdist.x-dist)*cornersmooth,0.0, 1.0);
  }

  // Calculate the influence of a scanline on the current pixel.
  //
  // 'distance' is the distance in texture coordinates from the current
  // pixel to the scanline in question.
  // 'color' is the colour of the scanline at the horizontal location of
  // the current pixel.
  float4 scanlineWeights(float distance, float4 color)
  {
    // "wid" controls the width of the scanline beam, for each RGB channel
    // The "weights" lines basically specify the formula that gives
    // you the profile of the beam, i.e. the intensity as
    // a function of distance from the vertical center of the
    // scanline. In this case, it is gaussian if width=2, and
    // becomes nongaussian for larger widths. Ideally this should
    // be normalized so that the integral across the beam is
    // independent of its width. That is, for a narrower beam
    // "weights" should have a higher peak at the center of the
    // scanline than for a wider beam.

#ifdef USEGAUSSIAN
    float4 wid = 0.3 + 0.1 * pow(color, 3.0);
    float4 weights = distance / wid;
    return 0.4 * exp(-weights * weights) / wid;
#else
    float4 wid = 2.0 + 2.0 * pow(color, 4.0);
    float4 weights = distance / 0.3;
    return 1.4 * exp(-pow(weights * rsqrt(0.5 * wid), wid)) / (0.6 + 0.2 * wid);
#endif

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
    float2 xy = transform(input.pixel0, input.stretch);
#else
    float2 xy = input.pixel0;
#endif

    float cval = corner(xy);

    // Of all the pixels that are mapped onto the texel we are
    // currently rendering, which pixel are we currently rendering?
    float2 ratio_scale = xy * SourceDims - 0.5;

#ifdef OVERSAMPLE
    float filter = fwidth(ratio_scale.y);
#endif

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
                           TEX2D(xy + float2(-TexelSize.r, 0.0)),
                           TEX2D(xy),
                           TEX2D(xy + float2(TexelSize.x, 0.0)),
                           TEX2D(xy + float2(2.0 * TexelSize.x, 0.0))
                    )), 0.0, 1.0);
    float4 col2 = clamp(
                    mul(coeffs, float4x4(
                           TEX2D(xy + float2(-TexelSize.x, TexelSize.y)),
                           TEX2D(xy + float2(0.0, TexelSize.y)),
                           TEX2D(xy + TexelSize),
                           TEX2D(xy + float2(2.0 * TexelSize.x, TexelSize.y))
                    )), 0.0, 1.0);

#ifndef LINEAR_PROCESSING
    col  = pow(col , CRTgamma);
    col2 = pow(col2, CRTgamma);
#endif

    // Calculate the influence of the current and next scanlines on
    // the current pixel.
    float4 weights  = scanlineWeights(uv_ratio.y, col);
    float4 weights2 = scanlineWeights(1.0 - uv_ratio.y, col2);

#ifdef OVERSAMPLE
    uv_ratio.y = uv_ratio.y+1.0/3.0*filter;
    weights = (weights+scanlineWeights(uv_ratio.y, col))/3.0;
    weights2 = (weights2+scanlineWeights(abs(1.0-uv_ratio.y), col2))/3.0;
    uv_ratio.y = uv_ratio.y-2.0/3.0*filter;
    weights = weights+scanlineWeights(abs(uv_ratio.y), col)/3.0;
    weights2 = weights2+scanlineWeights(abs(1.0-uv_ratio.y), col2)/3.0;
#endif

    float3 mul_res  = (col * weights + col2 * weights2).rgb * cval;

    // dot-mask emulation:
    // Output pixels are alternately tinted green and magenta.
    float3 dotMaskWeights = lerp(
          float3(1.0, 0.7, 1.0),
          float3(0.7, 1.0, 0.7),
          floor(input.abspos.z % 2.0)
    );

    mul_res *= dotMaskWeights;

    // Convert the image gamma for display on our output device.
    mul_res = pow(mul_res, 1.0 / monitorgamma);

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
