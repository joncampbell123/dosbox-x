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

#ifndef __DIRECT3D_H_
#define __DIRECT3D_H_

#include <d3d9.h>
#include "dosbox.h"
#include "hq2x_d3d.h"

#define LOG_D3D 0		// Set this to 1 to enable D3D debug messages
#define D3D_THREAD 1		// Set this to 1 to thread Direct3D

#if LOG_D3D
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#endif

#if D3D_THREAD
#include "SDL_thread.h"
#endif

#define SAFE_RELEASE(p)		{ if(p) { (p)->Release(); (p)=NULL; } }

#if defined (_MSC_VER)						/* MS Visual C++ */
#define	strcasecmp(a,b) stricmp(a,b)
#endif

// Vertex format
#define D3DFVF_TLVERTEX D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1

#if C_D3DSHADERS
#include "ScalingEffect.h"
#else
#define D3DXMATRIX D3DMATRIX
#define D3DXVECTOR3 vec3f
#define D3DXVECTOR2 vec2f
#define D3DXMatrixOrthoOffCenterLH MatrixOrthoOffCenterLH
#define D3DXMatrixTranslation MatrixTranslation
#define D3DXMatrixScaling MatrixScaling

struct vec3f {
    float x, y, z;
    vec3f() { }
    vec3f(float vx, float vy, float vz)
    : x(vx), y(vy), z(vz) { }
};

struct vec2f {
    float x, y;
    vec2f() { }
    vec2f(float vx, float vy)
    : x(vx), y(vy) { }
};

#endif

class CDirect3D {
private:

    // globals
    HMODULE			mhmodDX9;
    IDirect3D9*			pD3D9;
    IDirect3DDevice9*		pD3DDevice9;

    D3DPRESENT_PARAMETERS 	d3dpp;			// Present parameters
    D3DLOCKED_RECT		d3dlr;			// Texture lock rectangle

    HWND hwnd;						// DOSBow window
    DWORD dwWidth, dwHeight;                            // DOSBox framebuffer size
    DWORD dwScaledWidth, dwScaledHeight;                // D3D backbuffer size
    const Bit16u* changedLines;

    // display modes
    D3DDISPLAYMODE*		modes;
    unsigned int		iMode;
    DWORD			dwNumModes;

    bool			deviceLost;

    // vertex stuff
    IDirect3DVertexBuffer9*	vertexBuffer;		// VertexBuffer

    // Custom vertex
    struct TLVERTEX {
	D3DXVECTOR3 position;       // vertex position
	D3DCOLOR    diffuse;
        D3DXVECTOR2 texcoord;       // texture coords
    };

    // Projection matrices
    D3DXMATRIX			m_matProj;
    D3DXMATRIX			m_matWorld;
    D3DXMATRIX			m_matView;

#if C_D3DSHADERS
    D3DXMATRIX			m_matPreProj;
    D3DXMATRIX			m_matPreView;
    D3DXMATRIX			m_matPreWorld;

    // Pixel shader
    char			pshader[30];
    ScalingEffect*		psEffect;
    LPDIRECT3DTEXTURE9		lpWorkTexture1;
    LPDIRECT3DTEXTURE9		lpWorkTexture2;
    LPDIRECT3DVOLUMETEXTURE9	lpHq2xLookupTexture;
#if LOG_D3D
    LPDIRECT3DTEXTURE9		lpDebugTexture;
#endif
#endif
    LPDIRECT3DTEXTURE9		lpTexture;		// D3D texture
    bool 			psEnabled;
    bool			preProcess;

    // function declarations
    HRESULT InitD3D(void);

    HRESULT RestoreDeviceObjects(void);
    HRESULT InvalidateDeviceObjects(void);
    HRESULT CreateDisplayTexture(void);
    HRESULT CreateVertex(void);
#if !(C_D3DSHADERS)
    D3DXMATRIX* MatrixOrthoOffCenterLH(D3DXMATRIX*, float, float, float, float, float, float);
    D3DXMATRIX* MatrixScaling(D3DXMATRIX*, float, float, float);
    D3DXMATRIX* MatrixTranslation(D3DXMATRIX*, float, float, float);
#endif

    void SetupSceneScaled(void);
    bool D3DSwapBuffers(void);

#if D3D_THREAD
    // Thread entry point must be static
    static int EntryPoint(void * pthis) { CDirect3D * pt = (CDirect3D *)pthis; return pt->Start(); }
    HRESULT Wait(bool unlock = true);
    int Start(void);

    SDL_Thread *thread;
    CRITICAL_SECTION cs;
    SDL_semaphore *thread_sem, *thread_ack;

    volatile enum D3D_state { D3D_IDLE = 0, D3D_LOADPS, D3D_LOCK, D3D_UNLOCK } thread_command;
    volatile bool thread_run, wait;
    volatile HRESULT thread_hr;
#if LOG_D3D
    void EnterLOGCriticalSection(LPCRITICAL_SECTION lpCriticalSection, int);
#endif
#endif

    HRESULT LoadPixelShader(void);
    HRESULT Resize3DEnvironment(void);
    HRESULT LockTexture(void);
    bool UnlockTexture(void);
    void DestroyD3D(void);

public:

    // texture stuff
    DWORD	dwTexHeight, dwTexWidth;

    bool 	square, pow2, dynamic, bpp16;		// Texture limitations
    Bit8s 	aspect, autofit;

    // Pixel shader status
    bool 	psActive;

    // function declarations
    HRESULT InitializeDX(HWND, bool);
    HRESULT LoadPixelShader(const char*, double, double, bool forced=false);
    HRESULT Resize3DEnvironment(Bitu, Bitu, Bitu, Bitu, bool fullscreen=false);
    bool LockTexture(Bit8u * & pixels,Bitu & pitch);
    bool UnlockTexture(const Bit16u *changed);

    CDirect3D(Bitu width = 640, Bitu height = 400):dwWidth(width),dwHeight(height) {
	mhmodDX9 = NULL;
	pD3D9 = NULL;
	pD3DDevice9 = NULL;
	modes = NULL;
	vertexBuffer = NULL;

	deviceLost = false;

	bpp16 = false;
	aspect = 0;
	lpTexture = NULL;

	psEnabled = false;
	psActive = false;
	preProcess = false;

#if C_D3DSHADERS
	lpWorkTexture1 = NULL;
	lpWorkTexture2 = NULL;
	lpHq2xLookupTexture = NULL;
#if LOG_D3D
	lpDebugTexture = NULL;
#endif
	strcpy(pshader, "shaders\\");
	psEffect = NULL;
#endif

#if D3D_THREAD
	thread = NULL;
	wait = false;
	thread_run = false;
	thread_command = D3D_IDLE;

	InitializeCriticalSection(&cs);
	thread_sem = SDL_CreateSemaphore(0);
	thread_ack = SDL_CreateSemaphore(0);
#endif

    }

    bool getForceUpdate(void) {
#if C_D3DSHADERS
	if (psEffect) return psEffect->getForceUpdate();
#endif
	return false;
    }

    ~CDirect3D() {
#if LOG_D3D
	LOG_MSG("D3D:Shutting down Direct3D");
#endif
	DestroyD3D();

#if D3D_THREAD
	DeleteCriticalSection(&cs);
	SDL_DestroySemaphore(thread_sem);
	SDL_DestroySemaphore(thread_ack);
#endif

	// Unload d3d9.dll
	if (mhmodDX9) {
	    FreeLibrary(mhmodDX9);
	    mhmodDX9 = NULL;
	}
    }
};

#endif // __DIRECT3D_H_
