/*
   Downsamples to 16 colors using quantization.
   Uses 4-bit RGBI values for an "EGA"/"Tandy" look
   
   Author: VileR
   License: public domain
*/

// 1.0 is the 'proper' value, 1.2 seems to give better results but brighter
// colors may clip.
#define color_enhance 1.2

// The name of this effect
string name : NAME = "EGAfilter";

// combineTechnique: Final combine steps. Outputs to destination frame buffer
string combineTechique : COMBINETECHNIQUE = "EGAfilter";

// Matrix Definitions for Scaler Effects
matrix World                : WORLD;
matrix View                 : VIEW;
matrix Projection           : PROJECTION;
matrix WorldView            : WORLDVIEW; // world * view
matrix ViewProjection       : VIEWPROJECTION; // view * projection
matrix WorldViewProjection  : WORLDVIEWPROJECTION; // world * view * projection

// Source Texture
texture SourceTexture : SOURCETEXTURE;

// Point Sampler
sampler SourcePointSampler = sampler_state {
 Texture   = (SourceTexture);
 MinFilter = POINT;
 MagFilter = POINT;
};  

// Vertex Shader Output
struct VS_OUTPUT_PRODUCT {
  float4  coord : POSITION;
  float2  TexCoord   : TEXCOORD0;
};

// Color lookup table (RGBI palette with brown fix)
const half3 rgbi_palette[16] = {
  half3(0.0,     0.0,     0.0),
  half3(0.0,     0.0,     0.66667),
  half3(0.0,     0.66667, 0.0),
  half3(0.0,     0.66667, 0.66667),
  half3(0.66667, 0.0,     0.0),
  half3(0.66667, 0.0,     0.66667),
  half3(0.66667, 0.33333, 0.0),
  half3(0.66667, 0.66667, 0.66667),
  half3(0.33333, 0.33333, 0.33333),
  half3(0.33333, 0.33333, 1.0),
  half3(0.33333, 1.0,     0.33333),
  half3(0.33333, 1.0,     1.0),
  half3(1.0,     0.33333, 0.33333),
  half3(1.0,     0.33333, 1.0),
  half3(1.0,     1.0,     0.33333),
  half3(1.0,     1.0,     1.0),
};

// Compare vector distances and return nearest RGBI color
half3 nearest_rgbi (half3 original) {
  half dst;
  half min_dst = 2.0;
  int idx = 0;
  for (int i=0; i<16; i++) {
    dst = distance(original, rgbi_palette[i]);
    if (dst < min_dst) {
      min_dst = dst;
      idx = i;
    }
  }
  return rgbi_palette[idx];
}

// Vertex shader
VS_OUTPUT_PRODUCT S_VERTEX (float3 p : POSITION, float2 tc : TEXCOORD0)
{
  VS_OUTPUT_PRODUCT OUT = (VS_OUTPUT_PRODUCT)0;

  OUT.coord = mul(float4(p,1),WorldViewProjection);
  OUT.TexCoord = tc;
  return OUT;
}

// Pixel shader
float4 S_FRAGMENT ( in VS_OUTPUT_PRODUCT VAR ) : COLOR
{
  half3 fragcolor = tex2D(SourcePointSampler, VAR.TexCoord).rgb;
  return half4(nearest_rgbi(fragcolor*color_enhance), 1);
}

technique EGAfilter
{
  pass P0
  {
    VertexShader = compile vs_3_0 S_VERTEX();
    PixelShader  = compile ps_3_0 S_FRAGMENT();
  }  
}
