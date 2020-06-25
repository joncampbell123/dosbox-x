/* ############################################################################################

   GS4XFilter shader - Copyright (C) 2005 guest(r) - guest.r@gmail.com

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

const float mx = 1.00;      // start smoothing wt.
const float k = -1.10;      // wt. decrease factor
const float max_w = 0.90;   // max filter weigth
const float min_w = 0.05;   // min filter weigth
const float lum_add = 0.33; // effects smoothing

string preprocessTechique : PREPROCESSTECHNIQUE = "GS4xFilter";
string combineTechique : COMBINETECHNIQUE =  "linear";


VERTEX_STUFF4 PASS1_VERTEX (float3 p : POSITION, float2 tc : TEXCOORD0)
{
  VERTEX_STUFF4 OUT = (VERTEX_STUFF4)0;
  
	float dx = ps.x*2.0;
	float dy = ps.y*2.0;
	float sx = ps.x;
	float sy = ps.y;

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
	half3 c  = tex2D(s_p, VAR.CT   ).xyz;
	half3 o1 = tex2D(s_p, VAR.t1.xy).xyz;
	half3 o2 = tex2D(s_p, VAR.t1.zw).xyz;
	half3 o3 = tex2D(s_p, VAR.t2.xy).xyz;
	half3 o4 = tex2D(s_p, VAR.t2.zw).xyz;
	half3 i1 = tex2D(s_p, VAR.t3.xy).xyz;
	half3 i2 = tex2D(s_p, VAR.t3.zw).xyz;
	half3 i3 = tex2D(s_p, VAR.t4.xy).xyz;
	half3 i4 = tex2D(s_p, VAR.t4.zw).xyz;
	half3 s1 = tex2D(s_p, VAR.t5.xy).xyz;
	half3 s2 = tex2D(s_p, VAR.t5.zw).xyz;
	half3 s3 = tex2D(s_p, VAR.t6.xy).xyz;
	half3 s4 = tex2D(s_p, VAR.t6.zw).xyz;

	half ko1=dot(abs(o1-c),dt);
	half ko2=dot(abs(o2-c),dt);
	half ko3=dot(abs(o3-c),dt);
	half ko4=dot(abs(o4-c),dt);

	half4 w;
	w.yw = min(dot(abs(i1-i3),dt),0.6*(ko1+ko3));
	w.xz = min(dot(abs(i2-i4),dt),0.6*(ko2+ko4));

	if (ko3<ko1) w.x*= ko3/ko1;
	if (ko4<ko2) w.y*= ko4/ko2;
	if (ko1<ko3) w.z*= ko1/ko3;
	if (ko2<ko4) w.w*= ko2/ko4;

	c = (w.x*o1+w.y*o2+w.z*o3+w.w*o4+0.001*c)/(dot(w,d4)+0.001);

	w.x = k*dot(abs(i1-c)+abs(i3-c),dt)/(0.125*dot(i1+i3,dt)+lum_add);
	w.y = k*dot(abs(i2-c)+abs(i4-c),dt)/(0.125*dot(i2+i4,dt)+lum_add);
	w.z = k*dot(abs(s1-c)+abs(s2-c),dt)/(0.125*dot(s1+s2,dt)+lum_add);
	w.w = k*dot(abs(s3-c)+abs(s4-c),dt)/(0.125*dot(s3+s4,dt)+lum_add);

	w.x = clamp(w.x+mx,min_w,max_w); 
	w.y = clamp(w.y+mx,min_w,max_w);
	w.z = clamp(w.z+mx,min_w,max_w); 
	w.w = clamp(w.w+mx,min_w,max_w);

	c = (w.x*(i1+i3)+w.y*(i2+i4)+w.z*(s1+s2)+w.w*(s3+s4)+c)/(2.0*dot(w,d4)+1.0);

	return float4(c,0);
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


technique GS4xFilter
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
