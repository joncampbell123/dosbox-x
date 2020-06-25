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

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include "dosbox.h"

#if (HAVE_D3D9_H) && (C_D3DSHADERS) && defined(WIN32)

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
#endif

#include "ScalingEffect.h"

ScalingEffect::ScalingEffect(LPDIRECT3DDEVICE9 pd3dDevice) :
    m_scale(0.0f), m_forceupdate(false), m_pd3dDevice(pd3dDevice), m_pEffect(0)
{
    m_iDim[0] = m_iDim[1] = 256.0f;	// Some sane default
    m_strName = "Unnamed";
    KillThis();
}

ScalingEffect::~ScalingEffect(void)
{
    KillThis();
}

void ScalingEffect::KillThis()
{
    SAFE_RELEASE(m_pEffect);
    m_strErrors.clear();
    m_strName = "Unnamed";

    m_MatWorldEffectHandle = 0;
    m_MatViewEffectHandle = 0;
    m_MatProjEffectHandle = 0;
    m_MatWorldViewEffectHandle = 0;
    m_MatViewProjEffectHandle = 0;
    m_MatWorldViewProjEffectHandle = 0;

    // Source Texture Handles
    m_SourceDimsEffectHandle = 0;
    m_TexelSizeEffectHandle = 0;
    m_SourceTextureEffectHandle = 0;
    m_WorkingTexture1EffectHandle = 0;
    m_WorkingTexture2EffectHandle = 0;
    m_Hq2xLookupTextureHandle = 0;

    // Technique stuff
    m_PreprocessTechnique1EffectHandle = 0;
    m_PreprocessTechnique2EffectHandle = 0;
    m_CombineTechniqueEffectHandle = 0;
}

HRESULT ScalingEffect::LoadEffect(const TCHAR *filename)
{
    KillThis();

    LPD3DXBUFFER		lpBufferEffect = 0;
    LPD3DXBUFFER		lpErrors = 0;
    LPD3DXEFFECTCOMPILER	lpEffectCompiler = 0;

    m_strErrors += filename;
    m_strErrors += ":\n";

    // First create an effect compiler
    HRESULT hr = D3DXCreateEffectCompilerFromFile(filename, NULL, NULL, 0,
						&lpEffectCompiler, &lpErrors);

    // Errors...
    if(FAILED(hr)) {
	if(lpErrors) {
	    m_strErrors += (char*) lpErrors->GetBufferPointer();
	    SAFE_RELEASE(lpErrors);
	}

	m_strErrors += "Unable to create effect compiler from ";
	m_strErrors += filename;
    }

    if(SUCCEEDED(hr)) {
#ifdef C_D3DSHADERS_COMPILE_WITH_DEBUG
	hr = lpEffectCompiler->CompileEffect(D3DXSHADER_DEBUG, &lpBufferEffect, &lpErrors);
#else
	hr = lpEffectCompiler->CompileEffect(0, &lpBufferEffect, &lpErrors);
#endif

	// Errors...
	if(FAILED(hr)) {
	    if(lpErrors) {
		m_strErrors += (char*) lpErrors->GetBufferPointer();
		SAFE_RELEASE(lpErrors);
	    }

	    m_strErrors += "Unable to compile effect from ";
	    m_strErrors += filename;
	}
    }

    if(SUCCEEDED(hr)) {
	hr = D3DXCreateEffect(m_pd3dDevice,
			    lpBufferEffect->GetBufferPointer(),
			    lpBufferEffect->GetBufferSize(),
			    NULL, NULL,
			    0,
			    NULL, &m_pEffect, &lpErrors);

	// Errors...
	if(FAILED(hr)) {
	    if(lpErrors) {
		m_strErrors += (char*) lpErrors->GetBufferPointer();
		SAFE_RELEASE(lpErrors);
	    }

	    m_strErrors += "Unable to create effect from compiled ";
	    m_strErrors += filename;
	}
    }

    if(SUCCEEDED(hr)) {
        m_pEffect->GetDesc(&m_EffectDesc);
	hr = ParseParameters(lpEffectCompiler);
    }

    SAFE_RELEASE(lpErrors);
    SAFE_RELEASE(lpBufferEffect);
    SAFE_RELEASE(lpEffectCompiler);
    return hr;
}

HRESULT ScalingEffect::ParseParameters(LPD3DXEFFECTCOMPILER lpEffectCompiler)
{
    HRESULT hr = S_OK;

    if(m_pEffect == NULL)
        return E_FAIL;

    // Look at parameters for semantics and annotations that we know how to interpret
    D3DXPARAMETER_DESC ParamDesc;
    D3DXPARAMETER_DESC AnnotDesc;
    D3DXHANDLE hParam;
    D3DXHANDLE hAnnot;
    LPDIRECT3DBASETEXTURE9 pTex = NULL;

    for(UINT iParam = 0; iParam < m_EffectDesc.Parameters; iParam++) {
        LPCSTR pstrName = NULL;
        LPCSTR pstrFunction = NULL;
        LPCSTR pstrTarget = NULL;
        LPCSTR pstrTextureType = NULL;
        INT Width = D3DX_DEFAULT;
        INT Height= D3DX_DEFAULT;
        INT Depth = D3DX_DEFAULT;

        hParam = m_pEffect->GetParameter(NULL, iParam);
        m_pEffect->GetParameterDesc(hParam, &ParamDesc);

	if(ParamDesc.Semantic != NULL) {
	    if(ParamDesc.Class == D3DXPC_MATRIX_ROWS || ParamDesc.Class == D3DXPC_MATRIX_COLUMNS) {
		if(strcmpi(ParamDesc.Semantic, "world") == 0)
		    m_MatWorldEffectHandle = hParam;
		else if(strcmpi(ParamDesc.Semantic, "view") == 0)
		    m_MatViewEffectHandle = hParam;
		else if(strcmpi(ParamDesc.Semantic, "projection") == 0)
		    m_MatProjEffectHandle = hParam;
		else if(strcmpi(ParamDesc.Semantic, "worldview") == 0)
		    m_MatWorldViewEffectHandle = hParam;
		else if(strcmpi(ParamDesc.Semantic, "viewprojection") == 0)
		    m_MatViewProjEffectHandle = hParam;
		else if(strcmpi(ParamDesc.Semantic, "worldviewprojection") == 0)
		    m_MatWorldViewProjEffectHandle = hParam;
	    }

	    else if(ParamDesc.Class == D3DXPC_VECTOR && ParamDesc.Type == D3DXPT_FLOAT) {
		if(strcmpi(ParamDesc.Semantic, "sourcedims") == 0)
		    m_SourceDimsEffectHandle = hParam;
		else if(strcmpi(ParamDesc.Semantic, "texelsize") == 0)
		    m_TexelSizeEffectHandle = hParam;
		else if(strcmpi(ParamDesc.Semantic, "inputdims") == 0) {
		    if(m_iDim[0] && m_iDim[1] && ((hr = m_pEffect->SetFloatArray(hParam, m_iDim, 2)) != D3D_OK)) {
			m_strErrors += "Could not set inputdims parameter!";
			return hr;
		    }
		}
	    }

	    else if(ParamDesc.Class == D3DXPC_SCALAR && ParamDesc.Type == D3DXPT_FLOAT) {
		if(strcmpi(ParamDesc.Semantic, "SCALING") == 0)
		    m_pEffect->GetFloat(hParam, &m_scale);
	    }

	    else if(ParamDesc.Class == D3DXPC_SCALAR && ParamDesc.Type == D3DXPT_BOOL) {
		if(strcmpi(ParamDesc.Semantic, "FORCEUPDATE") == 0)
		    m_pEffect->GetBool(hParam, &m_forceupdate);
	    }

	    else if(ParamDesc.Class == D3DXPC_OBJECT && ParamDesc.Type == D3DXPT_TEXTURE) {
		if(strcmpi(ParamDesc.Semantic, "SOURCETEXTURE") == 0)
		    m_SourceTextureEffectHandle = hParam;
		if(strcmpi(ParamDesc.Semantic, "WORKINGTEXTURE") == 0)
		    m_WorkingTexture1EffectHandle = hParam;
		if(strcmpi(ParamDesc.Semantic, "WORKINGTEXTURE1") == 0)
		    m_WorkingTexture2EffectHandle = hParam;
		if(strcmpi(ParamDesc.Semantic, "HQ2XLOOKUPTEXTURE") == 0)
		    m_Hq2xLookupTextureHandle = hParam;
	    }

	    else if(ParamDesc.Class == D3DXPC_OBJECT && ParamDesc.Type == D3DXPT_STRING) {
	        LPCSTR pstrTechnique = NULL;

		if(strcmpi(ParamDesc.Semantic, "COMBINETECHNIQUE") == 0) {
		    m_pEffect->GetString(hParam, &pstrTechnique);
		    m_CombineTechniqueEffectHandle = m_pEffect->GetTechniqueByName(pstrTechnique);
		}
		else if(strcmpi(ParamDesc.Semantic, "PREPROCESSTECHNIQUE") == 0) {
		    m_pEffect->GetString(hParam, &pstrTechnique);
		    m_PreprocessTechnique1EffectHandle = m_pEffect->GetTechniqueByName(pstrTechnique);
		}
		else if(strcmpi(ParamDesc.Semantic, "PREPROCESSTECHNIQUE1") == 0) {
		    m_pEffect->GetString(hParam, &pstrTechnique);
		    m_PreprocessTechnique2EffectHandle = m_pEffect->GetTechniqueByName(pstrTechnique);
		}
		else if(strcmpi(ParamDesc.Semantic, "NAME") == 0)
		    m_pEffect->GetString(hParam, &m_strName);

	    }


	}

	for(UINT iAnnot = 0; iAnnot < ParamDesc.Annotations; iAnnot++) {
            hAnnot = m_pEffect->GetAnnotation (hParam, iAnnot);
            m_pEffect->GetParameterDesc(hAnnot, &AnnotDesc);
            if(strcmpi(AnnotDesc.Name, "name") == 0)
                m_pEffect->GetString(hAnnot, &pstrName);
            else if(strcmpi(AnnotDesc.Name, "function") == 0)
                m_pEffect->GetString(hAnnot, &pstrFunction);
            else if(strcmpi(AnnotDesc.Name, "target") == 0)
                m_pEffect->GetString(hAnnot, &pstrTarget);
            else if(strcmpi(AnnotDesc.Name, "width") == 0)
                m_pEffect->GetInt(hAnnot, &Width);
            else if(strcmpi(AnnotDesc.Name, "height") == 0)
                m_pEffect->GetInt(hAnnot, &Height);
            else if(strcmpi(AnnotDesc.Name, "depth") == 0)
                m_pEffect->GetInt(hAnnot, &Depth);
            else if(strcmpi(AnnotDesc.Name, "type") == 0)
                m_pEffect->GetString(hAnnot, &pstrTextureType);

        }

	// Not used in DOSBox
/*	if(pstrName != NULL) {
            pTex = NULL;
            DXUtil_ConvertAnsiStringToGenericCch(strPath, pstrName, MAX_PATH);
            if(pstrTextureType != NULL) {
                if(strcmpi(pstrTextureType, "volume") == 0) {
                    LPDIRECT3DVOLUMETEXTURE9 pVolumeTex = NULL;
                    if(SUCCEEDED(hr = D3DXCreateVolumeTextureFromFileEx(m_pd3dDevice, strPath,
                            Width, Height, Depth, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                            D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, &pVolumeTex))) {
                        pTex = pVolumeTex;
                    }
                    else {
                        m_strErrors += "Could not load volume texture ";
                        m_strErrors += pstrName;
                    }
                }
                else if(strcmpi(pstrTextureType, "cube") == 0) {
                    LPDIRECT3DCUBETEXTURE9 pCubeTex = NULL;
                    if(SUCCEEDED(hr = D3DXCreateCubeTextureFromFileEx(m_pd3dDevice, strPath,
                            Width, D3DX_DEFAULT, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                            D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, &pCubeTex))) {
                        pTex = pCubeTex;
                    }
                    else {
                        m_strErrors += "Could not load cube texture ";
                        m_strErrors += pstrName;
                    }
                }
            }
            else {
                LPDIRECT3DTEXTURE9 p2DTex = NULL;
                if(SUCCEEDED(hr = D3DXCreateTextureFromFileEx(m_pd3dDevice, strPath,
                        Width, Height, D3DX_DEFAULT, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                        D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, &p2DTex))) {
                    pTex = p2DTex;
                }
                else {
                    m_strErrors += "Could not load texture ";
                    m_strErrors += pstrName;
                    m_strErrors += "\n";
                }
            }

            // Apply successfully loaded texture to effect
            if(SUCCEEDED(hr) && pTex != NULL) {
                m_pEffect->SetTexture(m_pEffect->GetParameter(NULL, iParam), pTex);
                SAFE_RELEASE(pTex);
            }
        }
	else */
	if(pstrFunction != NULL) {
	    LPD3DXBUFFER pTextureShader = NULL;
	    LPD3DXBUFFER lpErrors = 0;

	    if(pstrTarget == NULL || strcmp(pstrTarget,"tx_1_1"))
                pstrTarget = "tx_1_0";

	    if(SUCCEEDED(hr = lpEffectCompiler->CompileShader(
				pstrFunction, pstrTarget,
				0, &pTextureShader, &lpErrors, NULL))) {
		SAFE_RELEASE(lpErrors);

		if((UINT)Width == D3DX_DEFAULT)
                    Width = 64;
		if((UINT)Height == D3DX_DEFAULT)
                    Height = 64;
		if((UINT)Depth == D3DX_DEFAULT)
                    Depth = 64;

#if D3DX_SDK_VERSION >= 22
		LPD3DXTEXTURESHADER ppTextureShader;
		D3DXCreateTextureShader((DWORD *)pTextureShader->GetBufferPointer(), &ppTextureShader);
#endif

		if(pstrTextureType != NULL) {
                    if(strcmpi(pstrTextureType, "volume") == 0) {
                        LPDIRECT3DVOLUMETEXTURE9 pVolumeTex = NULL;
                        if(SUCCEEDED(hr = D3DXCreateVolumeTexture(m_pd3dDevice,
                                Width, Height, Depth, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &pVolumeTex))) {
#if D3DX_SDK_VERSION >= 22
                           if(SUCCEEDED(hr = D3DXFillVolumeTextureTX(pVolumeTex, ppTextureShader))) {
#else
                           if(SUCCEEDED(hr = D3DXFillVolumeTextureTX(pVolumeTex,
					(DWORD *)pTextureShader->GetBufferPointer(), NULL, 0))) {
#endif
                                pTex = pVolumeTex;
                            }
                        }
                    } else if(strcmpi(pstrTextureType, "cube") == 0) {
                        LPDIRECT3DCUBETEXTURE9 pCubeTex = NULL;
                        if(SUCCEEDED(hr = D3DXCreateCubeTexture(m_pd3dDevice,
                                Width, D3DX_DEFAULT, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &pCubeTex))) {
#if D3DX_SDK_VERSION >= 22
                            if(SUCCEEDED(hr = D3DXFillCubeTextureTX(pCubeTex, ppTextureShader))) {
#else
                            if(SUCCEEDED(hr = D3DXFillCubeTextureTX(pCubeTex,
                                (DWORD *)pTextureShader->GetBufferPointer(), NULL, 0))) {
#endif
                                pTex = pCubeTex;
                            }
                        }
                    }
		} else {
                    LPDIRECT3DTEXTURE9 p2DTex = NULL;
                    if(SUCCEEDED(hr = D3DXCreateTexture(m_pd3dDevice, Width, Height,
                                D3DX_DEFAULT, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &p2DTex))) {
#if D3DX_SDK_VERSION >= 22
                        if(SUCCEEDED(hr = D3DXFillTextureTX(p2DTex, ppTextureShader))) {
#else
                        if(SUCCEEDED(hr = D3DXFillTextureTX(p2DTex,
				(DWORD *)pTextureShader->GetBufferPointer(), NULL, 0))) {
#endif
                            pTex = p2DTex;
                        }
                    }
		}
		m_pEffect->SetTexture(m_pEffect->GetParameter(NULL, iParam), pTex);
		SAFE_RELEASE(pTex);
		SAFE_RELEASE(pTextureShader);
#if D3DX_SDK_VERSION >= 22
		SAFE_RELEASE(ppTextureShader);
#endif
	    } else {
		if(lpErrors) {
		    m_strErrors += (char*) lpErrors->GetBufferPointer();
		}

		m_strErrors += "Could not compile texture shader ";
		m_strErrors += pstrFunction;

		SAFE_RELEASE(lpErrors);
		return hr;
	    }
        }
    }

    return S_OK;
}

// Set Source texture
HRESULT ScalingEffect::SetTextures(LPDIRECT3DTEXTURE9 lpSource, LPDIRECT3DTEXTURE9 lpWorking1,
			LPDIRECT3DTEXTURE9 lpWorking2, LPDIRECT3DVOLUMETEXTURE9 lpHq2xLookupTexture)
{
    if(!m_SourceTextureEffectHandle) {
	m_strErrors += "Texture with SOURCETEXTURE semantic not found";
	return E_FAIL;
    }

    HRESULT hr = m_pEffect->SetTexture(m_SourceTextureEffectHandle, lpSource);

    if(FAILED(hr)) {
	m_strErrors += "Unable to set SOURCETEXTURE";
	return hr;
    }

    if(m_WorkingTexture1EffectHandle) {
	hr = m_pEffect->SetTexture(m_WorkingTexture1EffectHandle, lpWorking1);

	if(FAILED(hr)) {
	    m_strErrors += "Unable to set WORKINGTEXTURE";
	    return hr;
	}
    }

    if(m_WorkingTexture2EffectHandle) {
	hr = m_pEffect->SetTexture(m_WorkingTexture2EffectHandle, lpWorking2);

	if(FAILED(hr)) {
	    m_strErrors += "Unable to set WORKINGTEXTURE1";
	    return hr;
	}
    }

    if(m_Hq2xLookupTextureHandle) {
	hr = m_pEffect->SetTexture(m_Hq2xLookupTextureHandle, lpHq2xLookupTexture);

	if(FAILED(hr)) {
	    m_strErrors += "Unable to set HQ2XLOOKUPTEXTURE";
	    return hr;
	}
    }

    D3DXVECTOR4 fDims(256,256,1,1), fTexelSize(1,1,1,1);

    if(lpSource) {
	D3DSURFACE_DESC Desc;
	lpSource->GetLevelDesc(0, &Desc);
	fDims[0] = (FLOAT) Desc.Width;
	fDims[1] = (FLOAT) Desc.Height;
    }

    fTexelSize[0] = (FLOAT)(1.0/fDims[0]);
    fTexelSize[1] = (FLOAT)(1.0/fDims[1]);

    if(m_SourceDimsEffectHandle) {
	hr = m_pEffect->SetVector(m_SourceDimsEffectHandle, &fDims);

	if(FAILED(hr)) {
	    m_strErrors += "Unable to set SOURCEDIMS";
	    return hr;
	}
    }

    if(m_TexelSizeEffectHandle) {
	hr = m_pEffect->SetVector(m_TexelSizeEffectHandle, &fTexelSize);

	if(FAILED(hr)) {
	    m_strErrors += "Unable to set TEXELSIZE";
	    return hr;
	}
    }

    return hr;
}

// Set the Matrices for this frame
HRESULT ScalingEffect::SetMatrices(D3DXMATRIX &matProj, D3DXMATRIX &matView, D3DXMATRIX &matWorld)
{
    HRESULT hr = S_OK;

    if(m_MatWorldEffectHandle != NULL) {
	hr = m_pEffect->SetMatrix(m_MatWorldEffectHandle, &matWorld);
	if(FAILED(hr)) {
	    m_strErrors += "Unable to set WORLD matrix";
	    return hr;
	}
    }

    if(m_MatViewEffectHandle != NULL) {
	hr = m_pEffect->SetMatrix(m_MatViewEffectHandle, &matView);
	if(FAILED(hr)) {
	    m_strErrors += "Unable to set VIEW matrix";
	    return hr;
	}
    }

    if(m_MatProjEffectHandle != NULL) {
	hr = m_pEffect->SetMatrix(m_MatProjEffectHandle, &matProj);
	if(FAILED(hr)) {
	    m_strErrors += "Unable to set PROJECTION matrix";
	    return hr;
	}
    }

    if(m_MatWorldViewEffectHandle != NULL) {
	D3DXMATRIX matWorldView = matWorld * matView;
	hr = m_pEffect->SetMatrix(m_MatWorldViewEffectHandle, &matWorldView);
	if(FAILED(hr)) {
	    m_strErrors += "Unable to set WORLDVIEW matrix";
	    return hr;
	}
    }

    if(m_MatViewProjEffectHandle != NULL) {
	D3DXMATRIX matViewProj = matView * matProj;
	hr = m_pEffect->SetMatrix(m_MatViewProjEffectHandle, &matViewProj);
	if(FAILED(hr)) {
	    m_strErrors += "Unable to set VIEWPROJECTION matrix";
	    return hr;
	}
    }

    if(m_MatWorldViewProjEffectHandle != NULL) {
	D3DXMATRIX matWorldViewProj = matWorld * matView * matProj;
	hr = m_pEffect->SetMatrix(m_MatWorldViewProjEffectHandle, &matWorldViewProj);
	if(FAILED(hr)) {
	    m_strErrors += "Unable to set WORLDVIEWPROJECTION matrix";
	    return hr;
	}
    }

    return hr;
}


// Returns Number of passes for this technique
HRESULT ScalingEffect::Begin(Pass pass, UINT* pPasses)
{
    HRESULT hr = S_OK;

    switch (pass) {
	case Preprocess1:
	    hr = m_pEffect->SetTechnique(m_PreprocessTechnique1EffectHandle);
	    break;
	case Preprocess2:
	    hr = m_pEffect->SetTechnique(m_PreprocessTechnique2EffectHandle);
	    break;
	case Combine:
	    hr = m_pEffect->SetTechnique(m_CombineTechniqueEffectHandle);
	    break;
    }

    if(FAILED(hr)) {
	m_strErrors += "SetTechnique failed";
	return E_FAIL;
    }

    return m_pEffect->Begin(pPasses, D3DXFX_DONOTSAVESTATE|D3DXFX_DONOTSAVESHADERSTATE);
}

// Render Pass in technique
HRESULT ScalingEffect::BeginPass(UINT Pass)
{
#if D3DX_SDK_VERSION >= 22
    return m_pEffect->BeginPass(Pass);
#else
    return m_pEffect->Pass(Pass);
#endif
}

// End Pass of this technique
HRESULT ScalingEffect::EndPass()
{
#if D3DX_SDK_VERSION >= 22
    return m_pEffect->EndPass();
#else
    return S_OK;
#endif
}

// End rendering of this technique
HRESULT ScalingEffect::End()
{
    return m_pEffect->End();
}

// Validates the effect
HRESULT ScalingEffect::Validate()
{
    HRESULT hr;

    // Now we check to see if they all exist
    if(!m_MatWorldEffectHandle || !m_MatWorldViewEffectHandle ||
		!m_MatWorldViewProjEffectHandle) {
        m_strErrors += "Effect doesn't have any world matrix handles";
	return E_FAIL;
    }

    if(!m_MatViewEffectHandle || !m_MatWorldViewEffectHandle ||
		!m_MatViewProjEffectHandle || !m_MatWorldViewProjEffectHandle) {
        m_strErrors += "Effect doesn't have any view matrix handles";
	return E_FAIL;
    }

    if(!m_MatProjEffectHandle || !m_MatViewProjEffectHandle ||
		!m_MatWorldViewProjEffectHandle) {
        m_strErrors += "Effect doesn't have any projection matrix handles";
	return E_FAIL;
    }

    if(!m_SourceTextureEffectHandle) {
        m_strErrors += "Effect doesn't have a SOURCETEXTURE handle";
	return E_FAIL;
    }

    if(!m_CombineTechniqueEffectHandle) {
        m_strErrors += "Effect doesn't have a COMBINETECHNIQUE handle";
	return E_FAIL;
    }

    if(!m_WorkingTexture1EffectHandle && m_PreprocessTechnique1EffectHandle) {
        m_strErrors += "Effect doesn't have a WORKINGTEXTURE handle but uses preprocess steps";
	return E_FAIL;
    }

    if(!m_WorkingTexture2EffectHandle && m_PreprocessTechnique2EffectHandle) {
        m_strErrors += "Effect doesn't have a WORKINGTEXTURE1 handle but uses 2 preprocess steps";
	return E_FAIL;
    }

    // Validate this
    if(m_PreprocessTechnique1EffectHandle) {
	hr = m_pEffect->ValidateTechnique(m_PreprocessTechnique1EffectHandle);

	if(FAILED(hr)) {
	    m_strErrors += "ValidateTechnique for PREPROCESSTECHNIQUE failed";
	    return hr;
	}
    }

    if(m_PreprocessTechnique2EffectHandle) {
	hr = m_pEffect->ValidateTechnique(m_PreprocessTechnique2EffectHandle);

	if(FAILED(hr)) {
	    m_strErrors += "ValidateTechnique for PREPROCESSTECHNIQUE1 failed";
	    return hr;
	}
    }

    hr = m_pEffect->ValidateTechnique(m_CombineTechniqueEffectHandle);

    if(FAILED(hr)) {
	m_strErrors += "ValidateTechnique for COMBINETECHNIQUE failed";
	return hr;
    }

    return S_OK;
}

#endif	// C_D3DSHADERS
