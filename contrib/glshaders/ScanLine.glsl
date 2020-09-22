#version 120
/*

dugan_CRT-EasyMode_tweaked.glsl
		dugan's ScanLine shader
		adapted by liPillON for usage with DosBox SVN r4319 and later
		no tweaks

source shader files:
https://github.com/duganchen/dosbox_shaders/blob/master/scanline.frag
https://github.com/duganchen/dosbox_shaders/blob/master/scanline.vert

first posted here:
https://www.vogons.org/viewtopic.php?f=32&t=72697

*/

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

uniform vec2 rubyTextureSize;
uniform vec2 rubyInputSize;
uniform vec2 rubyOutputSize;

COMPAT_ATTRIBUTE vec4 a_position;
COMPAT_VARYING vec2 v_texCoord;

COMPAT_VARYING vec2 omega;

void main()
{
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x+1.0,1.0-a_position.y)/2.0*rubyInputSize/rubyTextureSize;

	omega = vec2(3.1415 * rubyOutputSize.x * rubyTextureSize.x / rubyInputSize.x, 2.0 * 3.1415 * rubyTextureSize.y);
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

const float SCANLINE_BASE_BRIGHTNESS = 0.95;
const float SCANLINE_SINE_COMP_A = 0.05;
const float SCANLINE_SINE_COMP_B = 0.15;

COMPAT_VARYING vec2 v_texCoord;
uniform sampler2D rubyTexture;

COMPAT_VARYING vec2 omega;

void main()
{
	vec2 sine_comp = vec2(SCANLINE_SINE_COMP_A, SCANLINE_SINE_COMP_B);
	vec3 res = COMPAT_TEXTURE(rubyTexture, v_texCoord).xyz;
	vec3 scanline = res * (SCANLINE_BASE_BRIGHTNESS + dot(sine_comp * sin(v_texCoord * omega), vec2(1.0, 1.0)));
	FragColor = vec4(scanline.x, scanline.y, scanline.z, 1.0);
}

#endif
