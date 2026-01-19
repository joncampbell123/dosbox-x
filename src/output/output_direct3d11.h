#pragma once

#include <sys/types.h>
#include <assert.h>
#include <math.h>

#include "control.h"
#include "dosbox.h"
#include "logging.h"
#include "menudef.h"
#include "render.h"

#include <output/output_direct3d.h>
#include <output/output_surface.h>
#include <output/output_tools.h>
#include <output/output_tools_xbrz.h>

#include "sdlmain.h"

using namespace std;
#if C_DIRECT3D
#if defined(C_SDL2)

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

void d3d11_init();
void OUTPUT_DIRECT3D11_Select();
Bitu OUTPUT_DIRECT3D11_GetBestMode(Bitu flags);
Bitu OUTPUT_DIRECT3D11_SetSize();
bool OUTPUT_DIRECT3D11_StartUpdate(uint8_t*& pixels, Bitu& pitch);
void OUTPUT_DIRECT3D11_EndUpdate(const uint16_t*);
void OUTPUT_DIRECT3D11_Shutdown();
void OUTPUT_DIRECT3D11_CheckSourceResolution();

class CDirect3D11 {
public:
    CDirect3D11();
    ~CDirect3D11();

    bool Initialize(HWND hwnd, int width, int height);
    void Shutdown();

    void Clear();
    void Present(bool vsync);
    bool StartUpdate(uint8_t*& pixels, Bitu& pitch);
    void EndUpdate();
    bool Resize(uint32_t window_w, uint32_t window_h, uint32_t tex_w, uint32_t tex_h);
    bool CreateFrameTextures(uint32_t w, uint32_t h);
    void CheckSourceResolution();
    void ResizeCPUBuffer(uint32_t src_w, uint32_t src_h);
    uint32_t frame_width = 0, frame_height = 0;   // Framebuffer size (Internal resolution)
    uint32_t window_width = 0; // Window width (Used when returning from fullscreen)
    bool was_fullscreen = false;

private:
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swapchain = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;

    ID3D11Texture2D* frameTexCPU = nullptr; // Map 用（D3D11_USAGE_DYNAMIC）
    ID3D11Texture2D* frameTexGPU = nullptr; // Copy 用（D3D11_USAGE_DEFAULT）
    ID3D11SamplerState* sampler = nullptr;
    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader* ps = nullptr;
    ID3D11InputLayout* inputLayout = nullptr;

    ID3D11ShaderResourceView* frameSRV = nullptr;

    ID3D11Buffer* fullscreenVB = nullptr;
    UINT stride = 0;
    UINT offset = 0;

    int width = 0, height = 0;
    int cpu_pitch = 0;
    std::vector<uint8_t> cpu_buffer;
    bool textureMapped = false;
};

#endif // defined(C_SDL2)
#endif // C_DIRECT3D
