/*
   Copyright (C) 2007 guest(r) - guest.r@gmail.com

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

*/


const float     halfpi     = 1.5707963267948966192313216916398;
const float         pi     = 3.1415926535897932384626433832795;


#include "shader.code"


string name : NAME = "Lanczos";
string combineTechique : COMBINETECHNIQUE =  "Lanczos";

// **VS**

VERTEX_STUFF_W S_VERTEX (float3 p : POSITION, float2 tc : TEXCOORD0)
{
  VERTEX_STUFF_W OUT = (VERTEX_STUFF_W)0;

  OUT.coord = mul(float4(p,1),WorldViewProjection);
  OUT.CT = tc;
  return OUT;
}


float l(float x)
{ 
  if (x==0.0) return pi*halfpi;
  else
  return sin(x*halfpi)*sin(x*pi)/(x*x);
}

// **PS**

float4 S_FRAGMENT ( in VERTEX_STUFF_W VAR ) : COLOR
{
	
  float2 crd[4][4];  float3 pix[4][4];

  float2 dx = float2(ps.x,0.0); float2 dy = float2(0.0,ps.y);
  
  float2 pixcoord  = VAR.CT/ps;
  float2 fract     = frac(pixcoord);
  float2 coord0    = VAR.CT-(fract)*ps;


// calculating coordinates for 16 texels
  
  crd[0][0]=coord0-ps;    crd[1][0]=crd[0][0]+dx; 
  crd[2][0]=crd[1][0]+dx; crd[3][0]=crd[2][0]+dx;
  crd[0][1]=crd[0][0]+dy; crd[1][1]=crd[0][1]+dx; 
  crd[2][1]=crd[1][1]+dx; crd[3][1]=crd[2][1]+dx;
  crd[0][2]=crd[0][1]+dy; crd[1][2]=crd[0][2]+dx; 
  crd[2][2]=crd[1][2]+dx; crd[3][2]=crd[2][2]+dx;
  crd[0][3]=crd[0][2]+dy; crd[1][3]=crd[0][3]+dx; 
  crd[2][3]=crd[1][3]+dx; crd[3][3]=crd[2][3]+dx; 


// calculating texel weights

  float a,b,c,d,p,q,r,s;

  a = l(1+fract.x); 
  b = l(  fract.x); 
  c = l(1-fract.x); 
  d = l(2-fract.x);

  p = l(1+fract.y); 
  q = l(  fract.y); 
  r = l(1-fract.y); 
  s = l(2-fract.y);


// reading the texels

  pix[0][0] = tex2D(s_p,crd[0][0]).xyz;
  pix[1][0] = tex2D(s_p,crd[1][0]).xyz;
  pix[2][0] = tex2D(s_p,crd[2][0]).xyz;
  pix[3][0] = tex2D(s_p,crd[3][0]).xyz;
  pix[0][1] = tex2D(s_p,crd[0][1]).xyz;
  pix[1][1] = tex2D(s_p,crd[1][1]).xyz;
  pix[2][1] = tex2D(s_p,crd[2][1]).xyz;
  pix[3][1] = tex2D(s_p,crd[3][1]).xyz;
  pix[0][2] = tex2D(s_p,crd[0][2]).xyz;
  pix[1][2] = tex2D(s_p,crd[1][2]).xyz;
  pix[2][2] = tex2D(s_p,crd[2][2]).xyz;
  pix[3][2] = tex2D(s_p,crd[3][2]).xyz;
  pix[0][3] = tex2D(s_p,crd[0][3]).xyz;
  pix[1][3] = tex2D(s_p,crd[1][3]).xyz;
  pix[2][3] = tex2D(s_p,crd[2][3]).xyz;
  pix[3][3] = tex2D(s_p,crd[3][3]).xyz;


// applying weights

  pix[0][0] = (-pix[0][0]*a+pix[1][0]*b+pix[2][0]*c-pix[3][0]*d)*p;
  pix[0][1] = ( pix[0][1]*a+pix[1][1]*b+pix[2][1]*c+pix[3][1]*d)*q;
  pix[0][2] = ( pix[0][2]*a+pix[1][2]*b+pix[2][2]*c+pix[3][2]*d)*r;
  pix[0][3] = (-pix[0][3]*a+pix[1][3]*b+pix[2][3]*c-pix[3][3]*d)*s;


// final sum and weight normalization
  
  return float4((pix[0][0]+pix[0][1]+pix[0][2]+pix[0][3])/((a+b+c+d)*(p+q+r+s)-2*(a+d)*(p+s)),1);

}

technique Lanczos
{
   pass P0
   {
     VertexShader = compile vs_3_0 S_VERTEX();
     PixelShader  = compile ps_3_0 S_FRAGMENT();
   }  
}
