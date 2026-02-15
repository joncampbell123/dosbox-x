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
#if defined(MACOSX)
#if defined(C_SDL2)

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <AppKit/AppKit.h>


void metal_init();

void OUTPUT_METAL_Select();
Bitu OUTPUT_METAL_GetBestMode(Bitu flags);
bool OUTPUT_Metal_StartUpdate(uint8_t*& pixels, Bitu& pitch);
void OUTPUT_Metal_EndUpdate(const uint16_t* changedLines);
Bitu OUTPUT_Metal_SetSize(void);
void OUTPUT_Metal_Shutdown();
void OUTPUT_Metal_CheckSourceResolution();

class CMetal {
public:
    bool Initialize(void* nsview, int w, int h);
    void Shutdown();

    bool StartUpdate(uint8_t*& pixels, Bitu& pitch);
    void EndUpdate();

    bool Resize(uint32_t window_w, uint32_t window_h,
        uint32_t tex_w, uint32_t tex_h);

    void ResizeCPUBuffer(uint32_t src_w, uint32_t src_h);
    bool CreateSampler();
    void GetRenderMode();
    bool CreatePipeline();

    uint32_t frame_width = 0;
    uint32_t frame_height = 0;

    int cpu_pitch = 0;
    std::vector<uint8_t> cpu_buffer;

    uint32_t window_width = 0;
    uint32_t window_height = 0;
    bool was_fullscreen = false;

private:
    NSView* view = nil;

    id<MTLDevice> device = nil;
    id<MTLCommandQueue> queue = nil;
    CAMetalLayer* layer = nil;

    id<MTLTexture> frameTexture = nil;
    id<MTLSamplerState> samplerNearest = nil;
    id<MTLSamplerState> samplerLinear = nil;
    id<MTLRenderPipelineState> pipeline = nil;

    bool textureMapped = false;
};

#endif // defined(C_SDL2)
#endif // defined(MACOSX)
