/* ############################################################################################

   GS2xSuper shader(linear) - Copyright (C) 2007 guest(r) - guest.r@gmail.com

   ############################################################################################

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

   ############################################################################################*/


// NOTES: Set scaler to normal2x (forced mode for hi-res)


#include "shader.code"

float scaling   : SCALING = 1.1;


string preprocessTechique : PREPROCESSTECHNIQUE = "GS2xSuper";
string combineTechique : COMBINETECHNIQUE =  "lin";


VERTEX_STUFF1 PASS1_VERTEX (float3 p : POSITION, float2 tc : TEXCOORD0)
{
	VERTEX_STUFF1 OUT = (VERTEX_STUFF1)0;
  
	float dx = ps.x;
	float dy = ps.y;

	OUT.coord = mul(float4(p,1),WorldViewProjection);
	OUT.CT = tc;
	OUT.UL = tc + float2(-dx,-dy);
	OUT.UR = tc + float2( dx,-dy);
	OUT.DL = tc + float2(-dx, dy);
	OUT.DR = tc + float2( dx, dy);
	return OUT;
}


float4 PASS1_FRAGMENT ( in VERTEX_STUFF1 VAR ) : COLOR

{
	float3 c11 = tex2D(s_p, VAR.CT).xyz;
	float3 c00 = tex2D(s_p, VAR.UL).xyz;
	float3 c20 = tex2D(s_p, VAR.UR).xyz;
	float3 c02 = tex2D(s_p, VAR.DL).xyz;
	float3 c22 = tex2D(s_p, VAR.DR).xyz;
	
	float m1=dot(abs(c00-c22),dt);
	float m2=dot(abs(c02-c20),dt);
	float w2=dot(abs(c02-c11),dt);
	float w1=dot(abs(c22-c11),dt);
	float w3=dot(abs(c00-c11),dt);
	float w4=dot(abs(c20-c11),dt);

	m1 =min(w1,m1); m2 =min(w2,m2);
	w1*=m2; w2*=m1; w3*=m2; w4*=m1;
	
	return float4((w1*c00+w2*c20+w3*c22+w4*c02+0.001*c11)/(w1+w2+w3+w4+0.001),0);
}



VERTEX_STUFF0 PASS2_VERTEX (float3 p : POSITION, float2 tc : TEXCOORD0)
{
	VERTEX_STUFF0 OUT = (VERTEX_STUFF0)0;

	OUT.coord = mul(float4(p,1),WorldViewProjection);
	OUT.CT = tc;
	return OUT;
}

float4 PASS2_FRAGMENT ( in VERTEX_STUFF0 VAR ) : COLOR
{
	half3 c11 = tex2D(w_l, VAR.CT).xyz; 
	return float4 (c11,1);
}


technique GS2xSuper
{
   pass P0
   {
	VertexShader = compile vs_2_0 PASS1_VERTEX();
	PixelShader  = compile ps_2_0 PASS1_FRAGMENT();
   }  
}


technique lin
{
   pass P0
   {
	VertexShader = compile vs_2_0 PASS2_VERTEX();
	PixelShader  = compile ps_2_0 PASS2_FRAGMENT();
   }  
}
