/*

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



#include "shader.code"


string name : NAME = "MCHerc";
string combineTechique : COMBINETECHNIQUE =  "MCHerc";

VERTEX_STUFF_W S_VERTEX (float3 p : POSITION, float2 tc : TEXCOORD0)
{
  VERTEX_STUFF_W OUT = (VERTEX_STUFF_W)0;

  OUT.coord = mul(float4(p,1),WorldViewProjection);
  OUT.CT = tc;
  return OUT;
}

// **PS**

float4 S_FRAGMENT ( in VERTEX_STUFF_W VAR ) : COLOR

{
// Green-Screen
  half3 ink = half3(0.21, 0.35, 0.0);
// Amber-Screen
//  half3 ink = half3(0.37, 0.27, 0.0);
  half3 c11 = tex2D(s_p, VAR.CT).xyz;
  half lct = length(c11);
// Alpha feedback
  return half4(lct*ink,0.25);
}

technique MCHerc
{
   pass P0
   {
     AlphaBlendEnable = true;
     SrcBlend = InvSrcAlpha;
     DestBlend = SrcAlpha;
     DitherEnable = True;

     VertexShader = compile vs_2_0 S_VERTEX();
     PixelShader  = compile ps_2_0 S_FRAGMENT();
   }
}
