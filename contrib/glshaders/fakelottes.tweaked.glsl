#version 120

// Simple scanlines with curvature and mask effects lifted from crt-lottes
// by hunterk

////////////////////////////////////////////////////////////////////
////////////////////////////  SETTINGS  ////////////////////////////
/////  comment these lines to disable effects and gain speed  //////
////////////////////////////////////////////////////////////////////

#define MASK // fancy, expensive phosphor mask effect
//#define CURVATURE // applies barrel distortion to the screen
#define SCANLINES // applies horizontal scanline effect
//#define ROTATE_SCANLINES // for TATE games; also disables the mask effects, which look bad with it
#define EXTRA_MASKS // disable these if you need extra registers freed up

////////////////////////////////////////////////////////////////////
//////////////////////////  END SETTINGS  //////////////////////////
////////////////////////////////////////////////////////////////////

///////////////////////  Runtime Parameters  ///////////////////////
#pragma parameter shadowMask "shadowMask" 1.0 0.0 4.0 1.0
#pragma parameter SCANLINE_SINE_COMP_B "Scanline Intensity" 0.40 0.0 1.0 0.05
#pragma parameter warpX "warpX" 0.031 0.0 0.125 0.01
#pragma parameter warpY "warpY" 0.041 0.0 0.125 0.01
#pragma parameter maskDark "maskDark" 0.5 0.0 2.0 0.1
#pragma parameter maskLight "maskLight" 1.5 0.0 2.0 0.1
#pragma parameter crt_gamma "CRT Gamma" 2.5 1.0 4.0 0.05
#pragma parameter monitor_gamma "Monitor Gamma" 2.2 1.0 4.0 0.05
#pragma parameter SCANLINE_SINE_COMP_A "Scanline Sine Comp A" 0.0 0.0 0.10 0.01
#pragma parameter SCANLINE_BASE_BRIGHTNESS "Scanline Base Brightness" 0.95 0.0 1.0 0.01

// prevent stupid behavior
#if defined ROTATE_SCANLINES && !defined SCANLINES
	#define SCANLINES
#endif

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
COMPAT_ATTRIBUTE vec4 COLOR;
COMPAT_ATTRIBUTE vec4 TexCoord;
COMPAT_VARYING vec4 COL0;
COMPAT_VARYING vec2 v_texCoord;

vec4 _oPosition1;
uniform mat4 MVPMatrix;
uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int rubyFrameCount;
uniform COMPAT_PRECISION vec2 rubyOutputSize;
uniform COMPAT_PRECISION vec2 rubyTextureSize;
uniform COMPAT_PRECISION vec2 rubyInputSize;

// compatibility #defines
#define vTexCoord v_texCoord.xy
#define SourceSize vec4(rubyTextureSize, 1.0 / rubyTextureSize) //either rubyTextureSize or rubyInputSize
#define OutSize vec4(rubyOutputSize, 1.0 / rubyOutputSize)

#ifdef PARAMETER_UNIFORM
uniform COMPAT_PRECISION float WHATEVER;
#else
#define WHATEVER 0.0
#endif

void main()
{
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0 * rubyInputSize / rubyTextureSize;
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

uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int rubyFrameCount;
uniform COMPAT_PRECISION vec2 rubyOutputSize;
uniform COMPAT_PRECISION vec2 rubyTextureSize;
uniform COMPAT_PRECISION vec2 rubyInputSize;
uniform sampler2D rubyTexture;
COMPAT_VARYING vec2 v_texCoord;

// compatibility #defines
#define Source rubyTexture
#define vTexCoord v_texCoord.xy

#define SourceSize vec4(rubyTextureSize, 1.0 / rubyTextureSize) //either rubyTextureSize or rubyInputSize
#define OutSize vec4(rubyOutputSize, 1.0 / rubyOutputSize)

#ifdef PARAMETER_UNIFORM
uniform COMPAT_PRECISION float SCANLINE_BASE_BRIGHTNESS;
uniform COMPAT_PRECISION float SCANLINE_SINE_COMP_A;
uniform COMPAT_PRECISION float SCANLINE_SINE_COMP_B;
uniform COMPAT_PRECISION float warpX;
uniform COMPAT_PRECISION float warpY;
uniform COMPAT_PRECISION float maskDark;
uniform COMPAT_PRECISION float maskLight;
uniform COMPAT_PRECISION float shadowMask;
uniform COMPAT_PRECISION float crt_gamma;
uniform COMPAT_PRECISION float monitor_gamma;
#else
#define SCANLINE_BASE_BRIGHTNESS 0.95
#define SCANLINE_SINE_COMP_A 0.0
#define SCANLINE_SINE_COMP_B 0.40
#define warpX 0.031
#define warpY 0.041
#define maskDark 0.5
#define maskLight 1.5
#define shadowMask 1.0
#define crt_gamma 2.5
#define monitor_gamma 2.2
#endif

/*
	The following code allows the shader to override any texture filtering
	configured in DOSBox. if 'output' is set to 'opengl', bilinear filtering
	will be enabled and OPENGLNB will not be defined, if 'output' is set to
	'openglnb', nearest neighbour filtering will be enabled and OPENGLNB will
	be defined.

	If you wish to use the default filtering method that is currently enabled
	in DOSBox, use COMPAT_TEXTURE to lookup a texel from the input texture.

	If you wish to force nearest-neighbor interpolation use NN_TEXTURE.

	If you wish to force bilinear interpolation use BL_TEXTURE.

	If DOSBox is configured to use the filtering method that is being forced,
	the default	hardware implementation will be used, otherwise the custom
	implementations below will be used instead.

	These custom implemenations rely on the `rubyTextureSize` uniform variable.
	The code could calculate the texture size from the sampler using the
	textureSize() GLSL function, but this would require a minimum of GLSL
	version 130, which may prevent the shader from working on older systems.
*/

#if defined(OPENGLNB)
#define NN_TEXTURE COMPAT_TEXTURE
#define BL_TEXTURE blTexture
vec4 blTexture(in sampler2D sampler, in vec2 uv)
{
	// subtract 0.5 here and add it again after the floor to centre the texel
	vec2 texCoord = uv * rubyTextureSize - vec2(0.5);
	vec2 s0t0 = floor(texCoord) + vec2(0.5);
	vec2 s0t1 = s0t0 + vec2(0.0, 1.0);
	vec2 s1t0 = s0t0 + vec2(1.0, 0.0);
	vec2 s1t1 = s0t0 + vec2(1.0);

	vec2 invTexSize = 1.0 / rubyTextureSize;
	vec4 c_s0t0 = COMPAT_TEXTURE(sampler, s0t0 * invTexSize);
	vec4 c_s0t1 = COMPAT_TEXTURE(sampler, s0t1 * invTexSize);
	vec4 c_s1t0 = COMPAT_TEXTURE(sampler, s1t0 * invTexSize);
	vec4 c_s1t1 = COMPAT_TEXTURE(sampler, s1t1 * invTexSize);

	vec2 weight = fract(texCoord);

	vec4 c0 = c_s0t0 + (c_s1t0 - c_s0t0) * weight.x;
	vec4 c1 = c_s0t1 + (c_s1t1 - c_s0t1) * weight.x;

	return (c0 + (c1 - c0) * weight.y);
}
#else
#define BL_TEXTURE COMPAT_TEXTURE
#define NN_TEXTURE nnTexture
vec4 nnTexture(in sampler2D sampler, in vec2 uv)
{
	vec2 texCoord = floor(uv * rubyTextureSize) + vec2(0.5);
	vec2 invTexSize = 1.0 / rubyTextureSize;
	return COMPAT_TEXTURE(sampler, texCoord * invTexSize);
}
#endif

vec4 scanline(vec2 coord, vec4 frame)
{
#if defined SCANLINES
	vec2 omega = vec2(3.1415 * rubyOutputSize.x, 2.0 * 3.1415 * rubyTextureSize.y);
	vec2 sine_comp = vec2(SCANLINE_SINE_COMP_A, SCANLINE_SINE_COMP_B);
	vec3 res = frame.xyz;
	#ifdef ROTATE_SCANLINES
		sine_comp = sine_comp.yx;
		omega = omega.yx;
	#endif
	vec3 scanline = res * (SCANLINE_BASE_BRIGHTNESS + dot(sine_comp * sin(coord * omega), vec2(1.0, 1.0)));

	return vec4(scanline.x, scanline.y, scanline.z, 1.0);
#else
	return frame;
#endif
}

#ifdef CURVATURE
// Distortion of scanlines, and end of screen alpha.
vec2 Warp(vec2 pos)
{
	pos  = pos*2.0-1.0;
	pos *= vec2(1.0 + (pos.y*pos.y)*warpX, 1.0 + (pos.x*pos.x)*warpY);

	return pos*0.5 + 0.5;
}
#endif

#if defined MASK && !defined ROTATE_SCANLINES
	// Shadow mask.
	vec4 Mask(vec2 pos)
	{
		vec3 mask = vec3(maskDark, maskDark, maskDark);

		// Very compressed TV style shadow mask.
		if (shadowMask == 1.0)
		{
			float line = maskLight;
			float odd = 0.0;

			if (fract(pos.x*0.166666666) < 0.5) odd = 1.0;
			if (fract((pos.y + odd) * 0.5) < 0.5) line = maskDark;

			pos.x = fract(pos.x*0.333333333);

			if      (pos.x < 0.333) mask.r = maskLight;
			else if (pos.x < 0.666) mask.g = maskLight;
			else                    mask.b = maskLight;
			mask*=line;
		}

		// Aperture-grille.
		else if (shadowMask == 2.0)
		{
			pos.x = fract(pos.x*0.333333333);

			if      (pos.x < 0.333) mask.r = maskLight;
			else if (pos.x < 0.666) mask.g = maskLight;
			else                    mask.b = maskLight;
		}
	#ifdef EXTRA_MASKS
		// These can cause moire with curvature and scanlines
		// so they're an easy target for freeing up registers

		// Stretched VGA style shadow mask (same as prior shaders).
		else if (shadowMask == 3.0)
		{
			pos.x += pos.y*3.0;
			pos.x  = fract(pos.x*0.166666666);

			if      (pos.x < 0.333) mask.r = maskLight;
			else if (pos.x < 0.666) mask.g = maskLight;
			else                    mask.b = maskLight;
		}

		// VGA style shadow mask.
		else if (shadowMask == 4.0)
		{
			pos.xy  = floor(pos.xy*vec2(1.0, 0.5));
			pos.x  += pos.y*3.0;
			pos.x   = fract(pos.x*0.166666666);

			if      (pos.x < 0.333) mask.r = maskLight;
			else if (pos.x < 0.666) mask.g = maskLight;
			else                    mask.b = maskLight;
		}
	#endif

		else mask = vec3(1.,1.,1.);

		return vec4(mask, 1.0);
	}
#endif

void main()
{
#ifdef CURVATURE
	vec2 pos = Warp(v_texCoord.xy*(rubyTextureSize.xy/rubyInputSize.xy))*(rubyInputSize.xy/rubyTextureSize.xy);
#else
	vec2 pos = v_texCoord.xy;
#endif

#if defined MASK && !defined ROTATE_SCANLINES
	// mask effects look bad unless applied in linear gamma space
	vec4 in_gamma = vec4(monitor_gamma, monitor_gamma, monitor_gamma, 1.0);
	vec4 out_gamma = vec4(1.0 / crt_gamma, 1.0 / crt_gamma, 1.0 / crt_gamma, 1.0);
	vec4 res = pow(BL_TEXTURE(Source, pos), in_gamma);
#else
	vec4 res = BL_TEXTURE(Source, pos);
#endif

#if defined MASK && !defined ROTATE_SCANLINES
	// apply the mask; looks bad with vert scanlines so make them mutually exclusive
	res *= Mask(gl_FragCoord.xy * 1.0001);
#endif

#if defined CURVATURE && defined GL_ES
	// hacky clamp fix for GLES
	vec2 bordertest = (pos);
	if ( bordertest.x > 0.0001 && bordertest.x < 0.9999 && bordertest.y > 0.0001 && bordertest.y < 0.9999)
		res = res;
	else
		res = vec4(0.,0.,0.,0.);
#endif

#if defined MASK && !defined ROTATE_SCANLINES
	// re-apply the gamma curve for the mask path
	FragColor = pow(scanline(pos, res), out_gamma);
#else
	FragColor = scanline(pos, res);
#endif
}
#endif
