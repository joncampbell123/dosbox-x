/* ############################################################################################

   GS2xFilter shader - Copyright (C) 2007 guest(r) - guest.r@gmail.com

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

const float mx = 0.325;      // start smoothing wt.
const float k = -0.250;      // wt. decrease factor
const float max_w = 0.25;    // max filter weigth
const float min_w = 0.00;    // min filter weigth
const float lum_add = 0.25;  // effects smoothing


string preprocessTechique : PREPROCESSTECHNIQUE = "GS2x";
string combineTechique : COMBINETECHNIQUE =  "linear";


VERTEX_STUFF0 PASS1_VERTEX (float3 p : POSITION, float2 tc : TEXCOORD0)
{
  VERTEX_STUFF0 OUT = (VERTEX_STUFF0)0;
  
	float dx = ps.x;
	float dy = ps.y;

	OUT.coord = mul(float4(p,1),WorldViewProjection);
	OUT.CT = tc;
	OUT.t1.xy = tc + float2(-dx, 0);
	OUT.t2.xy = tc + float2( dx, 0);
	OUT.t3.xy = tc + float2( 0,-dy);
	OUT.t4.xy = tc + float2( 0, dy);
	OUT.t1.zw = tc + float2(-dx,-dy);
	OUT.t2.zw = tc + float2(-dx, dy);
	OUT.t3.zw = tc + float2( dx,-dy);
	OUT.t4.zw = tc + float2( dx, dy);

	return OUT;
}

// **PS**

float4 PASS1_FRAGMENT ( in VERTEX_STUFF0 VAR ) : COLOR
{
	float3 c00 = tex2D(s_p, VAR.t1.zw).xyz; 
	float3 c10 = tex2D(s_p, VAR.t3.xy).xyz; 
	float3 c20 = tex2D(s_p, VAR.t3.zw).xyz; 
	float3 c01 = tex2D(s_p, VAR.t1.xy).xyz; 
	float3 c11 = tex2D(s_p, VAR.CT   ).xyz; 
	float3 c21 = tex2D(s_p, VAR.t2.xy).xyz; 
	float3 c02 = tex2D(s_p, VAR.t2.zw).xyz; 
	float3 c12 = tex2D(s_p, VAR.t4.xy).xyz; 
	float3 c22 = tex2D(s_p, VAR.t4.zw).xyz;

	float md1=dot(abs(c00-c22),dt);
	float md2=dot(abs(c02-c20),dt);

	float w1=dot(abs(c22-c11),dt)*md2;
	float w2=dot(abs(c02-c11),dt)*md1;
	float w3=dot(abs(c00-c11),dt)*md2;
	float w4=dot(abs(c20-c11),dt)*md1;

	float t1 = w1+w3; float t2 = w2+w4;
	float ww = max(t1,t2)+0.001;

	c11 = (w1*c00+w2*c20+w3*c22+w4*c02+ww*c11)/(t1+t2+ww);

	float lc1 = k/(0.115*dot(c10+c12+c11,dt)+lum_add);
	float lc2 = k/(0.115*dot(c01+c21+c11,dt)+lum_add);

	w1 = clamp(lc1*dot(abs(c11-c10),dt)+mx,min_w,max_w);
	w2 = clamp(lc2*dot(abs(c11-c21),dt)+mx,min_w,max_w);
	w3 = clamp(lc1*dot(abs(c11-c12),dt)+mx,min_w,max_w);
	w4 = clamp(lc2*dot(abs(c11-c01),dt)+mx,min_w,max_w);

	c11 = w1*c10 + w2*c21 + w3*c12 + w4*c01 + (1.0-w1-w2-w3-w4)*c11;

	return float4(c11,0);
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
	float3 c11 = tex2D(w_l, VAR.CT).xyz; 
	return float4 (c11,1);
}


technique GS2x
{
   pass P0
   {
	VertexShader = compile vs_3_0 PASS1_VERTEX();
	PixelShader  = compile ps_3_0 PASS1_FRAGMENT();
   }  
}


technique linear
{
   pass P0
   {
	VertexShader = compile vs_2_0 PASS2_VERTEX();
	PixelShader  = compile ps_2_0 PASS2_FRAGMENT();
   }  
}
