#version 120

/*
   Hyllian's xBR-lv2 Shader

   Copyright (C) 2011-2015 Hyllian - sergiogdb@gmail.com

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to permit
   persons to whom the Software is furnished to do so, subject to the
   following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

   Incorporates some of the ideas from SABR shader. Thanks to Joshua Street.
*/

#pragma parameter XBR_Y_WEIGHT "Y Weight" 48.0 0.0 100.0 1.0
#pragma parameter XBR_EQ_THRESHOLD "Eq Threshold" 15.0 0.0 50.0 1.0
#pragma parameter XBR_LV1_COEFFICIENT "Lv1 Coefficient" 0.5 0.0 30.0 0.5
#pragma parameter XBR_LV2_COEFFICIENT "Lv2 Coefficient" 2.0 1.0 3.0 0.1
#pragma parameter XBR_RES "Internal Res Multiplier" 2.0 1.0 8.0 1.0
#pragma parameter XBR_SCALE "xBR Scale" 3.0 1.0 5.0 1.0

#define mul(a,b) (b*a)
#define saturate(c) clamp(c, 0.0, 1.0)

#if defined(VERTEX)

#if __VERSION__ >= 130
#define COMPAT_VARYING out
#define COMPAT_ATTRIBUTE in
#define COMPAT_TEXTURE texture
#else
#define COMPAT_VARYING varying
#define COMPAT_ATTRIBUTE attribute
#define COMPAT_TEXTURE texture2D
#endif

#ifdef GL_ES
#define COMPAT_PRECISION mediump
#else
#define COMPAT_PRECISION
#endif

COMPAT_ATTRIBUTE vec4 VertexCoord;
COMPAT_ATTRIBUTE vec4 TexCoord;

COMPAT_ATTRIBUTE vec4 a_position;
COMPAT_VARYING vec2 v_texCoord;
COMPAT_VARYING vec4 t1;

vec4 _oPosition1;
uniform mat4 MVPMatrix;
uniform COMPAT_PRECISION vec2 rubyOutputSize;
uniform COMPAT_PRECISION vec2 rubyTextureSize;
uniform COMPAT_PRECISION vec2 rubyInputSize;

// compatibility #defines
#define vTexCoord v_texCoord
#define SourceSize vec4(rubyTextureSize, 1.0 / rubyTextureSize) //either TextureSize or InputSize
#define OutSize vec4(rubyOutputSize, 1.0 / rubyOutputSize)

#ifdef PARAMETER_UNIFORM
uniform COMPAT_PRECISION float XBR_Y_WEIGHT;
uniform COMPAT_PRECISION float XBR_EQ_THRESHOLD;
uniform COMPAT_PRECISION float XBR_LV1_COEFFICIENT;
uniform COMPAT_PRECISION float XBR_LV2_COEFFICIENT;
uniform COMPAT_PRECISION float XBR_RES;
uniform COMPAT_PRECISION float XBR_SCALE;
#else
#define XBR_Y_WEIGHT 48.0
#define XBR_EQ_THRESHOLD 15.0
#define XBR_LV1_COEFFICIENT 0.5
#define XBR_LV2_COEFFICIENT 2.0
#define XBR_RES 2.0
#define XBR_SCALE 3.0
#endif

void main()
{
	gl_Position = a_position;
	v_texCoord.xy = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0 * rubyInputSize / rubyTextureSize;
	vec2 ps = XBR_RES/SourceSize.xy;
	float dx = ps.x;
	float dy = ps.y;

	t1 = vec4( dx, 0, 0, dy); // F H
}

#elif defined(FRAGMENT)

#if __VERSION__ >= 130
#define COMPAT_VARYING in
#define COMPAT_TEXTURE texture
out vec4 FragColor;
#else
#define COMPAT_VARYING varying
#define FragColor gl_FragColor
#define COMPAT_TEXTURE texture2D
#endif

#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#define COMPAT_PRECISION mediump
#else
#define COMPAT_PRECISION
#endif

uniform COMPAT_PRECISION vec2 rubyOutputSize;
uniform COMPAT_PRECISION vec2 rubyTextureSize;
uniform COMPAT_PRECISION vec2 rubyInputSize;
uniform sampler2D rubyTexture;
COMPAT_VARYING vec2 v_texCoord;
COMPAT_VARYING vec4 t1;

// compatibility #defines
#define Source rubyTexture
#define vTexCoord v_texCoord.xy

#define SourceSize vec4(rubyTextureSize, 1.0 / rubyTextureSize) //either TextureSize or InputSize
#define OutSize vec4(rubyOutputSize, 1.0 / rubyOutputSize)

#ifdef PARAMETER_UNIFORM
uniform COMPAT_PRECISION float XBR_Y_WEIGHT;
uniform COMPAT_PRECISION float XBR_EQ_THRESHOLD;
uniform COMPAT_PRECISION float XBR_LV1_COEFFICIENT;
uniform COMPAT_PRECISION float XBR_LV2_COEFFICIENT;
uniform COMPAT_PRECISION float XBR_RES;
uniform COMPAT_PRECISION float XBR_SCALE;
#else
#define XBR_Y_WEIGHT 48.0
#define XBR_EQ_THRESHOLD 15.0
#define XBR_LV1_COEFFICIENT 0.5
#define XBR_LV2_COEFFICIENT 2.0
#define XBR_RES 2.0
#define XBR_SCALE 3.0
#endif

// Uncomment just one of the three params below to choose the corner detection
//#define CORNER_A
//#define CORNER_B
#define CORNER_C
//#define CORNER_D

#ifndef CORNER_A
	#define SMOOTH_TIPS
#endif

#define lv2_cf XBR_LV2_COEFFICIENT

const   float coef          = 2.0;
const   vec3  rgbw          = vec3(14.352, 28.176, 5.472);
const   vec4  eq_threshold  = vec4(15.0, 15.0, 15.0, 15.0);

vec4 delta   = vec4(1.0/XBR_SCALE, 1.0/XBR_SCALE, 1.0/XBR_SCALE, 1.0/XBR_SCALE);
vec4 delta_l = vec4(0.5/XBR_SCALE, 1.0/XBR_SCALE, 0.5/XBR_SCALE, 1.0/XBR_SCALE);
vec4 delta_u = delta_l.yxwz;

const  vec4 Ao = vec4( 1.0, -1.0, -1.0, 1.0 );
const  vec4 Bo = vec4( 1.0,  1.0, -1.0,-1.0 );
const  vec4 Co = vec4( 1.5,  0.5, -0.5, 0.5 );
const  vec4 Ax = vec4( 1.0, -1.0, -1.0, 1.0 );
const  vec4 Bx = vec4( 0.5,  2.0, -0.5,-2.0 );
const  vec4 Cx = vec4( 1.0,  1.0, -0.5, 0.0 );
const  vec4 Ay = vec4( 1.0, -1.0, -1.0, 1.0 );
const  vec4 By = vec4( 2.0,  0.5, -2.0,-0.5 );
const  vec4 Cy = vec4( 2.0,  0.0, -1.0, 0.5 );
const  vec4 Ci = vec4(0.25, 0.25, 0.25, 0.25);

const vec3 Y = vec3(0.2126, 0.7152, 0.0722);

// Difference between vector components.
vec4 df(vec4 A, vec4 B)
{
	return vec4(abs(A-B));
}

// Determine if two vector components are equal based on a threshold.
bvec4 eq(vec4 A, vec4 B)
{
	return (lessThan(df(A, B), vec4(XBR_EQ_THRESHOLD)));
}

vec4 weighted_distance(vec4 a, vec4 b, vec4 c, vec4 d, vec4 e, vec4 f, vec4 g, vec4 h)
{
	return (df(a,b) + df(a,c) + df(d,e) + df(d,f) + 4.0*df(g,h));
}

float c_df(vec3 c1, vec3 c2)
{
	vec3 df = abs(c1 - c2);
	return df.r + df.g + df.b;
}

void main()
{
	bvec4 edri, edr, edr_left, edr_up, px; // px = pixel, edr = edge detection rule
	bvec4 interp_restriction_lv0, interp_restriction_lv1, interp_restriction_lv2_left, interp_restriction_lv2_up, block_3d, block_3d_left, block_3d_up;
	vec4 fx, fx_left, fx_up; // inequations of straight lines.

	vec4 delta         = vec4(1.0/XBR_SCALE, 1.0/XBR_SCALE, 1.0/XBR_SCALE, 1.0/XBR_SCALE);
	vec4 deltaL        = vec4(0.5/XBR_SCALE, 1.0/XBR_SCALE, 0.5/XBR_SCALE, 1.0/XBR_SCALE);
	vec4 deltaU        = deltaL.yxwz;

	vec2 fp = fract(vTexCoord*SourceSize.xy/XBR_RES);

	vec2 tex = (floor(vTexCoord*SourceSize.xy/XBR_RES) + vec2(0.5, 0.5))*XBR_RES/SourceSize.xy;

	vec2 dx = t1.xy;
	vec2 dy = t1.zw;

	vec3 A = COMPAT_TEXTURE(Source, vTexCoord -dx -dy).xyz;
	vec3 B = COMPAT_TEXTURE(Source, vTexCoord     -dy).xyz;
	vec3 C = COMPAT_TEXTURE(Source, vTexCoord +dx -dy).xyz;
	vec3 D = COMPAT_TEXTURE(Source, vTexCoord -dx    ).xyz;
	vec3 E = COMPAT_TEXTURE(Source, vTexCoord        ).xyz;
	vec3 F = COMPAT_TEXTURE(Source, vTexCoord +dx    ).xyz;
	vec3 G = COMPAT_TEXTURE(Source, vTexCoord -dx +dy).xyz;
	vec3 H = COMPAT_TEXTURE(Source, vTexCoord     +dy).xyz;
	vec3 I = COMPAT_TEXTURE(Source, vTexCoord +dx +dy).xyz;

	vec3  A1 = COMPAT_TEXTURE(Source, vTexCoord     -dx -2.0*dy).xyz;
	vec3  B1 = COMPAT_TEXTURE(Source, vTexCoord         -2.0*dy).xyz;
	vec3  C1 = COMPAT_TEXTURE(Source, vTexCoord     +dx -2.0*dy).xyz;
	vec3  G5 = COMPAT_TEXTURE(Source, vTexCoord     -dx +2.0*dy).xyz;
	vec3  H5 = COMPAT_TEXTURE(Source, vTexCoord         +2.0*dy).xyz;
	vec3  I5 = COMPAT_TEXTURE(Source, vTexCoord     +dx +2.0*dy).xyz;
	vec3  A0 = COMPAT_TEXTURE(Source, vTexCoord -2.0*dx     -dy).xyz;
	vec3  D0 = COMPAT_TEXTURE(Source, vTexCoord -2.0*dx        ).xyz;
	vec3  G0 = COMPAT_TEXTURE(Source, vTexCoord -2.0*dx     +dy).xyz;
	vec3  C4 = COMPAT_TEXTURE(Source, vTexCoord +2.0*dx     -dy).xyz;
	vec3  F4 = COMPAT_TEXTURE(Source, vTexCoord +2.0*dx        ).xyz;
	vec3  I4 = COMPAT_TEXTURE(Source, vTexCoord +2.0*dx     +dy).xyz;

	vec3 F6 = COMPAT_TEXTURE(Source,  tex +dx+0.25*dx+0.25*dy).xyz;
	vec3 F7 = COMPAT_TEXTURE(Source,  tex +dx+0.25*dx-0.25*dy).xyz;
	vec3 F8 = COMPAT_TEXTURE(Source,  tex +dx-0.25*dx-0.25*dy).xyz;
	vec3 F9 = COMPAT_TEXTURE(Source,  tex +dx-0.25*dx+0.25*dy).xyz;

	vec3 B6 = COMPAT_TEXTURE(Source,  tex +0.25*dx+0.25*dy-dy).xyz;
	vec3 B7 = COMPAT_TEXTURE(Source,  tex +0.25*dx-0.25*dy-dy).xyz;
	vec3 B8 = COMPAT_TEXTURE(Source,  tex -0.25*dx-0.25*dy-dy).xyz;
	vec3 B9 = COMPAT_TEXTURE(Source,  tex -0.25*dx+0.25*dy-dy).xyz;

	vec3 D6 = COMPAT_TEXTURE(Source,  tex -dx+0.25*dx+0.25*dy).xyz;
	vec3 D7 = COMPAT_TEXTURE(Source,  tex -dx+0.25*dx-0.25*dy).xyz;
	vec3 D8 = COMPAT_TEXTURE(Source,  tex -dx-0.25*dx-0.25*dy).xyz;
	vec3 D9 = COMPAT_TEXTURE(Source,  tex -dx-0.25*dx+0.25*dy).xyz;

	vec3 H6 = COMPAT_TEXTURE(Source,  tex +0.25*dx+0.25*dy+dy).xyz;
	vec3 H7 = COMPAT_TEXTURE(Source,  tex +0.25*dx-0.25*dy+dy).xyz;
	vec3 H8 = COMPAT_TEXTURE(Source,  tex -0.25*dx-0.25*dy+dy).xyz;
	vec3 H9 = COMPAT_TEXTURE(Source,  tex -0.25*dx+0.25*dy+dy).xyz;

	float y_weight = XBR_Y_WEIGHT;

	vec4 b = mul( mat4x3(B, D, H, F), y_weight*Y );
	vec4 c = mul( mat4x3(C, A, G, I), y_weight*Y );
	vec4 e = mul( mat4x3(E, E, E, E), y_weight*Y );
	vec4 d = b.yzwx;
	vec4 f = b.wxyz;
	vec4 g = c.zwxy;
	vec4 h = b.zwxy;
	vec4 i = c.wxyz;

	vec4 i4 = mul( mat4x3(I4, C1, A0, G5), y_weight*Y );
	vec4 i5 = mul( mat4x3(I5, C4, A1, G0), y_weight*Y );
	vec4 h5 = mul( mat4x3(H5, F4, B1, D0), y_weight*Y );
	vec4 f4 = h5.yzwx;

	vec4 f0 = mul( mat4x3(F6, B6, D6, H6), y_weight*Y );
	vec4 f1 = mul( mat4x3(F7, B7, D7, H7), y_weight*Y );
	vec4 f2 = mul( mat4x3(F8, B8, D8, H8), y_weight*Y );
	vec4 f3 = mul( mat4x3(F9, B9, D9, H9), y_weight*Y );

	vec4 h0 = f0.wxyz;
	vec4 h1 = f1.wxyz;
	vec4 h2 = f2.wxyz;
	vec4 h3 = f3.wxyz;

	// These inequations define the line below which interpolation occurs.
	fx      = (Ao*fp.y+Bo*fp.x);
	fx_left = (Ax*fp.y+Bx*fp.x);
	fx_up   = (Ay*fp.y+By*fp.x);

	block_3d.x    =  ((f0.x==f1.x && f1.x==f2.x && f2.x==f3.x) && (h0.x==h1.x && h1.x==h2.x && h2.x==h3.x));
	block_3d.y    =  ((f0.y==f1.y && f1.y==f2.y && f2.y==f3.y) && (h0.y==h1.y && h1.y==h2.y && h2.y==h3.y));
	block_3d.z    =  ((f0.z==f1.z && f1.z==f2.z && f2.z==f3.z) && (h0.z==h1.z && h1.z==h2.z && h2.z==h3.z));
	block_3d.w    =  ((f0.w==f1.w && f1.w==f2.w && f2.w==f3.w) && (h0.w==h1.w && h1.w==h2.w && h2.w==h3.w));
	interp_restriction_lv1.x = interp_restriction_lv0.x = ((e.x!=f.x) && (e.x!=h.x) && block_3d.x);
	interp_restriction_lv1.y = interp_restriction_lv0.y = ((e.y!=f.y) && (e.y!=h.y) && block_3d.y);
	interp_restriction_lv1.z = interp_restriction_lv0.z = ((e.z!=f.z) && (e.z!=h.z) && block_3d.z);
	interp_restriction_lv1.w = interp_restriction_lv0.w = ((e.w!=f.w) && (e.w!=h.w) && block_3d.w);

#ifdef CORNER_B
	interp_restriction_lv1      = (interp_restriction_lv0  &&  ( !eq(f,b) && !eq(h,d) || eq(e,i) && !eq(f,i4) && !eq(h,i5) || eq(e,g) || eq(e,c) ) );
#endif
#ifdef CORNER_D
	vec4 c1 = i4.yzwx;
	vec4 g0 = i5.wxyz;
	interp_restriction_lv1      = (interp_restriction_lv0  &&  ( !eq(f,b) && !eq(h,d) || eq(e,i) && !eq(f,i4) && !eq(h,i5) || eq(e,g) || eq(e,c) ) && (f!=f4 && f!=i || h!=h5 && h!=i || h!=g || f!=c || eq(b,c1) && eq(d,g0)));
#endif
#ifdef CORNER_C
	interp_restriction_lv1.x    = (interp_restriction_lv0.x  && ( !eq(f,b).x && !eq(f,c).x || !eq(h,d).x && !eq(h,g).x || eq(e,i).x && (!eq(f,f4).x && !eq(f,i4).x || !eq(h,h5).x && !eq(h,i5).x) || eq(e,g).x || eq(e,c).x) );
	interp_restriction_lv1.y    = (interp_restriction_lv0.y  && ( !eq(f,b).y && !eq(f,c).y || !eq(h,d).y && !eq(h,g).y || eq(e,i).y && (!eq(f,f4).y && !eq(f,i4).y || !eq(h,h5).y && !eq(h,i5).y) || eq(e,g).y || eq(e,c).y) );
	interp_restriction_lv1.z    = (interp_restriction_lv0.z  && ( !eq(f,b).z && !eq(f,c).z || !eq(h,d).z && !eq(h,g).z || eq(e,i).z && (!eq(f,f4).z && !eq(f,i4).z || !eq(h,h5).z && !eq(h,i5).z) || eq(e,g).z || eq(e,c).z) );
	interp_restriction_lv1.w    = (interp_restriction_lv0.w  && ( !eq(f,b).w && !eq(f,c).w || !eq(h,d).w && !eq(h,g).w || eq(e,i).w && (!eq(f,f4).w && !eq(f,i4).w || !eq(h,h5).w && !eq(h,i5).w) || eq(e,g).w || eq(e,c).w) );
#endif

	interp_restriction_lv2_left.x = ((e.x!=g.x) && (d.x!=g.x));
	interp_restriction_lv2_left.y = ((e.y!=g.y) && (d.y!=g.y));
	interp_restriction_lv2_left.z = ((e.z!=g.z) && (d.z!=g.z));
	interp_restriction_lv2_left.w = ((e.w!=g.w) && (d.w!=g.w));
	interp_restriction_lv2_up.x   = ((e.x!=c.x) && (b.x!=c.x));
	interp_restriction_lv2_up.y   = ((e.y!=c.y) && (b.y!=c.y));
	interp_restriction_lv2_up.z   = ((e.z!=c.z) && (b.z!=c.z));
	interp_restriction_lv2_up.w   = ((e.w!=c.w) && (b.w!=c.w));

	vec4 fx45i = saturate((fx      + delta  -Co - Ci)/(2*delta ));
	vec4 fx45  = saturate((fx      + delta  -Co     )/(2*delta ));
	vec4 fx30  = saturate((fx_left + deltaL -Cx     )/(2*deltaL));
	vec4 fx60  = saturate((fx_up   + deltaU -Cy     )/(2*deltaU));

	vec4 wd1 = weighted_distance( e, c, g, i, h5, f4, h, f);
	vec4 wd2 = weighted_distance( h, d, i5, f, i4, b, e, i);

    edri.x     = (wd1.x <= wd2.x) && interp_restriction_lv0.x;
	edri.y     = (wd1.y <= wd2.y) && interp_restriction_lv0.y;
	edri.z     = (wd1.z <= wd2.z) && interp_restriction_lv0.z;
	edri.w     = (wd1.w <= wd2.w) && interp_restriction_lv0.w;
	edr.x      = (wd1.x <  wd2.x) && interp_restriction_lv1.x;
	edr.y      = (wd1.y <  wd2.y) && interp_restriction_lv1.y;
	edr.z      = (wd1.z <  wd2.z) && interp_restriction_lv1.z;
	edr.w      = (wd1.w <  wd2.w) && interp_restriction_lv1.w;
	edr_left.x = ((XBR_LV2_COEFFICIENT*df(f,g).x) <= df(h,c).x) && interp_restriction_lv2_left.x && edr.x;
	edr_left.y = ((XBR_LV2_COEFFICIENT*df(f,g).y) <= df(h,c).y) && interp_restriction_lv2_left.y && edr.y;
	edr_left.z = ((XBR_LV2_COEFFICIENT*df(f,g).z) <= df(h,c).z) && interp_restriction_lv2_left.z && edr.z;
	edr_left.w = ((XBR_LV2_COEFFICIENT*df(f,g).w) <= df(h,c).w) && interp_restriction_lv2_left.w && edr.w;
	edr_up.x   = (df(f,g).x >= (XBR_LV2_COEFFICIENT*df(h,c).x)) && interp_restriction_lv2_up.x && edr.x;
	edr_up.y   = (df(f,g).y >= (XBR_LV2_COEFFICIENT*df(h,c).y)) && interp_restriction_lv2_up.y && edr.y;
	edr_up.z   = (df(f,g).z >= (XBR_LV2_COEFFICIENT*df(h,c).z)) && interp_restriction_lv2_up.z && edr.z;
	edr_up.w   = (df(f,g).w >= (XBR_LV2_COEFFICIENT*df(h,c).w)) && interp_restriction_lv2_up.w && edr.w;

	fx45  = vec4(edr)*fx45;
	fx30  = vec4(edr_left)*fx30;
	fx60  = vec4(edr_up)*fx60;
	fx45i = vec4(edri)*fx45i;

	px.x = (df(e,f3).x <= df(e,h1).x);
	px.y = (df(e,f3).y <= df(e,h1).y);
	px.z = (df(e,f3).z <= df(e,h1).z);
	px.w = (df(e,f3).w <= df(e,h1).w);

#ifdef SMOOTH_TIPS
	vec4 maximos = max(max(fx30, fx60), max(fx45, fx45i));
#else
	vec4 maximos = max(max(fx30, fx60), fx45);
#endif

	vec3 res1 = E;
	res1 = mix(res1, mix(H, F, float(px.x)), maximos.x);
	res1 = mix(res1, mix(B, D, float(px.z)), maximos.z);

	vec3 res2 = E;
	res2 = mix(res2, mix(F, B, float(px.y)), maximos.y);
	res2 = mix(res2, mix(D, H, float(px.w)), maximos.w);

	vec3 res = mix(res1, res2, step(c_df(E, res1), c_df(E, res2)));

	FragColor = vec4(res, 1.0);
}
#endif
