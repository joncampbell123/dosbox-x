#version 120

/*
	 Phosphor shader - Copyright (C) 2011 caligari.

	 Ported by Hyllian.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// Parameter lines go here:
// 0.5 = the spot stays inside the original pixel
// 1.0 = the spot bleeds up to the center of next pixel
#pragma parameter SPOT_WIDTH "CRTCaligari Spot Width" 0.9 0.1 1.5 0.05
#pragma parameter SPOT_HEIGHT "CRTCaligari Spot Height" 0.65 0.1 1.5 0.05
// Used to counteract the desaturation effect of weighting.
#pragma parameter COLOR_BOOST "CRTCaligari Color Boost" 1.45 1.0 2.0 0.05
// Constants used with gamma correction.
#pragma parameter InputGamma "CRTCaligari Input Gamma" 2.4 0.0 5.0 0.1
#pragma parameter OutputGamma "CRTCaligari Output Gamma" 2.2 0.0 5.0 0.1

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
COMPAT_ATTRIBUTE vec4 TexCoord;
COMPAT_VARYING vec2 v_texCoord;
COMPAT_VARYING vec2 onex;
COMPAT_VARYING vec2 oney;

vec4 _oPosition1;
uniform mat4 MVPMatrix;
uniform COMPAT_PRECISION int FrameDirection;
uniform COMPAT_PRECISION int rubyFrameCount;
uniform COMPAT_PRECISION vec2 rubyOutputSize;
uniform COMPAT_PRECISION vec2 rubyTextureSize;
uniform COMPAT_PRECISION vec2 rubyInputSize;

#define SourceSize vec4(rubyTextureSize, 1.0 / rubyTextureSize) //either rubyTextureSize or rubyInputSize

void main()
{
	gl_Position = a_position;
	v_texCoord = vec2(a_position.x + 1.0, 1.0 - a_position.y) / 2.0 * rubyInputSize / rubyTextureSize;

	onex = vec2(SourceSize.z, 0.0);
	oney = vec2(0.0, SourceSize.w);
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
COMPAT_VARYING vec2 onex;
COMPAT_VARYING vec2 oney;

// compatibility #defines
#define Source rubyTexture
#define vTexCoord v_texCoord.xy

#define SourceSize vec4(rubyTextureSize, 1.0 / rubyTextureSize) //either rubyTextureSize or rubyInputSize
#define rubyOutputSize vec4(rubyOutputSize, 1.0 / rubyOutputSize)

#ifdef PARAMETER_UNIFORM
// All parameter floats need to have COMPAT_PRECISION in front of them
uniform COMPAT_PRECISION float SPOT_WIDTH;
uniform COMPAT_PRECISION float SPOT_HEIGHT;
uniform COMPAT_PRECISION float COLOR_BOOST;
uniform COMPAT_PRECISION float InputGamma;
uniform COMPAT_PRECISION float OutputGamma;
#else
#define SPOT_WIDTH 0.9
#define SPOT_HEIGHT 0.65
#define COLOR_BOOST 1.45
#define InputGamma 2.4
#define OutputGamma 2.2
#endif

#define GAMMA_IN(color)     pow(color,vec4(InputGamma))
#define GAMMA_OUT(color)    pow(color, vec4(1.0 / OutputGamma))

#define TEX2D(coords)	GAMMA_IN( COMPAT_TEXTURE(Source, coords) )

// Macro for weights computing
#define WEIGHT(w) \
	if(w>1.0) w=1.0; \
w = 1.0 - w * w; \
w = w * w;

void main()
{
	vec2 coords = ( vTexCoord * SourceSize.xy );
	vec2 pixel_center = floor( coords ) + vec2(0.5, 0.5);
	vec2 texture_coords = pixel_center * SourceSize.zw;

	vec4 color = TEX2D( texture_coords );

	float dx = coords.x - pixel_center.x;

	float h_weight_00 = dx / SPOT_WIDTH;
	WEIGHT( h_weight_00 );

	color *= vec4( h_weight_00, h_weight_00, h_weight_00, h_weight_00  );

	// get closest horizontal neighbour to blend
	vec2 coords01;
	if (dx>0.0) {
		coords01 = onex;
		dx = 1.0 - dx;
	} else {
		coords01 = -onex;
		dx = 1.0 + dx;
	}
	vec4 colorNB = TEX2D( texture_coords + coords01 );

	float h_weight_01 = dx / SPOT_WIDTH;
	WEIGHT( h_weight_01 );

	color = color + colorNB * vec4( h_weight_01 );

	//////////////////////////////////////////////////////
	// Vertical Blending
	float dy = coords.y - pixel_center.y;
	float v_weight_00 = dy / SPOT_HEIGHT;
	WEIGHT( v_weight_00 );
	color *= vec4( v_weight_00 );

	// get closest vertical neighbour to blend
	vec2 coords10;
	if (dy>0.0) {
		coords10 = oney;
		dy = 1.0 - dy;
	} else {
		coords10 = -oney;
		dy = 1.0 + dy;
	}
	colorNB = TEX2D( texture_coords + coords10 );

	float v_weight_10 = dy / SPOT_HEIGHT;
	WEIGHT( v_weight_10 );

	color = color + colorNB * vec4( v_weight_10 * h_weight_00, v_weight_10 * h_weight_00, v_weight_10 * h_weight_00, v_weight_10 * h_weight_00 );

	colorNB = TEX2D(  texture_coords + coords01 + coords10 );

	color = color + colorNB * vec4( v_weight_10 * h_weight_01, v_weight_10 * h_weight_01, v_weight_10 * h_weight_01, v_weight_10 * h_weight_01 );

	color *= vec4( COLOR_BOOST );

	FragColor = clamp( GAMMA_OUT(color), 0.0, 1.0 );
}
#endif
