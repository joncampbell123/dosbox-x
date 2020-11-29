#version 120

// ported from ReShade

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

void main()
{
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0 * rubyInputSize / rubyTextureSize;
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

vec4 cmp(vec4 src0, vec4 src1, vec4 src2) {
	return vec4(
		src0.x >= 0 ? src1.x : src2.x,
		src0.y >= 0 ? src1.y : src2.y,
		src0.z >= 0 ? src1.z : src2.z,
		src0.w >= 0 ? src1.w : src2.w
	);
}

#define saturate(c) clamp(c, 0.0, 1.0)

void main()
{
	//Declare parameters
	//pixelSize
	vec4 c0 = rubyInputSize.xyyy;
	//textureSize
	vec4 c1 = SourceSize;
	//viewSize
	vec4 c2 = OutSize;

	//Declare constants
	const vec4 c3 = vec4(1.5, 0.800000012, 1.25, 0.75);
	const vec4 c4 = vec4(6.28318548, -3.14159274, 0.25, -0.25);
	const vec4 c5 = vec4(1., 0.5, 720., 3.);
	const vec4 c6 = vec4(0.166666672, -0.333000004, -0.666000009, 0.899999976);
	const vec4 c7 = vec4(0.899999976, 1.10000002, 0., 0.);
	const vec4 c8 = vec4(-0.5, -0.25, 2., 0.5);

	//Declare registers
	vec4 r0, r1, r2, r3, r4, r5, r6, r7, r8, r9;

	//Code starts here
	vec4 v0 = vTexCoord.xyyy;
	//dcl_2d s0
	r0.x = 1.0 / c0.x;
	r0.y = 1.0 / c0.y;
	r0.xy = (r0 * c1).xy;
	r0.xy = (r0 * v0).xy;
	r0.xy = (r0 * c2).xy;
	r0.zw = fract(r0.xyxy).zw;
	r0.xy = (-r0.zwzw + r0).xy;
	r0.xy = (r0 + c8.wwww).xy;
	r0.x = r0.y * c5.w + r0.x;
	r0.x = r0.x * c6.x;
	r0.x = fract(r0.x);
	r0.xy = (r0.xxxx + c6.yzzw).xy;
	r1.yz = (r0.y >= 0 ? c7.xxyw : c7.xyxw).yz;
	r1.x = c6.w;
	r0.xyz = (r0.x >= 0 ? r1 : c7.yxxw).xyz;
	r1.xy = (c1 * v0).xy;
	r0.w = r1.y * c8.w + c8.w;
	r0.w = fract(r0.w);
	r0.w = r0.w * c4.x + c4.y;
	r2.y = sin(r0.w);
	r1.zw = (abs(r2).yyyy + c4).zw;
	r1.z = clamp(r1.z, 0.0, 1.0);
	r0.w = r1.w >= 0 ? r1.z : c8.w;
	r2 = fract(r1.xyxy);
	r1.xy = (r1 + -r2.zwzw).xy;
	r2 = r2 + c8.xxyy;
	r1.zw = (r1.xyxy + c8.wwww).zw;
	r1.zw = (v0.xyxy * -c1.xyxy + r1).zw;
	r1.w = r1.w + r1.w;
	r1.z = r1.z * c8.w;
	r1.z = -abs(r1).z + c3.x;
	r3.x = max(c3.y, r1.z);
	r4.x = min(r3.x, c3.z);
	r1.zw = (-abs(r1).wwww + c3).zw;
	r1.z = clamp(r1.z, 0.0, 1.0);
	r1.z = r1.w >= 0 ? r1.z : c8.w;
	r4.y = r0.w + r1.z;
	r0.w = r0.w * r4.x;
	r1.z = r1.z * r4.x;
	r3.xy = (r4 * c5).xy;
	r1.w = r3.y * r3.x;
	r2.z = cmp(r2, r2.xyxy, c8.yyyy).z;
	r3.xy = max(c8.yyyy, -r2.zwzw).xy;
	r2.xy = (r2 + r3).xy;
	r1.xy = (r2 * c8.zzzz + r1).xy;
	r1.xy = (r1 + c8.wwww).xy;
	r2.x = 1.0 / c1.x;
	r2.y = 1.0 / c1.y;
	r1.xy = (r1 * r2).xy;
	r2 = NN_TEXTURE(Source, r1.xy);
	r3.x = r0.w * r2.x;
	r3.yz = (r1.xzww * r2).yz;
	FragColor.w = r2.w;
	r0.xyz = (r0 * r3).xyz;
	r1.z = c5.z;
	r0.w = r1.z + -c2.y;
	FragColor.xyz = (r0.w >= 0 ? r3 : r0).xyz;
}
#endif
