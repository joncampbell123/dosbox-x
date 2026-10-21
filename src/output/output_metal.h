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
#if C_METAL_OUTPUT

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <AppKit/AppKit.h>


void metal_init();

void OUTPUT_Metal_Select();
Bitu OUTPUT_Metal_GetBestMode(Bitu flags);
bool OUTPUT_Metal_StartUpdate(uint8_t*& pixels, Bitu& pitch);
void OUTPUT_Metal_EndUpdate(const uint16_t* changedLines);
Bitu OUTPUT_Metal_SetSize(void);
void OUTPUT_Metal_Shutdown();
void OUTPUT_Metal_CheckSourceResolution();
bool OUTPUT_Metal_Active(void);
class CMetal;
CMetal* OUTPUT_Metal_Get(void);

// how the frame is scaled to the window (render.aspect: "nearest" or "bilinear")
enum { METAL_SAMPLE_NEAREST, METAL_SAMPLE_BILINEAR };

class CMetal {
public:
    CMetal();
    ~CMetal();

    // nsview: the window's content view. sdl_layer: SDL1, the layer of SDL's Metal view (SDL_metal.h);
    // SDL2, NULL (a view and layer of our own are added to the content view)
    bool Initialize(void* nsview, void* sdl_layer, int w, int h);
    // SDL1: draw in the layer of a video mode set with SDL_METAL, after every SDL_SetVideoMode()
    void AttachLayer(void* nsview, void* sdl_layer);
    void Shutdown();

    bool StartUpdate(uint8_t*& pixels, Bitu& pitch);
    void EndUpdate();

    bool Resize(uint32_t window_w, uint32_t window_h,
        uint32_t tex_w, uint32_t tex_h);

    void ResizeCPUBuffer(uint32_t src_w, uint32_t src_h);
    bool CreateSampler();
    void GetRenderMode();
    bool CreatePipeline();
    bool CreateFrameTexture(uint32_t w, uint32_t h);
    void SetSamplerMode(id<MTLRenderCommandEncoder> encoder);
    void CheckSourceResolution();

    // for the Metal 3dfx emulation (voodoo_metal.mm), which renders with this device and queue
    id<MTLDevice> GetDevice(void) { return device; }
    id<MTLCommandQueue> GetQueue(void) { return queue; }
    // its frame, drawn instead of the frame texture while set
    void SetExternalTexture(id<MTLTexture> tex) { externalTexture = tex; }
    void PresentExternal(void);

    uint32_t frame_width = 0;
    uint32_t frame_height = 0;

    int cpu_pitch = 0;
    std::vector<uint8_t> cpu_buffer;
    uint32_t window_width = 0;
    uint32_t window_height = 0;
    uint32_t last_window_w = 0;
    uint32_t last_window_h = 0;
    uint32_t last_tex_w = 0;
    uint32_t last_tex_h = 0;
    uint32_t last_scalesize = 0;
    bool last_aspect = false;        // what the shown region was worked out for (render.aspect)
    double last_target_ratio = 0.0;
    bool was_fullscreen = false;

private:
    NSView* view = nil;

    id<MTLDevice> device = nil;
    id<MTLCommandQueue> queue = nil;
    CAMetalLayer* layer = nil;
    bool layer_retained = false;  // SDL1: SDL's layer, kept while drawn in

    void ConfigureLayer(void);
    void SetLayerColorspace(void);

    id<MTLTexture> frameTexture = nil;
    id<MTLTexture> externalTexture = nil;
    id<MTLSamplerState> samplerNearest = nil;
    id<MTLSamplerState> samplerLinear = nil;
    id<MTLRenderPipelineState> pipeline = nil;

    bool textureMapped = false;
    int current_render_mode = METAL_SAMPLE_NEAREST;
    MTLViewport currentViewport;

    void DrawFrame(id<MTLTexture> tex);
};

#endif // C_METAL_OUTPUT
