#version 120

/*
	CRT Shader by EasyMode
	License: GPL
*/

#pragma parameter SHARPNESS_IMAGE "Sharpness Image" 1.0 1.0 5.0 1.0
#pragma parameter SHARPNESS_EDGES "Sharpness Edges" 3.0 1.0 5.0 1.0
#pragma parameter GLOW_WIDTH "Glow Width" 0.5 0.05 0.65 0.05
#pragma parameter GLOW_HEIGHT "Glow Height" 0.5 0.05 0.65 0.05
#pragma parameter GLOW_HALATION "Glow Halation" 0.1 0.0 1.0 0.01
#pragma parameter GLOW_DIFFUSION "Glow Diffusion" 0.05 0.0 1.0 0.01
#pragma parameter MASK_COLORS "Mask Colors" 2.0 2.0 3.0 1.0
#pragma parameter MASK_STRENGTH "Mask Strength" 0.3 0.0 1.0 0.05
#pragma parameter MASK_SIZE "Mask Size" 1.0 1.0 9.0 1.0
#pragma parameter SCANLINE_SIZE_MIN "Scanline Size Min." 0.5 0.5 1.5 0.05
#pragma parameter SCANLINE_SIZE_MAX "Scanline Size Max." 1.5 0.5 1.5 0.05
#pragma parameter SCANLINE_SHAPE "Scanline Shape" 2.5 1.0 100.0 0.1
#pragma parameter SCANLINE_OFFSET "Scanline Offset" 1.0 0.0 1.0 1.0
#pragma parameter GAMMA_INPUT "Gamma Input" 2.4 1.0 5.0 0.1
#pragma parameter GAMMA_OUTPUT "Gamma Output" 2.4 1.0 5.0 0.1
#pragma parameter BRIGHTNESS "Brightness" 1.5 0.0 2.0 0.05

#define Coord v_texCoord

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

IN  vec4 a_position;
IN  vec4 Color;
IN  vec2 TexCoord;
OUT vec4 color;
OUT vec2 Coord;

uniform PRECISION vec2 rubyTextureSize;
uniform PRECISION vec2 rubyInputSize;

void main()
{
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0 * rubyInputSize / rubyTextureSize;

	color = Color;
	Coord = v_texCoord * 1.0001;
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
IN vec2 Coord;

#ifdef PARAMETER_UNIFORM
uniform PRECISION float SHARPNESS_IMAGE;
uniform PRECISION float SHARPNESS_EDGES;
uniform PRECISION float GLOW_WIDTH;
uniform PRECISION float GLOW_HEIGHT;
uniform PRECISION float GLOW_HALATION;
uniform PRECISION float GLOW_DIFFUSION;
uniform PRECISION float MASK_COLORS;
uniform PRECISION float MASK_STRENGTH;
uniform PRECISION float MASK_SIZE;
uniform PRECISION float SCANLINE_SIZE_MIN;
uniform PRECISION float SCANLINE_SIZE_MAX;
uniform PRECISION float SCANLINE_SHAPE;
uniform PRECISION float SCANLINE_OFFSET;
uniform PRECISION float GAMMA_INPUT;
uniform PRECISION float GAMMA_OUTPUT;
uniform PRECISION float BRIGHTNESS;
#else
#define SHARPNESS_IMAGE 1.0
#define SHARPNESS_EDGES 3.0
#define GLOW_WIDTH 0.5
#define GLOW_HEIGHT 0.5
#define GLOW_HALATION 0.1
#define GLOW_DIFFUSION 0.05
#define MASK_COLORS 2.0
#define MASK_STRENGTH 0.3
#define MASK_SIZE 1.0
#define SCANLINE_SIZE_MIN 0.5
#define SCANLINE_SIZE_MAX 1.5
#define SCANLINE_SHAPE 1.5
#define SCANLINE_OFFSET 1.0
#define GAMMA_INPUT 2.4
#define GAMMA_OUTPUT 2.4
#define BRIGHTNESS 1.5
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
#define NN_TEXTURE tex2D
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
	vec4 c_s0t0 = tex2D(sampler, s0t0 * invTexSize);
	vec4 c_s0t1 = tex2D(sampler, s0t1 * invTexSize);
	vec4 c_s1t0 = tex2D(sampler, s1t0 * invTexSize);
	vec4 c_s1t1 = tex2D(sampler, s1t1 * invTexSize);

	vec2 weight = fract(texCoord);

	vec4 c0 = c_s0t0 + (c_s1t0 - c_s0t0) * weight.x;
	vec4 c1 = c_s0t1 + (c_s1t1 - c_s0t1) * weight.x;

	return (c0 + (c1 - c0) * weight.y);
}
#else
#define BL_TEXTURE tex2D
#define NN_TEXTURE nnTexture
vec4 nnTexture(in sampler2D sampler, in vec2 uv)
{
	vec2 texCoord = floor(uv * rubyTextureSize) + vec2(0.5);
	vec2 invTexSize = 1.0 / rubyTextureSize;
	return tex2D(sampler, texCoord * invTexSize);
}
#endif

#define FIX(c) max(abs(c), 1e-5)
#define PI 3.141592653589
#define saturate(c) clamp(c, 0.0, 1.0)
#define TEX2D(c) pow(NN_TEXTURE(tex, c).rgb, vec3(GAMMA_INPUT))

mat3 get_color_matrix(sampler2D tex, vec2 co, vec2 dx)
{
	return mat3(TEX2D(co - dx), TEX2D(co), TEX2D(co + dx));
}

vec3 blur(mat3 m, float dist, float rad)
{
	vec3 x = vec3(dist - 1.0, dist, dist + 1.0) / rad;
	vec3 w = exp2(x * x * -1.0);

	return (m[0] * w.x + m[1] * w.y + m[2] * w.z) / (w.x + w.y + w.z);
}

vec3 filter_gaussian(sampler2D tex, vec2 co, vec2 tex_size)
{
	vec2 dx = vec2(1.0 / tex_size.x, 0.0);
	vec2 dy = vec2(0.0, 1.0 / tex_size.y);
	vec2 pix_co = co * tex_size;
	vec2 tex_co = (floor(pix_co) + 0.5) / tex_size;
	vec2 dist = (fract(pix_co) - 0.5) * -1.0;

	mat3 line0 = get_color_matrix(tex, tex_co - dy, dx);
	mat3 line1 = get_color_matrix(tex, tex_co, dx);
	mat3 line2 = get_color_matrix(tex, tex_co + dy, dx);
	mat3 column = mat3(blur(line0, dist.x, GLOW_WIDTH),
							   blur(line1, dist.x, GLOW_WIDTH),
							   blur(line2, dist.x, GLOW_WIDTH));

	return blur(column, dist.y, GLOW_HEIGHT);
}

vec3 filter_lanczos(sampler2D tex, vec2 co, vec2 tex_size, float sharp)
{
	tex_size.x *= sharp;

	vec2 dx = vec2(1.0 / tex_size.x, 0.0);
	vec2 pix_co = co * tex_size - vec2(0.5, 0.0);
	vec2 tex_co = (floor(pix_co) + vec2(0.5, 0.0)) / tex_size;
	vec2 dist = fract(pix_co);
	vec4 coef = PI * vec4(dist.x + 1.0, dist.x, dist.x - 1.0, dist.x - 2.0);

	coef = FIX(coef);
	coef = 2.0 * sin(coef) * sin(coef / 2.0) / (coef * coef);
	coef /= dot(coef, vec4(1.0));

	vec4 col1 = vec4(TEX2D(tex_co), 1.0);
	vec4 col2 = vec4(TEX2D(tex_co + dx), 1.0);

	return (mat4(col1, col1, col2, col2) * coef).rgb;
}

vec3 get_scanline_weight(float x, vec3 col)
{
	vec3 beam = mix(vec3(SCANLINE_SIZE_MIN), vec3(SCANLINE_SIZE_MAX), pow(col, vec3(1.0 / SCANLINE_SHAPE)));
	vec3 x_mul = 2.0 / beam;
	vec3 x_offset = x_mul * 0.5;

	return smoothstep(0.0, 1.0, 1.0 - abs(x * x_mul - x_offset)) * x_offset;
}

vec3 get_mask_weight(float x)
{
	float i = mod(floor(x * rubyOutputSize.x * rubyTextureSize.x / (rubyInputSize.x * MASK_SIZE)), MASK_COLORS);

	if (i == 0.0) return mix(vec3(1.0, 0.0, 1.0), vec3(1.0, 0.0, 0.0), MASK_COLORS - 2.0);
	else if (i == 1.0) return vec3(0.0, 1.0, 0.0);
	else return vec3(0.0, 0.0, 1.0);
}

void main()
{
	float scale = floor((rubyOutputSize.y / rubyInputSize.y) + 0.001);
	float offset = 1.0 / scale * 0.5;

	if (bool(mod(scale, 2.0))) offset = 0.0;

	vec2 co = (Coord * rubyTextureSize - vec2(0.0, offset * SCANLINE_OFFSET)) / rubyTextureSize;

	vec3 col_glow = filter_gaussian(rubyTexture, co, rubyTextureSize);
	vec3 col_soft = filter_lanczos(rubyTexture, co, rubyTextureSize, SHARPNESS_IMAGE);
	vec3 col_sharp = filter_lanczos(rubyTexture, co, rubyTextureSize, SHARPNESS_EDGES);
	vec3 col = sqrt(col_sharp * col_soft);

	col *= get_scanline_weight(fract(co.y * rubyTextureSize.y), col_soft);
	col_glow = saturate(col_glow - col);
	col += col_glow * col_glow * GLOW_HALATION;
	col = mix(col, col * get_mask_weight(co.x) * MASK_COLORS, MASK_STRENGTH);
	col += col_glow * GLOW_DIFFUSION;
	col = pow(col * BRIGHTNESS, vec3(1.0 / GAMMA_OUTPUT));

	FragColor = vec4(col, 1.0);
}

#endif
