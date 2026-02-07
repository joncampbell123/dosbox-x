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
#include "vga.h"
#include "..\ints\int10.h"
#include "output_surface.h"

#include "output_direct3d11.h"
//#pragma comment(lib, "d3d11.lib")
//#pragma comment(lib, "dxgi.lib")

#include <d3dcompiler.h>
//#pragma comment(lib, "d3dcompiler.lib")

typedef HRESULT(WINAPI* PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)(
    IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT,
    const D3D_FEATURE_LEVEL*, UINT, UINT,
    const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**,
    ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

typedef HRESULT(WINAPI* PFN_D3D_COMPILE)(
    LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*,
    ID3DInclude*, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);

static PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN pD3D11CreateDeviceAndSwapChain = nullptr;
static PFN_D3D_COMPILE my_pD3DCompile = nullptr;
static HMODULE hD3D11 = nullptr;
static HMODULE hD3DCompiler = nullptr;

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

extern VGA_Type vga;
extern VideoModeBlock* CurMode;

CDirect3D11::CDirect3D11() {}
CDirect3D11::~CDirect3D11() { Shutdown(); }

bool CDirect3D11::Initialize(HWND hwnd, int w, int h)
{
    last_window_w = 0;
    last_window_h = 0;
    last_tex_w = 0;
    last_tex_h = 0;

    width = w;
    height = h;

    cpu_pitch = width * 4;                 
    cpu_buffer.resize(cpu_pitch * height); 

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

    HRESULT hr = pD3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &swapchain, &device, nullptr, &context);

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

    if(!CreateFrameTextures(width, height))
        return false;

    /**
    hr = device->CreateShaderResourceView(frameTexGPU, nullptr, &frameSRV);
    if(FAILED(hr)) {
        LOG_MSG("D3D11: Create SRV failed (0x%08lx)", hr);
        return false;
    }
    */

    /* -------------------------------------------------
     * Shaders（SV_VertexID）
     * ------------------------------------------------- */
    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* err = nullptr;

    hr = my_pD3DCompile(
        vs_source, strlen(vs_source),
        nullptr, nullptr, nullptr,
        "main", "vs_4_0",
        0, 0, &vs_blob, &err);

    if(FAILED(hr)) {
        LOG_MSG("VS compile error: %s",
            err ? (char*)err->GetBufferPointer() : "");
        return false;
    }

    hr = my_pD3DCompile(
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

    if(!CreateSamplers()) {
        LOG_MSG("D3D11: CreateSamplers failed");
        return false;
    }
    GetRenderMode();
    SetSamplerMode();

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
    if(context) {
        context->ClearState();
        context->Flush();
    }
    SAFE_RELEASE(frameSRV);
    SAFE_RELEASE(frameTexGPU);
    SAFE_RELEASE(frameTexCPU);
    SAFE_RELEASE(rtv);
    SAFE_RELEASE(swapchain);
    //SAFE_RELEASE(context);
    SAFE_RELEASE(device);
    SAFE_RELEASE(frameSRV);
    if(vs) { vs->Release(); vs = nullptr; }
    if(ps) { ps->Release(); ps = nullptr; }
    SAFE_RELEASE(samplerNearest);
    SAFE_RELEASE(samplerLinear);
    SAFE_RELEASE(samplerStretch);
    if(fullscreenVB) {fullscreenVB->Release(); fullscreenVB = nullptr;}
    //if(inputLayout) { inputLayout->Release(); inputLayout = nullptr; }
    d3d11_UnloadDLL();
}

bool CDirect3D11::StartUpdate(uint8_t*& pixels, Bitu& pitch)
{
    if(textureMapped) return false;

    // Begin frame update by returning the CPU-side framebuffer
    pixels = cpu_buffer.data();
    pitch = cpu_pitch;
    render.scale.outWrite = cpu_buffer.data();
    render.scale.outPitch = cpu_pitch;
    textureMapped = true;
    return true;
}

void CDirect3D11::EndUpdate()
{
    if(!textureMapped) return;

    /* Map dynamic texture for CPU write using WRITE_DISCARD (full frame update) */
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = context->Map(frameTexGPU, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if(FAILED(hr)) {
        LOG_MSG("D3D11: Map failed in EndUpdate (0x%08lx)", hr);
        textureMapped = false;
        return;
    }

    uint8_t* dst = static_cast<uint8_t*>(mapped.pData);
    uint8_t* src = cpu_buffer.data();

    const uint32_t copy_w = frame_width;
    const uint32_t copy_h = frame_height;
    const size_t lineBytes = copy_w * 4;

    // Copy each scanline from the CPU framebuffer to the mapped GPU texture.
    // The GPU texture rows are padded (RowPitch), so we must copy line by line.
    if(mapped.RowPitch == cpu_pitch) {
        memcpy(dst, src, lineBytes * copy_h);
    }
    else {
        for(uint32_t y = 0; y < copy_h; y++) {
            memcpy(dst, src, lineBytes);
            dst += mapped.RowPitch;
            src += cpu_pitch;
        }
    }

    context->Unmap(frameTexGPU, 0);
    //context->CopyResource(frameTexGPU, frameTexCPU);

    // Bind the back buffer render target (no depth buffer needed for 2D rendering)
    context->OMSetRenderTargets(1, &rtv, nullptr);

    // Set viewport to cover the entire output surface
    // This maps normalized device coordinates directly to the window area
    /**
    D3D11_VIEWPORT vp = {};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context->RSSetViewports(1, &vp);
    */

    // Draw a full-screen quad using two triangles (6 vertices)
    // Vertex positions are generated in the vertex shader via SV_VertexID
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetInputLayout(inputLayout); // For SV_VertexID 

    context->VSSetShader(vs, nullptr, 0);
    context->PSSetShader(ps, nullptr, 0);
    context->PSSetShaderResources(0, 1, &frameSRV);

    SetSamplerMode();
    // Draw two triangles (6 vertices) to cover the entire screen
    context->Draw(6, 0);

    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    context->PSSetShaderResources(0, 1, nullSRV);

    // Present the rendered frame to the screen (vsync is currently disabled)
    swapchain->Present(0, 0);

    textureMapped = false;
}

bool CDirect3D11::CreateSamplers(void) {
    D3D11_SAMPLER_DESC samp = {};
    samp.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samp.MinLOD = 0;
    samp.MaxLOD = D3D11_FLOAT32_MAX;
    samp.MaxAnisotropy = 16;

    // Nearest
    samp.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    if(FAILED(device->CreateSamplerState(&samp, &samplerNearest))) return false;

    // Bilinear
    samp.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    if(FAILED(device->CreateSamplerState(&samp, &samplerLinear))) return false;

    // Stretch
    samp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    if(FAILED(device->CreateSamplerState(&samp, &samplerStretch))) return false;

    return true;
}

void CDirect3D11::SetSamplerMode() {
    static int last_mode = -1;
    if(last_mode == current_render_mode) return;
    ID3D11SamplerState* s = samplerLinear;
    if (current_render_mode == ASPECT_NEAREST) s = samplerNearest;

    context->PSSetSamplers(0, 1, &s);
    last_mode = current_render_mode;
}

void CDirect3D11::GetRenderMode() {
    Section_prop* section = static_cast<Section_prop*>(control->GetSection("render"));
    std::string s_aspect = section->Get_string("aspect");

    if(s_aspect == "nearest") {
        current_render_mode = ASPECT_NEAREST;
    }
    else if(s_aspect == "bilinear") {
        current_render_mode = ASPECT_BILINEAR;
    }
    else {
        current_render_mode = ASPECT_NEAREST; // default
    }
}

bool d3d11_LoadDLL() {
    if(!hD3D11) {
        hD3D11 = LoadLibraryA("d3d11.dll");
        if(!hD3D11) return false;
        pD3D11CreateDeviceAndSwapChain = (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)GetProcAddress(hD3D11, "D3D11CreateDeviceAndSwapChain");
    }

    if(!hD3DCompiler) {
        const char* dlls[] = { "d3dcompiler_47.dll", "d3dcompiler_43.dll", "d3dcompiler.dll" };
        for(int i = 0; i < 3; i++) {
            hD3DCompiler = LoadLibraryA(dlls[i]);
            if(hD3DCompiler) break;
        }
        if(hD3DCompiler) {
            my_pD3DCompile = (PFN_D3D_COMPILE)GetProcAddress(hD3DCompiler, "D3DCompile");
        }
    }

    return (pD3D11CreateDeviceAndSwapChain != nullptr && my_pD3DCompile != nullptr);
}

void d3d11_UnloadDLL() {
    pD3D11CreateDeviceAndSwapChain = nullptr;
    my_pD3DCompile = nullptr;

    if(hD3DCompiler) {
        FreeLibrary(hD3DCompiler);
        hD3DCompiler = nullptr;
    }
    if(hD3D11) {
        FreeLibrary(hD3D11);
        hD3D11 = nullptr;
    }

    //LOG_MSG("D3D11: DLLs unloaded.");
}

static CDirect3D11* d3d11 = nullptr;

void d3d11_init(void)
{
    if(d3d11) {
        d3d11->Shutdown();
    }
    sdl.desktop.want_type = SCREEN_DIRECT3D11;

    LOG_MSG("D3D11: Init called");

    if(!sdl.window) { // Create window if not yet created
        sdl.window = GFX_SetSDLWindowMode(640, 400, SCREEN_SURFACE);

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

    if(d3d11) delete d3d11;
    d3d11 = new CDirect3D11();

    if(!d3d11) {
        LOG_MSG("D3D11: Failed to create object");
        OUTPUT_SURFACE_Select();
        return;
    }

    if(!d3d11_LoadDLL()) {
        LOG_MSG("D3D11: Failed to load d3d11.dll or d3dcompiler_xx.dll");
        OUTPUT_SURFACE_Select();
        return;
    }

    if(!d3d11->Initialize(hwnd, sdl.draw.width?sdl.draw.width:640, sdl.draw.height?sdl.draw.height:400)) {
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

Bitu OUTPUT_DIRECT3D11_SetSize(void)
{
    if(!d3d11) {
        LOG_MSG("D3D11: Not initialized");
        return 0;
    }

    // Framebuffer size
    uint32_t tex_w = d3d11->frame_width? d3d11->frame_width: sdl.draw.width;
    uint32_t tex_h = d3d11->frame_height? d3d11->frame_height : sdl.draw.height;

    // Window Size
    int cur_w = 0, cur_h = 0;
    SDL_GetWindowSize(sdl.window, &cur_w, &cur_h);
    if(!sdl.desktop.fullscreen && !d3d11->was_fullscreen){
        d3d11->window_width = cur_w;
    }

    if(sdl.desktop.fullscreen && !d3d11->was_fullscreen) {
        d3d11->was_fullscreen = true;
        d3d11->window_width = cur_w;
        d3d11->window_height = cur_w * sdl.draw.height / sdl.draw.width;
        SDL_SetWindowFullscreen(
            sdl.window,
            SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
    else if(!sdl.desktop.fullscreen && d3d11->was_fullscreen){
        SDL_SetWindowFullscreen(sdl.window, 0);
        cur_w = d3d11->window_width;
        cur_h = d3d11->window_height;
        d3d11->was_fullscreen = false;
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
    uint32_t window_w, // current window width
    uint32_t window_h, // current window height
    uint32_t tex_w,    // texture width
    uint32_t tex_h)    // texture height
{
    const bool reset_window_size =
        (userResizeWindowWidth == 0) &&
        (userResizeWindowHeight == 0) &&
        !sdl.desktop.fullscreen;

    double target_ratio = 4.0 / 3.0; // default aspect ratio 4:3
    if(render.aspect) { // "Fit to aspect ratio" is enabled 
        if(aspect_ratio_x > 0 && aspect_ratio_y > 0)
            target_ratio = (double)aspect_ratio_x / aspect_ratio_y; // default is 4:3 if <=0
    }
    else target_ratio = (double)tex_w / tex_h;

    if(!sdl.desktop.fullscreen) {
        if(reset_window_size) {
            window_w = tex_w;
            window_h = (uint32_t)(window_w / target_ratio + 0.5);
        }
    }

    if(window_w == last_window_w &&
        window_h == last_window_h &&
        tex_w == last_tex_w &&
        tex_h == last_tex_h) {
        return true; // No change
    }

    // Texture size is fixed
    frame_width = tex_w;
    frame_height = tex_h;

    if(sdl.window && !sdl.desktop.fullscreen) {
        SDL_SetWindowSize(sdl.window, window_w, window_h);
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

    double screen_ratio = (double)width / height;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float vp_w = (float)width;
    float vp_h = (float)height;

    if(sdl.desktop.fullscreen && render.aspect) {
        if(screen_ratio > target_ratio) {
            vp_w = vp_h * target_ratio;
            offset_x = ((float)width - vp_w) * 0.5f;
        }
        else if(screen_ratio < target_ratio) {
            vp_h = vp_w / target_ratio;
            offset_y = ((float)height - vp_h) * 0.5f;
        }
    }

    /* ----------------------------
     * 4. Update viewport to match the window size
     * ---------------------------- */
    D3D11_VIEWPORT vp = {};
    vp.TopLeftX = offset_x;
    vp.TopLeftY = offset_y;
    vp.Width = (FLOAT)vp_w;
    vp.Height = (FLOAT)vp_h;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context->RSSetViewports(1, &vp);

    /* ----------------------------
     * 5. Recreate frame textures at internal resolution
     * ---------------------------- */
    if(tex_w != last_tex_w || tex_h != last_tex_h) {
        SAFE_RELEASE(frameTexCPU);
        SAFE_RELEASE(frameTexGPU);
        SAFE_RELEASE(frameSRV);
        if(!CreateFrameTextures(frame_width, frame_height))
            return false;
    }

    LOG_MSG(
        "D3D11 Resize: window=%ux%u frame=%ux%u",
        width, height,
        frame_width, frame_height);
    last_window_w = width;
    last_window_h = height;
    last_tex_w = frame_width;
    last_tex_h = frame_height;
    return true;
}

bool CDirect3D11::CreateFrameTextures(
    uint32_t w,
    uint32_t h)
{
    /* ----------------------------
     * CPU-writable texture (mapped via Map)
     * ---------------------------- */

    /**
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
    */

    /* ----------------------------
     * GPU render texture
     * ---------------------------- */
    D3D11_TEXTURE2D_DESC gpu = {};
    gpu.Width = w;
    gpu.Height = h;
    gpu.MipLevels = 1;
    gpu.ArraySize = 1;
    gpu.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    gpu.SampleDesc.Count = 1;
    gpu.Usage = D3D11_USAGE_DYNAMIC;
    gpu.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    gpu.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    gpu.MiscFlags = 0;

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

#if 0
void D3D11_DrawLine_8bpp(const void* src)
{
    const uint8_t* s = static_cast<const uint8_t*>(src);
    const unsigned int w = render.src.width;
    if(d3d11) {
        uint32_t* dst = reinterpret_cast<uint32_t*>(d3d11->cpu_buffer.data() + vga.draw.lines_done * d3d11->cpu_pitch);

        for(unsigned int x = 0; x < w; x++) {
            dst[x] = vga.dac.xlat32[s[x]]; // 8bpp → 32bpp
            //LOG_MSG("D3D11: DrawLine_8bpp x=%u idx=%u color=0x%08lx", x, idx, dst[x]);
        }
    }

    if(vga.draw.lines_done == vga.draw.lines_total-1){
        RENDER_EndUpdate(false);
    }

}
#endif

#endif //#if defined(C_SDL2)
#endif //#if C_DIRECT3D
