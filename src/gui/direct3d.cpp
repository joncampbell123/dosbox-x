/*
 *  Direct3D rendering code by gulikoza
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "dosbox.h"
#include "control.h"

#if (HAVE_D3D9_H) && defined(WIN32)

#include "direct3d.h"
#include "render.h" // IMPLEMENTED
#include <sstream>

#if LOG_D3D && D3D_THREAD
#define EnterCriticalSection(x) EnterLOGCriticalSection(x, __LINE__)

void CDirect3D::EnterLOGCriticalSection(LPCRITICAL_SECTION lpCriticalSection, int line)
{
    Bitu i = 0;
    static int oldline = -1;
    while(!TryEnterCriticalSection(lpCriticalSection)) {
	i++; Sleep(0);
	if(!(i&0xFFF)) {
	    LOG_MSG("D3D:Possible deadlock in %u (line: %d, oldline: %d, active command: %d)", SDL_ThreadID(), line, oldline, thread_command);
	}
    }
    if(i) LOG_MSG("D3D:Thread %u waited %u to enter critical section (line: %d, oldline: %d, active command: %d)", SDL_ThreadID(), i, line, oldline, thread_command);
    oldline = line;
}
#endif

#include "d3d_components.h"

HRESULT CDirect3D::InitializeDX(HWND wnd, bool triplebuf)
{
#if LOG_D3D
    LOG_MSG("D3D:Starting Direct3D");
#endif

    // Check for display window
    if(!wnd) {
	LOG_MSG("Error: No display window set!");
	return E_FAIL;
    }

    hwnd = wnd;

    mhmodDX9 = LoadLibrary("d3d9.dll");
    if(!mhmodDX9)
	return E_FAIL;

    // Set the presentation parameters
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.BackBufferWidth = dwWidth;
    d3dpp.BackBufferHeight = dwHeight;
    d3dpp.BackBufferCount = 1;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.Windowed = true;
    d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
	Section_prop * sec=static_cast<Section_prop *>(control->GetSection("vsync"));
	if(sec) {
			d3dpp.PresentationInterval = (!strcmp(sec->Get_string("vsyncmode"),"host"))?D3DPRESENT_INTERVAL_DEFAULT:D3DPRESENT_INTERVAL_IMMEDIATE;
	}


    if(triplebuf) {
	LOG_MSG("D3D:Using triple buffering");
	d3dpp.BackBufferCount = 2;
    }

    // Init Direct3D
    if(FAILED(InitD3D())) {
	DestroyD3D();
	LOG_MSG("Error: Unable to initialize DirectX9!");
	return E_FAIL;
    }

#if D3D_THREAD
#if LOG_D3D
    LOG_MSG("D3D:Starting worker thread from thread: %u", SDL_ThreadID());
#endif

    thread_run = true;
    thread_command = D3D_IDLE;
    thread = SDL_CreateThread(EntryPoint, this);
    SDL_SemWait(thread_ack);
#endif

    return S_OK;
}

#if D3D_THREAD
// Wait for D3D to finish processing and return the result
HRESULT CDirect3D::Wait(bool unlock) {
    HRESULT res;

    EnterCriticalSection(&cs);
    while(thread_command != D3D_IDLE) {
	wait = true;
	LeaveCriticalSection(&cs);
#if LOG_D3D
	LOG_MSG("D3D:Waiting for D3D thread to finish processing...(command: %d)", thread_command);
#endif
	SDL_SemWait(thread_ack);
	EnterCriticalSection(&cs);
	wait = false;
    }

#if LOG_D3D
    if(SDL_SemValue(thread_ack)) LOG_MSG("D3D:Semaphore has value: %d!", SDL_SemValue(thread_ack));
#endif

    res = thread_hr;
    if(unlock) LeaveCriticalSection(&cs);
    return res;
}
#endif

#if D3D_THREAD
int CDirect3D::Start(void)
{
#if LOG_D3D
    LOG_MSG("D3D:Worker thread %u starting", SDL_ThreadID());
#endif

    SDL_SemPost(thread_ack);

    EnterCriticalSection(&cs);
    while(thread_run) {

	HRESULT hr;
	D3D_state tmp = thread_command;
	LeaveCriticalSection(&cs);

	if(tmp == D3D_IDLE) {
	    SDL_SemWait(thread_sem);
	    EnterCriticalSection(&cs);
	    continue;
	}

	switch(tmp) {
	    case D3D_LOADPS: hr = LoadPixelShader(); break;
	    case D3D_LOCK: hr = LockTexture(); break;
	    case D3D_UNLOCK: (UnlockTexture() ? hr = S_OK : hr = E_FAIL); break;
	    default: hr = S_OK; break;
	}

	EnterCriticalSection(&cs);
	thread_hr = hr;
	thread_command = D3D_IDLE;
	if(wait) {
		LeaveCriticalSection(&cs);
		SDL_SemPost(thread_ack);
		EnterCriticalSection(&cs);
	}
    }

#if LOG_D3D
    LOG_MSG("D3D:Thread %u is finishing...", SDL_ThreadID());
#endif
    LeaveCriticalSection(&cs);

    return 0;
}
#endif

bool CDirect3D::LockTexture(Bit8u * & pixels,Bitu & pitch)
{
#if D3D_THREAD
    Wait(false);

    // Locks take a bit, waiting for the worker thread to do it will most certainly
    // take us waiting in the kernel mode...try to lock the texture directly...
    if(FAILED(LockTexture())) {

	// OK, let the worker thread do it...
	thread_command = D3D_LOCK;
	LeaveCriticalSection(&cs);
	SDL_SemPost(thread_sem);

	if(FAILED(Wait(false))) {
	    LeaveCriticalSection(&cs);
	    LOG_MSG("D3D:No texture to draw to!?");
	    return false;
	}
    }
    LeaveCriticalSection(&cs);
#else
    if(FAILED(LockTexture())) {
	LOG_MSG("D3D:No texture to draw to!?");
	return false;
    }
#endif

    pixels=(Bit8u *)d3dlr.pBits;
    pitch=d3dlr.Pitch;
    return true;
}

HRESULT CDirect3D::LockTexture(void)
{
    // Lock the surface and write the pixels
    static DWORD d3dflags = D3DLOCK_NO_DIRTY_UPDATE;
lock_texture:
    if(GCC_UNLIKELY(!lpTexture || deviceLost)) {
	// Try to reset the device, but only when running main thread
#if D3D_THREAD
	if((SDL_ThreadID() != SDL_GetThreadID(thread)))
#endif
	{
	    Resize3DEnvironment();
	}
	if(GCC_UNLIKELY(!lpTexture || deviceLost)) {
	    LOG_MSG("D3D:Device is lost, locktexture() failed...");
	    return E_FAIL;
	}
	// Success, continue as planned...
    }
    if(GCC_UNLIKELY(lpTexture->LockRect(0, &d3dlr, NULL, d3dflags) != D3D_OK)) {
        if(d3dflags) {
            d3dflags = 0;
            LOG_MSG("D3D:Cannot lock texture, fallback to compatible mode");
            goto lock_texture;
        } else {
            LOG_MSG("D3D:Failed to lock texture!");
	}
        return E_FAIL;
    }

    return S_OK;
}

bool CDirect3D::UnlockTexture(const Bit16u *changed)
{
	changedLines = changed;
#if D3D_THREAD
	Wait(false);
	thread_command = D3D_UNLOCK;
	LeaveCriticalSection(&cs);

	SDL_SemPost(thread_sem);
	return true;
#else
	return UnlockTexture();
#endif
}

bool CDirect3D::UnlockTexture(void)
{
    if(GCC_UNLIKELY(deviceLost)) return false;
    // Support EndUpdate without new data...needed by some shaders
    if(GCC_UNLIKELY(!d3dlr.pBits)) return D3DSwapBuffers();
    lpTexture->UnlockRect(0);
    d3dlr.pBits = NULL;
    RECT rect;
    rect.left = 0;
    rect.right = dwWidth;
    if(!dynamic && changedLines) {
        Bitu y = 0, index = 0;
        while(y < dwHeight) {
            if(!(index & 1)) {
                y += changedLines[index];
            } else {
                rect.top = y;
                rect.bottom = y + changedLines[index];
                lpTexture->AddDirtyRect(&rect);
                y += changedLines[index];
            }
            index++;
        }
    } else {
        rect.top = 0; rect.bottom = dwHeight;
        lpTexture->AddDirtyRect(&rect);
    }

    return D3DSwapBuffers();
}

HRESULT CDirect3D::InitD3D(void)
{
    IDirect3D9 *(APIENTRY *pDirect3DCreate9)(UINT) = (IDirect3D9 *(APIENTRY *)(UINT))GetProcAddress(mhmodDX9, "Direct3DCreate9");
    if(!pDirect3DCreate9)
	return E_FAIL;

    // create the IDirect3D9 object
    pD3D9 = pDirect3DCreate9(D3D_SDK_VERSION);
    if(pD3D9 == NULL)
	return E_FAIL;

    D3DCAPS9 d3dCaps;
    // get device capabilities
    ZeroMemory(&d3dCaps, sizeof(d3dCaps));
    if(FAILED(pD3D9->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &d3dCaps)))
	return E_FAIL;

    // get the display mode
    D3DDISPLAYMODE d3ddm;
    pD3D9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm);
    d3dpp.BackBufferFormat = d3ddm.Format;

    HRESULT hr;

    // Check if hardware vertex processing is available
    if(d3dCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) {
        // Create device with hardware vertex processing
        hr = pD3D9->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL, hwnd,
            D3DCREATE_HARDWARE_VERTEXPROCESSING|0x00000800L|D3DCREATE_FPU_PRESERVE, &d3dpp, &pD3DDevice9);
    } else {
        // Create device with software vertex processing
        hr = pD3D9->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL, hwnd,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING|0x00000800L|D3DCREATE_FPU_PRESERVE, &d3dpp, &pD3DDevice9);
    }

    // Make sure device was created
    if(FAILED(hr)) {
	LOG_MSG("D3D:Unable to create D3D device!");
	return E_FAIL;
    }

    if(FAILED(pD3D9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3ddm.Format,
			    0, D3DRTYPE_TEXTURE, D3DFMT_X8R8G8B8))) {
	if(FAILED(pD3D9->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3ddm.Format,
			    0, D3DRTYPE_TEXTURE, D3DFMT_R5G6B5))) {
	    DestroyD3D();
	    LOG_MSG("Error: Cannot find a working texture color format!");
	    return E_FAIL;
	}

	bpp16 = true;
	LOG_MSG("D3D:Running in 16-bit color mode");
    }

    if(d3dCaps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY) {
#if LOG_D3D
	LOG_MSG("D3D:Square-only textures");
#endif
	square = true;
    } else {
#if LOG_D3D
	LOG_MSG("D3D:Non-square textures");
#endif
	square = false;
    }

    if(d3dCaps.TextureCaps & D3DPTEXTURECAPS_POW2) {
        if(d3dCaps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL) {
#if LOG_D3D
	    LOG_MSG("D3D:Conditional non-pow2 texture size support");
#endif
	    pow2 = false;
        } else {
#if LOG_D3D
	    LOG_MSG("D3D:Textures must be a power of 2 in size");
#endif
	    pow2 = true;
	}
    } else {
#if LOG_D3D
       LOG_MSG("D3D:Textures do not need to be a power of 2 in size");
#endif
       pow2 = false;
    }

    if(d3dCaps.Caps2 & D3DCAPS2_DYNAMICTEXTURES) {
#if LOG_D3D
	LOG_MSG("D3D:Dynamic textures supported");
#endif
	dynamic = true;
    } else {
	LOG_MSG("D3D:Dynamic textures NOT supported. Performance will suffer!");
	dynamic = false;
    }

#if LOG_D3D
    LOG_MSG("D3D:Max texture width: %d", d3dCaps.MaxTextureWidth);
    LOG_MSG("D3D:Max texture height: %d", d3dCaps.MaxTextureHeight);
#endif

    if((d3dCaps.MaxTextureWidth < 1024) || (d3dCaps.MaxTextureHeight < 1024)) {
	DestroyD3D();
	LOG_MSG("Error: Your card does not support large textures!");
	return E_FAIL;
    }

#if C_D3DSHADERS
    // allow scale2x_ps14.fx with 1.4 shaders, everything else requires 2.0
    if(d3dCaps.PixelShaderVersion >= D3DPS_VERSION(1,4)) {
#if LOG_D3D
	LOG_MSG("D3D:Hardware PS version %d.%d", D3DSHADER_VERSION_MAJOR(d3dCaps.PixelShaderVersion),
						    D3DSHADER_VERSION_MINOR(d3dCaps.PixelShaderVersion));
#endif
	if(d3dCaps.PixelShaderVersion == D3DPS_VERSION(1,4))
	    LOG_MSG("D3D:Hardware PS version 1.4 detected. Most shaders probably won't work...");
	if((d3dCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) && (dynamic)) {
	    psEnabled = true;
	    square = true;
	    pow2 = true;			// pow2 texture size has to be enabled as well
	} else {
	    LOG_MSG("D3D:Error when initializing pixel shader support. Disabling shaders.");
	    psEnabled = false;
	}
    }
    else {
	LOG_MSG("D3D:Hardware PS version too low. Disabling support for shaders.");
	psEnabled = false;
    }
#endif

    DWORD		dwNumAdapterModes;
    D3DDISPLAYMODE	DisplayMode;
    DWORD		m;

    dwNumModes = 0;

    if(bpp16)
	dwNumAdapterModes = pD3D9->GetAdapterModeCount(D3DADAPTER_DEFAULT, D3DFMT_R5G6B5);
    else
	dwNumAdapterModes = pD3D9->GetAdapterModeCount(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8);

    if(dwNumAdapterModes == 0) {
	LOG_MSG("D3D:No display modes found");
	return E_FAIL;
    }

#if LOG_D3D
    LOG_MSG("D3D:Found %d display modes", dwNumAdapterModes);
#endif
    modes = (D3DDISPLAYMODE*)malloc(sizeof(D3DDISPLAYMODE)*dwNumAdapterModes);

    if(!modes) {
	LOG_MSG("D3D:Error allocating memory!");
	DestroyD3D();
	return E_FAIL;
    }

    for(iMode=0;iMode<dwNumAdapterModes;iMode++) {
	// Get the display mode attributes
	if(bpp16)
	    pD3D9->EnumAdapterModes(D3DADAPTER_DEFAULT, D3DFMT_R5G6B5, iMode, &DisplayMode);
	else
	    pD3D9->EnumAdapterModes(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8, iMode, &DisplayMode);

	// Check if the mode already exists
	for(m=0;m<dwNumModes;m++) {
	    if((modes[m].Width == DisplayMode.Width) && (modes[m].Height == DisplayMode.Height) &&
		(modes[m].Format == DisplayMode.Format))
		break;
	}

	// If we found a new mode, add it to the list of modes
	if(m==dwNumModes) {
#if LOG_D3D
	    LOG_MSG("D3D:Display mode: %dx%dx%d", DisplayMode.Width, DisplayMode.Height, DisplayMode.Format);
#endif
	    // Try to sort resolutions
	    if(dwNumModes>0) {
		if(((modes[m - 1].Width == DisplayMode.Width) && (modes[m - 1].Height > DisplayMode.Height)) ||
		   (modes[m - 1].Width  > DisplayMode.Width)) {
		    modes[m].Width  = modes[m - 1].Width;
		    modes[m].Height = modes[m - 1].Height;
		    modes[m].Format = modes[m - 1].Format;
		    m--;
		}
	    }

	    modes[m].Width       = DisplayMode.Width;
	    modes[m].Height      = DisplayMode.Height;
	    modes[m].Format      = DisplayMode.Format;
	    modes[m].RefreshRate = 0;
	    ++dwNumModes;
	}
    }

    // Free some unused memory
    modes = (D3DDISPLAYMODE*)realloc(modes, sizeof(D3DDISPLAYMODE)*dwNumModes);

    if(!modes) {
	LOG_MSG("D3D:Error allocating memory!");
	DestroyD3D();
	return E_FAIL;
    }

    dwTexHeight = 0;
    dwTexWidth = 0;

    return S_OK;
}

void CDirect3D::DestroyD3D(void)
{
#if D3D_THREAD
    // Kill child thread
    if(thread != NULL) {
	Wait(false);
	thread_run = false;
	thread_command = D3D_IDLE;
	LeaveCriticalSection(&cs);
	SDL_SemPost(thread_sem);
	SDL_WaitThread(thread, NULL);
	thread = NULL;
    }
#endif

    InvalidateDeviceObjects();

    // Delete D3D device
    SAFE_RELEASE(pD3DDevice9);
    SAFE_RELEASE(pD3D9);

    if(modes) {
	free(modes);
	modes = NULL;
    }
}

// Draw a textured quad on the back-buffer
bool CDirect3D::D3DSwapBuffers(void)
{
    HRESULT hr;
    UINT uPasses;

    // begin rendering
    pD3DDevice9->BeginScene();

#if C_D3DSHADERS
    /* PS 2.0 path */
    if(psActive) {

	if(preProcess) {

	    // Set preprocess matrices
	    if(FAILED(psEffect->SetMatrices(m_matPreProj, m_matPreView, m_matPreWorld))) {
		LOG_MSG("D3D:Set matrices failed.");
		return false;
	    }

	    // Save render target
	    LPDIRECT3DSURFACE9 lpRenderTarget;
	    pD3DDevice9->GetRenderTarget(0, &lpRenderTarget);
	    LPDIRECT3DTEXTURE9 lpWorkTexture = lpWorkTexture1;
pass2:
	    // Change the render target
	    LPDIRECT3DSURFACE9 lpNewRenderTarget;
	    lpWorkTexture->GetSurfaceLevel(0, &lpNewRenderTarget);

	    if(FAILED(pD3DDevice9->SetRenderTarget(0, lpNewRenderTarget))) {
		LOG_MSG("D3D:Failed to set render target");
		return false;
	    }

	    SAFE_RELEASE(lpNewRenderTarget);

	    uPasses = 0;
	    if(FAILED(psEffect->Begin((lpWorkTexture==lpWorkTexture1) ?
		    (ScalingEffect::Preprocess1):(ScalingEffect::Preprocess2), &uPasses))) {
		LOG_MSG("D3D:Failed to begin PS");
		return false;
	    }

	    for(UINT uPass=0;uPass<uPasses;uPass++) {
		hr=psEffect->BeginPass(uPass);
		if(FAILED(hr)) {
		    LOG_MSG("D3D:Failed to begin pass %d", uPass);
		    return false;
		}

		// Render the vertex buffer contents
		pD3DDevice9->DrawPrimitive(D3DPT_TRIANGLESTRIP, 4, 2);
		psEffect->EndPass();
	    }

	    if(FAILED(psEffect->End())) {
		LOG_MSG("D3D:Failed to end effect");
		return false;
	    }

#if DEBUG_PS
	    // Save rendertarget data
	    LPDIRECT3DSURFACE9 lpTexRenderTarget;
	    lpWorkTexture->GetSurfaceLevel(0, &lpTexRenderTarget);
	    lpDebugTexture->GetSurfaceLevel(0, &lpNewRenderTarget);
	    if(FAILED(hr=pD3DDevice9->GetRenderTargetData(lpTexRenderTarget, lpNewRenderTarget))) {
		LOG_MSG("D3D:Unable to get render target data: 0x%x", hr);
		SAFE_RELEASE(lpTexRenderTarget);
		SAFE_RELEASE(lpNewRenderTarget);
	    } else {
		SAFE_RELEASE(lpTexRenderTarget);
		SAFE_RELEASE(lpNewRenderTarget);
		LOG_MSG("D3D:Got render target data, writing debug file (%dx%d)", dwTexWidth, dwTexHeight);
		if(lpDebugTexture->LockRect(0, &d3dlr, NULL, D3DLOCK_READONLY) == D3D_OK) {
		    FILE * debug = fopen(((lpWorkTexture==lpWorkTexture1)?"pass1.raw":"pass2.raw"), "wb");
		    if(debug == NULL) {
			LOG_MSG("D3D:Unable to create file!");
		    } else {
			for(int i = 0; i < dwTexHeight; i++) {
			    Bit8u * ptr = (Bit8u*)d3dlr.pBits;
			    for(int j = 0; j < dwTexWidth; j++) {
				fwrite(ptr, 3, sizeof(char), debug);
				ptr += 4;
			    }
			    d3dlr.pBits = (Bit8u*)d3dlr.pBits + d3dlr.Pitch;
			}
			fclose(debug);
		    }
		    lpDebugTexture->UnlockRect(0);
		}
		d3dlr.pBits = NULL;
	    }
#endif

	    if((psEffect->hasPreprocess2()) && (lpWorkTexture == lpWorkTexture1)) {
		lpWorkTexture = lpWorkTexture2;
		goto pass2;
	    }

	    // Reset the rendertarget
	    pD3DDevice9->SetRenderTarget(0, lpRenderTarget);
	    SAFE_RELEASE(lpRenderTarget);

	    // Set matrices for final pass
	    if(FAILED(psEffect->SetMatrices(m_matProj, m_matView, m_matWorld))) {
		LOG_MSG("D3D:Set matrices failed.");
		return false;
	    }
	}

	uPasses = 0;

	if(FAILED(psEffect->Begin(ScalingEffect::Combine, &uPasses))) {
	    LOG_MSG("D3D:Failed to begin PS");
	    return false;
	}

	for(UINT uPass=0;uPass<uPasses;uPass++) {
	    hr=psEffect->BeginPass(uPass);
	    if(FAILED(hr)) {
		LOG_MSG("D3D:Failed to begin pass %d", uPass);
		return false;
	    }

	    // Render the vertex buffer contents
	    pD3DDevice9->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
	    psEffect->EndPass();
	}

	if(FAILED(psEffect->End())) {
	    LOG_MSG("D3D:Failed to end effect");
	    return false;
	}

    } else
#endif
    {
	// Normal path
	pD3DDevice9->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
    }

    // end rendering
    pD3DDevice9->EndScene();

    if(GCC_UNLIKELY(hr=pD3DDevice9->Present(NULL, NULL, NULL, NULL)) != D3D_OK) {
	switch(hr) {
	    case D3DERR_DEVICELOST:
		// This will be handled when SDL catches alt-tab and resets GFX
		return true;
		break;
	    case D3DERR_DRIVERINTERNALERROR:
		LOG_MSG("D3D:Driver internal error");
		return false;
		break;
	    case D3DERR_INVALIDCALL:
	    default:
		LOG_MSG("D3D:Invalid call");
		return false;
		break;
	}
    }

    return true;
}

HRESULT CDirect3D::InvalidateDeviceObjects(void)
{
    SAFE_RELEASE(lpTexture);
#if C_D3DSHADERS
    SAFE_RELEASE(lpWorkTexture1);
    SAFE_RELEASE(lpWorkTexture2);
    SAFE_RELEASE(lpHq2xLookupTexture);
#if LOG_D3D
    SAFE_RELEASE(lpDebugTexture);
#endif

    // Delete pixel shader
    if(psEffect) {
	delete psEffect;
	psEffect = NULL;
    }
#endif

    // clear stream source
    if(pD3DDevice9)
	pD3DDevice9->SetStreamSource(0, NULL, 0, 0);

    SAFE_RELEASE(vertexBuffer);

    return S_OK;
}

HRESULT CDirect3D::RestoreDeviceObjects(void)
{
    unsigned int vertexbuffersize = sizeof(TLVERTEX) * 4;
    preProcess = false;

#if C_D3DSHADERS
    if(psActive) {
	// Set up pixel shaders
	LoadPixelShader();

	if(psEffect && psEffect->hasPreprocess()) {
#if LOG_D3D
	    LOG_MSG("D3D:Pixel shader preprocess active");
#endif
	    preProcess = true;
	    vertexbuffersize = sizeof(TLVERTEX) * 8;
	}
    }
#endif
    // Initialize vertex
    pD3DDevice9->SetFVF(D3DFVF_TLVERTEX);

    // Create vertex buffer
    if(FAILED(pD3DDevice9->CreateVertexBuffer(vertexbuffersize, D3DUSAGE_WRITEONLY,
        D3DFVF_TLVERTEX, D3DPOOL_MANAGED, &vertexBuffer, NULL))) {
	LOG_MSG("D3D:Failed to create Vertex Buffer");
	return E_FAIL;
    }

    // Lock vertex buffer and set vertices
    CreateVertex();

    pD3DDevice9->SetStreamSource(0, vertexBuffer, 0, sizeof(TLVERTEX));

    // Turn off culling
    pD3DDevice9->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

    // Turn off D3D lighting
    pD3DDevice9->SetRenderState(D3DRS_LIGHTING, false);

    // Turn off the zbuffer
    pD3DDevice9->SetRenderState(D3DRS_ZENABLE, false);

    CreateDisplayTexture();
    SetupSceneScaled();

    if(!psActive) {
	pD3DDevice9->SetTexture(0, lpTexture);

	// Disable Shaders
	pD3DDevice9->SetVertexShader(0);
	pD3DDevice9->SetPixelShader(0);

	pD3DDevice9->SetTransform(D3DTS_PROJECTION, &m_matProj);
	pD3DDevice9->SetTransform(D3DTS_VIEW, &m_matView);
	pD3DDevice9->SetTransform(D3DTS_WORLD, &m_matWorld);
    }
#if C_D3DSHADERS
    else if(psEffect) {
	if(preProcess) {
	    // Projection is (0,0,0) -> (1,1,1)
	    D3DXMatrixOrthoOffCenterLH(&m_matPreProj, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);

	    // Align texels with pixels
	    D3DXMatrixTranslation(&m_matPreView, -0.5f/dwTexWidth, 0.5f/dwTexHeight, 0.0f);

	    // Identity for world
	    D3DXMatrixIdentity(&m_matPreWorld);

	} else if(FAILED(psEffect->SetMatrices(m_matProj, m_matView, m_matWorld))) {
	    LOG_MSG("D3D:Set matrices failed.");
	    InvalidateDeviceObjects();
	    return E_FAIL;
	}
    }
#endif

    return S_OK;
}

extern void RENDER_SetForceUpdate(bool);
HRESULT CDirect3D::LoadPixelShader(const char * shader, double scalex, double scaley, bool forced)
{
    if(!psEnabled) {
	RENDER_SetForceUpdate(false);
	psActive = false;
	return E_FAIL;
    }

#if C_D3DSHADERS
    // See if the shader is already running
    if((!psEffect) || (strcmp(pshader+8, shader))) {

	// This is called in main thread to test if pixelshader needs to be changed
	if((scalex == 0) && (scaley == 0)) return S_OK;

	RENDER_SetForceUpdate(false);
	safe_strncpy(pshader+8, shader, 22); pshader[29] = '\0';
#if D3D_THREAD
	Wait(false);
	thread_command = D3D_LOADPS;
	LeaveCriticalSection(&cs);
	SDL_SemPost(thread_sem);

	if(FAILED(Wait(true))) return E_FAIL;
#else
	if(FAILED(LoadPixelShader())) return E_FAIL;
#endif

    } else {
#if LOG_D3D
	LOG_MSG("D3D:Shader %s is already loaded", shader);
#endif
	return E_FAIL;
    }

    if (psEffect) {
#if LOG_D3D
	LOG_MSG("D3D:Shader scale: %.2f", psEffect->getScale());
#endif
	// Compare optimal scaling factor
	bool dblgfx=((scalex < scaley ? scalex : scaley) >= psEffect->getScale());

	if(dblgfx || forced) {
	    LOG_MSG("D3D:Pixel shader %s active", shader);
	    RENDER_SetForceUpdate(psEffect->getForceUpdate());
	    psActive = true;
	    return S_OK;
	} else {
	    LOG_MSG("D3D:Pixel shader not needed");
	    psActive = false;
	    return E_FAIL;
	}
    } else psActive = false;

#endif // C_D3DSHADERS

    return S_OK;
}

HRESULT CDirect3D::LoadPixelShader(void)
{
#if C_D3DSHADERS
    // Load new shader
    if(psEffect) {
	delete psEffect;
	psEffect = NULL;
    }

    if(!strcmp(pshader+8, "none")) {
	// Returns E_FAIL so that further shader processing is disabled
	psActive = false;
	return E_FAIL;
    }

    psEffect = new ScalingEffect(pD3DDevice9);
    if (psEffect == NULL) {
	LOG_MSG("D3D:Error creating shader object!");
	psActive = false;
	return E_FAIL;
    }

#if LOG_D3D
    LOG_MSG("D3D:Loading pixel shader from %s", pshader);
#endif

    psEffect->setinputDim(dwWidth, dwHeight);
    if(FAILED(psEffect->LoadEffect(pshader)) || FAILED(psEffect->Validate())) {
	/*LOG_MSG("D3D:Pixel shader error:");

	// The resulting string can exceed 512 char LOG_MSG limit, split on newlines
	std::stringstream ss(psEffect->getErrors());
	std::string line;
	while(std::getline(ss, line)) {
	    LOG_MSG(" %s", line.c_str());
	}

	LOG_MSG("D3D:Pixel shader output disabled");*/
	delete psEffect;
	psEffect = NULL;
	psActive = false;
	pshader[8] = '\0';
	return E_FAIL;
    }

#endif // C_D3DSHADERS

    return S_OK;
}

HRESULT CDirect3D::Resize3DEnvironment(Bitu width, Bitu height, Bitu rwidth, Bitu rheight, bool fullscreen)
{
#if LOG_D3D
    LOG_MSG("D3D:Resizing D3D screen...");
#endif

#if D3D_THREAD
    Wait(false);
#endif

    // set the presentation parameters
    d3dpp.BackBufferWidth = width;
    d3dpp.BackBufferHeight = height;

    if(fullscreen) {
	// Find correct display mode
	bool fullscreen_ok = false;

	for(iMode=0;iMode<dwNumModes;iMode++) {
	    if((modes[iMode].Width >= width) && (modes[iMode].Height >= height)) {
		d3dpp.BackBufferWidth = modes[iMode].Width;
		d3dpp.BackBufferHeight = modes[iMode].Height;
		fullscreen_ok = true;

		// Some cards no longer support 320xXXX resolutions,
		// even if they list them as supported. In this case
		// the card will silently switch to 640xXXX, leaving black
		// borders around displayed picture. Using hardware scaling
		// to 640xXXX doesn't cost anything in this case and should
		// look exactly the same as 320xXXX.
		if(d3dpp.BackBufferWidth < 512) {
			d3dpp.BackBufferWidth *= 2;
			d3dpp.BackBufferHeight *= 2;
			width *= 2; height *= 2;
		}
		break;
	    }
	}

	if(!fullscreen_ok) {
	    LOG_MSG("D3D:No suitable fullscreen mode found!");
	    //d3dpp.Windowed = !d3dpp.Windowed;
	}
    }

    dwScaledWidth = width;
    dwScaledHeight = height;

    dwWidth = rwidth;
    dwHeight = rheight;

#if LOG_D3D
    LOG_MSG("D3D:Resolution set to %dx%d%s", d3dpp.BackBufferWidth, d3dpp.BackBufferHeight, fullscreen ? ", fullscreen" : "");
#endif

    HRESULT hr = Resize3DEnvironment();

#if D3D_THREAD
    LeaveCriticalSection(&cs);
#endif
    return hr;
}

HRESULT CDirect3D::Resize3DEnvironment(void)
{
    // Release all vidmem objects
    HRESULT hr;

#if LOG_D3D
    LOG_MSG("D3D:Resize3DEnvironment() called");
#endif

    if(FAILED(hr=InvalidateDeviceObjects())) {
	LOG_MSG("D3D:Failed to invalidate objects");
	return hr;
    }

    // Reset the device
reset_device:
    Bitu i = 20;
    // Don't bother too much, when device is already lost
    if(deviceLost) i = 5;
    deviceLost = false;

    if(FAILED(hr=pD3DDevice9->Reset(&d3dpp))) {
	if(hr==D3DERR_DEVICELOST) {
	    while((hr=pD3DDevice9->TestCooperativeLevel()) != D3DERR_DEVICENOTRESET) {
		if(hr==D3DERR_DRIVERINTERNALERROR) {
		    LOG_MSG("D3D:Driver internal error when resetting device!");
		    return hr;
		}
#if LOG_D3D
		LOG_MSG("D3D:Wait for D3D device to become available...");
#endif
		Sleep(50); i--;
		if(i == 0) {
		    deviceLost = true;
#if LOG_D3D
		    LOG_MSG("D3D:Giving up on D3D wait...");
#endif
		    // Return ok or dosbox will quit, we'll try to reset the device later
		    return S_OK;
		}
	    }
#if LOG_D3D
	    LOG_MSG("D3D:Performing another reset...");
#endif
	    goto reset_device;
	} else {
	    LOG_MSG("D3D:Failed to reset device!");
	    return hr;
	}
    }

    // Clear all backbuffers
    pD3DDevice9->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

    pD3DDevice9->Present(NULL, NULL, NULL, NULL);
    pD3DDevice9->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

    if(d3dpp.BackBufferCount == 2) {
	pD3DDevice9->Present(NULL, NULL, NULL, NULL);
	pD3DDevice9->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    }

#if LOG_D3D
    LOG_MSG("D3D:Mode: %dx%d (x %.2fx%.2f) --> scaled size: %dx%d", dwWidth, dwHeight,
		    (float)dwScaledWidth/dwWidth, (float)dwScaledHeight/dwHeight, dwScaledWidth, dwScaledHeight);
#endif

    return RestoreDeviceObjects();
}

HRESULT CDirect3D::CreateDisplayTexture(void)
{
    SAFE_RELEASE(lpTexture);

    if((dwTexWidth == 0) || (dwTexHeight == 0))
	return S_OK;

#if LOG_D3D
    LOG_MSG("D3D:Creating Textures: %dx%d", dwTexWidth, dwTexHeight);
#endif

    HRESULT hr;

    D3DFORMAT d3dtexformat;

    if(bpp16)
	d3dtexformat = D3DFMT_R5G6B5;
    else
	d3dtexformat = D3DFMT_X8R8G8B8;

    if(!dynamic) {
	hr = pD3DDevice9->CreateTexture(dwTexWidth, dwTexHeight, 1, 0, d3dtexformat,
                                D3DPOOL_MANAGED, &lpTexture, NULL);

    } else {
	hr = pD3DDevice9->CreateTexture(dwTexWidth, dwTexHeight, 1, D3DUSAGE_DYNAMIC, d3dtexformat,
			    D3DPOOL_DEFAULT, &lpTexture, NULL);
    }

    if(FAILED(hr)) {
	LOG_MSG("D3D:Failed to create %stexture: 0x%x", (dynamic ? "dynamic " : ""), hr);

	switch(hr) {
	case D3DERR_INVALIDCALL:
	    LOG_MSG("D3D:Invalid call");
	    break;
	case D3DERR_OUTOFVIDEOMEMORY:
	    LOG_MSG("D3D:D3DERR_OUTOFVIDEOMEMORY");
	    break;
	case E_OUTOFMEMORY:
	    LOG_MSG("D3D:E_OUTOFMEMORY");
	    break;
	default:
	    LOG_MSG("D3D:E_UNKNOWN");
	}
	return E_FAIL;
    }

    // Initialize texture to black
    if(LockTexture() == S_OK) {
	Bit8u * pixels = (Bit8u *)d3dlr.pBits;

	for(Bitu lines = dwTexHeight; lines; lines--) {
	    memset(pixels, 0, (dwTexWidth<<2)>>bpp16);
	    pixels += d3dlr.Pitch;
	}

	lpTexture->UnlockRect(0);
    }

    d3dlr.pBits = NULL;
    RECT rect;

    rect.left = rect.top = 0;
    rect.right = dwTexWidth; rect.bottom = dwTexHeight;
    lpTexture->AddDirtyRect(&rect);

#if C_D3DSHADERS
    if(psActive) {
	// Working textures for pixel shader
	if(FAILED(hr=pD3DDevice9->CreateTexture(dwTexWidth, dwTexHeight, 1, D3DUSAGE_RENDERTARGET,
			    D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &lpWorkTexture1, NULL))) {
	    LOG_MSG("D3D:Failed to create working texture: 0x%x", hr);

	    switch(hr) {
	    case D3DERR_INVALIDCALL:
	        LOG_MSG("D3D:Invalid call");
		break;
	    case D3DERR_OUTOFVIDEOMEMORY:
		LOG_MSG("D3D:D3DERR_OUTOFVIDEOMEMORY");
		break;
	    case E_OUTOFMEMORY:
		LOG_MSG("D3D:E_OUTOFMEMORY");
		break;
	    default:
		LOG_MSG("D3D:E_UNKNOWN");
	    }
	    return E_FAIL;
	}

	if(FAILED(hr=pD3DDevice9->CreateTexture(dwTexWidth, dwTexHeight, 1, D3DUSAGE_RENDERTARGET,
			    D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &lpWorkTexture2, NULL))) {
	    LOG_MSG("D3D:Failed to create working texture: 0x%x", hr);

	    switch(hr) {
	    case D3DERR_INVALIDCALL:
	        LOG_MSG("D3D:Invalid call");
		break;
	    case D3DERR_OUTOFVIDEOMEMORY:
		LOG_MSG("D3D:D3DERR_OUTOFVIDEOMEMORY");
		break;
	    case E_OUTOFMEMORY:
		LOG_MSG("D3D:E_OUTOFMEMORY");
		break;
	    default:
		LOG_MSG("D3D:E_UNKNOWN");
	    }
	    return E_FAIL;
	}

	if(FAILED(hr=pD3DDevice9->CreateVolumeTexture(256, 16, 256, 1, 0, D3DFMT_A8R8G8B8,
			    D3DPOOL_MANAGED, &lpHq2xLookupTexture, NULL))) {
	    LOG_MSG("D3D:Failed to create volume texture: 0x%x", hr);

	    switch(hr) {
	    case D3DERR_INVALIDCALL:
	        LOG_MSG("D3D:Invalid call");
		break;
	    case D3DERR_OUTOFVIDEOMEMORY:
		LOG_MSG("D3D:D3DERR_OUTOFVIDEOMEMORY");
		break;
	    case E_OUTOFMEMORY:
		LOG_MSG("D3D:E_OUTOFMEMORY");
		break;
	    default:
		LOG_MSG("D3D:E_UNKNOWN");
	    }
	    return E_FAIL;
	}

	// build lookup table
	D3DLOCKED_BOX lockedBox;

	if(FAILED(hr = lpHq2xLookupTexture->LockBox(0, &lockedBox, NULL, 0))) {
	    LOG_MSG("D3D:Failed to lock box of volume texture: 0x%x", hr);

	    switch(hr) {
		case D3DERR_INVALIDCALL:
		    LOG_MSG("D3D:Invalid call");
		    break;
		default:
		    LOG_MSG("D3D:E_UNKNOWN");
	    }
	    return E_FAIL;
	}

	BuildHq2xLookupTexture(dwScaledWidth, dwScaledHeight, dwWidth, dwHeight, (Bit8u *)lockedBox.pBits);

#if LOG_D3D
	// Debug: Write look-up texture to file
	int fd = _open("hq2xLookupTexture.pam",_O_WRONLY|_O_CREAT|_O_TRUNC|_O_BINARY,0666);
	unsigned char table[4096] = HQ2X_D3D_TABLE_DATA;
	sprintf((char *)table,"P7\nWIDTH %i\nHEIGHT %i\nMAXVAL 255\nDEPTH 4\nTUPLTYPE RGB_ALPHA\nENDHDR\n",16*HQ2X_RESOLUTION,4096/16*HQ2X_RESOLUTION);
	write(fd,table,strlen((char *)table));
	write(fd, lockedBox.pBits, HQ2X_RESOLUTION * HQ2X_RESOLUTION * 4096 * 4);
	_close(fd);
#endif

	if(FAILED(hr = lpHq2xLookupTexture->UnlockBox(0))) {
	    LOG_MSG("D3D:Failed to unlock box of volume texture: 0x%x", hr);

	    switch(hr) {
		case D3DERR_INVALIDCALL:
		    LOG_MSG("D3D:Invalid call");
		    break;
		default:
		    LOG_MSG("D3D:E_UNKNOWN");
	    }
	    return E_FAIL;
	}

#if LOG_D3D
	// Debug texture for pixel shader
	if(FAILED(hr=pD3DDevice9->CreateTexture(dwTexWidth, dwTexHeight, 1, 0, D3DFMT_A8R8G8B8,
			    D3DPOOL_SYSTEMMEM, &lpDebugTexture, NULL))) {
	    LOG_MSG("D3D:Failed to create debug texture: 0x%x", hr);

	    switch(hr) {
	    case D3DERR_INVALIDCALL:
	        LOG_MSG("D3D:Invalid call");
		break;
	    case D3DERR_OUTOFVIDEOMEMORY:
		LOG_MSG("D3D:D3DERR_OUTOFVIDEOMEMORY");
		break;
	    case E_OUTOFMEMORY:
		LOG_MSG("D3D:E_OUTOFMEMORY");
		break;
	    default:
		LOG_MSG("D3D:E_UNKNOWN");
	    }
	    return E_FAIL;
	}
#endif	// LOG_D3D

	// Set textures
	if(FAILED(psEffect->SetTextures(lpTexture, lpWorkTexture1, lpWorkTexture2, lpHq2xLookupTexture))) {
	    LOG_MSG("D3D:Failed to set PS textures");
	    return false;
	}

    }
#endif	// C_D3DSHADERS

    return S_OK;
}

HRESULT CDirect3D::CreateVertex(void)
{
    TLVERTEX* vertices;

    // Texture coordinates
    float sizex=1.0f, sizey=1.0f;

    // If texture is larger than DOSBox FB
    if(dwTexWidth != dwWidth)
	sizex = (float)dwWidth/dwTexWidth;
    if(dwTexHeight != dwHeight)
	sizey = (float)dwHeight/dwTexHeight;

#if LOG_D3D
    LOG_MSG("D3D:Quad size: %dx%d, tex. coord.: 0,0-->%.2f,%.2f", dwWidth, dwHeight, sizex, sizey);
#endif

    // Lock the vertex buffer
    vertexBuffer->Lock(0, 0, (void**)&vertices, 0);

    //Setup vertices
    vertices[0].position = D3DXVECTOR3(-0.5f, -0.5f, 0.0f);
    vertices[0].diffuse  = 0xFFFFFFFF;
    vertices[0].texcoord = D3DXVECTOR2( 0.0f,  sizey);
    vertices[1].position = D3DXVECTOR3(-0.5f,  0.5f, 0.0f);
    vertices[1].diffuse  = 0xFFFFFFFF;
    vertices[1].texcoord = D3DXVECTOR2( 0.0f,  0.0f);
    vertices[2].position = D3DXVECTOR3( 0.5f, -0.5f, 0.0f);
    vertices[2].diffuse  = 0xFFFFFFFF;
    vertices[2].texcoord = D3DXVECTOR2( sizex, sizey);
    vertices[3].position = D3DXVECTOR3( 0.5f,  0.5f, 0.0f);
    vertices[3].diffuse  = 0xFFFFFFFF;
    vertices[3].texcoord = D3DXVECTOR2( sizex, 0.0f);

    // Additional vertices required for some PS effects
    if(preProcess) {
	vertices[4].position = D3DXVECTOR3( 0.0f, 0.0f, 0.0f);
	vertices[4].diffuse  = 0xFFFFFF00;
	vertices[4].texcoord = D3DXVECTOR2( 0.0f, 1.0f);
	vertices[5].position = D3DXVECTOR3( 0.0f, 1.0f, 0.0f);
	vertices[5].diffuse  = 0xFFFFFF00;
	vertices[5].texcoord = D3DXVECTOR2( 0.0f, 0.0f);
	vertices[6].position = D3DXVECTOR3( 1.0f, 0.0f, 0.0f);
	vertices[6].diffuse  = 0xFFFFFF00;
	vertices[6].texcoord = D3DXVECTOR2( 1.0f, 1.0f);
	vertices[7].position = D3DXVECTOR3( 1.0f, 1.0f, 0.0f);
	vertices[7].diffuse  = 0xFFFFFF00;
	vertices[7].texcoord = D3DXVECTOR2( 1.0f, 0.0f);
    }

    // Unlock the vertex buffer
    vertexBuffer->Unlock();

    return S_OK;
}

void CDirect3D::SetupSceneScaled(void)
{
    unsigned char x=1, y=1;
    double sizex,sizey,ratio;

    pD3DDevice9->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
    pD3DDevice9->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    pD3DDevice9->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    pD3DDevice9->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
    pD3DDevice9->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    pD3DDevice9->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    pD3DDevice9->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    pD3DDevice9->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    pD3DDevice9->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

    D3DVIEWPORT9 Viewport;
    pD3DDevice9->GetViewport(&Viewport);

    // Projection is screenspace coords
    D3DXMatrixOrthoOffCenterLH(&m_matProj, 0.0f, (float)Viewport.Width, 0.0f, (float)Viewport.Height, 0.0f, 1.0f);

    // View matrix does offset
    // A -0.5f modifier is applied to vertex coordinates to match texture
    // and screen coords. Some drivers may compensate for this
    // automatically, but on others texture alignment errors are introduced
    // More information on this can be found in the Direct3D 9 documentation
    D3DXMatrixTranslation(&m_matView, (float)Viewport.Width/2-0.5f, (float)Viewport.Height/2+0.5f, 0.0f);

    // World View does scaling
    sizex = dwScaledWidth;
    sizey = dwScaledHeight;

    // aspect = -1 when in window mode
    if(render.aspect && (aspect > -1) && (dwWidth > 0) && (dwHeight > 0)) { // render.aspect IMPLEMENTED
	if(aspect == 0) {

	    // Do only integer scaling to avoid blurring
	    x = (Bitu)dwScaledWidth/dwWidth;
	    y = (Bitu)dwScaledHeight/dwHeight;

	    ratio = (double)(dwWidth*x)/(dwHeight*y);
#if LOG_D3D
	    LOG_MSG("D3D:Image aspect ratio is: %f (%dx%d)", ratio, x, y);
#endif

	    // Ajdust width
	    sizey = 4.0/3.0;
	    while(x > 1) {
		sizex = (double)(dwWidth*(x-1))/(dwHeight*y);
		if(fabs(sizex - sizey) > fabs(ratio - sizey)) {
		    break;
		} else {
		    x--;
		    ratio = sizex;
		}
#if LOG_D3D
		LOG_MSG("D3D:X adjust, new aspect ratio is: %f (%dx%d)", ratio, x, y);
#endif
	    }

	    // Adjust height
	    while(y > 1) {
		sizex = (double)(dwWidth*x)/(dwHeight*(y-1));
		if(fabs(sizex - sizey) > fabs(ratio - sizey)) {
		    break;
		} else {
		    y--;
		    ratio = sizex;
		}
#if LOG_D3D
		LOG_MSG("D3D:Y adjust, new aspect ratio is: %f (%dx%d)", ratio, x, y);
#endif
	    }

	    sizex = dwWidth*x;
	    sizey = dwHeight*y;

	} else if(aspect == 1) {

	    // We'll try to make the image as close as possible to 4:3
	    // (square pixels assumed (as in lcd not crt))
#if LOG_D3D
	    LOG_MSG("D3D:Scaling image to 4:3");
#endif
	    ratio = 4.0/3.0;

	    /*if(sizex*3 > sizey*4)
		sizex = sizey*ratio;
	    else if(sizex*3 < sizey*4)
		sizey = sizex/ratio;*/
				// shrink image down
				if( autofit == 0 ) {
					if(sizex*3 > sizey*4)
						sizex = sizey*ratio;
					else if(sizex*3 < sizey*4)
						sizey = sizex/ratio;
				}

				// stretch image to window + aspect ratio
				else if( autofit == 1 ) {
					double ratio_x, ratio_y;

					ratio_x = d3dpp.BackBufferWidth / sizex;
					ratio_y = d3dpp.BackBufferHeight / sizey;

					if( ratio_x < ratio_y ) {
						sizex = d3dpp.BackBufferWidth;
						sizey = d3dpp.BackBufferWidth / ratio;
					} else {
						sizey = d3dpp.BackBufferHeight;
						sizex = d3dpp.BackBufferHeight * ratio;
					}
				}

	}
    }

#if LOG_D3D
    LOG_MSG("D3D:Scaled resolution: %.1fx%.1f, factor: %dx%d", sizex, sizey, x, y);
#endif

    D3DXMatrixScaling(&m_matWorld, sizex, sizey, 1.0f);
}

#if !(C_D3DSHADERS)

D3DXMATRIX* CDirect3D::MatrixOrthoOffCenterLH(D3DXMATRIX *pOut, float l, float r, float b, float t, float zn, float zf)
{
    pOut->_11=2.0f/r; pOut->_12=0.0f;   pOut->_13=0.0f;  pOut->_14=0.0f;
    pOut->_21=0.0f;   pOut->_22=2.0f/t; pOut->_23=0.0f;  pOut->_24=0.0f;
    pOut->_31=0.0f;   pOut->_32=0.0f;   pOut->_33=1.0f;  pOut->_34=0.0f;
    pOut->_41=-1.0f;  pOut->_42=-1.0f;  pOut->_43=0.0f;  pOut->_44=1.0f;

    return pOut;
}

D3DXMATRIX* CDirect3D::MatrixScaling(D3DXMATRIX *pOut, float sx, float sy, float sz)
{
    pOut->_11=sx;     pOut->_12=0.0f;   pOut->_13=0.0f;  pOut->_14=0.0f;
    pOut->_21=0.0f;   pOut->_22=sy;     pOut->_23=0.0f;  pOut->_24=0.0f;
    pOut->_31=0.0f;   pOut->_32=0.0f;   pOut->_33=sz;    pOut->_34=0.0f;
    pOut->_41=0.0f;   pOut->_42=0.0f;   pOut->_43=0.0f;  pOut->_44=1.0f;

    return pOut;
}

D3DXMATRIX* CDirect3D::MatrixTranslation(D3DXMATRIX *pOut, float tx, float ty, float tz)
{
    pOut->_11=1.0f;   pOut->_12=0.0f;   pOut->_13=0.0f;  pOut->_14=0.0f;
    pOut->_21=0.0f;   pOut->_22=1.0f;   pOut->_23=0.0f;  pOut->_24=0.0f;
    pOut->_31=0.0f;   pOut->_32=0.0f;   pOut->_33=1.0f;  pOut->_34=0.0f;
    pOut->_41=tx;     pOut->_42=ty;     pOut->_43=tz;    pOut->_44=1.0f;

    return pOut;
}

#endif  // !(C_D3DSHADERS)

#endif 	// (HAVE_D3D9_H)
