/*
 *    Dot 'n bloom shader
 *    Author: Themaister
 *    License: Public domain
 *
 *    Direct3D port by gulikoza at users.sourceforge.net
 *
 */

      #include "Scaling.inc"

      // The name of this effect
      string name : NAME = "DOTNBLOOM";
      float scaling : SCALING = 1.0;

      #define gamma 2.4
      #define shine 0.05
      #define blend 0.65

      //
      // Techniques
      //

      // combineTechnique: Final combine steps. Outputs to destination frame buffer
      string combineTechique : COMBINETECHNIQUE = "DOTNBLOOM";

      struct VS_OUTPUT_PRODUCT
      {
		float4 Position		: POSITION;
		float2 pixel0		: TEXCOORD0;
		float2 pixel_no		: TEXCOORD1;
		float2 pixel_s		: TEXCOORD2;
      };

      // vertex shader
      VS_OUTPUT_PRODUCT VS_Product(
			float3 Position : POSITION,
			float2 TexCoord : TEXCOORD0)
      {
         VS_OUTPUT_PRODUCT Out = (VS_OUTPUT_PRODUCT)0;

         // Do the standard vertex processing.
         Out.Position = mul(half4(Position, 1), WorldViewProjection);

         // Texture coords.
         Out.pixel0 = TexCoord;
         Out.pixel_no = TexCoord * SourceDims;
         Out.pixel_s = TexelSize.x;

         return Out;
      }


      float dist(float2 coord, float2 source)
      {
         float2 delta = coord - source;
         return sqrt(dot(delta, delta));
      }

      float color_bloom(float3 color)
      {
         const float3 gray_coeff = float3(0.30, 0.59, 0.11);
         float bright = dot(color, gray_coeff);
         return lerp(1.0 + shine, 1.0 - shine, bright);
      }

      float3 lookup(float2 offset, float2 coord, float2 pixel_no)
      {
         float3 color = tex2D(SourceSampler, coord).rgb;
         float delta = dist(frac(pixel_no), offset + float2(0.5, 0.5));
         return color * exp(-gamma * delta * color_bloom(color));
      }

      // Build function parameters
      #define OFFSET(c)		c, input.pixel0 + (c * input.pixel_s), input.pixel_no

      half4 PS_Product ( in VS_OUTPUT_PRODUCT input ) : COLOR
      {
         float3 color = float3(0.0, 0.0, 0.0);
         float3 mid_color = lookup(OFFSET(float2(0.0, 0.0)));

         color += lookup(OFFSET(float2(-1.0, -1.0)));
         color += lookup(OFFSET(float2( 0.0, -1.0)));
         color += lookup(OFFSET(float2( 1.0, -1.0)));
         color += lookup(OFFSET(float2(-1.0,  0.0)));
         color += mid_color;
         color += lookup(OFFSET(float2( 1.0,  0.0)));
         color += lookup(OFFSET(float2(-1.0,  1.0)));
         color += lookup(OFFSET(float2( 0.0,  1.0)));
         color += lookup(OFFSET(float2( 1.0,  1.0)));

         float3 out_color = lerp(1.2 * mid_color, color, blend);

	 return half4(out_color, 1.0);
      }

technique DOTNBLOOM
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
