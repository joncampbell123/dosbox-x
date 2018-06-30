/*
Copyright (C) 2003 Ryan A. Nunn

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

#pragma once

#include <string>
#include <d3d9.h>
#include <d3dx9.h>

#define SAFE_RELEASE(p)		{ if(p) { (p)->Release(); (p)=NULL; } }

class ScalingEffect
{
    float				m_scale;
    float				m_iDim[2];
    BOOL				m_forceupdate;
    LPCSTR				m_strName;
    std::string				m_strErrors;
    LPDIRECT3DDEVICE9			m_pd3dDevice;

    LPD3DXEFFECT			m_pEffect;
    D3DXEFFECT_DESC			m_EffectDesc;

    // Matrix Handles
    D3DXHANDLE m_MatWorldEffectHandle;
    D3DXHANDLE m_MatViewEffectHandle;
    D3DXHANDLE m_MatProjEffectHandle;
    D3DXHANDLE m_MatWorldViewEffectHandle;
    D3DXHANDLE m_MatViewProjEffectHandle;
    D3DXHANDLE m_MatWorldViewProjEffectHandle;

    // Texture Handles
    D3DXHANDLE m_SourceDimsEffectHandle;
    D3DXHANDLE m_TexelSizeEffectHandle;
    D3DXHANDLE m_SourceTextureEffectHandle;
    D3DXHANDLE m_WorkingTexture1EffectHandle;
    D3DXHANDLE m_WorkingTexture2EffectHandle;
    D3DXHANDLE m_Hq2xLookupTextureHandle;

    // Technique stuff
    D3DXHANDLE	m_PreprocessTechnique1EffectHandle;
    D3DXHANDLE	m_PreprocessTechnique2EffectHandle;
    D3DXHANDLE	m_CombineTechniqueEffectHandle;

public:
    enum Pass { Preprocess1, Preprocess2, Combine };

    ScalingEffect(LPDIRECT3DDEVICE9 pd3dDevice);
    ~ScalingEffect(void);

    void KillThis();

    HRESULT LoadEffect(const TCHAR *filename);

    const char *getErrors() { return m_strErrors.c_str(); }

    LPCSTR getName() { return m_strName; }
    float getScale() { return m_scale; }
    bool getForceUpdate() { return m_forceupdate != FALSE; }
    void setinputDim(float w, float h) { m_iDim[0] = w; m_iDim[1] = h; }

    // Does it have a preprocess step
    BOOL hasPreprocess() { return m_PreprocessTechnique1EffectHandle!=0; }
    BOOL hasPreprocess2() { return m_PreprocessTechnique2EffectHandle!=0; }

    // Set The Textures
    HRESULT SetTextures( LPDIRECT3DTEXTURE9 lpSource, LPDIRECT3DTEXTURE9 lpWorking1,
		LPDIRECT3DTEXTURE9 lpWorking2, LPDIRECT3DVOLUMETEXTURE9 lpHq2xLookupTexture );

    // Set the Matrices for this frame
    HRESULT SetMatrices( D3DXMATRIX &matProj, D3DXMATRIX &matView, D3DXMATRIX &matWorld );

    // Begin the technique
    // Returns Number of passes for this technique
    HRESULT Begin(Pass pass, UINT* pPasses);

    // Render Pass in technique
    HRESULT BeginPass(UINT Pass);
    HRESULT EndPass();

    // End rendering of this technique
    HRESULT End();

    // Validates the effect
    HRESULT Validate();

private:
    HRESULT ParseParameters(LPD3DXEFFECTCOMPILER lpEffectCompiler);
};
