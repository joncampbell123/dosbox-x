/* ############################################################################################

   2xSaLAA shader - Copyright (C) 2013 guest(r) - guest.r@gmail.com

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
 
float scaling   : SCALING = 0.5;
float3 dtt = float3(0.005,0.005,0.005);
 

string preprocessTechique : PREPROCESSTECHNIQUE = "SaL";
string combineTechique : COMBINETECHNIQUE =  "AA";
 


VERTEX_STUFF4 PASS1_VERTEX (float3 p : POSITION, float2 tc : TEXCOORD0)
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

float4 PASS1_FRAGMENT ( in VERTEX_STUFF4 VAR ) : COLOR
{
	float2 x = float2(ps.x*0.5,0);
	float2 y = float2(0,ps.y*0.5);

	float3 c00, c02, c20, c22, c11, u, d, r, l;
	float m1, m2;

	c00 = tex2D(s_p, VAR.t1.xy-y).xyz; 
	c20 = tex2D(s_p, VAR.t2.xy-y).xyz; 
	c02 = tex2D(s_p, VAR.t4.xy-y).xyz; 
	c22 = tex2D(s_p, VAR.t3.xy-y).xyz; 

	m1 = dot(abs(c00-c22),dt)+0.001;
	m2 = dot(abs(c02-c20),dt)+0.001;

	u = (m2*(c00+c22)+m1*(c02+c20))/(m1+m2);

	c00 = tex2D(s_p, VAR.t1.xy+y).xyz; 
	c20 = tex2D(s_p, VAR.t2.xy+y).xyz; 
	c02 = tex2D(s_p, VAR.t4.xy+y).xyz; 
	c22 = tex2D(s_p, VAR.t3.xy+y).xyz; 
	
	m1 = dot(abs(c00-c22),dt)+0.001;
	m2 = dot(abs(c02-c20),dt)+0.001;

	d = (m2*(c00+c22)+m1*(c02+c20))/(m1+m2);

	c00 = tex2D(s_p, VAR.t1.xy-x).xyz; 
	c20 = tex2D(s_p, VAR.t2.xy-x).xyz; 
	c02 = tex2D(s_p, VAR.t4.xy-x).xyz; 
	c22 = tex2D(s_p, VAR.t3.xy-x).xyz; 

	m1 = dot(abs(c00-c22),dt)+0.001;
	m2 = dot(abs(c02-c20),dt)+0.001;

	l = (m2*(c00+c22)+m1*(c02+c20))/(m1+m2);

	c00 = tex2D(s_p, VAR.t1.xy+x).xyz; 
	c20 = tex2D(s_p, VAR.t2.xy+x).xyz; 
	c02 = tex2D(s_p, VAR.t4.xy+x).xyz; 
	c22 = tex2D(s_p, VAR.t3.xy+x).xyz; 

	m1 = dot(abs(c00-c22),dt)+0.001;
	m2 = dot(abs(c02-c20),dt)+0.001;

	r = (m2*(c00+c22)+m1*(c02+c20))/(m1+m2);

	float3 d11 = 0.125*(u+d+r+l);

	float2 dx  = float2(0.5*ps.x,0.0);
	float2 dy  = float2(0.0,0.5*ps.y);
	float2 g1  = float2(0.5*ps.x,0.5*ps.y);
	float2 g2  = float2(-0.5*ps.x,0.5*ps.y);
	float2 pC4 = VAR.CT;

	
	// Reading the texels

	float3 c10,c01,c21,c12; 

	c00 = tex2D(s_p,pC4-g1).xyz; 
	c10 = tex2D(s_p,pC4-dy).xyz;
	c20 = tex2D(s_p,pC4-g2).xyz;
	c01 = tex2D(s_p,pC4-dx).xyz;
	c11 = tex2D(s_p,pC4   ).xyz;
	c21 = tex2D(s_p,pC4+dx).xyz;
	c02 = tex2D(s_p,pC4+g2).xyz;
	c12 = tex2D(s_p,pC4+dy).xyz;
	c22 = tex2D(s_p,pC4+g1).xyz;
 
	float d1=dot(abs(c00-c22),dt)+0.001;
    	float d2=dot(abs(c20-c02),dt)+0.001;
    	float hl=dot(abs(c01-c21),dt)+0.001;
    	float vl=dot(abs(c10-c12),dt)+0.001;

    	float k1=hl+vl;
    	float k2=d1+d2;
	
    	float3 t1=(hl*(c10+c12)+vl*(c01+c21)+0.0*k1*c11)/(2.0*(hl+vl));
    	float3 t2=(d1*(c20+c02)+d2*(c00+c22)+0.0*k2*c11)/(2.0*(d1+d2));

    	k1=dot(abs(t1-d11),dt)+0.001;
    	k2=dot(abs(t2-d11),dt)+0.001;

    	c11 = (k1*t2 + k2*t1)/(k1+k2);

    	float3 mn1 = min(min(c00,c01),c02);
    	float3 mn2 = min(min(c10,c11),c12);
    	float3 mn3 = min(min(c20,c21),c22);

    	float3 mx1 = max(max(c00,c01),c02);
    	float3 mx2 = max(max(c10,c11),c12);
    	float3 mx3 = max(max(c20,c21),c22);

    	mn1 = min(min(mn1,mn2),mn3);
    	mx1 = max(max(mx1,mx2),mx3);

    	float filterparam = 1.7; 

    	float3 dif1 = abs(c11-mn1) + 0.001*dt; 
    	float3 dif2 = abs(c11-mx1) + 0.001*dt; 
	
    	dif1=float3(pow(dif1.x,filterparam),pow(dif1.y,filterparam),pow(dif1.z,filterparam));
    	dif2=float3(pow(dif2.x,filterparam),pow(dif2.y,filterparam),pow(dif2.z,filterparam));

    	c11.r = (dif1.x*mx1.x + dif2.x*mn1.x)/(dif1.x + dif2.x);
    	c11.g = (dif1.y*mx1.y + dif2.y*mn1.y)/(dif1.y + dif2.y);
    	c11.b = (dif1.z*mx1.z + dif2.z*mn1.z)/(dif1.z + dif2.z);

	return float4(c11,1);
}
 

VERTEX_STUFF0 PASS2_VERTEX (float3 p : POSITION, float2 tc : TEXCOORD0)
{
	VERTEX_STUFF0 OUT = (VERTEX_STUFF0)0;
  
  	float dx = 0.5*ps.x;
  	float dy = 0.5*ps.y;

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

   	float d1 = dot(abs(c00-c22),dt)+0.001;
   	float d2 = dot(abs(c20-c02),dt)+0.001;
   	float hl = dot(abs(c01-c21),dt)+0.001;
   	float vl = dot(abs(c10-c12),dt)+0.001;

   	float3 mn1 = min (min (c00,c01), c02);
   	float3 mn2 = min (min (c10,c11), c12);
   	float3 mn3 = min (min (c20,c21), c22);
   	float3 mx1 = max (max (c00,c01), c02);
   	float3 mx2 = max (max (c10,c11), c12);
   	float3 mx3 = max (max (c20,c21), c22);

   	mn1 = min(min(mn1,mn2),mn3);
   	mx1 = max(max(mx1,mx2),mx3);
	
   	float md = d1+d2;   float mc = hl+vl;
   	hl*=  md;vl*= md;   d1*=  mc;d2*= mc;

   	float ww = d1+d2+vl+hl;
   	c11=(hl*(c10+c12)+vl*(c01+c21)+d1*(c20+c02)+d2*(c00+c22)+ww*c11)/(3.0*ww);

   	float3 dif1 = abs(c11-mn1) + dtt; float df1 = dot(dif1,dt);
   	float3 dif2 = abs(c11-mx1) + dtt; float df2 = dot(dif2,dt);

    	//float filterparam = 5.0;

        float3 dif = mx1-mn1;	
        float filterparam = clamp(8.0*max(dif.x,max(dif.y,dif.z)+0.1),1.0,6.0);


   	df1=pow(df1,filterparam);
   	df2=pow(df2,filterparam);

   	c11 = (df2*mn1 + df1*mx1)/(df1+df2);

   	return float4(c11,1);
}


technique SaL
{
       pass P0
       {
             VertexShader = compile vs_3_0 PASS1_VERTEX();
             PixelShader  = compile ps_3_0 PASS1_FRAGMENT();
       }  
}
 

 
technique AA
 
{
       pass P0
 
       {
             VertexShader = compile vs_3_0 PASS2_VERTEX();
             PixelShader  = compile ps_3_0 PASS2_FRAGMENT();
       }  
}
 
