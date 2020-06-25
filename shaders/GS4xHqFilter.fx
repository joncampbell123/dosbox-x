/* ############################################################################################

   GS4XHqFilter shader - Copyright (C) 2013 guest(r) - guest.r@gmail.com

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


// NOTES: Set scaler to normal4x (forced mode for hi-res)


#include "shader.code"

float scaling   : SCALING = 1.0;
float3 dtt = float3(0.001,0.001,0.001);


string preprocessTechique : PREPROCESSTECHNIQUE = "GS4x";
string combineTechique : COMBINETECHNIQUE =  "HqFilter";


VERTEX_STUFF2 PASS1_VERTEX (float3 p : POSITION, float2 tc : TEXCOORD0)
{
  VERTEX_STUFF2 OUT = (VERTEX_STUFF2)0;
  
	float dx = ps.x*2.0;
	float dy = ps.y*2.0;
	float sx = ps.x;
	float sy = ps.y;

	OUT.coord = mul(float4(p,1),WorldViewProjection);
	OUT.CT = tc;
	OUT.UL = tc + float2(-dx,-dy);
	OUT.UR = tc + float2( dx,-dy);
	OUT.DL = tc + float2(-dx, dy);
	OUT.DR = tc + float2( dx, dy);
  
	OUT.iUL = tc + float2(-sx,-sy);
	OUT.iUR = tc + float2( sx,-sy);
	OUT.iDD = float4(tc,tc) + float4(-sx, sy, sx, sy);
  
	return OUT;
}


float4 PASS1_FRAGMENT ( in VERTEX_STUFF2 VAR ) : COLOR

{
	half3 c  = tex2D(s_p, VAR.CT).xyz;
	half3 o1 = tex2D(s_p, VAR.UL).xyz;
	half3 o2 = tex2D(s_p, VAR.UR).xyz;
	half3 o3 = tex2D(s_p, VAR.DR).xyz;
	half3 o4 = tex2D(s_p, VAR.DL).xyz;
	half3 i1 = tex2D(s_p, VAR.iUL).xyz;
	half3 i2 = tex2D(s_p, VAR.iUR).xyz;
	half3 i3 = tex2D(s_p, VAR.iDD.zw).xyz;
	half3 i4 = tex2D(s_p, VAR.iDD.xy).xyz;

	half ko1=dot(abs(o1-c),dt);
	half ko2=dot(abs(o2-c),dt);
	half ko3=dot(abs(o3-c),dt);
	half ko4=dot(abs(o4-c),dt);

	half k1=dot(abs(i1-i3),dt)*dot(abs(o1-o3),dt);
	half k2=dot(abs(i2-i4),dt)*dot(abs(o2-o4),dt);
  
	half4 w = half4(k2,k1,k2,k1);

	if (ko3<ko1) w.x = 0.0;
	if (ko4<ko2) w.y = 0.0;
	if (ko1<ko3) w.z = 0.0;
	if (ko2<ko4) w.w = 0.0;  

	return float4((w.x*o1+w.y*o2+w.z*o3+w.w*o4+0.0001*c)/(dot(w,d4)+0.0001),0);
}




VERTEX_STUFF4 PASS2_VERTEX (float3 p : POSITION, float2 tc : TEXCOORD0)
{
  VERTEX_STUFF4 OUT = (VERTEX_STUFF4)0;
  
	float dx = ps.x*0.5;
	float dy = ps.y*0.5;
	float2 d1 = float2( dx,dy);
	float2 d2 = float2(-dx,dy);

	OUT.coord = mul(float4(p,1),WorldViewProjection);
	OUT.CT = tc;

	OUT.t1.xy = float2(tc).xy - d1; 
	OUT.t2.xy = float2(tc).xy - d2;
	OUT.t3.xy = float2(tc).xy + d1; 
	OUT.t4.xy = float2(tc).xy + d2;

	return OUT;
}


// **PS**

float4 PASS2_FRAGMENT ( in VERTEX_STUFF4 VAR ) : COLOR

{
	float2 x = float2(ps.x*0.5,0);
	float2 y = float2(0,ps.y*0.5);

	float3 c00, c02, c20, c22, c11, u, d, r, l;
	float m1, m2;

	c00 = tex2D(w_l, VAR.t1.xy-y).xyz; 
	c20 = tex2D(w_l, VAR.t2.xy-y).xyz; 
	c02 = tex2D(w_l, VAR.t4.xy-y).xyz; 
	c22 = tex2D(w_l, VAR.t3.xy-y).xyz; 

	m1 = dot(abs(c00-c22),dt)+0.001;
	m2 = dot(abs(c02-c20),dt)+0.001;

	u = (m2*(c00+c22)+m1*(c02+c20))/(m1+m2);

	c00 = tex2D(w_l, VAR.t1.xy+y).xyz; 
	c20 = tex2D(w_l, VAR.t2.xy+y).xyz; 
	c02 = tex2D(w_l, VAR.t4.xy+y).xyz; 
	c22 = tex2D(w_l, VAR.t3.xy+y).xyz; 
	
	m1 = dot(abs(c00-c22),dt)+0.001;
	m2 = dot(abs(c02-c20),dt)+0.001;

	d = (m2*(c00+c22)+m1*(c02+c20))/(m1+m2);

	c00 = tex2D(w_l, VAR.t1.xy-x).xyz; 
	c20 = tex2D(w_l, VAR.t2.xy-x).xyz; 
	c02 = tex2D(w_l, VAR.t4.xy-x).xyz; 
	c22 = tex2D(w_l, VAR.t3.xy-x).xyz; 

	m1 = dot(abs(c00-c22),dt)+0.001;
	m2 = dot(abs(c02-c20),dt)+0.001;

	l = (m2*(c00+c22)+m1*(c02+c20))/(m1+m2);

	c00 = tex2D(w_l, VAR.t1.xy+x).xyz; 
	c20 = tex2D(w_l, VAR.t2.xy+x).xyz; 
	c02 = tex2D(w_l, VAR.t4.xy+x).xyz; 
	c22 = tex2D(w_l, VAR.t3.xy+x).xyz; 

	m1 = dot(abs(c00-c22),dt)+0.001;
	m2 = dot(abs(c02-c20),dt)+0.001;

	r = (m2*(c00+c22)+m1*(c02+c20))/(m1+m2);

	c11 = 0.125*(u+d+r+l);

	float2 dx  = float2(ps.x,0.0);
	float2 dy  = float2(0.0,ps.y);
	float2 g1  = float2( ps.x,ps.y);
	float2 g2  = float2(-ps.x,ps.y);
	float2 pC4 = VAR.CT;

	
	// Reading the texels
 
	float3 C0 = tex2D(w_l,pC4-g1).xyz; 
	float3 C1 = tex2D(w_l,pC4-dy).xyz;
	float3 C2 = tex2D(w_l,pC4-g2).xyz;
	float3 C3 = tex2D(w_l,pC4-dx).xyz;
	float3 C4 = tex2D(w_l,pC4   ).xyz;
	float3 C5 = tex2D(w_l,pC4+dx).xyz;
	float3 C6 = tex2D(w_l,pC4+g2).xyz;
	float3 C7 = tex2D(w_l,pC4+dy).xyz;
	float3 C8 = tex2D(w_l,pC4+g1).xyz;
 
	float3 mn1 = min(min(C0,C1),C2);
	float3 mn2 = min(min(C3,C4),C5);
	float3 mn3 = min(min(C6,C7),C8);
	float3 mx1 = max(max(C0,C1),C2);
	float3 mx2 = max(max(C3,C4),C5);
	float3 mx3 = max(max(C6,C7),C8);
 
	mn1 = min(min(mn1,mn2),mn3);
	mx1 = max(max(mx1,mx2),mx3);
 
	float3 dif1 = abs(c11-mn1) + 0.001*dt;
	float3 dif2 = abs(c11-mx1) + 0.001*dt;
 
 //  float filterparam = 2.5; 
 
	float3 dif = sqrt(mx1)-sqrt(mn1);    
	float filterparam = clamp(4.0*max(dif.x,max(dif.y,dif.z)),1.0,2.4);

	dif1=float3(pow(dif1.x,filterparam),pow(dif1.y,filterparam),pow(dif1.z,filterparam));
	dif2=float3(pow(dif2.x,filterparam),pow(dif2.y,filterparam),pow(dif2.z,filterparam));
 
	c11.r = (dif1.x*mx1.x + dif2.x*mn1.x)/(dif1.x + dif2.x);
	c11.g = (dif1.y*mx1.y + dif2.y*mn1.y)/(dif1.y + dif2.y);
	c11.b = (dif1.z*mx1.z + dif2.z*mn1.z)/(dif1.z + dif2.z);
	
	// r  = length(c11);
	// c11= float3(pow(c11,1.175));
        // c11=hd(r)*normalize(c11);

	return float4(c11,1);
}

technique GS4x
{
    pass P0
    {
	VertexShader = compile vs_2_0 PASS1_VERTEX();
	PixelShader  = compile ps_2_0 PASS1_FRAGMENT();
    }  
}


technique HqFilter
{
   pass P0
   {
   	VertexShader = compile vs_3_0 PASS2_VERTEX();
   	PixelShader  = compile ps_3_0 PASS2_FRAGMENT();

   }  
}
