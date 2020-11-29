/*
   Copyright (C) 2005 guest(r) - guest.r@gmail.com

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

/*

   This SimpleAA shader is well used to:
  
   - AA 2xscaled gfx. to its 4x absolute size,
   
   - AA hi-res "screens" (640x480) to their 2x size.

*/


#include "shader.code"

float scaling   : SCALING = 2.0;


string name : NAME = "SimpleAA";
string combineTechique : COMBINETECHNIQUE =  "SimpleAA";


// **VS**

VERTEX_STUFF3 S_VERTEX (float3 p : POSITION, float2 tc : TEXCOORD0)
{
  VERTEX_STUFF3 OUT = (VERTEX_STUFF3)0;
  
  float dx = ps.x*0.5;
  float dy = ps.y*0.5;

  OUT.coord = mul(float4(p,1),WorldViewProjection);
  OUT.C = tc;
  OUT.L = tc + float2(-dx, 0);
  OUT.R = tc + float2( dx, 0);
  OUT.U = tc + float2( 0,-dy);
  OUT.D = tc + float2( 0, dy);
  return OUT;
}

// **PS**

float4 S_FRAGMENT ( in VERTEX_STUFF3 VAR ) : COLOR
{
  half3 c11 = tex2D(s_p, VAR.C ).xyz;
  half3 c01 = tex2D(s_p, VAR.L ).xyz;
  half3 c21 = tex2D(s_p, VAR.R ).xyz;
  half3 c10 = tex2D(s_p, VAR.U ).xyz;
  half3 c12 = tex2D(s_p, VAR.D ).xyz;

  half k1 = dot(abs(c01-c21),dt)+0.0001;
  half k2 = dot(abs(c10-c12),dt)+0.0001;

  return half4((k1*(c10+c12)+k2*(c01+c21))/(2.0*(k1+k2)),0);
}

technique SimpleAA
{
   pass P0
   {
     VertexShader = compile vs_2_0 S_VERTEX();
     PixelShader  = compile ps_2_0 S_FRAGMENT();
   }  
}
