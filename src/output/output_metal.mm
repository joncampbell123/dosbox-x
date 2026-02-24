#include "config.h"

#include <SDL_syswm.h>

#include "sdlmain.h"
#include "control.h"
#include "dosbox.h"
#include "logging.h"
#include "menudef.h"
#include "render.h"
#include "vga.h"
#include "../ints/int10.h"
#include "output_surface.h"

#if defined(MACOSX)
#if defined(C_SDL2)

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <AppKit/AppKit.h>

#include "output_metal.h"

extern VGA_Type vga;
extern VideoModeBlock* CurMode;

CMetal::CMetal() {}
CMetal::~CMetal() { Shutdown(); }

bool CMetal::Initialize(void* nsview, int w, int h)
{
    /* ---------------------------------
     * 1. Metal Device
     * --------------------------------- */
    device = MTLCreateSystemDefaultDevice();
    if (!device) {
        LOG_MSG("Metal: No device available");
        return false;
    }

    queue = [device newCommandQueue];
    if (!queue) {
        LOG_MSG("Metal: Failed to create command queue");
        return false;
    }
    this->view = (__bridge NSView*)nsview;

    /* ---------------------------------
    * 2. Create Metal Layer + SubView
    * --------------------------------- */
    layer = [CAMetalLayer layer];
    layer.device = device;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = NO;

    /* Metal専用NSViewを作る */
    NSView* parentView = (__bridge NSView*)view;
    NSView* metalView = [[NSView alloc] initWithFrame:parentView.bounds];

    metalView.autoresizingMask =
        NSViewWidthSizable | NSViewHeightSizable;

    metalView.wantsLayer = YES;
    metalView.layer = layer;

    /* SDLのcontentViewに追加 */
    [view addSubview:metalView];

    /* layerサイズ同期 */
    layer.frame = metalView.bounds;

    /* ---------------------------------
     * 3. Retina / drawableSize
     * --------------------------------- */
    CGFloat scale = 1.0;

#if TARGET_OS_OSX
    if (view.window)
        scale = view.window.backingScaleFactor;
#endif

    LOG_MSG("Metal: drawableSize = %f x %f",
            layer.drawableSize.width,
            layer.drawableSize.height);

    /* ---------------------------------
     * 4. CPU framebuffer
     * --------------------------------- */
    frame_width  = w;
    frame_height = h;

    cpu_pitch = w * 4;
    cpu_buffer.resize(cpu_pitch * h);

    /* ---------------------------------
     * 5. GPU resources
     * --------------------------------- */
    if (!CreateFrameTexture(w, h)) {
        LOG_MSG("Metal: CreateFrameTexture failed");
        return false;
    }

    if (!CreatePipeline()) {
        LOG_MSG("Metal: CreatePipeline failed");
        return false;
    }

    if (!CreateSampler()) {
        LOG_MSG("Metal: CreateSampler failed");
        return false;
    }
    CheckSourceResolution();
    LOG_MSG("Metal: Initialize complete");

    return true;
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
    if(!textureMapped){
        LOG_MSG("METAL: EndUpdate textureMapped=false");
        return;
    }

    MTLRegion region = {
        {0,0,0},
        {frame_width, frame_height, 1}
    };

    [frameTexture replaceRegion : region
        mipmapLevel : 0
        withBytes : cpu_buffer.data()
        bytesPerRow : cpu_pitch] ;

    id<CAMetalDrawable> drawable = [layer nextDrawable];
    if (!drawable) {
        LOG_MSG("Metal: drawable is NULL");
        return;
    }
    //LOG_MSG("Drawable texture size = %f x %f",
    //    drawable.texture.width,
    //    drawable.texture.height);

    MTLRenderPassDescriptor* pass =
        [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].clearColor =
        MTLClearColorMake(0, 1, 0, 1); // 緑
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

    [enc drawPrimitives : MTLPrimitiveTypeTriangleStrip
            vertexStart : 0
            vertexCount : 4];
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
    if(!(samplerNearest && samplerLinear)) LOG_MSG("METAL:CreateSampler failed");
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

    NSString* src = @"\
    #include <metal_stdlib>\n\
    using namespace metal;\n\
    \
    struct VSOut {\
        float4 pos [[position]];\
        float2 uv;\
    };\
    \
    vertex VSOut vs_main(uint vid [[vertex_id]]) {\
        float2 pos[4] = {\
            {-1.0,-1.0},{ 1.0,-1.0},\
            {-1.0, 1.0},{ 1.0, 1.0}\
        };\
        float2 uv[4] = {\
            {0.0,1.0},{1.0,1.0},\
            {0.0,0.0},{1.0,0.0}\
        };\
        VSOut o;\
        o.pos = float4(pos[vid],0,1);\
        o.uv  = uv[vid];\
        return o;\
    }\
    \
    fragment float4 ps_main(VSOut in [[stage_in]],\
                            texture2d<float> tex [[texture(0)]],\
                            sampler smp [[sampler(0)]]) {\
        return tex.sample(smp, in.uv);\
    }";

    id<MTLLibrary> lib =
        [device newLibraryWithSource:src options:nil error:&err];

    if (!lib) {
        LOG_MSG("Metal: Shader compile error: %s",
                err.localizedDescription.UTF8String);
        return false;
    }

    id<MTLFunction> vs = [lib newFunctionWithName:@"vs_main"];
    id<MTLFunction> ps = [lib newFunctionWithName:@"ps_main"];

    if (!vs || !ps) {
        LOG_MSG("Metal: Shader function missing");
        return false;
    }

    MTLRenderPipelineDescriptor* desc =
        [[MTLRenderPipelineDescriptor alloc] init];

    desc.vertexFunction   = vs;
    desc.fragmentFunction = ps;
    desc.colorAttachments[0].pixelFormat =
        MTLPixelFormatBGRA8Unorm;

    desc.colorAttachments[0].blendingEnabled = NO;

    pipeline =
        [device newRenderPipelineStateWithDescriptor:desc
                                               error:&err];

    if (!pipeline) {
        LOG_MSG("Metal: Pipeline creation failed: %s",
                err.localizedDescription.UTF8String);
        return false;
    }

    return true;
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


void OUTPUT_Metal_Select()
{
    sdl.desktop.want_type = SCREEN_METAL;
    render.aspectOffload = true;
}

Bitu OUTPUT_Metal_GetBestMode(Bitu flags)
{
    flags |= GFX_SCALING;
    flags &= ~(GFX_CAN_8 | GFX_CAN_15 | GFX_CAN_16);
    flags |= GFX_CAN_32;
    return flags;
}

Bitu OUTPUT_Metal_SetSize(void)
{
    if (!metal) {
        LOG_MSG("Metal: Not initialized");
        return 0;
    }

    /* ------------------------
     * Framebuffer (texture) size
     * ------------------------ */
    uint32_t tex_w = metal->frame_width  ? metal->frame_width  : sdl.draw.width;
    uint32_t tex_h = metal->frame_height ? metal->frame_height : sdl.draw.height;

    /* ------------------------
     * Window logical size
     * ------------------------ */
    int win_w = 0, win_h = 0;
    SDL_GetWindowSize(sdl.window, &win_w, &win_h);

    if (win_w <= 0 || win_h <= 0)
        return 0;

    /* ------------------------
     * Fullscreen handling
     * ------------------------ */
    if (sdl.desktop.fullscreen && !metal->was_fullscreen) {

        metal->was_fullscreen = true;
        metal->window_width  = win_w;
        metal->window_height = win_h;

        SDL_SetWindowFullscreen(
            sdl.window,
            SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
    else if (!sdl.desktop.fullscreen && metal->was_fullscreen) {

        SDL_SetWindowFullscreen(sdl.window, 0);

        win_w = metal->window_width;
        win_h = metal->window_height;

        metal->was_fullscreen = false;
    }
    else if (!sdl.desktop.fullscreen) {
        metal->window_width  = win_w;
        metal->window_height = win_h;
    }

    /* ------------------------
     * Metal resize
     * ------------------------ */
    if (!metal->Resize(win_w, win_h, tex_w, tex_h)) {
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
    if (!layer || !view)
        return false;
    LOG_MSG("Resize called: win=%u,%u tex=%u,%u", window_w, window_h, tex_w, tex_h);

    /* ---------------------------------
     * 2. Retina / HiDPI
     * --------------------------------- */
    CGFloat scale = 1.0;
    NSView* nsv = (__bridge NSView*)view;

    if (nsv.window) {
        scale = nsv.window.backingScaleFactor;
    } else {
        // ウィンドウにアタッチされていない場合はメイン画面のスケールを参照
        scale = [NSScreen mainScreen].backingScaleFactor;
    }
    /* ---------------------------------
     * 2. Layerサイズの更新
     * --------------------------------- */
    layer.contentsScale = scale;

    // 直接引数の window_w/h を使ってフレームを設定
    // これにより superlayer が nil でもサイズが決まる
    CGRect newFrame = CGRectMake(0, 0, (CGFloat)window_w, (CGFloat)window_h);
    layer.frame = newFrame;

    // 実際に描画されるピクセル解像度を設定
    layer.drawableSize = CGSizeMake(window_w * scale, window_h * scale);
    /* ---------------------------------
     * 3. フレームテクスチャ再生成
     * --------------------------------- */
    if (tex_w != frame_width ||
        tex_h != frame_height)
    {
        frame_width  = tex_w;
        frame_height = tex_h;

        cpu_pitch = frame_width * 4;
        cpu_buffer.resize(cpu_pitch * frame_height);

        if (!CreateFrameTexture(frame_width,
                                frame_height))
        {
            LOG_MSG("Metal: CreateFrameTexture failed in Resize");
            return false;
        }
    }
    LOG_MSG("Metal: Texture resized to %ux%u", tex_w, tex_h);
    return true;
}


bool CMetal::CreateFrameTexture(uint32_t w, uint32_t h)
{
    MTLTextureDescriptor* desc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
            MTLPixelFormatBGRA8Unorm
            width:w
            height:h
            mipmapped:NO];

    desc.usage =
        MTLTextureUsageShaderRead |
        MTLTextureUsageShaderWrite;
    
    desc.storageMode = MTLStorageModeShared;

    frameTexture = [device newTextureWithDescriptor:desc];

    if(!frameTexture)
        LOG_MSG("METAL: CreateFrameTexture failed");

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
    //LOG_MSG("METAL: EndUpdate called, changedLines=%p", changedLines);
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
