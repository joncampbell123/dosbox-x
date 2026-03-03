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

#if defined(MACOSX) && C_METAL
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

    /**
    LOG_MSG("Metal: drawableSize = %f x %f",
            layer.drawableSize.width,
            layer.drawableSize.height);
    */

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
        //LOG_MSG("METAL: EndUpdate textureMapped=false");
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
        MTLClearColorMake(0, 0, 0, 1); // Black
    pass.colorAttachments[0].texture = drawable.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;

    id<MTLCommandBuffer> cmd = [queue commandBuffer];

    id<MTLRenderCommandEncoder> enc =
        [cmd renderCommandEncoderWithDescriptor : pass];
    [enc setViewport:currentViewport];
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

    //LOG_MSG("OUTPUT METAL: Init called");

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
    int cur_w = 0, cur_h = 0;
    SDL_GetWindowSize(sdl.window, &cur_w, &cur_h);

    if (cur_w <= 0 || cur_h <= 0)
        return 0;

    /* ------------------------
     * Fullscreen handling
     * ------------------------ */
    if(!sdl.desktop.fullscreen && !metal->was_fullscreen){
        metal->window_width = cur_w;
        metal->window_height = cur_h;
    }

    if(sdl.desktop.fullscreen && !metal->was_fullscreen) {
        metal->was_fullscreen = true;
        metal->window_width = cur_w;
        metal->window_height = cur_w * sdl.draw.height / sdl.draw.width;
        SDL_SetWindowFullscreen(
            sdl.window,
            SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
    else if(!sdl.desktop.fullscreen && metal->was_fullscreen){
        SDL_SetWindowFullscreen(sdl.window, 0);
        cur_w = metal->window_width;
        cur_h = metal->window_height;
        metal->was_fullscreen = false;
    }

    /* ------------------------
     * Metal resize
     * ------------------------ */
    if (!metal->Resize(cur_w, cur_h, tex_w, tex_h)) {
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
    //LOG_MSG("Resize called: win=%u,%u tex=%u,%u", window_w, window_h, tex_w, tex_h);

    const bool reset_window_size =
        (((userResizeWindowWidth == 0) && (userResizeWindowHeight == 0)) ||
        (tex_w != last_tex_w || tex_h != last_tex_h))
        && !sdl.desktop.fullscreen;

    double target_ratio = 4.0 / 3.0; // default aspect ratio 4:3
    if(render.aspect) { // "Fit to aspect ratio" is enabled 
        if(aspect_ratio_x > 0 && aspect_ratio_y > 0)
            target_ratio = (double)aspect_ratio_x / aspect_ratio_y;    // user-defined / preset aspect ratio
        else if(aspect_ratio_x < 0 && aspect_ratio_y < 0 || IS_PC98_ARCH)
            target_ratio = (double)CurMode->swidth / CurMode->sheight; // Use current mode's aspect ratio
    }
    else if(tex_h != CurMode->sheight) {
        target_ratio = (double)CurMode->swidth / CurMode->sheight;
    }
    else target_ratio = (double)tex_w / tex_h;

    uint32_t width=0, height=0;
    if(!sdl.desktop.fullscreen) {
        if(hardware_scaler_selected) {
            render.scale.hardware = true;
            hardware_scaler_selected = false;
        }
        if(reset_window_size || render.scale.size != last_scalesize){
            if(tex_h >= CurMode->sheight * 2) { // doublescan mode
                width = tex_w;
                height = tex_h;
                if(render.aspect) {
                    width = (uint32_t)((double)height * CurMode->swidth / CurMode->sheight +0.5); // First adjust width to match the original aspect ratio.
                    height = (uint32_t)((double)width / target_ratio + 0.5); // Then adjust height to match the target aspect ratio. This ensures the final window size maintains the target aspect ratio, even in doublescan mode.
                }
                window_w = (uint32_t)(height * target_ratio * (render.scale.hardware ? (double)render.scale.size / 2.0 : 1u) + 0.5);
                window_h = (uint32_t)(height * (render.scale.hardware ? (double)render.scale.size / 2.0 : 1u) + 0.5);
            }
            else {
                window_w = tex_w * (render.scale.hardware ? render.scale.size : 1);
                if(CurMode->type == M_TEXT && vga.mode != M_HERC_GFX) window_w = (uint32_t)((double)window_w / 2.0 + 0.5); // Suppress window size in text mode
                if(window_w < tex_w) window_w = tex_w; // Keep at least original size
                window_h = (uint32_t)((double)window_w / target_ratio + 0.5);
            }
            if(window_w != last_window_w || window_h != last_window_h) SDL_SetWindowSize(sdl.window, window_w, window_h);
            last_scalesize = render.scale.size;
        }
        if(render.aspect) {
            int real_w = 0, real_h = 0;
            SDL_GetWindowSize(sdl.window, &real_w, &real_h);
            if(real_w > 0) {
                window_w = real_w;
                window_h = (uint32_t)((double)window_w / target_ratio + 0.5);
            }
            if(window_w != last_window_w || window_h != last_window_h) SDL_SetWindowSize(sdl.window, window_w, window_h);
            //LOG_MSG("window_w=%d, window_h=%d, sdl.draw.width=%d, real_w=%d, real_h=%d, w/h=%lf, target=%lf", window_w, window_h, sdl.draw.width, real_w, real_h, (double)real_w/real_h, target_ratio);
        }
    }

    if(window_w == last_window_w &&
        window_h == last_window_h &&
        tex_w == last_tex_w &&
        tex_h == last_tex_h) {
        return true; // No change
    }

    /* ---------------------------------
     * 1. Recreate Frame Texture
     * --------------------------------- */

    if (tex_w != frame_width ||
        tex_h != frame_height)
    {
        frame_width  = tex_w;
        frame_height = tex_h;
        ResizeCPUBuffer(frame_width, frame_height);

        if (!CreateFrameTexture(frame_width,
                                frame_height))
        {
            LOG_MSG("Metal: CreateFrameTexture failed in Resize");
            return false;
        }
        LOG_MSG("Metal: Texture resized to %ux%u", tex_w, tex_h);
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

    /* ---------------------------------
     * 2. Retina / HiDPI
     * --------------------------------- */
    CGFloat scale = 1.0;
    NSView* nsv = (__bridge NSView*)view;

    if (nsv.window) {
        scale = nsv.window.backingScaleFactor;
    } else {
        scale = [NSScreen mainScreen].backingScaleFactor;
    }
    /* ---------------------------------
     * 3. Update Layer size
     * --------------------------------- */
    layer.contentsScale = scale;

    CGRect newFrame = CGRectMake(0, 0, (CGFloat)width, (CGFloat)height);
    layer.frame = newFrame;

    layer.drawableSize = CGSizeMake(width * scale, height * scale);

    uint32_t dw = (uint32_t)layer.drawableSize.width;
    uint32_t dh = (uint32_t)layer.drawableSize.height;

    if (sdl.desktop.fullscreen && render.aspect) {

        double win_ratio = (double)dw / (double)dh;

        double vp_w, vp_h;
        double vp_x = 0.0;
        double vp_y = 0.0;

        if (win_ratio > target_ratio) {
            vp_h = dh;
            vp_w = vp_h * target_ratio;
            vp_x = (dw - vp_w) * 0.5;
        }
        else {
            vp_w = dw;
            vp_h = vp_w / target_ratio;
            vp_y = (dh - vp_h) * 0.5;
        }

        currentViewport = { vp_x, vp_y, vp_w, vp_h, 0.0, 1.0 };
    }
    else {
        currentViewport = { 0.0, 0.0, (double)dw, (double)dh, 0.0, 1.0 };
    }

    last_window_w = width;
    last_window_h = height;
    last_tex_w = frame_width;
    last_tex_h = frame_height;
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
