#include <metal_stdlib>
using namespace metal;

struct VSOut {
    float4 position [[position]];
    float2 uv;
};

vertex VSOut vs_main(uint vid [[vertex_id]])
{
    float2 pos[6] = {
        {-1,-1},{-1,1},{1,-1},
        {1,-1},{-1,1},{1,1}
    };

    float2 uv[6] = {
        {0,1},{0,0},{1,1},
        {1,1},{0,0},{1,0}
    };

    VSOut out;
    out.position = float4(pos[vid],0,1);
    out.uv = uv[vid];
    return out;
}

fragment float4 ps_main(
    VSOut in [[stage_in]],
    texture2d<float> tex [[texture(0)]],
    sampler smp [[sampler(0)]])
{
    return tex.sample(smp, in.uv);
}