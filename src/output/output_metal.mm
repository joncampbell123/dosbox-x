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

#if C_METAL_OUTPUT

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <AppKit/AppKit.h>

#include "output_metal.h"

extern VGA_Type vga;
extern VideoModeBlock* CurMode;

void Voodoo_OGL_SetDisplaySize(unsigned int w, unsigned int h);
void voodoo_metal_device_lost(void);

#if !defined(C_SDL2)
#include "SDL_metal.h"
extern "C" NSWindow *sdl1_hax_get_window(void);
extern "C" bool sdl1_hax_highdpi_enable;
#endif

CMetal::CMetal() {}
CMetal::~CMetal() { Shutdown(); }

void CMetal::ConfigureLayer(void)
{
    layer.device = device;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = NO;

    /* the frame's colors go to the display as they are, as with the surface and OpenGL outputs:
     * without a color space, macOS takes them as sRGB and converts them to the display's color
     * space, which makes them less saturated on wide gamut (Display P3) displays */
    SetLayerColorspace();
}

/* the layer is in the color space of the display the window is on, so its colors are not converted
 * (again on every resize: the window can be moved to another display) */
void CMetal::SetLayerColorspace(void)
{
    NSScreen* screen = (view && view.window && view.window.screen) ? view.window.screen : [NSScreen mainScreen];
    CGColorSpaceRef cs = screen ? [[screen colorSpace] CGColorSpace] : NULL;
    if(cs && !(layer.colorspace && CFEqual(layer.colorspace, cs))) layer.colorspace = cs;
}

void CMetal::AttachLayer(void* nsview, void* sdl_layer)
{
    CAMetalLayer* l = (__bridge CAMetalLayer*)sdl_layer;
    view = (__bridge NSView*)nsview;
    if(l != layer) {
        // (the window, and its layer, can be destroyed by SDL_SetVideoMode while it is still used)
        [l retain];
        if(layer_retained) [layer release];
        layer = l;
        layer_retained = true;
    }
    if(layer) ConfigureLayer();
    // the next Resize sets the size of the layer
    last_window_w = last_window_h = 0;
}

bool CMetal::Initialize(void* nsview, void* sdl_layer, int w, int h)
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
    if (sdl_layer) {
        /* SDL1: SDL's Metal view and layer (SDL_SetVideoMode with SDL_METAL) */
        AttachLayer(nsview, sdl_layer);
    }
    else {
        layer = [CAMetalLayer layer];
        ConfigureLayer();

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
    }

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
    /* the 3dfx emulation renders with this device */
    if(device) voodoo_metal_device_lost();
    externalTexture = nil;
    if(layer_retained) {
        /* SDL1: SDL's layer */
        [layer release];
        layer_retained = false;
        layer = nil;
    }

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

    // the 3dfx emulation's frame instead, while it is active
    DrawFrame(externalTexture ? externalTexture : frameTexture);
    textureMapped = false;
}

void CMetal::PresentExternal()
{
    if(!externalTexture || !layer) return;
    DrawFrame(externalTexture);
}

void CMetal::DrawFrame(id<MTLTexture> tex)
{
    @autoreleasepool {
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
    [enc setFragmentTexture : tex atIndex : 0] ;
    GetRenderMode();
    SetSamplerMode(enc);

    [enc drawPrimitives : MTLPrimitiveTypeTriangleStrip
            vertexStart : 0
            vertexCount : 4];
    [enc endEncoding] ;

    [cmd presentDrawable : drawable] ;
    [cmd commit] ;
    }
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
    // (every encoder starts without one)
    id<MTLSamplerState> s = samplerLinear;

    if(current_render_mode == METAL_SAMPLE_NEAREST)
        s = samplerNearest;

    [encoder setFragmentSamplerState : s atIndex : 0] ;
}

void CMetal::GetRenderMode() {
    Section_prop* section = static_cast<Section_prop*>(control->GetSection("render"));
    std::string s_aspect = section->Get_string("aspect");

    if(s_aspect == "nearest") {
        current_render_mode = METAL_SAMPLE_NEAREST;
    }
    else if(s_aspect == "bilinear") {
        current_render_mode = METAL_SAMPLE_BILINEAR;
    }
    else {
        current_render_mode = METAL_SAMPLE_NEAREST; // default
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

/* the size of the window's content area (what is drawn in), in the units the window size is set in */
static void metal_GetWindowSize(int* w, int* h)
{
#if defined(C_SDL2)
    SDL_GetWindowSize(sdl.window, w, h);
#else
    /* (from the window itself: it follows the user resizing it before the video mode is set again) */
    NSWindow* win = sdl1_hax_get_window();
    *w = *h = 0;
    if(win != nil) {
        NSRect r = [[win contentView] bounds];
        if(sdl1_hax_highdpi_enable) r = [[win contentView] convertRectToBacking:r];
        *w = (int)r.size.width;
        *h = (int)r.size.height;
    }
#endif
}

#if !defined(C_SDL2)
/* SDL1: the window is the video mode (SDL_SetVideoMode with SDL_METAL), w x h in a window, or the
 * screen at the desktop resolution in fullscreen (as SDL_WINDOW_FULLSCREEN_DESKTOP). SDL does not
 * draw in it, the layer of its view is drawn in. Setting the video mode can make another window,
 * and layer, so the output draws in the one there is after it. */
static bool metal_SetVideoMode(int w, int h)
{
    Uint32 flags = SDL_METAL | SDL_RESIZABLE;
    if(sdl.desktop.fullscreen) {
        w = (int)sdl.desktop.full.width;
        h = (int)sdl.desktop.full.height;
        flags = SDL_METAL | SDL_FULLSCREEN;
    }
    if(w <= 0 || h <= 0) return false;
    if(sdl.surface != NULL && (sdl.surface->flags & SDL_METAL) && sdl.surface->w == w && sdl.surface->h == h &&
        ((sdl.surface->flags & SDL_FULLSCREEN) != 0) == sdl.desktop.fullscreen)
        return true;
    SDL_Surface* s = SDL_SetVideoMode(w, h, 0, flags);
    if(s == NULL && sdl.desktop.fullscreen) {
        LOG_MSG("Metal: Fullscreen %dx%d not supported: %s", w, h, SDL_GetError());
        sdl.desktop.fullscreen = false;
        w = metal && metal->window_width ? (int)metal->window_width : 640;
        h = metal && metal->window_height ? (int)metal->window_height : 400;
        s = SDL_SetVideoMode(w, h, 0, SDL_METAL | SDL_RESIZABLE);
    }
    if(s == NULL) {
        LOG_MSG("Metal: Could not set video mode %dx%d: %s", w, h, SDL_GetError());
        return false;
    }
    sdl.surface = s;
    sdl.deferred_resize = false;
    sdl.must_redraw_all = true;
    NSWindow* win = sdl1_hax_get_window();
    if(metal && win != nil) metal->AttachLayer((__bridge void*)[win contentView], SDL_Metal_GetLayer());
    return true;
}
#endif

/* sets the window to w x h */
static void metal_SetWindowSize(int w, int h)
{
#if defined(C_SDL2)
    SDL_SetWindowSize(sdl.window, w, h);
#else
    metal_SetVideoMode(w, h);
#endif
}

void metal_init(void)
{
    if(metal) {
        metal->Shutdown();
#if !defined(C_SDL2)
        /* (not drawn in while the video mode is set for the new one) */
        delete metal;
        metal = nullptr;
#endif
    }

    sdl.desktop.want_type = SCREEN_METAL;

    //LOG_MSG("OUTPUT METAL: Init called");

#if defined(C_SDL2)
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
    void* sdl_layer = NULL;
#else
    /* a window SDL does not draw in, the size it is (or 640x400 at first) */
    if(sdl.surface == NULL || !(sdl.surface->flags & SDL_METAL)) {
        int cur_w = 640, cur_h = 400;
        if(sdl.surface != NULL && !sdl.desktop.fullscreen) {
            cur_w = sdl.surface->w;
            cur_h = sdl.surface->h;
        }
        if(!metal_SetVideoMode(cur_w, cur_h)) {
            OUTPUT_SURFACE_Select();
            return;
        }
    }

    NSWindow* nswin = sdl1_hax_get_window();
    NSView* view = nswin != nil ? [nswin contentView] : nil;
    void* sdl_layer = SDL_Metal_GetLayer();
    if(!sdl_layer) {
        LOG_MSG("METAL: No Metal layer: %s", SDL_GetError());
        OUTPUT_SURFACE_Select();
        return;
    }
#endif

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

    if(!metal->Initialize((__bridge void*)view, sdl_layer, w, h)) {
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
    /* follow the size being set (sdl.draw) here: the VGA code only does it while it draws, and
     * the 3dfx output (drawn in software) sets its own size while the VGA does not draw. Otherwise
     * the CPU buffer stays at the size before, and a larger frame is drawn past its end. */
    metal->CheckSourceResolution();
    uint32_t tex_w = metal->frame_width  ? metal->frame_width  : sdl.draw.width;
    uint32_t tex_h = metal->frame_height ? metal->frame_height : sdl.draw.height;

    /* ------------------------
     * Window logical size
     * ------------------------ */
    int cur_w = 0, cur_h = 0;
    metal_GetWindowSize(&cur_w, &cur_h);

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
#if defined(C_SDL2)
        SDL_SetWindowFullscreen(
            sdl.window,
            SDL_WINDOW_FULLSCREEN_DESKTOP);
#else
        metal_SetVideoMode(cur_w, cur_h);
        /* (fullscreen can fail, and leave a window) */
        metal->was_fullscreen = sdl.desktop.fullscreen;
#endif
    }
    else if(!sdl.desktop.fullscreen && metal->was_fullscreen){
        cur_w = metal->window_width;
        cur_h = metal->window_height;
#if defined(C_SDL2)
        SDL_SetWindowFullscreen(sdl.window, 0);
#else
        metal_SetVideoMode(cur_w, cur_h);
#endif
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

    /* the window is the size the user made it (or maximized), as with the surface and OpenGL outputs:
     * otherwise its size follows the video mode */
    const bool user_sized =
        (userResizeWindowWidth != 0 && userResizeWindowHeight != 0) || menu.maxwindow;
    const bool reset_window_size =
        !user_sized &&
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
        if(reset_window_size || (!user_sized && render.scale.size != last_scalesize)){
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
            if(window_w != last_window_w || window_h != last_window_h) metal_SetWindowSize(window_w, window_h);
            last_scalesize = render.scale.size;
        }
    }

    /* (also when only "fit to aspect ratio" or the aspect ratio changed: the shown region is) */
    if(window_w == last_window_w &&
        window_h == last_window_h &&
        tex_w == last_tex_w &&
        tex_h == last_tex_h &&
        (render.aspect != 0) == last_aspect &&
        (!render.aspect || target_ratio == last_target_ratio)) {
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

#if defined(C_SDL2)
    if(sdl.window && !sdl.desktop.fullscreen) {
#else
    if(sdl.surface && !sdl.desktop.fullscreen) {
#endif
        metal_SetWindowSize(window_w, window_h);
    }

    int real_w = 0, real_h = 0;
    metal_GetWindowSize(&real_w, &real_h);

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
    SetLayerColorspace();

#if defined(C_SDL2)
    CGRect newFrame = CGRectMake(0, 0, (CGFloat)width, (CGFloat)height);
    layer.frame = newFrame;

    layer.drawableSize = CGSizeMake(width * scale, height * scale);
#else
    /* SDL1: the layer of SDL's view, which fills the window (its size is in points; the window
     * size is in pixels with SDL1's high DPI support) */
    {
        CGRect b = NSRectToCGRect([nsv bounds]);
        layer.frame = b;
        layer.drawableSize = CGSizeMake(b.size.width * scale, b.size.height * scale);
    }
#endif

    uint32_t dw = (uint32_t)layer.drawableSize.width;
    uint32_t dh = (uint32_t)layer.drawableSize.height;

    /* with "fit to aspect ratio", the frame is shown in the largest region of the window (or screen)
     * with the aspect ratio, in the middle (black around it), as with the surface and OpenGL outputs.
     * Otherwise it fills the window. */
    if (render.aspect) {

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

    /* the 3dfx emulation can render at the size it is shown at */
    Voodoo_OGL_SetDisplaySize((unsigned int)(currentViewport.width + 0.5), (unsigned int)(currentViewport.height + 0.5));

    last_window_w = width;
    last_window_h = height;
    last_tex_w = frame_width;
    last_tex_h = frame_height;
    last_aspect = (render.aspect != 0);
    last_target_ratio = target_ratio;
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

/* the 3dfx emulation renders with Metal only with this output */
bool OUTPUT_Metal_Active(void)
{
    return metal != nullptr && sdl.desktop.type == SCREEN_METAL && metal->GetDevice() != nil;
}

CMetal* OUTPUT_Metal_Get(void)
{
    return metal;
}

#endif //#if C_METAL_OUTPUT
