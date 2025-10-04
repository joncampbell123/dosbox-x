#version 120

/*
   Hyllian's xBR-lv2-noblend Shader

   Copyright (C) 2011-2016 Hyllian - sergiogdb@gmail.com

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

#pragma parameter XBR_EQ_THRESHOLD "Eq Threshold" 0.6 0.0 2.0 0.1
#pragma parameter XBR_LV2_COEFFICIENT "Lv2 Coefficient" 2.0 1.0 3.0 0.1

#define mul(a,b) (b*a)

// Uncomment just one of the three params below to choose the corner detection
#define CORNER_A
//#define CORNER_B
//#define CORNER_C
//#define CORNER_D

#define lv2_cf XBR_LV2_COEFFICIENT

#if defined(VERTEX)

#if __VERSION__ >= 130
#define OUT out
#define IN  in
#define tex2D texture
#else
#define OUT varying
#define IN attribute
#define tex2D texture2D
#endif

#ifdef GL_ES
#define PRECISION mediump
#else
#define PRECISION
#endif


IN vec4 a_position;

OUT vec2 texCoord;
OUT vec4 t1;
OUT vec4 t2;
OUT vec4 t3;
OUT vec4 t4;
OUT vec4 t5;
OUT vec4 t6;
OUT vec4 t7;

uniform PRECISION vec2 rubyOutputSize;
uniform PRECISION vec2 rubyTextureSize;
uniform PRECISION vec2 rubyInputSize;

void main()
{
	gl_Position = a_position;

	float dx = (1.0/rubyTextureSize.x);
	float dy = (1.0/rubyTextureSize.y);

	vec2 TexCoord = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0 * rubyInputSize / rubyTextureSize;
	texCoord     = TexCoord;
	texCoord.x *= 1.00000001;
	t1 = TexCoord.xxxy + vec4( -dx, 0, dx,-2.0*dy); // A1 B1 C1
	t2 = TexCoord.xxxy + vec4( -dx, 0, dx,    -dy); //  A  B  C
	t3 = TexCoord.xxxy + vec4( -dx, 0, dx,      0); //  D  E  F
	t4 = TexCoord.xxxy + vec4( -dx, 0, dx,     dy); //  G  H  I
	t5 = TexCoord.xxxy + vec4( -dx, 0, dx, 2.0*dy); // G5 H5 I5
	t6 = TexCoord.xyyy + vec4(-2.0*dx,-dy, 0,  dy); // A0 D0 G0
	t7 = TexCoord.xyyy + vec4( 2.0*dx,-dy, 0,  dy); // C4 F4 I4
}


#elif defined(FRAGMENT)

#if __VERSION__ >= 130
#define IN in
#define tex2D texture
out vec4 FragColor;
#else
#define IN varying
#define FragColor gl_FragColor
#define tex2D texture2D
#endif

#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#define PRECISION mediump
#else
#define PRECISION
#endif

uniform PRECISION vec2 rubyOutputSize;
uniform PRECISION vec2 rubyTextureSize;
uniform PRECISION vec2 rubyInputSize;
uniform sampler2D rubyTexture;
IN vec2 texCoord;
IN vec4 t1;
IN vec4 t2;
IN vec4 t3;
IN vec4 t4;
IN vec4 t5;
IN vec4 t6;
IN vec4 t7;

#ifdef PARAMETER_UNIFORM
uniform PRECISION float XBR_EQ_THRESHOLD;
uniform PRECISION float XBR_LV2_COEFFICIENT;
#else
#define XBR_EQ_THRESHOLD 0.6
#define XBR_LV2_COEFFICIENT 2.0
#endif
// END PARAMETERS //

const vec3 Y = vec3(0.2126, 0.7152, 0.0722);

// Difference between vector components.
vec4 df(vec4 A, vec4 B)
{
	return vec4(abs(A-B));
}

// Compare two vectors and return their components are different.
vec4 diff(vec4 A, vec4 B)
{
	return vec4(notEqual(A, B));
}

// Determine if two vector components are equal based on a threshold.
vec4 eq(vec4 A, vec4 B)
{
	return (step(df(A, B), vec4(XBR_EQ_THRESHOLD)));
}

// Determine if two vector components are NOT equal based on a threshold.
vec4 neq(vec4 A, vec4 B)
{
	return (vec4(1.0, 1.0, 1.0, 1.0) - eq(A, B));
}

// Weighted distance.
vec4 wd(vec4 a, vec4 b, vec4 c, vec4 d, vec4 e, vec4 f, vec4 g, vec4 h)
{
	return (df(a,b) + df(a,c) + df(d,e) + df(d,f) + 4.0*df(g,h));
}

vec4 weighted_distance(vec4 a, vec4 b, vec4 c, vec4 d, vec4 e, vec4 f, vec4 g, vec4 h, vec4 i, vec4 j, vec4 k, vec4 l)
{
	return (df(a,b) + df(a,c) + df(d,e) + df(d,f) + df(i,j) + df(k,l) + 2.0*df(g,h));
}

float c_df(vec3 c1, vec3 c2)
{
	vec3 df = abs(c1 - c2);
	return df.r + df.g + df.b;
}

void main()
{
	vec4 edri, edr, edr_l, edr_u; // px = pixel, edr = edge detection rule
	vec4 irlv1, irlv2l, irlv2u, block_3d;
	bvec4 nc, px;
	vec4 fx, fx_l, fx_u; // inequations of straight lines.

	vec2 fp  = fract(texCoord*rubyTextureSize);

	vec3 A1 = tex2D(rubyTexture, t1.xw ).xyz;
	vec3 B1 = tex2D(rubyTexture, t1.yw ).xyz;
	vec3 C1 = tex2D(rubyTexture, t1.zw ).xyz;
	vec3 A  = tex2D(rubyTexture, t2.xw ).xyz;
	vec3 B  = tex2D(rubyTexture, t2.yw ).xyz;
	vec3 C  = tex2D(rubyTexture, t2.zw ).xyz;
	vec3 D  = tex2D(rubyTexture, t3.xw ).xyz;
	vec3 E  = tex2D(rubyTexture, t3.yw ).xyz;
	vec3 F  = tex2D(rubyTexture, t3.zw ).xyz;
	vec3 G  = tex2D(rubyTexture, t4.xw ).xyz;
	vec3 H  = tex2D(rubyTexture, t4.yw ).xyz;
	vec3 I  = tex2D(rubyTexture, t4.zw ).xyz;
	vec3 G5 = tex2D(rubyTexture, t5.xw ).xyz;
	vec3 H5 = tex2D(rubyTexture, t5.yw ).xyz;
	vec3 I5 = tex2D(rubyTexture, t5.zw ).xyz;
	vec3 A0 = tex2D(rubyTexture, t6.xy ).xyz;
	vec3 D0 = tex2D(rubyTexture, t6.xz ).xyz;
	vec3 G0 = tex2D(rubyTexture, t6.xw ).xyz;
	vec3 C4 = tex2D(rubyTexture, t7.xy ).xyz;
	vec3 F4 = tex2D(rubyTexture, t7.xz ).xyz;
	vec3 I4 = tex2D(rubyTexture, t7.xw ).xyz;

	vec4 b = mul( mat4x3(B, D, H, F), Y );
	vec4 c = mul( mat4x3(C, A, G, I), Y );
	vec4 e = mul( mat4x3(E, E, E, E), Y );
	vec4 d  = b.yzwx;
	vec4 f  = b.wxyz;
	vec4 g  = c.zwxy;
	vec4 h  = b.zwxy;
	vec4 i  = c.wxyz;

	vec4 i4 = mul( mat4x3(I4, C1, A0, G5), Y );
	vec4 i5 = mul( mat4x3(I5, C4, A1, G0), Y );
	vec4 h5 = mul( mat4x3(H5, F4, B1, D0), Y );
	vec4 f4 = h5.yzwx;

	vec4 Ao = vec4( 1.0, -1.0, -1.0, 1.0 );
	vec4 Bo = vec4( 1.0,  1.0, -1.0,-1.0 );
	vec4 Co = vec4( 1.5,  0.5, -0.5, 0.5 );
	vec4 Ax = vec4( 1.0, -1.0, -1.0, 1.0 );
	vec4 Bx = vec4( 0.5,  2.0, -0.5,-2.0 );
	vec4 Cx = vec4( 1.0,  1.0, -0.5, 0.0 );
	vec4 Ay = vec4( 1.0, -1.0, -1.0, 1.0 );
	vec4 By = vec4( 2.0,  0.5, -2.0,-0.5 );
	vec4 Cy = vec4( 2.0,  0.0, -1.0, 0.5 );

	// These inequations define the line below which interpolation occurs.
	fx     = vec4(greaterThan(Ao*fp.y+Bo*fp.x, Co));
	fx_l   = vec4(greaterThan(Ax*fp.y+Bx*fp.x, Cx));
	fx_u   = vec4(greaterThan(Ay*fp.y+By*fp.x, Cy));

#ifdef CORNER_A
	irlv1      = diff(e,f) * diff(e,h);
#endif
#ifdef CORNER_B
	irlv1      = (neq(f,b) * neq(h,d) + eq(e,i) * neq(f,i4) * neq(h,i5) + eq(e,g) + eq(e,c));
#endif
#ifdef CORNER_D
	vec4 c1 = i4.yzwx;
	vec4 g0 = i5.wxyz;
	irlv1     = (neq(f,b) * neq(h,d) + eq(e,i) * neq(f,i4) * neq(h,i5) + eq(e,g) + eq(e,c) ) * (diff(f,f4) * diff(f,i) + diff(h,h5) * diff(h,i) + diff(h,g) + diff(f,c) + eq(b,c1) * eq(d,g0));
#endif
#ifdef CORNER_C
	irlv1     = (neq(f,b) * neq(f,c) + neq(h,d) * neq(h,g) + eq(e,i) * (neq(f,f4) * neq(f,i4) + neq(h,h5) * neq(h,i5)) + eq(e,g) + eq(e,c));
#endif

	irlv2l = diff(e,g) * diff(d,g);
	irlv2u = diff(e,c) * diff(b,c);

	vec4 wd1 = wd( e, c,  g, i, h5, f4, h, f);
	vec4 wd2 = wd( h, d, i5, f, i4,  b, e, i);

	edri  = step(wd1, wd2) * irlv1;
	edr   = step(wd1 + vec4(0.1, 0.1, 0.1, 0.1), wd2) * step(vec4(0.5, 0.5, 0.5, 0.5), irlv1);
	edr_l = step( lv2_cf*df(f,g), df(h,c) ) * irlv2l * edr;
	edr_u = step( lv2_cf*df(h,c), df(f,g) ) * irlv2u * edr;

	nc = bvec4( edr * ( fx + edr_l * (fx_l)) + edr_u * fx_u);

	px = lessThanEqual(df(e, f), df(e, h));

	vec3 res1 = nc.x ? px.x ? F : H : nc.y ? px.y ? B : F : nc.z ? px.z ? D : B : E;
	vec3 res2 = nc.w ? px.w ? H : D : nc.z ? px.z ? D : B : nc.y ? px.y ? B : F : E;

	vec2 df12 = abs( mul( mat2x3(res1, res2), Y ) - e.xy);

	vec3 res = mix(res1, res2, step(df12.x, df12.y));

	FragColor.xyz = res;
}
#endif
