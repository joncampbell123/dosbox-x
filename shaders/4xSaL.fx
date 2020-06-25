/* ############################################################################################

   4XSaL shader - Copyright (C) 2013 guest(r) - guest.r@gmail.com

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
float3 dtt = float3(0.001,0.001,0.001);


string preprocessTechique : PREPROCESSTECHNIQUE = "SaL1";
string combineTechique : COMBINETECHNIQUE =  "SaL2";


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


VERTEX_STUFF1 PASS2_VERTEX (float3 p : POSITION, float2 tc : TEXCOORD0)
{
  VERTEX_STUFF1 OUT = (VERTEX_STUFF1)0;
  
	float dx = ps.x*0.25;
	float dy = ps.y*0.25;

	OUT.coord = mul(float4(p,1),WorldViewProjection);
	OUT.CT = tc;
	OUT.UL = tc + float2(-dx,-dy);
	OUT.UR = tc + float2( dx,-dy);
	OUT.DL = tc + float2(-dx, dy);
	OUT.DR = tc + float2( dx, dy);
	return OUT;
}


float4 PASS2_FRAGMENT ( in VERTEX_STUFF1 VAR ) : COLOR

{
	half3 c00 = tex2D(w_l, VAR.UL).xyz;
	half3 c20 = tex2D(w_l, VAR.UR).xyz;
	half3 c02 = tex2D(w_l, VAR.DL).xyz;
	half3 c22 = tex2D(w_l, VAR.DR).xyz;
	
	half m1=dot(abs(c00-c22),dt)+0.001;
	half m2=dot(abs(c02-c20),dt)+0.001;

	float3 c11 = (m1*(c02+c20)+m2*(c22+c00))/(2.0*(m1+m2));

	float3 mn1 = min(min(c00,c20),min(c02,c22));
	float3 mx1 = max(max(c00,c20),max(c02,c22));

	float3 dif1 = abs(c11-mn1) + dtt;
	float3 dif2 = abs(c11-mx1) + dtt;

	float3 filterparam = float3(2.5,2.5,2.5);

	dif1 = pow(dif1,filterparam);
	dif2 = pow(dif2,filterparam);
	  
	return float4( (dif1.x*mx1.x + dif2.x*mn1.x)/(dif1.x + dif2.x),
                       (dif1.y*mx1.y + dif2.y*mn1.y)/(dif1.y + dif2.y),
                       (dif1.z*mx1.z + dif2.z*mn1.z)/(dif1.z + dif2.z),1);
}

technique SaL1
{
    pass P0
    {
	VertexShader = compile vs_2_0 PASS1_VERTEX();
	PixelShader  = compile ps_2_0 PASS1_FRAGMENT();
    }  
}

technique SaL2
{
   pass P0
   {
   	VertexShader = compile vs_2_0 PASS2_VERTEX();
   	PixelShader  = compile ps_2_0 PASS2_FRAGMENT();
   }  
}
