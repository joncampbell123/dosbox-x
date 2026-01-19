#include "config.h"

#if C_DIRECT3D
#if defined(C_SDL2)

#include <SDL_syswm.h>

#include "sdlmain.h"
#include "control.h"
#include "dosbox.h"
#include "logging.h"
#include "menudef.h"
#include "render.h"
#include "output_surface.h"

#include "output_direct3d11.h"
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

static const char* vs_source = R"(
struct VSOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOut main(uint id : SV_VertexID)
{
    float2 pos[6] = {
        {-1,-1},{-1,1},{1,-1},
        {1,-1},{-1,1},{1,1}
    };

    float2 uv[6] = {
        {0,1},{0,0},{1,1},
        {1,1},{0,0},{1,0}
    };

    VSOut o;
    o.pos = float4(pos[id], 0, 1);
    o.uv  = uv[id];
    return o;
}
)";

static const char* ps_source = R"(
Texture2D tex0 : register(t0);
SamplerState smp : register(s0);

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0)
    : SV_Target
{
    return tex0.Sample(smp, uv);
}
)";

CDirect3D11::CDirect3D11() {}
CDirect3D11::~CDirect3D11() { Shutdown(); }

bool CDirect3D11::Initialize(HWND hwnd, int w, int h)
{
    width = w;
    height = h;

    cpu_pitch = width * 4;                 // 1ピクセル4バイト
    cpu_buffer.resize(cpu_pitch * height); // フレーム全体

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &sd,
        &swapchain,
        &device,
        nullptr,
        &context);

    if(FAILED(hr)) {
        LOG_MSG("D3D11: CreateDeviceAndSwapChain failed (0x%08lx)", hr);
        return false;
    }

    ID3D11Texture2D* backbuffer = nullptr;
    hr = swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer));
    if(FAILED(hr)) {
        LOG_MSG("D3D11: GetBuffer failed (0x%08lx)", hr);
        return false;
    }

    hr = device->CreateRenderTargetView(backbuffer, nullptr, &rtv);
    backbuffer->Release();
    if(FAILED(hr)) {
        LOG_MSG("D3D11: CreateRTV failed (0x%08lx)", hr);
        return false;
    }

    context->OMSetRenderTargets(1, &rtv, nullptr);

    D3D11_VIEWPORT vp = {};
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context->RSSetViewports(1, &vp);

    /* -------------------------------------------------
     * Create CPU texture
     * ------------------------------------------------- */
    D3D11_TEXTURE2D_DESC td_cpu = {};
    td_cpu.Width = width;
    td_cpu.Height = height;
    td_cpu.MipLevels = 1;
    td_cpu.ArraySize = 1;
    td_cpu.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td_cpu.SampleDesc.Count = 1;
    td_cpu.Usage = D3D11_USAGE_DYNAMIC;
    td_cpu.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td_cpu.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = device->CreateTexture2D(&td_cpu, nullptr, &frameTexCPU);
    if(FAILED(hr)) {
        LOG_MSG("D3D11: Create CPU texture failed (0x%08lx)", hr);
        return false;
    }

    /* -------------------------------------------------
     * Create GPU texture
     * ------------------------------------------------- */
    D3D11_TEXTURE2D_DESC td_gpu = td_cpu;
    td_gpu.Usage = D3D11_USAGE_DEFAULT;
    td_gpu.CPUAccessFlags = 0;

    hr = device->CreateTexture2D(&td_gpu, nullptr, &frameTexGPU);
    if(FAILED(hr)) {
        LOG_MSG("D3D11: Create GPU texture failed (0x%08lx)", hr);
        return false;
    }

    hr = device->CreateShaderResourceView(frameTexGPU, nullptr, &frameSRV);
    if(FAILED(hr)) {
        LOG_MSG("D3D11: Create SRV failed (0x%08lx)", hr);
        return false;
    }

    /* -------------------------------------------------
     * Shaders（SV_VertexID）
     * ------------------------------------------------- */
    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* err = nullptr;

    hr = D3DCompile(
        vs_source, strlen(vs_source),
        nullptr, nullptr, nullptr,
        "main", "vs_4_0",
        0, 0, &vs_blob, &err);

    if(FAILED(hr)) {
        LOG_MSG("VS compile error: %s",
            err ? (char*)err->GetBufferPointer() : "");
        return false;
    }

    hr = D3DCompile(
        ps_source, strlen(ps_source),
        nullptr, nullptr, nullptr,
        "main", "ps_4_0",
        0, 0, &ps_blob, &err);

    if(FAILED(hr)) {
        LOG_MSG("PS compile error: %s",
            err ? (char*)err->GetBufferPointer() : "");
        vs_blob->Release();
        return false;
    }

    device->CreateVertexShader(
        vs_blob->GetBufferPointer(),
        vs_blob->GetBufferSize(),
        nullptr,
        &vs);

    device->CreatePixelShader(
        ps_blob->GetBufferPointer(),
        ps_blob->GetBufferSize(),
        nullptr,
        &ps);

    vs_blob->Release();
    ps_blob->Release();

    /* -------------------------------------------------
     * Sampler
     * ------------------------------------------------- */
    D3D11_SAMPLER_DESC samp = {};
    samp.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samp.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samp.MinLOD = 0;
    samp.MaxLOD = D3D11_FLOAT32_MAX;

    hr = device->CreateSamplerState(&samp, &sampler);
    if(FAILED(hr)) {
        LOG_MSG("D3D11: CreateSamplerState failed (0x%08lx)", hr);
        return false;
    }

    return true;
}

void CDirect3D11::CheckSourceResolution()
{
    static uint32_t last_w = 0;
    static uint32_t last_h = 0;

    if(last_w == sdl.draw.width &&
        last_h == sdl.draw.height)
        return;

    LOG_MSG("D3D11: VGA source resolution changed %ux%u -> %ux%u",
        last_w, last_h,
        sdl.draw.width, sdl.draw.height);

    // Resize CPU buffer（don't shrink if smaller）
    ResizeCPUBuffer(
        sdl.draw.width,
        sdl.draw.height);

    last_w = sdl.draw.width;
    last_h = sdl.draw.height;

    Resize(
        sdl.draw.width, sdl.draw.height,   // Window size
        sdl.draw.width, sdl.draw.height);  // Frame texture size 
}

void CDirect3D11::ResizeCPUBuffer(uint32_t src_w, uint32_t src_h)
{
    const uint32_t required_pitch = src_w * 4; // BGRA32
    const uint32_t required_size = required_pitch * src_h;

    // Resize CPU buffer（don't shrink if smaller）
    if(cpu_buffer.size() < required_size) {
        cpu_buffer.resize(required_size);
        //LOG_MSG("D3D11: CPU buffer resized to %u bytes", required_size);
    }

    cpu_pitch = required_pitch;
}

void CDirect3D11::Shutdown()
{
    if(rtv) { rtv->Release(); rtv = nullptr; }
    if(swapchain) { swapchain->Release(); swapchain = nullptr; }
    if(context) { context->Release(); context = nullptr; }
    if(device) { device->Release(); device = nullptr; }
    if(frameTexCPU) { frameTexCPU->Release(); frameTexCPU = nullptr; }
    if(frameTexGPU) { frameTexGPU->Release(); frameTexGPU = nullptr; }
    if(vs) { vs->Release(); vs = nullptr; }
    if(ps) { ps->Release(); ps = nullptr; }
    if(sampler) { sampler->Release(); sampler = nullptr; }
    if(frameSRV) { frameSRV->Release(); frameSRV = nullptr; }
    if(fullscreenVB) {fullscreenVB->Release(); fullscreenVB = nullptr;}
    if(inputLayout) { inputLayout->Release(); inputLayout = nullptr; }
}

bool CDirect3D11::StartUpdate(uint8_t*& pixels, Bitu& pitch)
{
    if(textureMapped) return false;

    // Begin frame update by returning the CPU-side framebuffer
    pixels = cpu_buffer.data();
    pitch = cpu_pitch;

    textureMapped = true;
    return true;
}

void CDirect3D11::EndUpdate()
{
    {
        if(!textureMapped) return;

        /* Map dynamic texture for CPU write using WRITE_DISCARD (full frame update) */
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        HRESULT hr = context->Map(frameTexCPU, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if(FAILED(hr)) {
            LOG_MSG("D3D11: Map failed in EndUpdate (0x%08lx)", hr);
            textureMapped = false;
            return;
        }

        uint8_t* dst = static_cast<uint8_t*>(mapped.pData);
        uint8_t* src = cpu_buffer.data();

        const uint32_t copy_w = frame_width;
        const uint32_t copy_h = frame_height;

        // Copy each scanline from the CPU framebuffer to the mapped GPU texture.
        // The GPU texture rows are padded (RowPitch), so we must copy line by line.
        for(auto y = 0; y < copy_h; y++) {
            memcpy(dst, src, copy_w * 4);
            dst += mapped.RowPitch;
            src += cpu_pitch;
        }

        context->Unmap(frameTexCPU, 0);
        context->CopyResource(frameTexGPU, frameTexCPU);

        // Bind the back buffer render target (no depth buffer needed for 2D rendering)
        context->OMSetRenderTargets(1, &rtv, nullptr);

        // Set viewport to cover the entire output surface
        // This maps normalized device coordinates directly to the window area
        D3D11_VIEWPORT vp = {};
        vp.TopLeftX = 0.0f;
        vp.TopLeftY = 0.0f;
        vp.Width = (FLOAT)width;
        vp.Height = (FLOAT)height;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        context->RSSetViewports(1, &vp);

        // Draw a full-screen quad using two triangles (6 vertices)
        // Vertex positions are generated in the vertex shader via SV_VertexID
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->IASetInputLayout(inputLayout); // SV_VertexID 用

        context->VSSetShader(vs, nullptr, 0);
        context->PSSetShader(ps, nullptr, 0);

        context->PSSetSamplers(0, 1, &sampler);
        context->PSSetShaderResources(0, 1, &frameSRV);

        // Draw two triangles (6 vertices) to cover the entire screen
        context->Draw(6, 0);

        ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
        context->PSSetShaderResources(0, 1, nullSRV);

        // Present the rendered frame to the screen (vsync is currently disabled)
        swapchain->Present(0, 0);

        textureMapped = false;
    }
}

static CDirect3D11* d3d11 = nullptr;

void d3d11_init(void)
{
    if(sdl.desktop.want_type == SCREEN_DIRECT3D11 && d3d11)
        return;

    sdl.desktop.want_type = SCREEN_DIRECT3D11;

    LOG_MSG("D3D11: Init called");

    if(!sdl.window) { // Create window if not yet created
        Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
        if(sdl.desktop.fullscreen)
            flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        else
            flags |= SDL_WINDOW_RESIZABLE;

        sdl.window = SDL_CreateWindow(
            "DOSBox-X",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            640, 400,
            flags
        );

        if(!sdl.window) {
            LOG_MSG("SDL: Failed to create window: %s", SDL_GetError());
            OUTPUT_SURFACE_Select();
            return;
        }

        sdl.surface = SDL_GetWindowSurface(sdl.window);
        sdl.desktop.pixelFormat = SDL_GetWindowPixelFormat(sdl.window);
    }

    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);

#if defined(C_SDL2)
    if(!SDL_GetWindowWMInfo(sdl.window, &wmi))
#else
    if(!SDL_GetWMInfo(&wmi))
#endif
    {
        LOG_MSG("D3D11: Failed to get window info");
        OUTPUT_SURFACE_Select();
        return;
    }

    HWND hwnd = wmi.info.win.window;

    if(sdl.desktop.fullscreen)
        GFX_CaptureMouse();

    delete d3d11;
    d3d11 = new CDirect3D11();

    if(!d3d11) {
        LOG_MSG("D3D11: Failed to create object");
        OUTPUT_SURFACE_Select();
        return;
    }

    if(!d3d11->Initialize(hwnd, 640, 400)) {
        LOG_MSG("D3D11: Initialize failed");
        delete d3d11;
        d3d11 = nullptr;
        OUTPUT_SURFACE_Select();
        return;
    }
    sdl.desktop.type = SCREEN_DIRECT3D11;
}


void OUTPUT_DIRECT3D11_Select()
{
    sdl.desktop.want_type = SCREEN_DIRECT3D11;
    render.aspectOffload = true;
}

Bitu OUTPUT_DIRECT3D11_GetBestMode(Bitu flags)
{
    flags |= GFX_SCALING;
    flags &= ~(GFX_CAN_8 | GFX_CAN_15 | GFX_CAN_16);
    flags |= GFX_CAN_32;
    return flags;
}

void RENDER_Reset(void);
Bitu OUTPUT_DIRECT3D11_SetSize(void)
{
    if(!d3d11) {
        LOG_MSG("D3D11: Not initialized");
        return 0;
    }

    // Framebuffer size
    const uint32_t tex_w = d3d11->frame_width? d3d11->frame_width: sdl.draw.width;
    const uint32_t tex_h = d3d11->frame_height? d3d11->frame_height : sdl.draw.height;

    // Window Size
    int cur_w = 0, cur_h = 0;
    SDL_GetWindowSize(sdl.window, &cur_w, &cur_h);
    if(!sdl.desktop.fullscreen && !d3d11->was_fullscreen){
        d3d11->window_width = cur_w;
    }

    double target_ratio = 4.0 / 3.0; // default aspect ratio 4:3

    if(sdl.desktop.fullscreen && !d3d11->was_fullscreen) {
        d3d11->was_fullscreen = true;
        SDL_SetWindowFullscreen(
            sdl.window,
            SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
    else if(!sdl.desktop.fullscreen && d3d11->was_fullscreen){
        SDL_SetWindowFullscreen(sdl.window, 0);
        cur_w = d3d11->window_width;
        cur_h = cur_w / ((double)tex_w / tex_h);
        target_ratio = (double)tex_w / tex_h;
        d3d11->was_fullscreen = false;
        d3d11->Resize(
            cur_w, cur_h,   // Window size
            tex_w, tex_h);  // Frame texture size 
    }

    LOG_MSG(
        "D3D11 Resize: window=%ux%u frame=%ux%u",
        cur_w, cur_h,
        tex_w, tex_h);

    if(render.aspect) { // "Fit to aspect ratio" is enabled 
        if(aspect_ratio_x > 0 && aspect_ratio_y > 0)
            target_ratio = (double)aspect_ratio_x / aspect_ratio_y; // default is 4:3 if <=0
        if(cur_w < cur_h * target_ratio + 0.5 || cur_w > cur_h * target_ratio + 0.5) {
            cur_h = (uint32_t)cur_w / target_ratio + 0.5;
        }
    }
    if(!d3d11->Resize(
        cur_w, cur_h,   // Window size
        tex_w, tex_h))  // Frame texture size 
    {
        LOG_MSG("D3D11: Resize failed");
        return 0;
    }
    return GFX_CAN_32 | GFX_SCALING | GFX_HARDWARE;
}

bool CDirect3D11::Resize(
    uint32_t window_w,
    uint32_t window_h,
    uint32_t tex_w,
    uint32_t tex_h)
{
    // Texture size is fixed
    frame_width = tex_w;
    frame_height = tex_h;

    const int set_width = window_w;
    const int set_height = window_h;

    if(sdl.window && !sdl.desktop.fullscreen) {
        SDL_SetWindowSize(sdl.window, set_width, set_height);
    }

    int real_w = 0, real_h = 0;
    SDL_GetWindowSize(sdl.window, &real_w, &real_h);

    width = (uint32_t)real_w;
    height = (uint32_t)real_h;

    /* ----------------------------
     * 1. Unbind pipeline resources
     * ---------------------------- */
    context->OMSetRenderTargets(0, nullptr, nullptr);

    SAFE_RELEASE(rtv);
    SAFE_RELEASE(frameTexCPU);
    SAFE_RELEASE(frameTexGPU);
    SAFE_RELEASE(frameSRV);

    /* ----------------------------
     * 2. Resize the swap chain buffers
     * ---------------------------- */
    HRESULT hr = swapchain->ResizeBuffers(
        0,
        width,
        height,
        DXGI_FORMAT_UNKNOWN,
        0);

    if(FAILED(hr)) {
        LOG_MSG("D3D11: ResizeBuffers failed");
        return false;
    }

    /* ----------------------------
     * 3. Recreate back buffer render target view
     * ---------------------------- */
    ID3D11Texture2D* backbuffer = nullptr;
    hr = swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer));
    if(FAILED(hr)) {
        LOG_MSG("D3D11: GetBuffer failed");
        return false;
    }

    hr = device->CreateRenderTargetView(backbuffer, nullptr, &rtv);
    backbuffer->Release();

    if(FAILED(hr)) {
        LOG_MSG("D3D11: CreateRenderTargetView failed");
        return false;
    }

    context->OMSetRenderTargets(1, &rtv, nullptr);

    /* ----------------------------
     * 4. Update viewport to match the window size
     * ---------------------------- */
    D3D11_VIEWPORT vp = {};
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context->RSSetViewports(1, &vp);

    /* ----------------------------
     * 5. Recreate frame textures at internal resolution
     * ---------------------------- */
    if(!CreateFrameTextures(frame_width, frame_height))
        return false;

    return true;
}

bool CDirect3D11::CreateFrameTextures(
    uint32_t w,
    uint32_t h)
{
    /* ----------------------------
     * CPU-writable texture (mapped via Map)
     * ---------------------------- */
    D3D11_TEXTURE2D_DESC cpu = {};
    cpu.Width = w;
    cpu.Height = h;
    cpu.MipLevels = 1;
    cpu.ArraySize = 1;
    cpu.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    cpu.SampleDesc.Count = 1;
    cpu.Usage = D3D11_USAGE_DYNAMIC;
    cpu.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    cpu.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if(FAILED(device->CreateTexture2D(
        &cpu, nullptr, &frameTexCPU))) {
        LOG_MSG("D3D11: Create CPU frame texture failed");
        return false;
    }

    /* ----------------------------
     * GPU render texture
     * ---------------------------- */
    D3D11_TEXTURE2D_DESC gpu = cpu;
    gpu.Usage = D3D11_USAGE_DEFAULT;
    gpu.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    gpu.CPUAccessFlags = 0;

    if(FAILED(device->CreateTexture2D(
        &gpu, nullptr, &frameTexGPU))) {
        LOG_MSG("D3D11: Create GPU frame texture failed");
        return false;
    }

    /* ----------------------------
     * Shader Resource View (SRV)
     * ---------------------------- */
    D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Format = gpu.Format;
    srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels = 1;

    if(FAILED(device->CreateShaderResourceView(
        frameTexGPU, &srv, &frameSRV))) {
        LOG_MSG("D3D11: Create SRV failed");
        return false;
    }

    return true;
}

bool OUTPUT_DIRECT3D11_StartUpdate(uint8_t*& pixels, Bitu& pitch)
{
    //LOG_MSG("D3D11: StartUpdate");
    bool result = false;
    if(d3d11) result = d3d11->StartUpdate(pixels, pitch);
    return result;
}

void OUTPUT_DIRECT3D11_EndUpdate(const uint16_t* changedLines)
{
    //LOG_MSG("D3D11: EndUpdate called, changedLines=%p", changedLines);
    if(d3d11)
        d3d11->EndUpdate();
}

void OUTPUT_DIRECT3D11_Shutdown()
{
    if(d3d11) d3d11->Shutdown();
}

void OUTPUT_DIRECT3D11_CheckSourceResolution()
{
    if(d3d11) d3d11->CheckSourceResolution();
}

#endif //#if defined(C_SDL2)
#endif //#if C_DIRECT3D
