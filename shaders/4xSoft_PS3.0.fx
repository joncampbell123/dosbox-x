/* ############################################################################################

   4XSoft_PS3.0 shader - Copyright (C) 2013 guest(r) - guest.r@gmail.com

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


// NOTES: Set scaler to normal3x or any other 3x scaler (forced mode for hi-res)



#include "shader.code"

float scaling   : SCALING = 1.1;
float3 dtt = float3(0.005,0.005,0.005);


string combinetechnique : COMBINETECHNIQUE =  "soft";



VERTEX_STUFF4 PASS1_VERTEX (float3 p : POSITION, float2 tc : TEXCOORD0)
{
  VERTEX_STUFF4 OUT = (VERTEX_STUFF4)0;
  
	float dx = ps.x*1.50;
	float dy = ps.y*1.50;
	float sx = ps.x*0.75;
	float sy = ps.y*0.75;

	OUT.coord = mul(float4(p,1),WorldViewProjection);
	OUT.CT = tc;

	OUT.t1 = float4(tc,tc) + float4(-dx,-dy, dx,-dy); // outer diag. texels
	OUT.t2 = float4(tc,tc) + float4( dx, dy,-dx, dy);
	OUT.t3 = float4(tc,tc) + float4(-sx,-sy, sx,-sy); // inner diag. texels
	OUT.t4 = float4(tc,tc) + float4( sx, sy,-sx, sy);
	OUT.t5 = float4(tc,tc) + float4(-dx,  0, dx,  0); // inner hor/vert texels
	OUT.t6 = float4(tc,tc) + float4(  0,-dy,  0, dy);

	return OUT;
}


float4 PASS1_FRAGMENT ( in VERTEX_STUFF4 VAR ) : COLOR

{
	half3 c11 = tex2D(s_l, VAR.CT   ).xyz;
	half3 c00 = tex2D(s_l, VAR.t1.xy).xyz;
	half3 c20 = tex2D(s_l, VAR.t1.zw).xyz;
	half3 c22 = tex2D(s_l, VAR.t2.xy).xyz;
	half3 c02 = tex2D(s_l, VAR.t2.zw).xyz;
	half3 s00 = tex2D(s_l, VAR.t3.xy).xyz;
	half3 s20 = tex2D(s_l, VAR.t3.zw).xyz;
	half3 s22 = tex2D(s_l, VAR.t4.xy).xyz;
	half3 s02 = tex2D(s_l, VAR.t4.zw).xyz;
	half3 c01 = tex2D(s_l, VAR.t5.xy).xyz;
	half3 c21 = tex2D(s_l, VAR.t5.zw).xyz;
	half3 c10 = tex2D(s_l, VAR.t6.xy).xyz;
	half3 c12 = tex2D(s_l, VAR.t6.zw).xyz;

	float d1=dot(abs(c00-c22),dt)+0.0001;
	float d2=dot(abs(c20-c02),dt)+0.0001;
	float hl=dot(abs(c01-c21),dt)+0.0001;
	float vl=dot(abs(c10-c12),dt)+0.0001;
	float m1=dot(abs(s00-s22),dt)+0.0001;
	float m2=dot(abs(s02-s20),dt)+0.0001;

	float3 mn1 = min (min (c00,c01), c02);
	float3 mn2 = min (min (c10,c11), c12);
	float3 mn3 = min (min (c20,c21), c22);
	float3 mx1 = max (max (c00,c01), c02);
	float3 mx2 = max (max (c10,c11), c12);
	float3 mx3 = max (max (c20,c21), c22);

	mn1 = min(min(mn1,mn2),mn3);
	mx1 = max(max(mx1,mx2),mx3);

	float3 t1=(hl*(c10+c12)+vl*(c01+c21)+(hl+vl)*c11)/(3.0*(hl+vl));
	float3 t2=(d1*(c20+c02)+d2*(c00+c22)+(d1+d2)*c11)/(3.0*(d1+d2));

	c11 = 0.25*(t1+t2+(m2*(s00+s22)+m1*(s02+s20))/(m1+m2));

	float3 dif1 = abs(c11-mn1) + dtt;
	float3 dif2 = abs(c11-mx1) + dtt;

	//float filterparam = 5.5;

	float3 dif = mx1-mn1;	
	float filterparam = clamp(3.0*length(dif),1.0,2.5);
  
	dif1=float3(pow(dif1.x,filterparam),pow(dif1.y,filterparam),pow(dif1.z,filterparam));
	dif2=float3(pow(dif2.x,filterparam),pow(dif2.y,filterparam),pow(dif2.z,filterparam));

	c11 = float3((dif1.x*mx1.x + dif2.x*mn1.x)/(dif1.x + dif2.x),
                     (dif1.y*mx1.y + dif2.y*mn1.y)/(dif1.y + dif2.y),
                     (dif1.z*mx1.z + dif2.z*mn1.z)/(dif1.z + dif2.z));

	return float4(c11,1);
}


technique soft
{
    pass P0
    {
	VertexShader = compile vs_3_0 PASS1_VERTEX();
	PixelShader  = compile ps_3_0 PASS1_FRAGMENT();
    }  
}
