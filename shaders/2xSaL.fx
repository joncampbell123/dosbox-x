/* ############################################################################################

   2xSaL (bilinear) shader - Copyright (C) 2013 guest(r) - guest.r@gmail.com

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


// NOTES: Set scaler to normal2x or any other 2x scaler (forced mode for hi-res)



#include "shader.code"

float scaling   : SCALING = 1.1;


string preprocessTechique : PREPROCESSTECHNIQUE = "SaL";
string combineTechique : COMBINETECHNIQUE =  "Ls";



VERTEX_STUFF1 PASS1_VERTEX (float3 p : POSITION, float2 tc : TEXCOORD0)
{
  VERTEX_STUFF1 OUT = (VERTEX_STUFF1)0;
  
	float dx = ps.x*0.5;
	float dy = ps.y*0.5;

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
	half3 c00 = tex2D(s_p, VAR.UL).xyz;
	half3 c20 = tex2D(s_p, VAR.UR).xyz;
	half3 c02 = tex2D(s_p, VAR.DL).xyz;
	half3 c22 = tex2D(s_p, VAR.DR).xyz;
	
	half m1=dot(abs(c00-c22),dt)+0.001;
	half m2=dot(abs(c02-c20),dt)+0.001;
  
	return float4((m1*(c02+c20)+m2*(c22+c00))/(2.0*(m1+m2)),0);
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


technique SaL
{
   pass P0
   {
   	VertexShader = compile vs_2_0 PASS1_VERTEX();
   	PixelShader  = compile ps_2_0 PASS1_FRAGMENT();
   }  
}


technique Ls
{
   pass P0
   {
   	VertexShader = compile vs_2_0 PASS2_VERTEX();
   	PixelShader  = compile ps_2_0 PASS2_FRAGMENT();
   }  
}
