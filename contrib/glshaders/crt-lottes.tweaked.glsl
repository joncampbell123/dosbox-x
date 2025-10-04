#version 120
/*

marty_CRT-Lottes_tweaked.glsl
		marty's adaptation of dugan's CRT-Lottes port
		adapted again by liPillON for usage with DosBox SVN r4319 and later
		slightly tweaked the parameters in order to:
		- reduce the blurring,
		- bump up the brightness a bit,
		- soften the crt mask,
		- tone down the display curvature

source shader files:
https://github.com/duganchen/dosbox_shaders/blob/master/crt-lottes.frag
https://github.com/duganchen/dosbox_shaders/blob/master/crt-lottes.vert

marty's source is available only within the release package of its port:
https://github.com/MartyShepard/DOSBox-Optionals

first posted here:
https://www.vogons.org/viewtopic.php?f=32&t=72697

*/

uniform vec2 rubyTextureSize;
uniform vec2 rubyInputSize;

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

COMPAT_ATTRIBUTE vec4 a_position;
COMPAT_VARYING vec2 v_texCoord;

void main()
{
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x+1.0,1.0-a_position.y)/2.0*rubyInputSize/rubyTextureSize;
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
#define COMPAT_PRECISION highp
#else
#define COMPAT_PRECISION
#endif

COMPAT_VARYING vec2 v_texCoord;

uniform vec2 rubyOutputSize;
uniform sampler2D rubyTexture;

const float hardScan = -7.0;               //tweaked
const float hardPix = -3.5;                //tweaked
const float warpX = 0.0075;                //tweaked
const float warpY = 0.0075;                //tweaked
const float maskDark = 0.5;
const float maskLight = 1.5;
const float scaleInLinearGamma = 1;
const float shadowMask = 1;
const float brightBoost = 1.4;             //tweaked

#define Blackmask 1

//------------------------------------------------------------------------

// sRGB to Linear.
// Assuing using sRGB typed textures this should not be needed.
float ToLinear1(float c)
{
	if (scaleInLinearGamma == 0)
	{
	return c;
	}
	return (c <= 0.04045) ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

vec3 ToLinear(vec3 c)
{
	if (scaleInLinearGamma == 0)
	{
	return c;
	}
	return vec3(ToLinear1(c.r), ToLinear1(c.g), ToLinear1(c.b));
}

// Linear to sRGB.
// Assuing using sRGB typed textures this should not be needed.
float ToSrgb1(float c)
{
	if (scaleInLinearGamma == 0)
	{
	return c;
	}
	return(c < 0.0031308 ? c *12.92 : 1.055 * pow(c, 0.41666) - 0.055);
}

vec3 ToSrgb(vec3 c)
{
	if (scaleInLinearGamma == 0)
	{
	return c;
	}
	return vec3(ToSrgb1(c.r), ToSrgb1(c.g), ToSrgb1(c.b));
}

// Nearest emulated sample given floating point position and texel offset.
// Also zero's off screen.
vec3 Fetch(vec2 pos,vec2 off)
{
	pos = (floor(pos * rubyTextureSize.xy + off) + vec2(0.5, 0.5)) / rubyTextureSize.xy;
	return ToLinear(brightBoost * COMPAT_TEXTURE(rubyTexture, pos.xy).rgb);
}

// Distance in emulated pixels to nearest texel.
vec2 Dist(vec2 pos)
{
	pos = pos * rubyTextureSize.xy;
	return -((pos - floor(pos)) - vec2(0.5));
}

// 1D Gaussian.
float Gaus(float pos, float scale)
{
	return exp2(scale * pos * pos);
}

// 3-tap Gaussian filter along horz line.
vec3 Horz3(vec2 pos, float off)
{
	vec3 b = Fetch(pos, vec2(-1.0, off));
	vec3 c = Fetch(pos, vec2(0.0, off));
	vec3 d = Fetch(pos, vec2(1.0, off));
	float dst = Dist(pos).x;
	// Convert distance to weight.
	float scale = hardPix;
	float wb = Gaus(dst - 1.0, scale);
	float wc = Gaus(dst + 0.0, scale);
	float wd = Gaus(dst + 1.0, scale);
	// Return filtered sample.
	return (b * wb + c * wc + d * wd) / (wb + wc + wd);
}

// 5-tap Gaussian filter along horz line.
vec3 Horz5(vec2 pos, float off)
{
	vec3 a = Fetch(pos, vec2(-2.0, off));
	vec3 b = Fetch(pos, vec2(-1.0, off));
	vec3 c = Fetch(pos, vec2(0.0, off));
	vec3 d = Fetch(pos, vec2(1.0, off));
	vec3 e = Fetch(pos, vec2(2.0, off));
	float dst = Dist(pos).x;
	// Convert distance to weight.
	float scale = hardPix;
	float wa = Gaus(dst - 2.0, scale);
	float wb = Gaus(dst - 1.0, scale);
	float wc = Gaus(dst + 0.0, scale);
	float wd = Gaus(dst + 1.0, scale);
	float we = Gaus(dst + 2.0, scale);
	// Return filtered sample.
	return (a * wa + b * wb + c * wc + d * wd + e * we) / (wa + wb + wc + wd + we);
}

// Return scanline weight.
float Scan(vec2 pos, float off)
{
	float dst = Dist(pos).y;
	return Gaus(dst + off, hardScan);
}

// Allow nearest three lines to effect pixel.
vec3 Tri(vec2 pos)
{
	vec3 a = Horz3(pos, -1.0);
	vec3 b = Horz5(pos, 0.0);
	vec3 c = Horz3(pos, 1.0);
	float wa = Scan(pos, -1.0);
	float wb = Scan(pos, 0.0);
	float wc = Scan(pos, 1.0);
	return a * wa + b * wb + c * wc;
}

// Distortion of scanlines, and end of screen alpha.
vec2 Warp(vec2 pos)
{
	pos = pos * 2.0 -1.0;
	pos *= vec2(1.0 + (pos.y * pos.y) * warpX, 1.0 + (pos.x * pos.x) * warpY);
	return pos * 0.5 + 0.5;
}

// Shadow mask.
vec3 Mask(vec2 pos)
{
	pos.x += pos.y * 3.0;
	vec3 mask = vec3(maskDark, maskDark, maskDark);
	pos.x = fract(pos.x / 6.0);
	if (pos.x < 0.333)
	{
	mask.r = maskLight;
	}
	else if (pos.x < 0.666)
	{
	mask.g = maskLight;
	}
	else
	{
	mask.b = maskLight;
	}
	return mask;
}

uniform vec2 resolution;
uniform vec2 mouse;
uniform float time;

float box(vec2 _st, vec2 _size, float _smoothEdges) {
	_size = vec2(.1) - _size*.2;
	vec2 aa = vec2(_smoothEdges * 0.1);
	vec2 uv = smoothstep(_size, _size+aa, _st);
	uv *= smoothstep(_size, _size+aa, vec2(1.0)-_st);
	return uv.x * uv.y;
}

vec3 drawRectangle(in vec2 st) {
	// Each result will return 1.0 (white) or 0.0 (
	vec3 color = vec3(0.0);
	vec2 borders = step(vec2(0.0),st);
	float pct = borders.x * borders.y;

	// top-right
	vec2 tr = step(vec2(0.1),1.0-st);
	pct *= tr.x * tr.y;

	// The multiplication of left*bottom will be similar to the logical AND.
	color = vec3(pct);
	return color;
}

void main()
{
	vec2 pos = Warp(v_texCoord.xy * (rubyTextureSize.xy / rubyInputSize.xy)) * (rubyInputSize.xy / rubyTextureSize.xy);


#ifdef Blackmask
	vec3 outColor = vec3(1.0);
	outColor *= Tri(pos);
	outColor *= drawRectangle(pos);
	outColor *= vec3(box(pos, vec2(0.5), 0.01));
#else
	vec3 outColor = Tri(pos);
#endif


	if (shadowMask != 0)
	{
		outColor.rgb *= Mask(floor(v_texCoord.xy * (rubyTextureSize.xy / rubyInputSize.xy) * rubyOutputSize.xy) + vec2(0.5, 0.5));
	}

	FragColor = vec4(ToSrgb(outColor.rgb), 1.0);
}

#endif
