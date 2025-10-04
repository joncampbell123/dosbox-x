/*
   Copyright (C) 2006 guest(r) - guest.r@gmail.com

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

  The shader tries to make a "BW sketch" of the screen that is currently being
  drawn.

*/


#include "shader.code"

float threshold = 0.125; // effects drawing


string name : NAME = "Sketch";
string combineTechique : COMBINETECHNIQUE =  "Sketch";


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
  float3 pap = float3(0.82, 0.77, 0.61);
  float3 ink = float3(0.28, 0.32, 0.32);  
  float3 c11 = tex2D(s_l, VAR.C).xyz;
  float3 c01 = tex2D(s_l, VAR.L).xyz;
  float3 c21 = tex2D(s_l, VAR.R).xyz;
  float3 c10 = tex2D(s_l, VAR.U).xyz;
  float3 c12 = tex2D(s_l, VAR.D).xyz;
	  
  float lum_ct = dot(c11,dt)*0.5 + 0.5;
  float lum_x  = dot(c01+c21,dt)*0.2 + lum_ct;
  float lum_y  = dot(c10+c12,dt)*0.2 + lum_ct;

  float d = dot(abs(c01-c21),dt)/lum_x + dot(abs(c10-c12),dt)/lum_y;
  float a = max(0.0, 3.0*d-0.15);

  return float4((1.0-a)*pap + a*ink,1);
}



technique Sketch
{
   pass P0
   {
     VertexShader = compile vs_2_0 S_VERTEX();
     PixelShader  = compile ps_2_0 S_FRAGMENT();
   }  
}
