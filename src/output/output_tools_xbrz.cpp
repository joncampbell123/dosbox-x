#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include "dosbox.h"
#include "sdlmain.h"

using namespace std;

#if C_XBRZ

struct SDL_xBRZ sdl_xbrz;

void xBRZ_Initialize()
{
    Section_prop* section = static_cast<Section_prop *>(control->GetSection("render"));

    LOG(LOG_MISC, LOG_DEBUG)("Early init (renderer): xBRZ options");

    // set some defaults
    sdl_xbrz.task_granularity = 16;
    sdl_xbrz.max_scale_factor = xbrz::SCALE_FACTOR_MAX;

    // read options related to xBRZ here
    Prop_multival* prop = section->Get_multival("scaler");
    std::string scaler = prop->GetSection()->Get_string("type");
    sdl_xbrz.enable = ((scaler == "xbrz") || (scaler == "xbrz_bilinear"));
    sdl_xbrz.postscale_bilinear = (scaler == "xbrz_bilinear");
    xBRZ_Change_Options(section);
}

void xBRZ_Change_Options(Section_prop* section)
{
    sdl_xbrz.task_granularity = section->Get_int("xbrz slice");
    sdl_xbrz.fixed_scale_factor = section->Get_int("xbrz fixed scale factor");
    sdl_xbrz.max_scale_factor = section->Get_int("xbrz max scale factor");
    if ((sdl_xbrz.max_scale_factor < 2) || (sdl_xbrz.max_scale_factor > xbrz::SCALE_FACTOR_MAX))
        sdl_xbrz.max_scale_factor = xbrz::SCALE_FACTOR_MAX;
    if ((sdl_xbrz.fixed_scale_factor < 2) || (sdl_xbrz.fixed_scale_factor > xbrz::SCALE_FACTOR_MAX))
        sdl_xbrz.fixed_scale_factor = 0;
}

// returns true if scaling possible/enabled, false otherwise
bool xBRZ_SetScaleParameters(int srcWidth, int srcHeight, int dstWidth, int dstHeight)
{
    sdl_xbrz.scale_factor = (sdl_xbrz.fixed_scale_factor == 0) ?
        static_cast<int>(std::sqrt((double)dstWidth * dstHeight / (srcWidth * srcHeight)) + 0.5) :
        sdl_xbrz.fixed_scale_factor;

    // enable minimal scaling if upscale is still possible but requires post-downscale
    // having aspect ratio correction on always implies enabled scaler because it gives better quality than DOSBox own method
    if (sdl_xbrz.scale_factor == 1 && (render.aspect || dstWidth > srcWidth || dstHeight > srcHeight))
        sdl_xbrz.scale_factor = 2;

    if (sdl_xbrz.scale_factor >= 2)
    {
        // ok to scale, now clamp scale factor if corresponding max option is set
        sdl_xbrz.scale_factor = min(sdl_xbrz.scale_factor, sdl_xbrz.max_scale_factor);
        sdl_xbrz.scale_on = true;
    }
    else
    {
        // scaling impossible
        sdl_xbrz.scale_on = false;
    }

    return sdl_xbrz.scale_on;
}

void xBRZ_Render(const uint32_t* renderBuf, uint32_t* xbrzBuf, const uint16_t *changedLines, const int srcWidth, const int srcHeight, int scalingFactor)
{
#ifdef XBRZ_PPL
    if (changedLines) // perf: in worst case similar to full input scaling
    {
        concurrency::task_group tg; // perf: task_group is slightly faster than pure parallel_for

        int yLast = 0;
        Bitu y = 0, index = 0;
        while (y < sdl.draw.height)
        {
            if (!(index & 1))
                y += changedLines[index];
            else
            {
                const int sliceFirst = (int)y;
                const int sliceLast = (int)y + changedLines[index];
                y += changedLines[index];

                int yFirst = max(yLast, sliceFirst - 2); // we need to update two adjacent lines as well since they are analyzed by xBRZ!
                yLast = min(srcHeight, sliceLast + 2);   // (and make sure to not overlap with last slice!)
                for (int i = yFirst; i < yLast; i += sdl_xbrz.task_granularity)
                {
                    tg.run([=] { 
                        const int iLast = min(i + sdl_xbrz.task_granularity, yLast);
                        xbrz::scale(scalingFactor, renderBuf, xbrzBuf, srcWidth, srcHeight, xbrz::ColorFormat::RGB, xbrz::ScalerCfg(), i, iLast);
                    });
                }
            }
            index++;
        }
        tg.wait();
    }
    else // process complete input image
    {
        concurrency::task_group tg;
        for (int i = 0; i < srcHeight; i += sdl_xbrz.task_granularity)
        {
            tg.run([=] { 
                const int iLast = min(i + sdl_xbrz.task_granularity, srcHeight);
                xbrz::scale(scalingFactor, renderBuf, xbrzBuf, srcWidth, srcHeight, xbrz::ColorFormat::RGB, xbrz::ScalerCfg(), i, iLast);
            });
        }
        tg.wait();
    }
#else
    /* non-PPL for non-Windows.
    * combine the code above, cleanly, if possible. */
    if (changedLines)
    {
        int yLast = 0;
        Bitu y = 0, index = 0;
        while (y < sdl.draw.height)
        {
            if (!(index & 1))
                y += changedLines[index];
            else
            {
                const int sliceFirst = int(y);
                const int sliceLast = int(y + changedLines[index]);
                y += changedLines[index];

                int yFirst = max(yLast, sliceFirst - 2); // we need to update two adjacent lines as well since they are analyzed by xBRZ!
                yLast = min(srcHeight, sliceLast + 2);  // (and make sure to not overlap with last slice!)
                xbrz::scale((size_t)scalingFactor, renderBuf, xbrzBuf, srcWidth, srcHeight, xbrz::ColorFormat::RGB, xbrz::ScalerCfg(), yFirst, yLast);
            }
            index++;
        }
    }
    else // process complete input image
    {
        xbrz::scale((size_t)scalingFactor, renderBuf, xbrzBuf, srcWidth, srcHeight, xbrz::ColorFormat::RGB, xbrz::ScalerCfg(), 0, srcHeight);
    }
#endif /*XBRZ_PPL*/
}

#endif /*C_XBRZ*/

#if C_XBRZ || C_SURFACE_POSTRENDER_ASPECT

void xBRZ_PostScale(const uint32_t* src, const int srcWidth, const int srcHeight, const int srcPitch, 
                    uint32_t* tgt, const int tgtWidth, const int tgtHeight, const int tgtPitch, 
                    const bool bilinear, const int task_granularity)
{
    (void)task_granularity;

# if defined(XBRZ_PPL)
    if (bilinear) {
        concurrency::task_group tg;
        for (int i = 0; i < tgtHeight; i += task_granularity)
            tg.run([=] {
                const int iLast = min(i + task_granularity, tgtHeight);
                xbrz::bilinearScale(&src[0], srcWidth, srcHeight, srcPitch, &tgt[0], tgtWidth, tgtHeight, tgtPitch, i, iLast, [](uint32_t pix) { return pix; });  
            });
        tg.wait();
    }
    else
    {
        concurrency::task_group tg;
        for (int i = 0; i < tgtHeight; i += task_granularity)
            tg.run([=] {
                const int iLast = min(i + task_granularity, tgtHeight);
                // perf: going over target is by factor 4 faster than going over source for similar image sizes
                xbrz::nearestNeighborScale(&src[0], srcWidth, srcHeight, srcPitch, &tgt[0], tgtWidth, tgtHeight, tgtPitch, i, iLast, [](uint32_t pix) { return pix; });
            });
        tg.wait();
    }
#else
    if (bilinear)
        xbrz::bilinearScale(&src[0], srcWidth, srcHeight, srcPitch, &tgt[0], tgtWidth, tgtHeight, tgtPitch, 0, tgtHeight, [](uint32_t pix) { return pix; });
    else
        xbrz::nearestNeighborScale(&src[0], srcWidth, srcHeight, srcPitch, &tgt[0], tgtWidth, tgtHeight, tgtPitch, 0, tgtHeight, [](uint32_t pix) { return pix; });
#endif
}

#endif /*C_XBRZ || C_SURFACE_POSTRENDER_ASPECT*/
