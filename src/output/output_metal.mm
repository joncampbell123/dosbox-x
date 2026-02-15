#include "config.h"

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

#if defined(MACOSX)
#if defined(C_SDL2)
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <AppKit/AppKit.h>

#include "output_metal.h"

vertex float4 vs_main(uint vid [[vertex_id]] )
{
    float2 pos[6] = {
        {-1,-1},{-1,1},{1,-1},
        {1,-1},{-1,1},{1,1}
    };

    return float4(pos[vid], 0, 1);
}

fragment float4 ps_main(
    float2 uv [[stage_in]],
    texture2d<float> tex [[texture(0)]],
    sampler smp [[sampler(0)]] )
{
    return tex.sample(smp, uv);
}

extern VGA_Type vga;
extern VideoModeBlock* CurMode;

CMetal::CMetal() {}
CMetal::~CMetal() { Shutdown(); }

bool CMetal::Initialize(void* nsview, int w, int h)
{
    device = MTLCreateSystemDefaultDevice();
    if(!device) {
        LOG_MSG("OUTPUT Metal: Backend not supported, fallback");
        return false;
    }

    queue = [device newCommandQueue];
    if(!queue) {
        LOG_MSG("OUTPUT Metal: Failed to create command queue");
        return false;
    }

    layer = [CAMetalLayer layer];
    layer.device = device;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = NO;

    view = (__bridge NSView*)nsview;
    view.wantsLayer = YES;
    view.layer = layer;

    frame_width = w;
    frame_height = h;

    cpu_pitch = w * 4;
    cpu_buffer.resize(cpu_pitch * h);

    return CreateFrameTexture(w, h) &&
        CreatePipeline() &&
        CreateSampler();
}

void CMetal::CheckSourceResolution()
{
    static uint32_t last_w = 0;
    static uint32_t last_h = 0;

    if(last_w == sdl.draw.width &&
        last_h == sdl.draw.height)
        return;

    LOG_MSG("Metal: VGA source resolution changed %ux%u -> %ux%u",
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

void CMetal::ResizeCPUBuffer(uint32_t src_w, uint32_t src_h)
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

void CMetal::Shutdown()
{
    frameTexture = nil;
    pipeline = nil;
    queue = nil;
    device = nil;
    samplerNearest = nil;
    samplerLinear = nil;
}

bool CMetal::StartUpdate(uint8_t*& pixels, Bitu& pitch)
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

void CMetal::EndUpdate()
{
    if(!textureMapped) return;

    MTLRegion region = {
        {0,0,0},
        {frame_width, frame_height, 1}
    };

    [frameTexture replaceRegion : region
        mipmapLevel : 0
        withBytes : cpu_buffer.data()
        bytesPerRow : cpu_pitch] ;

    id<CAMetalDrawable> drawable = [layer nextDrawable];
    if(!drawable) return;

    MTLRenderPassDescriptor* pass =
        [MTLRenderPassDescriptor renderPassDescriptor];

    pass.colorAttachments[0].texture = drawable.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;

    id<MTLCommandBuffer> cmd = [queue commandBuffer];

    id<MTLRenderCommandEncoder> enc =
        [cmd renderCommandEncoderWithDescriptor : pass];

    [enc setRenderPipelineState : pipeline] ;
    [enc setFragmentTexture : frameTexture atIndex : 0] ;
    GetRenderMode();
    SetSamplerMode(enc);

    [enc drawPrimitives : MTLPrimitiveTypeTriangle
        vertexStart : 0
        vertexCount : 6] ;

    [enc endEncoding] ;

    [cmd presentDrawable : drawable] ;
    [cmd commit] ;

    textureMapped = false;
}


bool CMetal::CreateSampler()
{
    MTLSamplerDescriptor* s = [[MTLSamplerDescriptor alloc]init];

    // ---- Nearest ----
    s.minFilter = MTLSamplerMinMagFilterNearest;
    s.magFilter = MTLSamplerMinMagFilterNearest;
    samplerNearest = [device newSamplerStateWithDescriptor : s];

    // ---- Linear ----
    s.minFilter = MTLSamplerMinMagFilterLinear;
    s.magFilter = MTLSamplerMinMagFilterLinear;
    samplerLinear = [device newSamplerStateWithDescriptor : s];

    return samplerNearest && samplerLinear;
}

void CMetal::SetSamplerMode(id<MTLRenderCommandEncoder> encoder)
{
    static int last_mode = -1;
    if(last_mode == current_render_mode) return;

    id<MTLSamplerState> s = samplerLinear;

    if(current_render_mode == ASPECT_NEAREST)
        s = samplerNearest;

    [encoder setFragmentSamplerState : s atIndex : 0] ;

    last_mode = current_render_mode;
}

void CMetal::GetRenderMode() {
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

bool CMetal::CreatePipeline()
{
    NSError* err = nil;

    id<MTLLibrary> lib = [device newDefaultLibrary];
    if(!lib) return nil;

    id<MTLFunction> vs = [lib newFunctionWithName : @"vs_main"];
    id<MTLFunction> ps = [lib newFunctionWithName : @"ps_main"];

    MTLRenderPipelineDescriptor* desc =
        [[MTLRenderPipelineDescriptor alloc]init];

    desc.vertexFunction = vs;
    desc.fragmentFunction = ps;
    desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

    pipeline = [device newRenderPipelineStateWithDescriptor : desc error : &err];
    if(!pipeline && err) {
        LOG_MSG("OUTPUT Metal: Pipeline error: %s", err.localizedDescription.UTF8String);
    }

    return pipeline != nil;
}

static CMetal* metal = nullptr;

void metal_init(void)
{
    if(metal) {
        metal->Shutdown();
    }

    sdl.desktop.want_type = SCREEN_METAL;

    LOG_MSG("OUTPUT METAL: Init called");

    if(!sdl.window) {
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

    if(!SDL_GetWindowWMInfo(sdl.window, &wmi)) {
        LOG_MSG("METAL: Failed to get WM info");
        OUTPUT_SURFACE_Select();
        return;
    }

    NSWindow* nswin = (__bridge NSWindow*)wmi.info.cocoa.window;
    NSView* view = [nswin contentView];

    if(!view) {
        LOG_MSG("METAL: Failed to get NSView");
        OUTPUT_SURFACE_Select();
        return;
    }

    if(sdl.desktop.fullscreen)
        GFX_CaptureMouse();

    delete metal;
    metal = new CMetal();

    if(!metal) {
        LOG_MSG("METAL: Failed to create object");
        OUTPUT_SURFACE_Select();
        return;
    }

    int w = sdl.draw.width ? sdl.draw.width : 640;
    int h = sdl.draw.height ? sdl.draw.height : 400;

    if(!metal->Initialize((__bridge void*)view, w, h)) {
        LOG_MSG("METAL: Initialize failed");
        delete metal;
        metal = nullptr;
        OUTPUT_SURFACE_Select();
        return;
    }

    sdl.desktop.type = SCREEN_METAL;
}


void OUTPUT_METAL_Select()
{
    sdl.desktop.want_type = SCREEN_METAL;
    render.aspectOffload = true;
}

Bitu OUTPUT_METAL_GetBestMode(Bitu flags)
{
    flags |= GFX_SCALING;
    flags &= ~(GFX_CAN_8 | GFX_CAN_15 | GFX_CAN_16);
    flags |= GFX_CAN_32;
    return flags;
}

Bitu OUTPUT_Metal_SetSize(void)
{
    if(!metal) {
        LOG_MSG("Metal: Not initialized");
        return 0;
    }

    /* ------------------------
     * Framebuffer size
     * ------------------------ */
    uint32_t tex_w = metal->frame_width ? metal->frame_width : sdl.draw.width;
    uint32_t tex_h = metal->frame_height ? metal->frame_height : sdl.draw.height;

    /* ------------------------
     * Window size
     * ------------------------ */
    int cur_w = 0, cur_h = 0;
    SDL_GetWindowSize(sdl.window, &cur_w, &cur_h);

    if(!sdl.desktop.fullscreen && !metal->was_fullscreen) {
        metal->window_width = cur_w;
        metal->window_height = cur_h;
    }

    /* ---- Enter fullscreen ---- */
    if(sdl.desktop.fullscreen && !metal->was_fullscreen) {

        metal->was_fullscreen = true;
        metal->window_width = cur_w;
        metal->window_height = cur_h;

        SDL_SetWindowFullscreen(
            sdl.window,
            SDL_WINDOW_FULLSCREEN_DESKTOP);
    }

    /* ---- Exit fullscreen ---- */
    else if(!sdl.desktop.fullscreen && metal->was_fullscreen) {

        SDL_SetWindowFullscreen(sdl.window, 0);

        cur_w = metal->window_width;
        cur_h = metal->window_height;

        metal->was_fullscreen = false;
    }

    /* ------------------------
     * Resize Metal side
     * ------------------------ */
    if(!metal->Resize(
        cur_w,
        cur_h,
        tex_w,
        tex_h))
    {
        LOG_MSG("Metal: Resize failed");
        return 0;
    }

    return GFX_CAN_32 | GFX_SCALING | GFX_HARDWARE;
}


extern bool hardware_scaler_selected;
bool CMetal::Resize(uint32_t window_w,
    uint32_t window_h,
    uint32_t tex_w,
    uint32_t tex_h)
{
    layer.drawableSize = CGSizeMake(window_w, window_h);

    if(tex_w != frame_width || tex_h != frame_height)
    {
        frame_width = tex_w;
        frame_height = tex_h;

        cpu_pitch = tex_w * 4;
        cpu_buffer.resize(cpu_pitch * tex_h);

        return CreateFrameTexture(tex_w, tex_h);
    }

    return true;
}


bool CMetal::CreateFrameTexture(uint32_t w, uint32_t h)
{
    MTLTextureDescriptor* desc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat :
    MTLPixelFormatBGRA8Unorm
        width : w
        height : h
        mipmapped : NO];

    desc.usage = MTLTextureUsageShaderRead;

    frameTexture = [device newTextureWithDescriptor : desc];

    return frameTexture != nil;
}

bool OUTPUT_Metal_StartUpdate(uint8_t*& pixels, Bitu& pitch)
{
    //LOG_MSG("D3D11: StartUpdate");
    bool result = false;
    if(metal) result = metal->StartUpdate(pixels, pitch);
    return result;
}

void OUTPUT_Metal_EndUpdate(const uint16_t* changedLines)
{
    //LOG_MSG("D3D11: EndUpdate called, changedLines=%p", changedLines);
    if(metal)
        metal->EndUpdate();
}

void OUTPUT_Metal_Shutdown()
{
    if(metal) metal->Shutdown();
}

void OUTPUT_Metal_CheckSourceResolution()
{
    if(metal) metal->CheckSourceResolution();
}

#endif //#if defined(C_SDL2)
#endif //#if defined(MACOSX)
