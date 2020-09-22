#version 120

/*
   Hyllian's xBR-lv3 Shader

   Copyright (C) 2011-2015 Hyllian - sergiogdb@gmail.com

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.


   Incorporates some of the ideas from SABR shader. Thanks to Joshua Street.
*/

// Parameter lines go here:
#pragma parameter XBR_Y_WEIGHT "Y Weight" 48.0 0.0 100.0 1.0
#pragma parameter XBR_EQ_THRESHOLD "EQ Threshold" 10.0 0.0 50.0 1.0
#pragma parameter XBR_EQ_THRESHOLD2 "EQ Threshold 2" 2.0 0.0 4.0 1.0
#pragma parameter XBR_LV2_COEFFICIENT "Lv2 Coefficient" 2.0 1.0 3.0 1.0
#pragma parameter corner_type "Corner Calculation" 3.0 1.0 3.0 1.0

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
COMPAT_VARYING vec4 t2;
COMPAT_VARYING vec4 t3;
COMPAT_VARYING vec4 t4;
COMPAT_VARYING vec4 t5;
COMPAT_VARYING vec4 t6;
COMPAT_VARYING vec4 t7;

vec4 _oPosition1;
uniform COMPAT_PRECISION vec2 rubyOutputSize;
uniform COMPAT_PRECISION vec2 rubyTextureSize;
uniform COMPAT_PRECISION vec2 rubyInputSize;

void main()
{
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0 * rubyInputSize / rubyTextureSize;
	vec2 ps = vec2(1.0) / rubyTextureSize.xy;
	float dx = ps.x;
	float dy = ps.y;

	//    A1 B1 C1
	// A0  A  B  C C4
	// D0  D  E  F F4
	// G0  G  H  I I4
	//    G5 H5 I5

	t1 = v_texCoord.xxxy + vec4( -dx, 0, dx,-2.0*dy); // A1 B1 C1
	t2 = v_texCoord.xxxy + vec4( -dx, 0, dx,    -dy); //  A  B  C
	t3 = v_texCoord.xxxy + vec4( -dx, 0, dx,      0); //  D  E  F
	t4 = v_texCoord.xxxy + vec4( -dx, 0, dx,     dy); //  G  H  I
	t5 = v_texCoord.xxxy + vec4( -dx, 0, dx, 2.0*dy); // G5 H5 I5
	t6 = v_texCoord.xyyy + vec4(-2.0*dx,-dy, 0,  dy); // A0 D0 G0
	t7 = v_texCoord.xyyy + vec4( 2.0*dx,-dy, 0,  dy); // C4 F4 I4
}

#elif defined(FRAGMENT)

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

#if __VERSION__ >= 130
#define COMPAT_VARYING in
#define COMPAT_TEXTURE texture
out COMPAT_PRECISION vec4 FragColor;
#else
#define COMPAT_VARYING varying
#define FragColor gl_FragColor
#define COMPAT_TEXTURE texture2D
#endif

uniform COMPAT_PRECISION vec2 rubyOutputSize;
uniform COMPAT_PRECISION vec2 rubyTextureSize;
uniform COMPAT_PRECISION vec2 rubyInputSize;
uniform sampler2D rubyTexture;
COMPAT_VARYING vec2 v_texCoord;
COMPAT_VARYING vec4 t1;
COMPAT_VARYING vec4 t2;
COMPAT_VARYING vec4 t3;
COMPAT_VARYING vec4 t4;
COMPAT_VARYING vec4 t5;
COMPAT_VARYING vec4 t6;
COMPAT_VARYING vec4 t7;

// compatibility #defines

#define Source rubyTexture
#define vTexCoord v_texCoord.xy

#define SourceSize vec4(rubyTextureSize, 1.0 / rubyTextureSize) //either TextureSize or InputSize
#define OutputSize vec4(rubyOutputSize, 1.0 / rubyOutputSize)

#ifdef PARAMETER_UNIFORM
uniform COMPAT_PRECISION float XBR_Y_WEIGHT;
uniform COMPAT_PRECISION float XBR_EQ_THRESHOLD;
uniform COMPAT_PRECISION float XBR_EQ_THRESHOLD2;
uniform COMPAT_PRECISION float XBR_LV2_COEFFICIENT;
uniform COMPAT_PRECISION float corner_type;
#else
#define XBR_Y_WEIGHT 48.0
#define XBR_EQ_THRESHOLD 10.0
#define XBR_EQ_THRESHOLD2 2.0
#define XBR_LV2_COEFFICIENT 2.0
#define corner_type 3.0
#endif

const mat3 yuv = mat3(0.299, 0.587, 0.114, -0.169, -0.331, 0.499, 0.499, -0.418, -0.0813);
const vec4 delta = vec4(0.4, 0.4, 0.4, 0.4);

vec4 df(vec4 A, vec4 B)
{
	return vec4(abs(A-B));
}

float c_df(vec3 c1, vec3 c2) {
	vec3 df = abs(c1 - c2);
	return df.r + df.g + df.b;
}

bvec4 eq(vec4 A, vec4 B)
{
	return lessThan(df(A, B), vec4(XBR_EQ_THRESHOLD));
}

bvec4 eq2(vec4 A, vec4 B)
{
	return lessThan(df(A, B), vec4(XBR_EQ_THRESHOLD2));
}

bvec4 and(bvec4 A, bvec4 B)
{
	return bvec4(A.x && B.x, A.y && B.y, A.z && B.z, A.w && B.w);
}

bvec4 or(bvec4 A, bvec4 B)
{
	return bvec4(A.x || B.x, A.y || B.y, A.z || B.z, A.w || B.w);
}

vec4 weighted_distance(vec4 a, vec4 b, vec4 c, vec4 d, vec4 e, vec4 f, vec4 g, vec4 h)
{
	return (df(a,b) + df(a,c) + df(d,e) + df(d,f) + 4.0*df(g,h));
}

void main()
{
	bvec4 edr, edr_left, edr_up, edr3_left, edr3_up, px; // px = pixel, edr = edge detection rule
	bvec4 interp_restriction_lv1, interp_restriction_lv2_left, interp_restriction_lv2_up;
	bvec4 interp_restriction_lv3_left, interp_restriction_lv3_up;
	bvec4 nc, nc30, nc60, nc45, nc15, nc75; // new_color
	vec4 fx, fx_left, fx_up, finalfx, fx3_left, fx3_up; // inequations of straight lines.
	vec3 res1, res2, pix1, pix2;
	float blend1, blend2;

	vec2 fp = fract(vTexCoord * SourceSize.xy);

	vec3 A1 = COMPAT_TEXTURE(rubyTexture, t1.xw).rgb;
	vec3 B1 = COMPAT_TEXTURE(rubyTexture, t1.yw).rgb;
	vec3 C1 = COMPAT_TEXTURE(rubyTexture, t1.zw).rgb;

	vec3 A = COMPAT_TEXTURE(rubyTexture, t2.xw).rgb;
	vec3 B = COMPAT_TEXTURE(rubyTexture, t2.yw).rgb;
	vec3 C = COMPAT_TEXTURE(rubyTexture, t2.zw).rgb;

	vec3 D = COMPAT_TEXTURE(rubyTexture, t3.xw).rgb;
	vec3 E = COMPAT_TEXTURE(rubyTexture, t3.yw).rgb;
	vec3 F = COMPAT_TEXTURE(rubyTexture, t3.zw).rgb;

	vec3 G = COMPAT_TEXTURE(rubyTexture, t4.xw).rgb;
	vec3 H = COMPAT_TEXTURE(rubyTexture, t4.yw).rgb;
	vec3 I = COMPAT_TEXTURE(rubyTexture, t4.zw).rgb;

	vec3 G5 = COMPAT_TEXTURE(rubyTexture, t5.xw).rgb;
	vec3 H5 = COMPAT_TEXTURE(rubyTexture, t5.yw).rgb;
	vec3 I5 = COMPAT_TEXTURE(rubyTexture, t5.zw).rgb;

	vec3 A0 = COMPAT_TEXTURE(rubyTexture, t6.xy).rgb;
	vec3 D0 = COMPAT_TEXTURE(rubyTexture, t6.xz).rgb;
	vec3 G0 = COMPAT_TEXTURE(rubyTexture, t6.xw).rgb;

	vec3 C4 = COMPAT_TEXTURE(rubyTexture, t7.xy).rgb;
	vec3 F4 = COMPAT_TEXTURE(rubyTexture, t7.xz).rgb;
	vec3 I4 = COMPAT_TEXTURE(rubyTexture, t7.xw).rgb;

	vec4 b = transpose(mat4x3(B, D, H, F)) * (XBR_Y_WEIGHT * yuv[0]);
	vec4 c = transpose(mat4x3(C, A, G, I)) * (XBR_Y_WEIGHT * yuv[0]);
	vec4 e = transpose(mat4x3(E, E, E, E)) * (XBR_Y_WEIGHT * yuv[0]);
	vec4 d = b.yzwx;
	vec4 f = b.wxyz;
	vec4 g = c.zwxy;
	vec4 h = b.zwxy;
	vec4 i = c.wxyz;

	vec4 i4 = transpose(mat4x3(I4, C1, A0, G5)) * (XBR_Y_WEIGHT*yuv[0]);
	vec4 i5 = transpose(mat4x3(I5, C4, A1, G0)) * (XBR_Y_WEIGHT*yuv[0]);
	vec4 h5 = transpose(mat4x3(H5, F4, B1, D0)) * (XBR_Y_WEIGHT*yuv[0]);
	vec4 f4 = h5.yzwx;

	vec4 c1 = i4.yzwx;
	vec4 g0 = i5.wxyz;
	vec4 b1 = h5.zwxy;
	vec4 d0 = h5.wxyz;

	vec4 Ao = vec4( 1.0, -1.0, -1.0, 1.0 );
	vec4 Bo = vec4( 1.0,  1.0, -1.0,-1.0 );
	vec4 Co = vec4( 1.5,  0.5, -0.5, 0.5 );
	vec4 Ax = vec4( 1.0, -1.0, -1.0, 1.0 );
	vec4 Bx = vec4( 0.5,  2.0, -0.5,-2.0 );
	vec4 Cx = vec4( 1.0,  1.0, -0.5, 0.0 );
	vec4 Ay = vec4( 1.0, -1.0, -1.0, 1.0 );
	vec4 By = vec4( 2.0,  0.5, -2.0,-0.5 );
	vec4 Cy = vec4( 2.0,  0.0, -1.0, 0.5 );

	vec4 Az = vec4( 6.0, -2.0, -6.0, 2.0 );
	vec4 Bz = vec4( 2.0, 6.0, -2.0, -6.0 );
	vec4 Cz = vec4( 5.0, 3.0, -3.0, -1.0 );
	vec4 Aw = vec4( 2.0, -6.0, -2.0, 6.0 );
	vec4 Bw = vec4( 6.0, 2.0, -6.0,-2.0 );
	vec4 Cw = vec4( 5.0, -1.0, -3.0, 3.0 );

	fx      = (Ao*fp.y+Bo*fp.x);
	fx_left = (Ax*fp.y+Bx*fp.x);
	fx_up   = (Ay*fp.y+By*fp.x);
	fx3_left= (Az*fp.y+Bz*fp.x);
	fx3_up  = (Aw*fp.y+Bw*fp.x);

	// It uses CORNER_C if none of the others are defined.
if(corner_type == 1.0)
	{interp_restriction_lv1 = and(notEqual(e, f), notEqual(e, h));}
else if(corner_type == 2.0)
	{interp_restriction_lv1      = and(and(notEqual(e,f) , notEqual(e,h))  ,
									( or(or(and(and(or(and(not(eq(f,b)) ,  not(eq(h,d))) ,
									eq(e,i)) , not(eq(f,i4))) , not(eq(h,i5))) , eq(e,g)) , eq(e,c)) ) );}
//commenting this one out because something got broken
/*else if(corner_type == 6.0)
	{interp_restriction_lv1      = and(and(and(notEqual(e,f) , notEqual(e,h))  ,
									( or(or(and(and(or(and(not(eq(f,b)) , not(eq(h,d))) ,
									eq(e,i)) , not(eq(f,i4))) , not(eq(h,i5))) , eq(e,g)) ,
									eq(e,c)) )) , (and(or(or(or(or(and(notEqual(f,f4) , notEqual(f,i)) ,
									and(notEqual(h,h5) , notEqual(h,i))) , notEqual(h,g)) , notEqual(f,c)) ,
									eq(b,c1)) , eq(d,g0))));} */
else
	{interp_restriction_lv1 = and(and(notEqual(e, f), notEqual(e, h)),
								 or(or(and(not(eq(f,b)), not(eq(f,c))),
									   and(not(eq(h,d)), not(eq(h,g)))),
									or(and(eq(e,i), or(and(not(eq(f,f4)), not(eq(f,i4))),
													   and(not(eq(h,h5)), not(eq(h,i5))))),
									   or(eq(e,g), eq(e,c)))));}

	interp_restriction_lv2_left = and(notEqual(e, g), notEqual(d, g));
	interp_restriction_lv2_up   = and(notEqual(e, c), notEqual(b, c));
	interp_restriction_lv3_left = and(eq2(g,g0), not(eq2(d0,g0)));
	interp_restriction_lv3_up   = and(eq2(c,c1), not(eq2(b1,c1)));

	vec4 fx45 = smoothstep(Co - delta, Co + delta, fx);
	vec4 fx30 = smoothstep(Cx - delta, Cx + delta, fx_left);
	vec4 fx60 = smoothstep(Cy - delta, Cy + delta, fx_up);
	vec4 fx15 = smoothstep(Cz - delta, Cz + delta, fx3_left);
	vec4 fx75 = smoothstep(Cw - delta, Cw + delta, fx3_up);

	edr = and(lessThan(weighted_distance( e, c, g, i, h5, f4, h, f), weighted_distance( h, d, i5, f, i4, b, e, i)), interp_restriction_lv1);
	edr_left = and(lessThanEqual((XBR_LV2_COEFFICIENT*df(f,g)), df(h,c)), interp_restriction_lv2_left);
	edr_up   = and(greaterThanEqual(df(f,g), (XBR_LV2_COEFFICIENT*df(h,c))), interp_restriction_lv2_up);
	edr3_left = interp_restriction_lv3_left;
	edr3_up = interp_restriction_lv3_up;

	nc45 = and(edr, bvec4(fx45));
	nc30 = and(edr, and(edr_left, bvec4(fx30)));
	nc60 = and(edr, and(edr_up, bvec4(fx60)));
	nc15 = and(and(edr, edr_left), and(edr3_left, bvec4(fx15)));
	nc75 = and(and(edr, edr_up), and(edr3_up, bvec4(fx75)));

	px = lessThanEqual(df(e, f), df(e, h));

	nc = bvec4(nc75.x || nc15.x || nc30.x || nc60.x || nc45.x, nc75.y || nc15.y || nc30.y || nc60.y || nc45.y, nc75.z || nc15.z || nc30.z || nc60.z || nc45.z, nc75.w || nc15.w || nc30.w || nc60.w || nc45.w);

	vec4 final45 = vec4(nc45) * fx45;
	vec4 final30 = vec4(nc30) * fx30;
	vec4 final60 = vec4(nc60) * fx60;
	vec4 final15 = vec4(nc15) * fx15;
	vec4 final75 = vec4(nc75) * fx75;

	vec4 maximo = max(max(max(final15, final75),max(final30, final60)), final45);

		 if (nc.x) {pix1 = px.x ? F : H; blend1 = maximo.x;}
	else if (nc.y) {pix1 = px.y ? B : F; blend1 = maximo.y;}
	else if (nc.z) {pix1 = px.z ? D : B; blend1 = maximo.z;}
	else if (nc.w) {pix1 = px.w ? H : D; blend1 = maximo.w;}

		 if (nc.w) {pix2 = px.w ? H : D; blend2 = maximo.w;}
	else if (nc.z) {pix2 = px.z ? D : B; blend2 = maximo.z;}
	else if (nc.y) {pix2 = px.y ? B : F; blend2 = maximo.y;}
	else if (nc.x) {pix2 = px.x ? F : H; blend2 = maximo.x;}

	res1 = mix(E, pix1, blend1);
	res2 = mix(E, pix2, blend2);
	vec3 res = mix(res1, res2, step(c_df(E, res1), c_df(E, res2)));

	FragColor = vec4(res, 1.0);
}
#endif
