/*
   Hyllian's 5xBR v3.8b Shader
   
   Copyright (C) 2011 Hyllian/Jararaca - sergiogdb@gmail.com

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

// The name of this effect
string name : NAME = "FiveXBR3";

float2 ps                       : TEXELSIZE;

float4x4 World                  : WORLD;
float4x4 View                   : VIEW;
float4x4 Projection             : PROJECTION;
float4x4 Worldview              : WORLDVIEW;               // world * view
float4x4 ViewProjection         : VIEWPROJECTION;          // view * projection
float4x4 WorldViewProjection    : WORLDVIEWPROJECTION;     // world * view * projection

string combineTechique          : COMBINETECHNIQUE = "FiveTimesBR3";

texture SourceTexture	        : SOURCETEXTURE;
texture WorkingTexture          : WORKINGTEXTURE;

sampler	decal = sampler_state
{
	Texture	  = (SourceTexture);
	MinFilter = POINT;
	MagFilter = POINT;
};


const static float coef            = 2.0;
const static float4 eq_threshold   = float4(15.0, 15.0, 15.0, 15.0);
const static float y_weight        = 48.0;
const static float u_weight        = 7.0;
const static float v_weight        = 6.0;
const static float3x3 yuv          = float3x3(0.299, 0.587, 0.114, -0.169, -0.331, 0.499, 0.499, -0.418, -0.0813);
const static float3x3 yuv_weighted = float3x3(y_weight*yuv[0], u_weight*yuv[1], v_weight*yuv[2]);
const static float4 delta          = float4(0.2, 0.2, 0.2, 0.2);


float4 df(float4 A, float4 B)
{
	return float4(abs(A-B));
}

bool4 eq(float4 A, float4 B)
{
	return (df(A, B) < eq_threshold);
}

float4 weighted_distance(float4 a, float4 b, float4 c, float4 d, float4 e, float4 f, float4 g, float4 h)
{
	return (df(a,b) + df(a,c) + df(d,e) + df(d,f) + 4.0*df(g,h));
}


struct out_vertex
{
	float4 position : POSITION;
	float2 texCoord : TEXCOORD0;
	float4 t1       : TEXCOORD1;
};

 

 
//
// Vertex Shader
//

out_vertex  VS_VERTEX(float3 position : POSITION, float2 texCoord : TEXCOORD0 )
{
	 
	out_vertex OUT;

	OUT.position = mul(float4(position,1),WorldViewProjection);

	float dx = ps.x;
	float dy = ps.y;

	OUT.texCoord = texCoord;
	OUT.t1.xy = float2( dx,  0); // F
	OUT.t1.zw = float2(  0, dy); // H

	return OUT;

 
}


float4 PS_FRAGMENT (in out_vertex VAR) : COLOR
{	
	bool4 edr, edr_left, edr_up, px; // px = pixel, edr = edge detection rule
	bool4 interp_restriction_lv1, interp_restriction_lv2_left, interp_restriction_lv2_up;
	bool4 nc, nc30, nc60, nc45; // new_color
	float4 fx, fx_left, fx_up;
   
	float2 fp = frac(VAR.texCoord/ps);

	float2 dx = VAR.t1.xy;
	float2 dy = VAR.t1.zw;

	float3 A = tex2D(decal, VAR.texCoord -dx -dy).xyz;
	float3 B = tex2D(decal, VAR.texCoord     -dy).xyz;
	float3 C = tex2D(decal, VAR.texCoord +dx -dy).xyz;
	float3 D = tex2D(decal, VAR.texCoord -dx    ).xyz;
	float3 E = tex2D(decal, VAR.texCoord        ).xyz;
	float3 F = tex2D(decal, VAR.texCoord +dx    ).xyz;
	float3 G = tex2D(decal, VAR.texCoord -dx +dy).xyz;
	float3 H = tex2D(decal, VAR.texCoord     +dy).xyz;
	float3 I = tex2D(decal, VAR.texCoord +dx +dy).xyz;

	float3  A1 = tex2D(decal, VAR.texCoord     -dx -2.0*dy).xyz;
	float3  C1 = tex2D(decal, VAR.texCoord     +dx -2.0*dy).xyz;
	float3  A0 = tex2D(decal, VAR.texCoord -2.0*dx     -dy).xyz;
	float3  G0 = tex2D(decal, VAR.texCoord -2.0*dx     +dy).xyz;
	float3  C4 = tex2D(decal, VAR.texCoord +2.0*dx     -dy).xyz;
	float3  I4 = tex2D(decal, VAR.texCoord +2.0*dx     +dy).xyz;
	float3  G5 = tex2D(decal, VAR.texCoord     -dx +2.0*dy).xyz;
	float3  I5 = tex2D(decal, VAR.texCoord     +dx +2.0*dy).xyz;
	float3  B1 = tex2D(decal, VAR.texCoord         -2.0*dy).xyz;
	float3  D0 = tex2D(decal, VAR.texCoord -2.0*dx        ).xyz;
	float3  H5 = tex2D(decal, VAR.texCoord         +2.0*dy).xyz;
	float3  F4 = tex2D(decal, VAR.texCoord +2.0*dx        ).xyz;

	float4 b = mul( float4x3(B, D, H, F), yuv_weighted[0] );
	float4 c = mul( float4x3(C, A, G, I), yuv_weighted[0] );
	float4 e = mul( float4x3(E, E, E, E), yuv_weighted[0] );
	float4 d = b.yzwx;
	float4 f = b.wxyz;
	float4 g = c.zwxy;
	float4 h = b.zwxy;
	float4 i = c.wxyz;

	float4 i4 = mul( float4x3(I4, C1, A0, G5), yuv_weighted[0] );
	float4 i5 = mul( float4x3(I5, C4, A1, G0), yuv_weighted[0] );
	float4 h5 = mul( float4x3(H5, F4, B1, D0), yuv_weighted[0] );
	float4 f4 = h5.yzwx;

	float4 Ao = float4( 1.0, -1.0, -1.0, 1.0 );
	float4 Bo = float4( 1.0,  1.0, -1.0,-1.0 );
	float4 Co = float4( 1.5,  0.5, -0.5, 0.5 );
	float4 Ax = float4( 1.0, -1.0, -1.0, 1.0 );
	float4 Bx = float4( 0.5,  2.0, -0.5,-2.0 );
	float4 Cx = float4( 1.0,  1.0, -0.5, 0.0 );
	float4 Ay = float4( 1.0, -1.0, -1.0, 1.0 );
	float4 By = float4( 2.0,  0.5, -2.0,-0.5 );
	float4 Cy = float4( 2.0,  0.0, -1.0, 0.5 );

	fx      = Ao * fp.y + Bo * fp.x; 
	fx_left = Ax * fp.y + Bx * fp.x;
	fx_up   = Ay * fp.y + By * fp.x;

	interp_restriction_lv1      = ((e!=f) && (e!=h) &&  ( !eq(f,b) && !eq(h,d) || eq(e,i) && !eq(f,i4) && !eq(h,i5) || eq(e,g) || eq(e,c) ) );
	interp_restriction_lv2_left = ((e!=g) && (d!=g));
	interp_restriction_lv2_up   = ((e!=c) && (b!=c));

	float4 fx45 = smoothstep(Co - delta, Co + delta, fx);
	float4 fx30 = smoothstep(Cx - delta, Cx + delta, fx_left);
	float4 fx60 = smoothstep(Cy - delta, Cy + delta, fx_up);

	edr      = (weighted_distance( e, c, g, i, h5, f4, h, f) < weighted_distance( h, d, i5, f, i4, b, e, i)) && interp_restriction_lv1;
	edr_left = ((coef*df(f,g)) <= df(h,c)) && interp_restriction_lv2_left;
	edr_up   = (df(f,g) >= (coef*df(h,c))) && interp_restriction_lv2_up;

	nc45 = ( edr &&             bool4(fx45) );
	nc30 = ( edr && edr_left && bool4(fx30) );
	nc60 = ( edr && edr_up   && bool4(fx60) );

	px = (df(e,f) <= df(e,h));

	nc = (nc30 || nc60 || nc45);

	float blend = 0.0;
	half3 pix = E;

	float4 final45 = dot(float4(nc45), fx45);
	float4 final30 = dot(float4(nc30), fx30);
	float4 final60 = dot(float4(nc60), fx60);

	float4 maximo = max(max(final30, final60), final45);

	     if (nc.x) {pix = px.x ? F : H; blend = maximo.x;}
	else if (nc.y) {pix = px.y ? B : F; blend = maximo.y;}
	else if (nc.z) {pix = px.z ? D : B; blend = maximo.z;}
	else if (nc.w) {pix = px.w ? H : D; blend = maximo.w;}

	half3 res = lerp(E, pix, blend);

	return float4(res, 1.0);

}


//
// Technique
//

technique FiveTimesBR3
{
    pass P0
    {
        // shaders		
        VertexShader = compile vs_3_0 VS_VERTEX();
        PixelShader  = compile ps_3_0 PS_FRAGMENT(); 
    }  
}

