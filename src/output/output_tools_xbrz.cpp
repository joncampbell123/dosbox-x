#include <stdlib.h>

#include "dosbox.h"
#include "sdlmain.h"

using namespace std;

#if C_XBRZ

// returns true if scaling possible/enabled, false otherwise
bool xBRZ_SetScaleParameters(int srcWidth, int srcHeight, int dstWidth, int dstHeight)
{
    sdl.xBRZ.scale_factor = (render.xBRZ.fixed_scale_factor == 0) ?
        static_cast<int>(std::sqrt((double)dstWidth * dstHeight / (srcWidth * srcHeight)) + 0.5) :
        render.xBRZ.fixed_scale_factor;

    // enable minimal scaling if upscale is still possible but requires post-downscale
    // having aspect ratio correction on always implies enabled scaler because it gives better quality than DOSBox own method
    if (sdl.xBRZ.scale_factor == 1 && (render.aspect || dstWidth > srcWidth || dstHeight > srcHeight))
        sdl.xBRZ.scale_factor = 2;

    if (sdl.xBRZ.scale_factor >= 2)
    {
        // ok to scale, now clamp scale factor if corresponding max option is set
        sdl.xBRZ.scale_factor = min(sdl.xBRZ.scale_factor, render.xBRZ.max_scale_factor);
        render.xBRZ.scale_on = true;
    }
    else
    {
        // scaling impossible
        render.xBRZ.scale_on = false;
    }

    return render.xBRZ.scale_on;
}

void xBRZ_Render(const uint32_t* renderBuf, uint32_t* xbrzBuf, const Bit16u *changedLines, const int srcWidth, const int srcHeight, int scalingFactor)
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
                const int sliceFirst = y;
                const int sliceLast = y + changedLines[index];
                y += changedLines[index];

                int yFirst = max(yLast, sliceFirst - 2); // we need to update two adjacent lines as well since they are analyzed by xBRZ!
                yLast = min(srcHeight, sliceLast + 2);   // (and make sure to not overlap with last slice!)
                for (int i = yFirst; i < yLast; i += render.xBRZ.task_granularity)
                {
                    tg.run([=] { 
                        const int iLast = min(i + render.xBRZ.task_granularity, yLast);
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
        for (int i = 0; i < srcHeight; i += render.xBRZ.task_granularity)
        {
            tg.run([=] { 
                const int iLast = min(i + render.xBRZ.task_granularity, srcHeight);
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
                const int sliceFirst = y;
                const int sliceLast = y + changedLines[index];
                y += changedLines[index];

                int yFirst = max(yLast, sliceFirst - 2); // we need to update two adjacent lines as well since they are analyzed by xBRZ!
                yLast = min(srcHeight, sliceLast + 2);  // (and make sure to not overlap with last slice!)
                xbrz::scale(scalingFactor, renderBuf, xbrzBuf, srcWidth, srcHeight, xbrz::ColorFormat::RGB, xbrz::ScalerCfg(), yFirst, yLast);
            }
            index++;
        }
    }
    else // process complete input image
    {
        xbrz::scale(scalingFactor, renderBuf, xbrzBuf, srcWidth, srcHeight, xbrz::ColorFormat::RGB, xbrz::ScalerCfg(), 0, srcHeight);
    }
#endif /*XBRZ_PPL*/
}

#endif /*C_XBRZ*/

#if C_XBRZ || C_SURFACE_POSTRENDER_ASPECT

void xBRZ_PostScale(const uint32_t* src, const int srcWidth, const int srcHeight, const int srcPitch, 
                    uint32_t* tgt, const int tgtWidth, const int tgtHeight, const int tgtPitch, 
                    const bool bilinear, const int task_granularity)
{
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
