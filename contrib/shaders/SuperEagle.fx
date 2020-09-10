/*
   SuperEagle shader 

           - Copyright (C) 2007 guest(r) - guest.r@gmail.com

           - License: GNU-GPL  


   The SuperEagle algorithm

           - Copyright (c) 1999-2001 by Derek Liauw Kie Fa.		

*/



#include "shader.code"


string name : NAME = "SuperEagle";
string combineTechique : COMBINETECHNIQUE =  "SuperEagle";


const float3 dtt = float3(65536,255,1);



/*  GET_RESULT function                            */
/*  Copyright (c) 1999-2001 by Derek Liauw Kie Fa  */
/*  License: GNU-GPL                               */


int GET_RESULT(float A, float B, float C, float D)
{
	int x = 0; int y = 0; int r = 0;
	if (A == C) x+=1; else if (B == C) y+=1;
	if (A == D) x+=1; else if (B == D) y+=1;
	if (x <= 1) r+=1; 
	if (y <= 1) r-=1;
	return r;
} 


float reduce(half3 color)
{ 
	return dot(color,dtt);
}



// **VS**

VERTEX_STUFF_W S_VERTEX (float3 p : POSITION, float2 tc : TEXCOORD0)
{
	VERTEX_STUFF_W OUT = (VERTEX_STUFF_W)0;

	OUT.coord = mul(float4(p,1),WorldViewProjection);
	OUT.CT = tc;
	return OUT;
}



// **PS**

float4 S_FRAGMENT ( in VERTEX_STUFF_W VAR ) : COLOR
{
	// calculating offsets, coordinates

	half2 dx = half2( ps.x, 0.0); 
	half2 dy = half2( 0.0, ps.y);
	half2 g1 = half2( ps.x,ps.y);
	half2 g2 = half2(-ps.x,ps.y);	
	
	float2 pixcoord  = VAR.CT/ps;
	float2 fp        = frac(pixcoord);
	float2 pC4       = VAR.CT-fp*ps;
	float2 pC8       = pC4+g1;

	

	// Reading the texels

	half3 C0 = tex2D(s_p,pC4-g1).xyz; 
	half3 C1 = tex2D(s_p,pC4-dy).xyz;
	half3 C2 = tex2D(s_p,pC4-g2).xyz;
	half3 D3 = tex2D(s_p,pC4-g2+dx).xyz;
	half3 C3 = tex2D(s_p,pC4-dx).xyz;
	half3 C4 = tex2D(s_p,pC4   ).xyz;
	half3 C5 = tex2D(s_p,pC4+dx).xyz;
	half3 D4 = tex2D(s_p,pC8-g2).xyz;
	half3 C6 = tex2D(s_p,pC4+g2).xyz;
	half3 C7 = tex2D(s_p,pC4+dy).xyz;
	half3 C8 = tex2D(s_p,pC4+g1).xyz;
	half3 D5 = tex2D(s_p,pC8+dx).xyz;
	half3 D0 = tex2D(s_p,pC4+g2+dy).xyz;
	half3 D1 = tex2D(s_p,pC8+g2).xyz;
	half3 D2 = tex2D(s_p,pC8+dy).xyz;
	half3 D6 = tex2D(s_p,pC8+g1).xyz;

	float3 p00,p10,p01,p11;


	// reducing half3 to float
	
	float c0 = reduce(C0);float c1 = reduce(C1);
	float c2 = reduce(C2);float c3 = reduce(C3);
	float c4 = reduce(C4);float c5 = reduce(C5);
	float c6 = reduce(C6);float c7 = reduce(C7);
	float c8 = reduce(C8);float d0 = reduce(D0);
	float d1 = reduce(D1);float d2 = reduce(D2);
	float d3 = reduce(D3);float d4 = reduce(D4);
	float d5 = reduce(D5);float d6 = reduce(D6);



	/*              SuperEagle code               */
	/*  Copied from the Dosbox source code        */
	/*  Copyright (C) 2002-2007  The DOSBox Team  */
	/*  License: GNU-GPL                          */
	/*  Adapted by guest(r) on 16.4.2007          */

       
	if (c4 != c8) {
                if (c7 == c5) {
                        p01 = p10 = C7;
                        if ((c6 == c7) || (c5 == c2)) {
                                p00 = 0.25*(3.0*C7+C4);
                        } else {
                                p00 = 0.5*(C4+C5);
                        }

                        if ((c5 == d4) || (c7 == d1)) {
                                p11 = 0.25*(3.0*C7+C8);
                        } else {
                                p11 = 0.5*(C7+C8);
                        }
                } else {
                        p11 = 0.125*(6.0*C8+C7+C5);
                        p00 = 0.125*(6.0*C4+C7+C5);

                        p10 = 0.125*(6.0*C7+C4+C8);
                        p01 = 0.125*(6.0*C5+C4+C8);
                }
         } else {
                if (c7 != c5) {
                        p11 = p00 = C4;

                        if ((c1 == c4) || (c8 == d5)) {
                                p01 = 0.25*(3.0*C4+C5);
                        } else {
                                p01 = 0.5*(C4+C5);
                        }

                        if ((c8 == d2) || (c3 == c4)) {
                                p10 = 0.25*(3.0*C4+C7);
                        } else {
                                p10 = 0.5*(C7+C8);
                        }
                } else {
                        int r = 0;
                        r += GET_RESULT(c5,c4,c6,d1);
                        r += GET_RESULT(c5,c4,c3,c1);
                        r += GET_RESULT(c5,c4,d2,d5);
                        r += GET_RESULT(c5,c4,c2,d4);

                        if (r > 0) {
                                p01 = p10 = C7;
                                p00 = p11 = 0.5*(C4+C5);
                        } else if (r < 0) {
                                p11 = p00 = C4;
                                p01 = p10 = 0.5*(C4+C5);
                        } else {
                                p11 = p00 = C4;
                                p01 = p10 = C7;
                        }
                }
        }



	// Distributing the four products
	
	if (fp.x < 0.50)
		{ if (fp.y < 0.50) p10 = p00;}
	else
		{ if (fp.y < 0.50) p10 = p01; else p10 = p11;}

	return float4(p10,1); 
}

technique SuperEagle
{
   pass P0
   {
     VertexShader = compile vs_3_0 S_VERTEX();
     PixelShader  = compile ps_3_0 S_FRAGMENT();
   }  
}