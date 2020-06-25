/* ############################################################################################

   Cartoon shader - Copyright (C) 2013 guest(r) - guest.r@gmail.com

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


const float cl = 9.0;
const float max_outline = 0.70;

#include "shader.code"

float scaling   : SCALING = 0.1;


string preprocessTechique : PREPROCESSTECHNIQUE = "AA";
string combineTechique : COMBINETECHNIQUE =  "cartoon";


VERTEX_STUFF0 PASS1_VERTEX (float3 p : POSITION, float2 tc : TEXCOORD0)
{
	VERTEX_STUFF0 OUT = (VERTEX_STUFF0)0;
  
	float dx = ps.x;
	float dy = ps.y;

	OUT.coord = mul(float4(p,1),WorldViewProjection);
	OUT.CT = tc;
	OUT.t1.xy = tc + float2(-dx,  0);
	OUT.t2.xy = tc + float2( dx,  0);
	OUT.t3.xy = tc + float2(  0,-dy);
	OUT.t4.xy = tc + float2(  0, dy);
	OUT.t1.zw = tc + float2(-dx,-dy);
	OUT.t2.zw = tc + float2(-dx, dy);
	OUT.t3.zw = tc + float2( dx,-dy);
	OUT.t4.zw = tc + float2( dx, dy);

	return OUT;
}

// **PS**

float4 PASS1_FRAGMENT ( in VERTEX_STUFF0 VAR ) : COLOR
{
	half3 c00 = tex2D(s_l, VAR.t1.zw).xyz; 
	half3 c10 = tex2D(s_l, VAR.t3.xy).xyz; 
	half3 c20 = tex2D(s_l, VAR.t3.zw).xyz; 
	half3 c01 = tex2D(s_l, VAR.t1.xy).xyz; 
	half3 c11 = tex2D(s_l, VAR.CT).xyz; 
	half3 c21 = tex2D(s_l, VAR.t2.xy).xyz; 
	half3 c02 = tex2D(s_l, VAR.t2.zw).xyz; 
	half3 c12 = tex2D(s_l, VAR.t4.xy).xyz; 
	half3 c22 = tex2D(s_l, VAR.t4.zw).xyz;

	float d1=dot(abs(c00-c22),dt)+0.001;
	float d2=dot(abs(c20-c02),dt)+0.001;
	float hl=dot(abs(c01-c21),dt)+0.001;
	float vl=dot(abs(c10-c12),dt)+0.001;

	float md = d1+d2;  float mc = hl+vl;
	hl*=  md;vl*= md;  d1*=  mc;d2*= mc;
	
	float ww = d1+d2+hl+vl;

	return float4 ((hl*(c10+c12)+vl*(c01+c21)+d1*(c20+c02)+d2*(c00+c22)+ww*c11)/(3.0*ww),1);
}



VERTEX_STUFF0 PASS2_VERTEX (float3 p : POSITION, float2 tc : TEXCOORD0)
{
  VERTEX_STUFF0 OUT = (VERTEX_STUFF0)0;

	float dx = ps.x*0.5;
	float dy = ps.y*0.5;

	OUT.coord = mul(float4(p,1),WorldViewProjection);
	OUT.CT = tc;
	OUT.t1.xy = tc + float2(-dx,  0);
	OUT.t2.xy = tc + float2( dx,  0);
	OUT.t3.xy = tc + float2(  0,-dy);
	OUT.t4.xy = tc + float2(  0, dy);
	OUT.t1.zw = tc + float2(-dx,-dy);
	OUT.t2.zw = tc + float2(-dx, dy);
	OUT.t3.zw = tc + float2( dx,-dy);
	OUT.t4.zw = tc + float2( dx, dy);

	return OUT;
}


float4 PASS2_FRAGMENT ( in VERTEX_STUFF0 VAR ) : COLOR
{
	half3 c00 = tex2D(w_l, VAR.t1.zw).xyz; 
	half3 c10 = tex2D(w_l, VAR.t3.xy).xyz; 
	half3 c20 = tex2D(w_l, VAR.t3.zw).xyz; 
	half3 c01 = tex2D(w_l, VAR.t1.xy).xyz; 
	half3 c11 = tex2D(w_l, VAR.CT).xyz; 
	half3 c21 = tex2D(w_l, VAR.t2.xy).xyz; 
	half3 c02 = tex2D(w_l, VAR.t2.zw).xyz; 
	half3 c12 = tex2D(w_l, VAR.t4.xy).xyz; 
	half3 c22 = tex2D(w_l, VAR.t4.zw).xyz;

	float d1=dot(abs(c00-c22),dt)+0.001;
	float d2=dot(abs(c20-c02),dt)+0.001;
	float hl=dot(abs(c01-c21),dt)+0.001;
	float vl=dot(abs(c10-c12),dt)+0.001;

	float d = max(max(d1,d2),max(hl,vl))/(length(0.20*(c11+c10+c01+c12+c21))+0.5);
	d = max(d-0.15,0.0)*1.25;	

	float l = length(c11); 
	l = l*cl;
	float f = frac(l); f = pow(f,5.0);
	l = (floor(l) + f)/cl;	

	c11 = (1.1-min(d,max_outline))*normalize(c11)*l;
	return float4 (c11,1);
}



technique AA
{
   pass P0
   {
   	VertexShader = compile vs_2_0 PASS1_VERTEX();
   	PixelShader  = compile ps_2_0 PASS1_FRAGMENT();
   }  
}


technique cartoon
{
   pass P0
   {
   	VertexShader = compile vs_2_0 PASS2_VERTEX();
   	PixelShader  = compile ps_2_0 PASS2_FRAGMENT();
   }  
}
