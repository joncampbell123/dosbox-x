// this a re-implementation of CGA palettes from DOSBox Staging by https://github.com/aybe
// the idea of custom CGA palettes is originally an idea from https://github.com/johnnovak

#include "shader.code"
string name : NAME = "Filter";
string combineTechique : COMBINETECHNIQUE = "Filter";

static const float4 colors[16] = {
    float4(0x00, 0x00, 0x00, 0xFF) / 0xFF,
    float4(0x00, 0x00, 0xAA, 0xFF) / 0xFF,
    float4(0x00, 0xAA, 0x00, 0xFF) / 0xFF,
    float4(0x00, 0xAA, 0xAA, 0xFF) / 0xFF,
    float4(0xAA, 0x00, 0x00, 0xFF) / 0xFF,
    float4(0xAA, 0x00, 0xAA, 0xFF) / 0xFF,
    float4(0xAA, 0x55, 0x00, 0xFF) / 0xFF,
    float4(0xAA, 0xAA, 0xAA, 0xFF) / 0xFF,
    float4(0x55, 0x55, 0x55, 0xFF) / 0xFF,
    float4(0x55, 0x55, 0xFF, 0xFF) / 0xFF,
    float4(0x55, 0xFF, 0x55, 0xFF) / 0xFF,
    float4(0x55, 0xFF, 0xFF, 0xFF) / 0xFF,
    float4(0xFF, 0x55, 0x55, 0xFF) / 0xFF,
    float4(0xFF, 0x55, 0xFF, 0xFF) / 0xFF,
    float4(0xFF, 0xFF, 0x55, 0xFF) / 0xFF,
    float4(0xFF, 0xFF, 0xFF, 0xFF) / 0xFF,
};

VERTEX_STUFF_W S_VERTEX(float3 p : POSITION, float2 tc : TEXCOORD0) {
  VERTEX_STUFF_W OUT = (VERTEX_STUFF_W)0;
  OUT.coord = mul(float4(p, 1), WorldViewProjection);
  OUT.CT = tc;
  return OUT;
}

float4 S_FRAGMENT(in VERTEX_STUFF_W VAR) : COLOR {
  float4 src = tex2D(s_p, VAR.CT);
  float4 tgt = float4(1, 1, 1, 1);
  float current;
  float minimum = 1.0;
  int index = 0;
  for (int i = 0; i < 16; i++) {
    current = distance(src.xyz, colors[i].xyz);
    if (current < minimum) {
      minimum = current;
      index = i;
    }
  }
  return palette[index];
}

technique Filter {
  pass P0 {
    VertexShader = compile vs_3_0 S_VERTEX();
    PixelShader = compile ps_3_0 S_FRAGMENT();
  }
}
