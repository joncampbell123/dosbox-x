#version 120

// Developed by tyrells
// Based on Marat Tanalin's algorithm: https://tanalin.com/en/articles/integer-scaling/

// includes scanline code from https://github.com/libretro/glsl-shaders/blob/master/scanlines/shaders/scanline.glsl

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

const vec2 targetAspectRatio = vec2(4.0, 3.0);

uniform vec2 rubyOutputSize;
uniform vec2 rubyTextureSize;

COMPAT_ATTRIBUTE vec4 a_position;
COMPAT_VARYING vec2 outCoord;

vec2 calculateScalingRatio(vec2 screenSize, vec2 imageSize, vec2 targetAspectRatio)
{
	float _TargetAspectRatio = targetAspectRatio.x / targetAspectRatio.y;
	float imageAspectRatio = imageSize.x / imageSize.y;

	vec2 maxIntRatio = floor(screenSize / imageSize);

	if (imageAspectRatio == _TargetAspectRatio)
	{
		float ratio = max(min(maxIntRatio.x, maxIntRatio.y), 1.0f);
		return vec2(ratio);
	}

	vec2 maxOutputSize = imageSize * maxIntRatio;
	float maxAspectRatio = maxOutputSize.x / maxOutputSize.y;
	vec2 scalingRatio = vec2(0.0, 0.0);
	// If the ratio MA is lower than the target aspect ratio TA
	if (maxAspectRatio < _TargetAspectRatio)
	{
		scalingRatio.x = maxIntRatio.x;
		float AUH = maxOutputSize.x / _TargetAspectRatio;

		float yUpperScaleFactor = ceil(AUH / imageSize.y);
		float yLowerScaleFactor = floor(AUH / imageSize.y);

		float upperAspectRatio = maxOutputSize.x / (yUpperScaleFactor * imageSize.y);
		float lowerAspectRatio = maxOutputSize.x / (yLowerScaleFactor * imageSize.y);

		float upperTargetError = abs(_TargetAspectRatio - upperAspectRatio);
		float lowerTargetError = abs(_TargetAspectRatio - lowerAspectRatio);

		if (abs(upperTargetError - lowerTargetError) < 0.001)
		{
			float upperImageError = abs(imageAspectRatio - upperAspectRatio);
			float lowerImageError = abs(imageAspectRatio - lowerAspectRatio);
			if (upperImageError < lowerImageError)
				scalingRatio.y = yUpperScaleFactor;
			else
				scalingRatio.y = yLowerScaleFactor;
		}
		// Added an extra check in here to prefer an aspect ratio above 1.0.
		// TODO: This will need to be looked at again for aspect ratios other than 4:3
		else if (lowerTargetError < upperTargetError || upperAspectRatio < 1.0)
			scalingRatio.y = yLowerScaleFactor;
		else
			scalingRatio.y = yUpperScaleFactor;
	}
	// If the ratio MA is greater than the target aspect ratio TA
	else if (maxAspectRatio > _TargetAspectRatio)
	{
		scalingRatio.y = maxIntRatio.y;
		float AUW = maxOutputSize.y * _TargetAspectRatio;

		float xUpperScaleFactor = ceil(AUW / imageSize.x);
		float xLowerScaleFactor = floor(AUW / imageSize.x);

		float upperAspectRatio = (xUpperScaleFactor * imageSize.x) / maxOutputSize.y;
		float lowerAspectRatio = (xLowerScaleFactor * imageSize.x) / maxOutputSize.y;

		float upperTargetError = abs(_TargetAspectRatio - upperAspectRatio);
		float lowerTargetError = abs(_TargetAspectRatio - lowerAspectRatio);

		if (abs(upperTargetError - lowerTargetError) < 0.001)
		{
			float upperImageError = abs(imageAspectRatio - upperAspectRatio);
			float lowerImageError = abs(imageAspectRatio - lowerAspectRatio);
			if (upperImageError < lowerImageError)
				scalingRatio.x = xUpperScaleFactor;
			else
				scalingRatio.x = xLowerScaleFactor;
		}
		// Added an extra check in here to prefer an aspect ratio above 1.0.
		// TODO: This will need to be looked at again for aspect ratios other than 4:3
		else if (upperTargetError < lowerTargetError || lowerAspectRatio < 1.0)
			scalingRatio.x = xUpperScaleFactor;
		else
			scalingRatio.x = xLowerScaleFactor;
	}
	// If the ratio MA is equal to the target aspect ratio TA
	else
		scalingRatio = maxIntRatio;

	if (scalingRatio.x < 1.0)
		scalingRatio.x = 1.0;
	if (scalingRatio.y < 1.0)
		scalingRatio.y = 1.0;

	return scalingRatio;
}

COMPAT_VARYING vec2 omega;
COMPAT_VARYING vec2 v_texCoord;

void main()
{
	gl_Position = a_position;

	//vec2 box_scale = vec2(5.0, 6.0);
	vec2 box_scale = calculateScalingRatio(rubyOutputSize, rubyInputSize, targetAspectRatio);
	vec2 scale = (rubyOutputSize / rubyInputSize) / box_scale;
	vec2 middle = vec2(0.5);
	vec2 TexCoord = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0;
	vec2 diff = (TexCoord - middle) * scale;

	outCoord = middle + diff;

#define pi 3.141592654

    omega = vec2(pi * rubyOutputSize.x * rubyTextureSize.x / rubyInputSize.x, 2.0 * pi * rubyTextureSize.y);

    v_texCoord = outCoord * rubyInputSize / rubyTextureSize;
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

uniform sampler2D rubyTexture;
uniform vec2 rubyTextureSize;

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

COMPAT_VARYING vec2 outCoord;

const float SCANLINE_BASE_BRIGHTNESS = 0.95;
const float SCANLINE_SINE_COMP_A = 0.05;
const float SCANLINE_SINE_COMP_B = 0.15;

COMPAT_VARYING vec2 omega;
COMPAT_VARYING vec2 v_texCoord;

void main()
{
    vec2 sine_comp = vec2(SCANLINE_SINE_COMP_A, SCANLINE_SINE_COMP_B);
    vec3 res = NN_TEXTURE(rubyTexture, v_texCoord).xyz;
    vec3 scanline = res * (SCANLINE_BASE_BRIGHTNESS + dot(sine_comp * sin(v_texCoord * omega), vec2(1.0, 1.0)));

	if ( outCoord.x >= 0.0 && outCoord.x <= 1.0 && outCoord.y >= 0.0 && outCoord.y <= 1.0)
		FragColor = vec4(scanline.x, scanline.y, scanline.z, 1.0);
	else
		// Can change the background filler colour below
		FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}

#endif
