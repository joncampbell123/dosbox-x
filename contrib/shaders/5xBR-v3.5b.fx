/*
   Hyllian's 5xBR v3.5b Shader
   
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
float2 ps : TEXELSIZE;

float4x4 World			: WORLD;
float4x4 View			: VIEW;
float4x4 Projection		: PROJECTION;
float4x4 Worldview		: WORLDVIEW;			// world * view
float4x4 ViewProjection		: VIEWPROJECTION;		// view * projection
float4x4 WorldViewProjection	: WORLDVIEWPROJECTION;		// world * view * projection

string combineTechique		: COMBINETECHNIQUE = "FiveTimesBR3";

texture SourceTexture		: SOURCETEXTURE;
texture WorkingTexture		: WORKINGTEXTURE;

sampler	decal = sampler_state
{
	Texture	  = (SourceTexture);
	MinFilter = POINT;
	MagFilter = POINT;
};


const static float coef            = 2.0;
const static float3 rgb_W          = float3(16.163, 23.351, 8.4772);

const static float4 Ao = float4( 1.0, -1.0, -1.0, 1.0 );
const static float4 Bo = float4( 1.0,  1.0, -1.0,-1.0 );
const static float4 Co = float4( 1.5,  0.5, -0.5, 0.5 );
const static float4 Ax = float4( 1.0, -1.0, -1.0, 1.0 );
const static float4 Bx = float4( 0.5,  2.0, -0.5,-2.0 );
const static float4 Cx = float4( 1.0,  1.0, -0.5, 0.0 );
const static float4 Ay = float4( 1.0, -1.0, -1.0, 1.0 );
const static float4 By = float4( 2.0,  0.5, -2.0,-0.5 );
const static float4 Cy = float4( 2.0,  0.0, -1.0, 0.5 );

float4 WeightRGB(float4x3 mat_color)
{
	float a= dot(rgb_W, mat_color[0]);
	float b= dot(rgb_W, mat_color[1]);
	float c= dot(rgb_W, mat_color[2]);
	float d= dot(rgb_W, mat_color[3]);

	return float4(a, b, c, d);
}

float4 df(float4 A, float4 B)
{
	return float4(abs(A-B));
}


float4 weighted_distance(float4 a, float4 b, float4 c, float4 d, float4 e, float4 f, float4 g, float4 h)
{
	return (df(a,b) + df(a,c) + df(d,e) + df(d,f) + 4.0*df(g,h));
}


struct out_vertex
{
	float4 position : POSITION;
	float2 texCoord : TEXCOORD0;
	float4 t1 : TEXCOORD1;
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
	bool4 nc; // new_color
	bool4 fx, fx_left, fx_up; // inequations of straight lines.
   
	float2 fp = frac(VAR.texCoord/ps);

	half2 dx = VAR.t1.xy; half2 x2 = dx + dx;
	half2 dy = VAR.t1.zw; half2 y2 = dy + dy;

	float3 A  = tex2D(decal, VAR.texCoord -dx -dy    ).xyz;
	float3 B  = tex2D(decal, VAR.texCoord     -dy    ).xyz;
	float3 C  = tex2D(decal, VAR.texCoord +dx -dy    ).xyz;
	float3 D  = tex2D(decal, VAR.texCoord -dx        ).xyz;
	float3 E  = tex2D(decal, VAR.texCoord            ).xyz;
	float3 F  = tex2D(decal, VAR.texCoord +dx        ).xyz;
	float3 G  = tex2D(decal, VAR.texCoord -dx +dy    ).xyz;
	float3 H  = tex2D(decal, VAR.texCoord     +dy    ).xyz;
	float3 I  = tex2D(decal, VAR.texCoord +dx +dy    ).xyz;
	float3 A1 = tex2D(decal, VAR.texCoord     -dx -y2).xyz;
	float3 C1 = tex2D(decal, VAR.texCoord     +dx -y2).xyz;
	float3 A0 = tex2D(decal, VAR.texCoord -x2     -dy).xyz;
	float3 G0 = tex2D(decal, VAR.texCoord -x2     +dy).xyz;
	float3 C4 = tex2D(decal, VAR.texCoord +x2     -dy).xyz;
	float3 I4 = tex2D(decal, VAR.texCoord +x2     +dy).xyz;
	float3 G5 = tex2D(decal, VAR.texCoord     -dx +y2).xyz;
	float3 I5 = tex2D(decal, VAR.texCoord     +dx +y2).xyz;
	float3 B1 = tex2D(decal, VAR.texCoord         -y2).xyz;
	float3 D0 = tex2D(decal, VAR.texCoord -x2        ).xyz;
	float3 H5 = tex2D(decal, VAR.texCoord         +y2).xyz;
	float3 F4 = tex2D(decal, VAR.texCoord +x2        ).xyz;

	float4 b  = WeightRGB( float4x3(B , D , H , F ) );
	float4 c  = WeightRGB( float4x3(C , A , G , I ) );
	float4 d  = WeightRGB( float4x3(D , H , F , B ) );
	float4 e  = WeightRGB( float4x3(E , E , E , E ) );
	float4 f  = WeightRGB( float4x3(F , B , D , H ) );
	float4 g  = WeightRGB( float4x3(G , I , C , A ) );
	float4 h  = WeightRGB( float4x3(H , F , B , D ) );
	float4 i  = WeightRGB( float4x3(I , C , A , G ) );
	float4 i4 = WeightRGB( float4x3(I4, C1, A0, G5) );
	float4 i5 = WeightRGB( float4x3(I5, C4, A1, G0) );
	float4 h5 = WeightRGB( float4x3(H5, F4, B1, D0) );
	float4 f4 = WeightRGB( float4x3(F4, B1, D0, H5) );

	// These inequations define the line below which interpolation occurs.
	fx      = (Ao*fp.y+Bo*fp.x > Co); 
	fx_left = (Ax*fp.y+Bx*fp.x > Cx);
	fx_up   = (Ay*fp.y+By*fp.x > Cy);

	interp_restriction_lv1      = ((e!=f) && (e!=h) && ( f!=b && h!=d || e==i && f!=i4 && h!=i5 || e==g || e==c ));
	interp_restriction_lv2_left = ((e!=g) && (d!=g));
	interp_restriction_lv2_up   = ((e!=c) && (b!=c));

	edr      = (weighted_distance( e, c, g, i, h5, f4, h, f) < weighted_distance( h, d, i5, f, i4, b, e, i)) && interp_restriction_lv1;
	edr_left = ((coef*df(f,g)) <= df(h,c)) && interp_restriction_lv2_left;
	edr_up   = (df(f,g) >= (coef*df(h,c))) && interp_restriction_lv2_up;

	nc = ( edr && (fx || edr_left && fx_left || edr_up && fx_up) );

	px = (df(e,f) <= df(e,h));

	float3 res = nc.x ? px.x ? F : H : nc.y ? px.y ? B : F : nc.z ? px.z ? D : B : nc.w ? px.w ? H : D : E;

	return float4(res, 1);
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

