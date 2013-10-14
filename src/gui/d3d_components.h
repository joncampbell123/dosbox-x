#include <TCHAR.h>

/*
 Not documented: D3DXFillTextureTX, D3DXFillVolumeTextureTX, D3DXCreateTextureShader
 Code borrowed from Wine and ReactOS
*/

#if 0 // disabled as this code currently does not support pixel shaders (see ID3DXBaseEffectImpl_GetPassDesc)
#include <d3dx9shader.h>
#include <Unknwn.h>

HRESULT map_view_of_file(LPCWSTR filename, LPVOID *buffer, DWORD *length);

//#define ARRAY_SIZE(array) (sizeof(array)/sizeof(*array))
#define INT_FLOAT_MULTI 255.0f
#define INT_FLOAT_MULTI_INVERSE (1/INT_FLOAT_MULTI)

static BOOL get_bool(D3DXPARAMETER_TYPE type, LPCVOID data)
{
    switch (type)
    {
        case D3DXPT_FLOAT:
        case D3DXPT_INT:
        case D3DXPT_BOOL:
            return *(DWORD *)data != 0;

        case D3DXPT_VOID:
            return *(BOOL *)data;

        default:
            //FIXME("Unhandled type %s.\n", debug_d3dxparameter_type(type));
            return FALSE;
    }
}

static INT get_int(D3DXPARAMETER_TYPE type, LPCVOID data)
{
    switch (type)
    {
        case D3DXPT_FLOAT:
            return *(FLOAT *)data;

        case D3DXPT_INT:
        case D3DXPT_VOID:
            return *(INT *)data;

        case D3DXPT_BOOL:
            return get_bool(type, data);

        default:
            //FIXME("Unhandled type %s.\n", debug_d3dxparameter_type(type));
            return 0;
    }
}

static FLOAT get_float(D3DXPARAMETER_TYPE type, LPCVOID data)
{
    switch (type)
    {
        case D3DXPT_FLOAT:
        case D3DXPT_VOID:
            return *(FLOAT *)data;

        case D3DXPT_INT:
            return *(INT *)data;

        case D3DXPT_BOOL:
            return get_bool(type, data);

        default:
            //FIXME("Unhandled type %s.\n", debug_d3dxparameter_type(type));
            return 0.0f;
    }
}

void set_number(LPVOID outdata, D3DXPARAMETER_TYPE outtype, LPCVOID indata, D3DXPARAMETER_TYPE intype)
{
    //TRACE("Changing from type %s to type %s\n", debug_d3dxparameter_type(intype), debug_d3dxparameter_type(outtype));

    if (outtype == intype)
    {
        *(DWORD *)outdata = *(DWORD *)indata;
        return;
    }

    switch (outtype)
    {
        case D3DXPT_FLOAT:
            *(FLOAT *)outdata = get_float(intype, indata);
            break;

        case D3DXPT_BOOL:
            *(BOOL *)outdata = get_bool(intype, indata);
            break;

        case D3DXPT_INT:
            *(INT *)outdata = get_int(intype, indata);
            break;

        default:
            //FIXME("Unhandled type %s.\n", debug_d3dxparameter_type(outtype));
            *(DWORD *)outdata = 0;
            break;
    }
}

HRESULT load_resource_into_memory(HMODULE module, HRSRC resinfo, LPVOID *buffer, DWORD *length)
{
    HGLOBAL resource;

    *length = SizeofResource(module, resinfo);
    if(*length == 0) return HRESULT_FROM_WIN32(GetLastError());

    resource = LoadResource(module, resinfo);
    if( !resource ) return HRESULT_FROM_WIN32(GetLastError());

    *buffer = LockResource(resource);
    if(*buffer == NULL) return HRESULT_FROM_WIN32(GetLastError());

    return S_OK;
}

/*
HRESULT write_buffer_to_file(const WCHAR *dst_filename, ID3DXBuffer *buffer)
{
    HRESULT hr = S_OK;
    void *buffer_pointer;
    DWORD buffer_size;
    HANDLE file = CreateFileW(dst_filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return HRESULT_FROM_WIN32(GetLastError());

    buffer_pointer = ID3DXBuffer_GetBufferPointer(buffer);
    buffer_size = ID3DXBuffer_GetBufferSize(buffer);

    if (!WriteFile(file, buffer_pointer, buffer_size, NULL, NULL))
        hr = HRESULT_FROM_WIN32(GetLastError());

    CloseHandle(file);
    return hr;
}
*/
static ULONG WINAPI IUnknown_Release(LPUNKNOWN iface)
{
    static LONG cLocks;
    InterlockedDecrement(&cLocks);
    return 1; /* non-heap-based object */
}

static ULONG WINAPI IUnknown_AddRef(LPUNKNOWN iface)
{
    static LONG cLocks;
    InterlockedDecrement(&cLocks);
    return 2; /* non-heap-based object */
}


struct vec4
{
    float x, y, z, w;
};

struct volume
{
    UINT width;
    UINT height;
    UINT depth;
};

/* for internal use */
enum format_type {
    FORMAT_ARGB,   /* unsigned */
    FORMAT_UNKNOWN
};

struct pixel_format_desc {
    D3DFORMAT format;
    BYTE bits[4];
    BYTE shift[4];
    UINT bytes_per_pixel;
    UINT block_width;
    UINT block_height;
    UINT block_byte_count;
    enum format_type type;
    void (*from_rgba)(const struct vec4 *src, struct vec4 *dst);
    void (*to_rgba)(const struct vec4 *src, struct vec4 *dst);
};



enum STATE_CLASS
{
    SC_LIGHTENABLE,
    SC_FVF,
    SC_LIGHT,
    SC_MATERIAL,
    SC_NPATCHMODE,
    SC_PIXELSHADER,
    SC_RENDERSTATE,
    SC_SETSAMPLER,
    SC_SAMPLERSTATE,
    SC_TEXTURE,
    SC_TEXTURESTAGE,
    SC_TRANSFORM,
    SC_VERTEXSHADER,
    SC_SHADERCONST,
    SC_UNKNOWN,
};

enum MATERIAL_TYPE
{
    MT_DIFFUSE,
    MT_AMBIENT,
    MT_SPECULAR,
    MT_EMISSIVE,
    MT_POWER,
};

enum LIGHT_TYPE
{
    LT_TYPE,
    LT_DIFFUSE,
    LT_SPECULAR,
    LT_AMBIENT,
    LT_POSITION,
    LT_DIRECTION,
    LT_RANGE,
    LT_FALLOFF,
    LT_ATTENUATION0,
    LT_ATTENUATION1,
    LT_ATTENUATION2,
    LT_THETA,
    LT_PHI,
};

enum SHADER_CONSTANT_TYPE
{
    SCT_VSFLOAT,
    SCT_VSBOOL,
    SCT_VSINT,
    SCT_PSFLOAT,
    SCT_PSBOOL,
    SCT_PSINT,
};

enum STATE_TYPE
{
    ST_CONSTANT,
    ST_PARAMETER,
    ST_FXLC,
};

struct d3dx_parameter
{
    char *name;
    char *semantic;
    void *data;
    D3DXPARAMETER_CLASS class2;
    D3DXPARAMETER_TYPE  type;
    UINT rows;
    UINT columns;
    UINT element_count;
    UINT annotation_count;
    UINT member_count;
    DWORD flags;
    UINT bytes;

    D3DXHANDLE *annotation_handles;
    D3DXHANDLE *member_handles;
};

struct d3dx_state
{
    UINT operation;
    UINT index;
    enum STATE_TYPE type;
    D3DXHANDLE parameter;
};

struct d3dx_sampler
{
    UINT state_count;
    struct d3dx_state *states;
};

struct d3dx_pass
{
    char *name;
    UINT state_count;
    UINT annotation_count;

    struct d3dx_state *states;
    D3DXHANDLE *annotation_handles;
};

struct d3dx_technique
{
    char *name;
    UINT pass_count;
    UINT annotation_count;

    D3DXHANDLE *annotation_handles;
    D3DXHANDLE *pass_handles;
};

struct ID3DXBaseEffectImpl
{
    ID3DXBaseEffect* ID3DXBaseEffect_iface;
    LONG ref;

    struct ID3DXEffectImpl *effect;

    UINT parameter_count;
    UINT technique_count;

    D3DXHANDLE *parameter_handles;
    D3DXHANDLE *technique_handles;
};

struct ID3DXEffectImpl
{
    ID3DXEffect* ID3DXEffect_iface;
    LONG ref;

    LPD3DXEFFECTSTATEMANAGER manager;
    LPDIRECT3DDEVICE9 device;
    LPD3DXEFFECTPOOL pool;
    D3DXHANDLE active_technique;
    D3DXHANDLE active_pass;

    ID3DXBaseEffect *base_effect;
};

struct ID3DXEffectCompilerImpl
{
    ID3DXEffectCompiler* ID3DXEffectCompiler_iface;
    LONG ref;

    ID3DXBaseEffect *base_effect;
};

static struct d3dx_parameter *get_parameter_by_name(struct ID3DXBaseEffectImpl *base,
        struct d3dx_parameter *parameter, LPCSTR name);
static struct d3dx_parameter *get_annotation_by_name(UINT handlecount, D3DXHANDLE *handles, LPCSTR name);
static HRESULT d3dx9_parse_state(struct d3dx_state *state, const char *data, const char **ptr, D3DXHANDLE *objects);
static void free_parameter_state(D3DXHANDLE handle, BOOL element, BOOL child, enum STATE_TYPE st);

static const struct
{
    enum STATE_CLASS class2;
    UINT op;
    LPCSTR name;
}
state_table[] =
{
    /* Render sates */
    {SC_RENDERSTATE, D3DRS_ZENABLE, "D3DRS_ZENABLE"}, /* 0x0 */
    {SC_RENDERSTATE, D3DRS_FILLMODE, "D3DRS_FILLMODE"},
    {SC_RENDERSTATE, D3DRS_SHADEMODE, "D3DRS_SHADEMODE"},
    {SC_RENDERSTATE, D3DRS_ZWRITEENABLE, "D3DRS_ZWRITEENABLE"},
    {SC_RENDERSTATE, D3DRS_ALPHATESTENABLE, "D3DRS_ALPHATESTENABLE"},
    {SC_RENDERSTATE, D3DRS_LASTPIXEL, "D3DRS_LASTPIXEL"},
    {SC_RENDERSTATE, D3DRS_SRCBLEND, "D3DRS_SRCBLEND"},
    {SC_RENDERSTATE, D3DRS_DESTBLEND, "D3DRS_DESTBLEND"},
    {SC_RENDERSTATE, D3DRS_CULLMODE, "D3DRS_CULLMODE"},
    {SC_RENDERSTATE, D3DRS_ZFUNC, "D3DRS_ZFUNC"},
    {SC_RENDERSTATE, D3DRS_ALPHAREF, "D3DRS_ALPHAREF"},
    {SC_RENDERSTATE, D3DRS_ALPHAFUNC, "D3DRS_ALPHAFUNC"},
    {SC_RENDERSTATE, D3DRS_DITHERENABLE, "D3DRS_DITHERENABLE"},
    {SC_RENDERSTATE, D3DRS_ALPHABLENDENABLE, "D3DRS_ALPHABLENDENABLE"},
    {SC_RENDERSTATE, D3DRS_FOGENABLE, "D3DRS_FOGENABLE"},
    {SC_RENDERSTATE, D3DRS_SPECULARENABLE, "D3DRS_SPECULARENABLE"},
    {SC_RENDERSTATE, D3DRS_FOGCOLOR, "D3DRS_FOGCOLOR"}, /* 0x10 */
    {SC_RENDERSTATE, D3DRS_FOGTABLEMODE, "D3DRS_FOGTABLEMODE"},
    {SC_RENDERSTATE, D3DRS_FOGSTART, "D3DRS_FOGSTART"},
    {SC_RENDERSTATE, D3DRS_FOGEND, "D3DRS_FOGEND"},
    {SC_RENDERSTATE, D3DRS_FOGDENSITY, "D3DRS_FOGDENSITY"},
    {SC_RENDERSTATE, D3DRS_RANGEFOGENABLE, "D3DRS_RANGEFOGENABLE"},
    {SC_RENDERSTATE, D3DRS_STENCILENABLE, "D3DRS_STENCILENABLE"},
    {SC_RENDERSTATE, D3DRS_STENCILFAIL, "D3DRS_STENCILFAIL"},
    {SC_RENDERSTATE, D3DRS_STENCILZFAIL, "D3DRS_STENCILZFAIL"},
    {SC_RENDERSTATE, D3DRS_STENCILPASS, "D3DRS_STENCILPASS"},
    {SC_RENDERSTATE, D3DRS_STENCILFUNC, "D3DRS_STENCILFUNC"},
    {SC_RENDERSTATE, D3DRS_STENCILREF, "D3DRS_STENCILREF"},
    {SC_RENDERSTATE, D3DRS_STENCILMASK, "D3DRS_STENCILMASK"},
    {SC_RENDERSTATE, D3DRS_STENCILWRITEMASK, "D3DRS_STENCILWRITEMASK"},
    {SC_RENDERSTATE, D3DRS_TEXTUREFACTOR, "D3DRS_TEXTUREFACTOR"},
    {SC_RENDERSTATE, D3DRS_WRAP0, "D3DRS_WRAP0"},
    {SC_RENDERSTATE, D3DRS_WRAP1, "D3DRS_WRAP1"}, /* 0x20 */
    {SC_RENDERSTATE, D3DRS_WRAP2, "D3DRS_WRAP2"},
    {SC_RENDERSTATE, D3DRS_WRAP3, "D3DRS_WRAP3"},
    {SC_RENDERSTATE, D3DRS_WRAP4, "D3DRS_WRAP4"},
    {SC_RENDERSTATE, D3DRS_WRAP5, "D3DRS_WRAP5"},
    {SC_RENDERSTATE, D3DRS_WRAP6, "D3DRS_WRAP6"},
    {SC_RENDERSTATE, D3DRS_WRAP7, "D3DRS_WRAP7"},
    {SC_RENDERSTATE, D3DRS_WRAP8, "D3DRS_WRAP8"},
    {SC_RENDERSTATE, D3DRS_WRAP9, "D3DRS_WRAP9"},
    {SC_RENDERSTATE, D3DRS_WRAP10, "D3DRS_WRAP10"},
    {SC_RENDERSTATE, D3DRS_WRAP11, "D3DRS_WRAP11"},
    {SC_RENDERSTATE, D3DRS_WRAP12, "D3DRS_WRAP12"},
    {SC_RENDERSTATE, D3DRS_WRAP13, "D3DRS_WRAP13"},
    {SC_RENDERSTATE, D3DRS_WRAP14, "D3DRS_WRAP14"},
    {SC_RENDERSTATE, D3DRS_WRAP15, "D3DRS_WRAP15"},
    {SC_RENDERSTATE, D3DRS_CLIPPING, "D3DRS_CLIPPING"},
    {SC_RENDERSTATE, D3DRS_LIGHTING, "D3DRS_LIGHTING"}, /* 0x30 */
    {SC_RENDERSTATE, D3DRS_AMBIENT, "D3DRS_AMBIENT"},
    {SC_RENDERSTATE, D3DRS_FOGVERTEXMODE, "D3DRS_FOGVERTEXMODE"},
    {SC_RENDERSTATE, D3DRS_COLORVERTEX, "D3DRS_COLORVERTEX"},
    {SC_RENDERSTATE, D3DRS_LOCALVIEWER, "D3DRS_LOCALVIEWER"},
    {SC_RENDERSTATE, D3DRS_NORMALIZENORMALS, "D3DRS_NORMALIZENORMALS"},
    {SC_RENDERSTATE, D3DRS_DIFFUSEMATERIALSOURCE, "D3DRS_DIFFUSEMATERIALSOURCE"},
    {SC_RENDERSTATE, D3DRS_SPECULARMATERIALSOURCE, "D3DRS_SPECULARMATERIALSOURCE"},
    {SC_RENDERSTATE, D3DRS_AMBIENTMATERIALSOURCE, "D3DRS_AMBIENTMATERIALSOURCE"},
    {SC_RENDERSTATE, D3DRS_EMISSIVEMATERIALSOURCE, "D3DRS_EMISSIVEMATERIALSOURCE"},
    {SC_RENDERSTATE, D3DRS_VERTEXBLEND, "D3DRS_VERTEXBLEND"},
    {SC_RENDERSTATE, D3DRS_CLIPPLANEENABLE, "D3DRS_CLIPPLANEENABLE"},
    {SC_RENDERSTATE, D3DRS_POINTSIZE, "D3DRS_POINTSIZE"},
    {SC_RENDERSTATE, D3DRS_POINTSIZE_MIN, "D3DRS_POINTSIZE_MIN"},
    {SC_RENDERSTATE, D3DRS_POINTSIZE_MAX, "D3DRS_POINTSIZE_MAX"},
    {SC_RENDERSTATE, D3DRS_POINTSPRITEENABLE, "D3DRS_POINTSPRITEENABLE"},
    {SC_RENDERSTATE, D3DRS_POINTSCALEENABLE, "D3DRS_POINTSCALEENABLE"}, /* 0x40 */
    {SC_RENDERSTATE, D3DRS_POINTSCALE_A, "D3DRS_POINTSCALE_A"},
    {SC_RENDERSTATE, D3DRS_POINTSCALE_B, "D3DRS_POINTSCALE_B"},
    {SC_RENDERSTATE, D3DRS_POINTSCALE_C, "D3DRS_POINTSCALE_C"},
    {SC_RENDERSTATE, D3DRS_MULTISAMPLEANTIALIAS, "D3DRS_MULTISAMPLEANTIALIAS"},
    {SC_RENDERSTATE, D3DRS_MULTISAMPLEMASK, "D3DRS_MULTISAMPLEMASK"},
    {SC_RENDERSTATE, D3DRS_PATCHEDGESTYLE, "D3DRS_PATCHEDGESTYLE"},
    {SC_RENDERSTATE, D3DRS_DEBUGMONITORTOKEN, "D3DRS_DEBUGMONITORTOKEN"},
    {SC_RENDERSTATE, D3DRS_INDEXEDVERTEXBLENDENABLE, "D3DRS_INDEXEDVERTEXBLENDENABLE"},
    {SC_RENDERSTATE, D3DRS_COLORWRITEENABLE, "D3DRS_COLORWRITEENABLE"},
    {SC_RENDERSTATE, D3DRS_TWEENFACTOR, "D3DRS_TWEENFACTOR"},
    {SC_RENDERSTATE, D3DRS_BLENDOP, "D3DRS_BLENDOP"},
    {SC_RENDERSTATE, D3DRS_POSITIONDEGREE, "D3DRS_POSITIONDEGREE"},
    {SC_RENDERSTATE, D3DRS_NORMALDEGREE, "D3DRS_NORMALDEGREE"},
    {SC_RENDERSTATE, D3DRS_SCISSORTESTENABLE, "D3DRS_SCISSORTESTENABLE"},
    {SC_RENDERSTATE, D3DRS_SLOPESCALEDEPTHBIAS, "D3DRS_SLOPESCALEDEPTHBIAS"},
    {SC_RENDERSTATE, D3DRS_ANTIALIASEDLINEENABLE, "D3DRS_ANTIALIASEDLINEENABLE"}, /* 0x50 */
    {SC_RENDERSTATE, D3DRS_MINTESSELLATIONLEVEL, "D3DRS_MINTESSELLATIONLEVEL"},
    {SC_RENDERSTATE, D3DRS_MAXTESSELLATIONLEVEL, "D3DRS_MAXTESSELLATIONLEVEL"},
    {SC_RENDERSTATE, D3DRS_ADAPTIVETESS_X, "D3DRS_ADAPTIVETESS_X"},
    {SC_RENDERSTATE, D3DRS_ADAPTIVETESS_Y, "D3DRS_ADAPTIVETESS_Y"},
    {SC_RENDERSTATE, D3DRS_ADAPTIVETESS_Z, "D3DRS_ADAPTIVETESS_Z"},
    {SC_RENDERSTATE, D3DRS_ADAPTIVETESS_W, "D3DRS_ADAPTIVETESS_W"},
    {SC_RENDERSTATE, D3DRS_ENABLEADAPTIVETESSELLATION, "D3DRS_ENABLEADAPTIVETESSELLATION"},
    {SC_RENDERSTATE, D3DRS_TWOSIDEDSTENCILMODE, "D3DRS_TWOSIDEDSTENCILMODE"},
    {SC_RENDERSTATE, D3DRS_CCW_STENCILFAIL, "D3DRS_CCW_STENCILFAIL"},
    {SC_RENDERSTATE, D3DRS_CCW_STENCILZFAIL, "D3DRS_CCW_STENCILZFAIL"},
    {SC_RENDERSTATE, D3DRS_CCW_STENCILPASS, "D3DRS_CCW_STENCILPASS"},
    {SC_RENDERSTATE, D3DRS_CCW_STENCILFUNC, "D3DRS_CCW_STENCILFUNC"},
    {SC_RENDERSTATE, D3DRS_COLORWRITEENABLE1, "D3DRS_COLORWRITEENABLE1"},
    {SC_RENDERSTATE, D3DRS_COLORWRITEENABLE2, "D3DRS_COLORWRITEENABLE2"},
    {SC_RENDERSTATE, D3DRS_COLORWRITEENABLE3, "D3DRS_COLORWRITEENABLE3"},
    {SC_RENDERSTATE, D3DRS_BLENDFACTOR, "D3DRS_BLENDFACTOR"}, /* 0x60 */
    {SC_RENDERSTATE, D3DRS_SRGBWRITEENABLE, "D3DRS_SRGBWRITEENABLE"},
    {SC_RENDERSTATE, D3DRS_DEPTHBIAS, "D3DRS_DEPTHBIAS"},
    {SC_RENDERSTATE, D3DRS_SEPARATEALPHABLENDENABLE, "D3DRS_SEPARATEALPHABLENDENABLE"},
    {SC_RENDERSTATE, D3DRS_SRCBLENDALPHA, "D3DRS_SRCBLENDALPHA"},
    {SC_RENDERSTATE, D3DRS_DESTBLENDALPHA, "D3DRS_DESTBLENDALPHA"},
    {SC_RENDERSTATE, D3DRS_BLENDOPALPHA, "D3DRS_BLENDOPALPHA"},
    /* Texture stages */
    {SC_TEXTURESTAGE, D3DTSS_COLOROP, "D3DTSS_COLOROP"},
    {SC_TEXTURESTAGE, D3DTSS_COLORARG0, "D3DTSS_COLORARG0"},
    {SC_TEXTURESTAGE, D3DTSS_COLORARG1, "D3DTSS_COLORARG1"},
    {SC_TEXTURESTAGE, D3DTSS_COLORARG2, "D3DTSS_COLORARG2"},
    {SC_TEXTURESTAGE, D3DTSS_ALPHAOP, "D3DTSS_ALPHAOP"},
    {SC_TEXTURESTAGE, D3DTSS_ALPHAARG0, "D3DTSS_ALPHAARG0"},
    {SC_TEXTURESTAGE, D3DTSS_ALPHAARG1, "D3DTSS_ALPHAARG1"},
    {SC_TEXTURESTAGE, D3DTSS_ALPHAARG2, "D3DTSS_ALPHAARG2"},
    {SC_TEXTURESTAGE, D3DTSS_RESULTARG, "D3DTSS_RESULTARG"},
    {SC_TEXTURESTAGE, D3DTSS_BUMPENVMAT00, "D3DTSS_BUMPENVMAT00"}, /* 0x70 */
    {SC_TEXTURESTAGE, D3DTSS_BUMPENVMAT01, "D3DTSS_BUMPENVMAT01"},
    {SC_TEXTURESTAGE, D3DTSS_BUMPENVMAT10, "D3DTSS_BUMPENVMAT10"},
    {SC_TEXTURESTAGE, D3DTSS_BUMPENVMAT11, "D3DTSS_BUMPENVMAT11"},
    {SC_TEXTURESTAGE, D3DTSS_TEXCOORDINDEX, "D3DTSS_TEXCOORDINDEX"},
    {SC_TEXTURESTAGE, D3DTSS_BUMPENVLSCALE, "D3DTSS_BUMPENVLSCALE"},
    {SC_TEXTURESTAGE, D3DTSS_BUMPENVLOFFSET, "D3DTSS_BUMPENVLOFFSET"},
    {SC_TEXTURESTAGE, D3DTSS_TEXTURETRANSFORMFLAGS, "D3DTSS_TEXTURETRANSFORMFLAGS"},
    {SC_TEXTURESTAGE, D3DTSS_CONSTANT, "D3DTSS_CONSTANT"},
    /* NPatchMode */
    {SC_NPATCHMODE, 0, "NPatchMode"},
    /* FVF */
    {SC_FVF, 0, "FVF"},
    /* Transform */
    {SC_TRANSFORM, D3DTS_PROJECTION, "D3DTS_PROJECTION"},
    {SC_TRANSFORM, D3DTS_VIEW, "D3DTS_VIEW"},
    {SC_TRANSFORM, D3DTS_WORLD, "D3DTS_WORLD"},
    {SC_TRANSFORM, D3DTS_TEXTURE0, "D3DTS_TEXTURE0"},
    /* Material */
    {SC_MATERIAL, MT_DIFFUSE, "MaterialDiffuse"},
    {SC_MATERIAL, MT_AMBIENT, "MaterialAmbient"}, /* 0x80 */
    {SC_MATERIAL, MT_SPECULAR, "MaterialSpecular"},
    {SC_MATERIAL, MT_EMISSIVE, "MaterialEmissive"},
    {SC_MATERIAL, MT_POWER, "MaterialPower"},
    /* Light */
    {SC_LIGHT, LT_TYPE, "LightType"},
    {SC_LIGHT, LT_DIFFUSE, "LightDiffuse"},
    {SC_LIGHT, LT_SPECULAR, "LightSpecular"},
    {SC_LIGHT, LT_AMBIENT, "LightAmbient"},
    {SC_LIGHT, LT_POSITION, "LightPosition"},
    {SC_LIGHT, LT_DIRECTION, "LightDirection"},
    {SC_LIGHT, LT_RANGE, "LightRange"},
    {SC_LIGHT, LT_FALLOFF, "LightFallOff"},
    {SC_LIGHT, LT_ATTENUATION0, "LightAttenuation0"},
    {SC_LIGHT, LT_ATTENUATION1, "LightAttenuation1"},
    {SC_LIGHT, LT_ATTENUATION2, "LightAttenuation2"},
    {SC_LIGHT, LT_THETA, "LightTheta"},
    {SC_LIGHT, LT_PHI, "LightPhi"}, /* 0x90 */
    /* Ligthenable */
    {SC_LIGHTENABLE, 0, "LightEnable"},
    /* Vertexshader */
    {SC_VERTEXSHADER, 0, "Vertexshader"},
    /* Pixelshader */
    {SC_PIXELSHADER, 0, "Pixelshader"},
    /* Shader constants */
    {SC_SHADERCONST, SCT_VSFLOAT, "VertexShaderConstantF"},
    {SC_SHADERCONST, SCT_VSBOOL, "VertexShaderConstantB"},
    {SC_SHADERCONST, SCT_VSINT, "VertexShaderConstantI"},
    {SC_SHADERCONST, SCT_VSFLOAT, "VertexShaderConstant"},
    {SC_SHADERCONST, SCT_VSFLOAT, "VertexShaderConstant1"},
    {SC_SHADERCONST, SCT_VSFLOAT, "VertexShaderConstant2"},
    {SC_SHADERCONST, SCT_VSFLOAT, "VertexShaderConstant3"},
    {SC_SHADERCONST, SCT_VSFLOAT, "VertexShaderConstant4"},
    {SC_SHADERCONST, SCT_PSFLOAT, "PixelShaderConstantF"},
    {SC_SHADERCONST, SCT_PSBOOL, "PixelShaderConstantB"},
    {SC_SHADERCONST, SCT_PSINT, "PixelShaderConstantI"},
    {SC_SHADERCONST, SCT_PSFLOAT, "PixelShaderConstant"},
    {SC_SHADERCONST, SCT_PSFLOAT, "PixelShaderConstant1"}, /* 0xa0 */
    {SC_SHADERCONST, SCT_PSFLOAT, "PixelShaderConstant2"},
    {SC_SHADERCONST, SCT_PSFLOAT, "PixelShaderConstant3"},
    {SC_SHADERCONST, SCT_PSFLOAT, "PixelShaderConstant4"},
    /* Texture */
    {SC_TEXTURE, 0, "Texture"},
    /* Sampler states */
    {SC_SAMPLERSTATE, D3DSAMP_ADDRESSU, "AddressU"},
    {SC_SAMPLERSTATE, D3DSAMP_ADDRESSV, "AddressV"},
    {SC_SAMPLERSTATE, D3DSAMP_ADDRESSW, "AddressW"},
    {SC_SAMPLERSTATE, D3DSAMP_BORDERCOLOR, "BorderColor"},
    {SC_SAMPLERSTATE, D3DSAMP_MAGFILTER, "MagFilter"},
    {SC_SAMPLERSTATE, D3DSAMP_MINFILTER, "MinFilter"},
    {SC_SAMPLERSTATE, D3DSAMP_MIPFILTER, "MipFilter"},
    {SC_SAMPLERSTATE, D3DSAMP_MIPMAPLODBIAS, "MipMapLodBias"},
    {SC_SAMPLERSTATE, D3DSAMP_MAXMIPLEVEL, "MaxMipLevel"},
    {SC_SAMPLERSTATE, D3DSAMP_MAXANISOTROPY, "MaxAnisotropy"},
    {SC_SAMPLERSTATE, D3DSAMP_SRGBTEXTURE, "SRGBTexture"},
    {SC_SAMPLERSTATE, D3DSAMP_ELEMENTINDEX, "ElementIndex"}, /* 0xb0 */
    {SC_SAMPLERSTATE, D3DSAMP_DMAPOFFSET, "DMAPOffset"},
    /* Set sampler */
    {SC_SETSAMPLER, 0, "Sampler"},
};

static inline void read_dword(const char **ptr, DWORD *d)
{
    memcpy(d, *ptr, sizeof(*d));
    *ptr += sizeof(*d);
}

static void skip_dword_unknown(const char **ptr, unsigned int count)
{
    unsigned int i;
    DWORD d;

    //FIXME("Skipping %u unknown DWORDs:\n", count);
    for (i = 0; i < count; ++i)
    {
        read_dword(ptr, &d);
        //FIXME("\t0x%08x\n", d);
    }
}

static inline struct d3dx_parameter *get_parameter_struct(D3DXHANDLE handle)
{
    return (struct d3dx_parameter *) handle;
}

static inline struct d3dx_pass *get_pass_struct(D3DXHANDLE handle)
{
    return (struct d3dx_pass *) handle;
}

static inline struct d3dx_technique *get_technique_struct(D3DXHANDLE handle)
{
    return (struct d3dx_technique *) handle;
}

static inline D3DXHANDLE get_parameter_handle(struct d3dx_parameter *parameter)
{
    return (D3DXHANDLE) parameter;
}

static inline D3DXHANDLE get_technique_handle(struct d3dx_technique *technique)
{
    return (D3DXHANDLE) technique;
}

static inline D3DXHANDLE get_pass_handle(struct d3dx_pass *pass)
{
    return (D3DXHANDLE) pass;
}

static struct d3dx_technique *get_technique_by_name(struct ID3DXBaseEffectImpl *base, LPCSTR name)
{
    UINT i;

    if (!name) return NULL;

    for (i = 0; i < base->technique_count; ++i)
    {
        struct d3dx_technique *tech = get_technique_struct(base->technique_handles[i]);

        if (!strcmp(tech->name, name)) return tech;
    }

    return NULL;
}

static struct d3dx_technique *is_valid_technique(struct ID3DXBaseEffectImpl *base, D3DXHANDLE technique)
{
    struct d3dx_technique *tech = NULL;
    unsigned int i;

    for (i = 0; i < base->technique_count; ++i)
    {
        if (base->technique_handles[i] == technique)
        {
            tech = get_technique_struct(technique);
            break;
        }
    }

    if (!tech) tech = get_technique_by_name(base, technique);

    return tech;
}

static struct d3dx_pass *is_valid_pass(struct ID3DXBaseEffectImpl *base, D3DXHANDLE pass)
{
    unsigned int i, k;

    for (i = 0; i < base->technique_count; ++i)
    {
        struct d3dx_technique *technique = get_technique_struct(base->technique_handles[i]);

        for (k = 0; k < technique->pass_count; ++k)
        {
            if (technique->pass_handles[k] == pass)
            {
                return get_pass_struct(pass);
            }
        }
    }

    return NULL;
}

static struct d3dx_parameter *is_valid_sub_parameter(struct d3dx_parameter *param, D3DXHANDLE parameter)
{
    unsigned int i, count;
    struct d3dx_parameter *p;

    for (i = 0; i < param->annotation_count; ++i)
    {
        if (param->annotation_handles[i] == parameter)
        {
            return get_parameter_struct(parameter);
        }

        p = is_valid_sub_parameter(get_parameter_struct(param->annotation_handles[i]), parameter);
        if (p) return p;
    }

    if (param->element_count) count = param->element_count;
    else count = param->member_count;

    for (i = 0; i < count; ++i)
    {
        if (param->member_handles[i] == parameter)
        {
            return get_parameter_struct(parameter);
        }

        p = is_valid_sub_parameter(get_parameter_struct(param->member_handles[i]), parameter);
        if (p) return p;
    }

    return NULL;
}

static struct d3dx_parameter *is_valid_parameter(struct ID3DXBaseEffectImpl *base, D3DXHANDLE parameter)
{
    unsigned int i, k, m;
    struct d3dx_parameter *p;

    for (i = 0; i < base->parameter_count; ++i)
    {
        if (base->parameter_handles[i] == parameter)
        {
            return get_parameter_struct(parameter);
        }

        p = is_valid_sub_parameter(get_parameter_struct(base->parameter_handles[i]), parameter);
        if (p) return p;
    }

    for (i = 0; i < base->technique_count; ++i)
    {
        struct d3dx_technique *technique = get_technique_struct(base->technique_handles[i]);

        for (k = 0; k < technique->pass_count; ++k)
        {
            struct d3dx_pass *pass = get_pass_struct(technique->pass_handles[k]);

            for (m = 0; m < pass->annotation_count; ++m)
            {
                if (pass->annotation_handles[i] == parameter)
                {
                    return get_parameter_struct(parameter);
                }

                p = is_valid_sub_parameter(get_parameter_struct(pass->annotation_handles[m]), parameter);
                if (p) return p;
            }
        }

        for (k = 0; k < technique->annotation_count; ++k)
        {
            if (technique->annotation_handles[k] == parameter)
            {
                return get_parameter_struct(parameter);
            }

            p = is_valid_sub_parameter(get_parameter_struct(technique->annotation_handles[k]), parameter);
            if (p) return p;
        }
    }

    return NULL;
}

static inline struct d3dx_parameter *get_valid_parameter(struct ID3DXBaseEffectImpl *base, D3DXHANDLE parameter)
{
    struct d3dx_parameter *param = is_valid_parameter(base, parameter);

    if (!param) param = get_parameter_by_name(base, NULL, parameter);

    return param;
}

static void free_state(struct d3dx_state *state)
{
    free_parameter_state(state->parameter, FALSE, FALSE, state->type);
}

static void free_sampler(struct d3dx_sampler *sampler)
{
    UINT i;

    for (i = 0; i < sampler->state_count; ++i)
    {
        free_state(&sampler->states[i]);
    }
    HeapFree(GetProcessHeap(), 0, sampler->states);
}

static void free_parameter(D3DXHANDLE handle, BOOL element, BOOL child)
{
    free_parameter_state(handle, element, child, ST_CONSTANT);
}

static void free_parameter_state(D3DXHANDLE handle, BOOL element, BOOL child, enum STATE_TYPE st)
{
    unsigned int i;
    struct d3dx_parameter *param = get_parameter_struct(handle);

    //TRACE("Free parameter %p, name %s, type %s, child %s, state_type %x\n", param, param->name,
    //        debug_d3dxparameter_type(param->type), child ? "yes" : "no", st);

    if (!param)
    {
        return;
    }

    if (param->annotation_handles)
    {
        for (i = 0; i < param->annotation_count; ++i)
        {
            free_parameter(param->annotation_handles[i], FALSE, FALSE);
        }
        HeapFree(GetProcessHeap(), 0, param->annotation_handles);
    }

    if (param->member_handles)
    {
        unsigned int count;

        if (param->element_count) count = param->element_count;
        else count = param->member_count;

        for (i = 0; i < count; ++i)
        {
            free_parameter(param->member_handles[i], param->element_count != 0, TRUE);
        }
        HeapFree(GetProcessHeap(), 0, param->member_handles);
    }

    if (param->class2 == D3DXPC_OBJECT && !param->element_count)
    {
        switch (param->type)
        {
            case D3DXPT_STRING:
                HeapFree(GetProcessHeap(), 0, *(LPSTR *)param->data);
                if (!child) HeapFree(GetProcessHeap(), 0, param->data);
                break;

            case D3DXPT_TEXTURE:
            case D3DXPT_TEXTURE1D:
            case D3DXPT_TEXTURE2D:
            case D3DXPT_TEXTURE3D:
            case D3DXPT_TEXTURECUBE:
            case D3DXPT_PIXELSHADER:
            case D3DXPT_VERTEXSHADER:
                if (st == ST_CONSTANT)
                {
                    if (*(IUnknown **)param->data) IUnknown_Release(*(IUnknown **)param->data);
                }
                else
                {
                    HeapFree(GetProcessHeap(), 0, *(LPSTR *)param->data);
                }
                if (!child) HeapFree(GetProcessHeap(), 0, param->data);
                break;

            case D3DXPT_SAMPLER:
            case D3DXPT_SAMPLER1D:
            case D3DXPT_SAMPLER2D:
            case D3DXPT_SAMPLER3D:
            case D3DXPT_SAMPLERCUBE:
                if (st == ST_CONSTANT)
                {
                    free_sampler((struct d3dx_sampler *)param->data);
                }
                else
                {
                    HeapFree(GetProcessHeap(), 0, *(LPSTR *)param->data);
                }
                /* samplers have always own data, so free that */
                HeapFree(GetProcessHeap(), 0, param->data);
                break;

            default:
                //FIXME("Unhandled type %s\n", debug_d3dxparameter_type(param->type));
                break;
        }
    }
    else
    {
        if (!child)
        {
            if (st != ST_CONSTANT)
            {
                HeapFree(GetProcessHeap(), 0, *(LPSTR *)param->data);
            }
            HeapFree(GetProcessHeap(), 0, param->data);
        }
    }

    /* only the parent has to release name and semantic */
    if (!element)
    {
        HeapFree(GetProcessHeap(), 0, param->name);
        HeapFree(GetProcessHeap(), 0, param->semantic);
    }

    HeapFree(GetProcessHeap(), 0, param);
}

static void free_pass(D3DXHANDLE handle)
{
    unsigned int i;
    struct d3dx_pass *pass = get_pass_struct(handle);

    //TRACE("Free pass %p\n", pass);

    if (!pass)
    {
        return;
    }

    if (pass->annotation_handles)
    {
        for (i = 0; i < pass->annotation_count; ++i)
        {
            free_parameter(pass->annotation_handles[i], FALSE, FALSE);
        }
        HeapFree(GetProcessHeap(), 0, pass->annotation_handles);
    }

    if (pass->states)
    {
        for (i = 0; i < pass->state_count; ++i)
        {
            free_state(&pass->states[i]);
        }
        HeapFree(GetProcessHeap(), 0, pass->states);
    }

    HeapFree(GetProcessHeap(), 0, pass->name);
    HeapFree(GetProcessHeap(), 0, pass);
}

static void free_technique(D3DXHANDLE handle)
{
    unsigned int i;
    struct d3dx_technique *technique = get_technique_struct(handle);

    //TRACE("Free technique %p\n", technique);

    if (!technique)
    {
        return;
    }

    if (technique->annotation_handles)
    {
        for (i = 0; i < technique->annotation_count; ++i)
        {
            free_parameter(technique->annotation_handles[i], FALSE, FALSE);
        }
        HeapFree(GetProcessHeap(), 0, technique->annotation_handles);
    }

    if (technique->pass_handles)
    {
        for (i = 0; i < technique->pass_count; ++i)
        {
            free_pass(technique->pass_handles[i]);
        }
        HeapFree(GetProcessHeap(), 0, technique->pass_handles);
    }

    HeapFree(GetProcessHeap(), 0, technique->name);
    HeapFree(GetProcessHeap(), 0, technique);
}

static void free_base_effect(struct ID3DXBaseEffectImpl *base)
{
    unsigned int i;

    //TRACE("Free base effect %p\n", base);

    if (base->parameter_handles)
    {
        for (i = 0; i < base->parameter_count; ++i)
        {
            free_parameter(base->parameter_handles[i], FALSE, FALSE);
        }
        HeapFree(GetProcessHeap(), 0, base->parameter_handles);
    }

    if (base->technique_handles)
    {
        for (i = 0; i < base->technique_count; ++i)
        {
            free_technique(base->technique_handles[i]);
        }
        HeapFree(GetProcessHeap(), 0, base->technique_handles);
    }
}

static void free_effect(struct ID3DXEffectImpl *effect)
{
    //TRACE("Free effect %p\n", effect);
    if (effect->base_effect)
    {
        //effect->base_effect->lpVtbl->Release(effect->base_effect);
        effect->base_effect->Release();
    }

    if (effect->pool)
    {
        //effect->pool->lpVtbl->Release(effect->pool);
	    effect->pool->Release();
    }

    if (effect->manager)
    {
        IUnknown_Release(effect->manager);
    }

    IDirect3DDevice9_Release(effect->device);
}

static void free_effect_compiler(struct ID3DXEffectCompilerImpl *compiler)
{
    //TRACE("Free effect compiler %p\n", compiler);

    if (compiler->base_effect)
    {
        //compiler->base_effect->lpVtbl->Release(compiler->base_effect);
          compiler->base_effect->Release();
    }
}

static void get_vector(struct d3dx_parameter *param, D3DXVECTOR4 *vector)
{
    UINT i;

    for (i = 0; i < 4; ++i)
    {
        if (i < param->columns)
            set_number((FLOAT *)vector + i, D3DXPT_FLOAT, (DWORD *)param->data + i, param->type);
        else
            ((FLOAT *)vector)[i] = 0.0f;
    }
}

static void set_vector(struct d3dx_parameter *param, CONST D3DXVECTOR4 *vector)
{
    UINT i;

    for (i = 0; i < param->columns; ++i)
    {
        set_number((FLOAT *)param->data + i, param->type, (FLOAT *)vector + i, D3DXPT_FLOAT);
    }
}

static void get_matrix(struct d3dx_parameter *param, D3DXMATRIX *matrix, BOOL transpose)
{
    UINT i, k;

    for (i = 0; i < 4; ++i)
    {
        for (k = 0; k < 4; ++k)
        {
            //FLOAT *tmp = transpose ? (FLOAT *)&matrix->u.m[k][i] : (FLOAT *)&matrix->u.m[i][k];
            FLOAT *tmp = transpose ? (FLOAT *)&matrix->m[k][i] : (FLOAT *)&matrix->m[i][k];

            if ((i < param->rows) && (k < param->columns))
                set_number(tmp, D3DXPT_FLOAT, (DWORD *)param->data + i * param->columns + k, param->type);
            else
                *tmp = 0.0f;
        }
    }
}

static void set_matrix(struct d3dx_parameter *param, const D3DXMATRIX *matrix, BOOL transpose)
{
    UINT i, k;

    for (i = 0; i < param->rows; ++i)
    {
        for (k = 0; k < param->columns; ++k)
        {
            set_number((FLOAT *)param->data + i * param->columns + k, param->type,
                    transpose ? &matrix->m[k][i] : &matrix->m[i][k], D3DXPT_FLOAT);
                    //transpose ? &matrix->u.m[k][i] : &matrix->u.m[i][k], D3DXPT_FLOAT);
        }
    }
}

static struct d3dx_parameter *get_parameter_element_by_name(struct d3dx_parameter *parameter, LPCSTR name)
{
    UINT element;
    struct d3dx_parameter *temp_parameter;
    LPCSTR part;

    //TRACE("parameter %p, name %s\n", parameter, debugstr_a(name));

    if (!name || !*name) return NULL;

    element = atoi(name);
    part = strchr(name, ']') + 1;

    /* check for empty [] && element range */
    if ((part - name) > 1 && parameter->element_count > element)
    {
        temp_parameter = get_parameter_struct(parameter->member_handles[element]);

        switch (*part++)
        {
            case '.':
                return get_parameter_by_name(NULL, temp_parameter, part);

            case '@':
                return get_annotation_by_name(temp_parameter->annotation_count, temp_parameter->annotation_handles, part);

            case '\0':
                //TRACE("Returning parameter %p\n", temp_parameter);
                return temp_parameter;

            default:
                //FIXME("Unhandled case \"%c\"\n", *--part);
                break;
        }
    }

    //TRACE("Parameter not found\n");
    return NULL;
}

static struct d3dx_parameter *get_annotation_by_name(UINT handlecount, D3DXHANDLE *handles, LPCSTR name)
{
    UINT i, length;
    struct d3dx_parameter *temp_parameter;
    LPCSTR part;

    //TRACE("handlecount %u, handles %p, name %s\n", handlecount, handles, debugstr_a(name));

    if (!name || !*name) return NULL;

    length = strcspn( name, "[.@" );
    part = name + length;

    for (i = 0; i < handlecount; ++i)
    {
        temp_parameter = get_parameter_struct(handles[i]);

        if (!strcmp(temp_parameter->name, name))
        {
            //TRACE("Returning parameter %p\n", temp_parameter);
            return temp_parameter;
        }
        else if (strlen(temp_parameter->name) == length && !strncmp(temp_parameter->name, name, length))
        {
            switch (*part++)
            {
                case '.':
                    return get_parameter_by_name(NULL, temp_parameter, part);

                case '[':
                    return get_parameter_element_by_name(temp_parameter, part);

                default:
                    //FIXME("Unhandled case \"%c\"\n", *--part);
                    break;
            }
        }
    }

    //TRACE("Parameter not found\n");
    return NULL;
}

static struct d3dx_parameter *get_parameter_by_name(struct ID3DXBaseEffectImpl *base,
        struct d3dx_parameter *parameter, LPCSTR name)
{
    UINT i, count, length;
    struct d3dx_parameter *temp_parameter;
    D3DXHANDLE *handles;
    LPCSTR part;

    //TRACE("base %p, parameter %p, name %s\n", base, parameter, debugstr_a(name));

    if (!name || !*name) return NULL;

    if (!parameter)
    {
        count = base->parameter_count;
        handles = base->parameter_handles;
    }
    else
    {
        count = parameter->member_count;
        handles = parameter->member_handles;
    }

    length = strcspn( name, "[.@" );
    part = name + length;

    for (i = 0; i < count; i++)
    {
        temp_parameter = get_parameter_struct(handles[i]);

        if (!strcmp(temp_parameter->name, name))
        {
            //TRACE("Returning parameter %p\n", temp_parameter);
            return temp_parameter;
        }
        else if (strlen(temp_parameter->name) == length && !strncmp(temp_parameter->name, name, length))
        {
            switch (*part++)
            {
                case '.':
                    return get_parameter_by_name(NULL, temp_parameter, part);

                case '@':
                    return get_annotation_by_name(temp_parameter->annotation_count, temp_parameter->annotation_handles, part);

                case '[':
                    return get_parameter_element_by_name(temp_parameter, part);

                default:
                    //FIXME("Unhandled case \"%c\"\n", *--part);
                    break;
            }
        }
    }

    //TRACE("Parameter not found\n");
    return NULL;
}

static inline DWORD d3dx9_effect_version(DWORD major, DWORD minor)
{
    return (0xfeff0000 | ((major) << 8) | (minor));
}

static inline struct ID3DXBaseEffectImpl *impl_from_ID3DXBaseEffect(ID3DXBaseEffect *iface)
{
    return CONTAINING_RECORD(iface, struct ID3DXBaseEffectImpl, ID3DXBaseEffect_iface);
}

/*** IUnknown methods ***/
static HRESULT WINAPI ID3DXBaseEffectImpl_QueryInterface(ID3DXBaseEffect *iface, REFIID riid, void **object)
{
    //TRACE("iface %p, riid %s, object %p.\n", iface, debugstr_guid(riid), object);

    if (IsEqualGUID(riid, (const GUID &)IID_IUnknown) ||
        IsEqualGUID(riid, (const GUID &)IID_ID3DXBaseEffect))
    {
        //iface->lpVtbl->AddRef(iface);
        iface->AddRef();
        *object = iface;
        return S_OK;
    }

    //ERR("Interface %s not found\n", debugstr_guid(riid));

    return E_NOINTERFACE;
}

static ULONG WINAPI ID3DXBaseEffectImpl_AddRef(ID3DXBaseEffect *iface)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);

    //TRACE("iface %p: AddRef from %u\n", iface, This->ref);

    return InterlockedIncrement(&This->ref);
}

static ULONG WINAPI ID3DXBaseEffectImpl_Release(ID3DXBaseEffect *iface)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    //TRACE("iface %p: Release from %u\n", iface, ref + 1);

    if (!ref)
    {
        free_base_effect(This);
        HeapFree(GetProcessHeap(), 0, This);
    }

    return ref;
}

/*** ID3DXBaseEffect methods ***/
static HRESULT WINAPI ID3DXBaseEffectImpl_GetDesc(ID3DXBaseEffect *iface, D3DXEFFECT_DESC *desc)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);

    //FIXME("iface %p, desc %p partial stub\n", This, desc);

    if (!desc)
    {
        //WARN("Invalid argument specified.\n");
        return D3DERR_INVALIDCALL;
    }

    /* Todo: add creator and function count */
    desc->Creator = NULL;
    desc->Functions = 0;
    desc->Parameters = This->parameter_count;
    desc->Techniques = This->technique_count;

    return D3D_OK;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_GetParameterDesc(ID3DXBaseEffect *iface, D3DXHANDLE parameter, D3DXPARAMETER_DESC *desc)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, desc %p\n", This, parameter, desc);

    if (!desc || !param)
    {
        //WARN("Invalid argument specified.\n");
        return D3DERR_INVALIDCALL;
    }

    desc->Name = param->name;
    desc->Semantic = param->semantic;
    desc->Class = param->class2;
    desc->Type = param->type;
    desc->Rows = param->rows;
    desc->Columns = param->columns;
    desc->Elements = param->element_count;
    desc->Annotations = param->annotation_count;
    desc->StructMembers = param->member_count;
    desc->Flags = param->flags;
    desc->Bytes = param->bytes;

    return D3D_OK;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_GetTechniqueDesc(ID3DXBaseEffect *iface, D3DXHANDLE technique, D3DXTECHNIQUE_DESC *desc)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_technique *tech = technique ? is_valid_technique(This, technique) : get_technique_struct(This->technique_handles[0]);

    //TRACE("iface %p, technique %p, desc %p\n", This, technique, desc);

    if (!desc || !tech)
    {
        //WARN("Invalid argument specified.\n");
        return D3DERR_INVALIDCALL;
    }

    desc->Name = tech->name;
    desc->Passes = tech->pass_count;
    desc->Annotations = tech->annotation_count;

    return D3D_OK;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_GetPassDesc(ID3DXBaseEffect *iface, D3DXHANDLE pass, D3DXPASS_DESC *desc)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_pass *p = is_valid_pass(This, pass);

    //TRACE("iface %p, pass %p, desc %p\n", This, pass, desc);

    if (!desc || !p)
    {
        //WARN("Invalid argument specified.\n");
        return D3DERR_INVALIDCALL;
    }

    desc->Name = p->name;
    desc->Annotations = p->annotation_count;

    //FIXME("Pixel shader and vertex shader are not supported, yet.\n");
    desc->pVertexShaderFunction = NULL;
    desc->pPixelShaderFunction = NULL;

    return D3D_OK;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_GetFunctionDesc(ID3DXBaseEffect *iface, D3DXHANDLE shader, D3DXFUNCTION_DESC *desc)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);

    //FIXME("iface %p, shader %p, desc %p stub\n", This, shader, desc);

    return E_NOTIMPL;
}

static D3DXHANDLE WINAPI ID3DXBaseEffectImpl_GetParameter(ID3DXBaseEffect *iface, D3DXHANDLE parameter, UINT index)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, index %u\n", This, parameter, index);

    if (!parameter)
    {
        if (index < This->parameter_count)
        {
            //TRACE("Returning parameter %p\n", This->parameter_handles[index]);
            return This->parameter_handles[index];
        }
    }
    else
    {
        if (param && !param->element_count && index < param->member_count)
        {
            //TRACE("Returning parameter %p\n", param->member_handles[index]);
            return param->member_handles[index];
        }
    }

    //WARN("Invalid argument specified.\n");

    return NULL;
}

static D3DXHANDLE WINAPI ID3DXBaseEffectImpl_GetParameterByName(ID3DXBaseEffect *iface, D3DXHANDLE parameter, LPCSTR name)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);
    D3DXHANDLE handle;

    //TRACE("iface %p, parameter %p, name %s\n", This, parameter, debugstr_a(name));

    if (!name)
    {
        handle = get_parameter_handle(param);
        //TRACE("Returning parameter %p\n", handle);
        return handle;
    }

    handle = get_parameter_handle(get_parameter_by_name(This, param, name));
    //TRACE("Returning parameter %p\n", handle);

    return handle;
}

static D3DXHANDLE WINAPI ID3DXBaseEffectImpl_GetParameterBySemantic(ID3DXBaseEffect *iface, D3DXHANDLE parameter, LPCSTR semantic)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);
    struct d3dx_parameter *temp_param;
    UINT i;

    //TRACE("iface %p, parameter %p, semantic %s\n", This, parameter, debugstr_a(semantic));

    if (!parameter)
    {
        for (i = 0; i < This->parameter_count; ++i)
        {
            temp_param = get_parameter_struct(This->parameter_handles[i]);

            if (!temp_param->semantic)
            {
                if (!semantic)
                {
                    //TRACE("Returning parameter %p\n", This->parameter_handles[i]);
                    return This->parameter_handles[i];
                }
                continue;
            }

            if (!strcasecmp(temp_param->semantic, semantic))
            {
                //TRACE("Returning parameter %p\n", This->parameter_handles[i]);
                return This->parameter_handles[i];
            }
        }
    }
    else if (param)
    {
        for (i = 0; i < param->member_count; ++i)
        {
            temp_param = get_parameter_struct(param->member_handles[i]);

            if (!temp_param->semantic)
            {
                if (!semantic)
                {
                    //TRACE("Returning parameter %p\n", param->member_handles[i]);
                    return param->member_handles[i];
                }
                continue;
            }

            if (!strcasecmp(temp_param->semantic, semantic))
            {
                //TRACE("Returning parameter %p\n", param->member_handles[i]);
                return param->member_handles[i];
            }
        }
    }

    //WARN("Invalid argument specified\n");

    return NULL;
}

static D3DXHANDLE WINAPI ID3DXBaseEffectImpl_GetParameterElement(ID3DXBaseEffect *iface, D3DXHANDLE parameter, UINT index)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, index %u\n", This, parameter, index);

    if (!param)
    {
        if (index < This->parameter_count)
        {
            //TRACE("Returning parameter %p\n", This->parameter_handles[index]);
            return This->parameter_handles[index];
        }
    }
    else
    {
        if (index < param->element_count)
        {
            //TRACE("Returning parameter %p\n", param->member_handles[index]);
            return param->member_handles[index];
        }
    }

    //WARN("Invalid argument specified\n");

    return NULL;
}

static D3DXHANDLE WINAPI ID3DXBaseEffectImpl_GetTechnique(ID3DXBaseEffect *iface, UINT index)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);

    //TRACE("iface %p, index %u\n", This, index);

    if (index >= This->technique_count)
    {
        //WARN("Invalid argument specified.\n");
        return NULL;
    }

    //TRACE("Returning technique %p\n", This->technique_handles[index]);

    return This->technique_handles[index];
}

static D3DXHANDLE WINAPI ID3DXBaseEffectImpl_GetTechniqueByName(ID3DXBaseEffect *iface, LPCSTR name)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_technique *tech = get_technique_by_name(This, name);

    //TRACE("iface %p, name %s stub\n", This, debugstr_a(name));

    if (tech)
    {
        D3DXHANDLE t = get_technique_handle(tech);
        //TRACE("Returning technique %p\n", t);
        return t;
    }

    //WARN("Invalid argument specified.\n");

    return NULL;
}

static D3DXHANDLE WINAPI ID3DXBaseEffectImpl_GetPass(ID3DXBaseEffect *iface, D3DXHANDLE technique, UINT index)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_technique *tech = is_valid_technique(This, technique);

    //TRACE("iface %p, technique %p, index %u\n", This, technique, index);

    if (tech && index < tech->pass_count)
    {
        //TRACE("Returning pass %p\n", tech->pass_handles[index]);
        return tech->pass_handles[index];
    }

    //WARN("Invalid argument specified.\n");

    return NULL;
}

static D3DXHANDLE WINAPI ID3DXBaseEffectImpl_GetPassByName(ID3DXBaseEffect *iface, D3DXHANDLE technique, LPCSTR name)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_technique *tech = is_valid_technique(This, technique);

    //TRACE("iface %p, technique %p, name %s\n", This, technique, debugstr_a(name));

    if (tech && name)
    {
        unsigned int i;

        for (i = 0; i < tech->pass_count; ++i)
        {
            struct d3dx_pass *pass = get_pass_struct(tech->pass_handles[i]);

            if (!strcmp(pass->name, name))
            {
                //TRACE("Returning pass %p\n", tech->pass_handles[i]);
                return tech->pass_handles[i];
            }
        }
    }

    //WARN("Invalid argument specified.\n");

    return NULL;
}

static D3DXHANDLE WINAPI ID3DXBaseEffectImpl_GetFunction(ID3DXBaseEffect *iface, UINT index)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);

    //FIXME("iface %p, index %u stub\n", This, index);

    return NULL;
}

static D3DXHANDLE WINAPI ID3DXBaseEffectImpl_GetFunctionByName(ID3DXBaseEffect *iface, LPCSTR name)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);

    //FIXME("iface %p, name %s stub\n", This, debugstr_a(name));

    return NULL;
}

static D3DXHANDLE WINAPI ID3DXBaseEffectImpl_GetAnnotation(ID3DXBaseEffect *iface, D3DXHANDLE object, UINT index)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, object);
    struct d3dx_pass *pass = is_valid_pass(This, object);
    struct d3dx_technique *technique = is_valid_technique(This, object);
    UINT annotation_count = 0;
    D3DXHANDLE *annotation_handles = NULL;

    //TRACE("iface %p, object %p, index %u\n", This, object, index);

    if (pass)
    {
        annotation_count = pass->annotation_count;
        annotation_handles = pass->annotation_handles;
    }
    else if (technique)
    {
        annotation_count = technique->annotation_count;
        annotation_handles = technique->annotation_handles;
    }
    else if (param)
    {
        annotation_count = param->annotation_count;
        annotation_handles = param->annotation_handles;
    }
    else
    {
        //FIXME("Functions are not handled, yet!\n");
    }

    if (index < annotation_count)
    {
        //TRACE("Returning parameter %p\n", annotation_handles[index]);
        return annotation_handles[index];
    }

    //WARN("Invalid argument specified\n");

    return NULL;
}

static D3DXHANDLE WINAPI ID3DXBaseEffectImpl_GetAnnotationByName(ID3DXBaseEffect *iface, D3DXHANDLE object, LPCSTR name)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, object);
    struct d3dx_pass *pass = is_valid_pass(This, object);
    struct d3dx_technique *technique = is_valid_technique(This, object);
    struct d3dx_parameter *anno = NULL;
    UINT annotation_count = 0;
    D3DXHANDLE *annotation_handles = NULL;

    //TRACE("iface %p, object %p, name %s\n", This, object, debugstr_a(name));

    if (!name)
    {
        //WARN("Invalid argument specified\n");
        return NULL;
    }

    if (pass)
    {
        annotation_count = pass->annotation_count;
        annotation_handles = pass->annotation_handles;
    }
    else if (technique)
    {
        annotation_count = technique->annotation_count;
        annotation_handles = technique->annotation_handles;
    }
    else if (param)
    {
        annotation_count = param->annotation_count;
        annotation_handles = param->annotation_handles;
    }
    else
    {
        //FIXME("Functions are not handled, yet!\n");
    }

    anno = get_annotation_by_name(annotation_count, annotation_handles, name);
    if (anno)
    {
        //TRACE("Returning parameter %p\n", anno);
        return get_parameter_handle(anno);
    }

    //WARN("Invalid argument specified\n");

    return NULL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_SetValue(ID3DXBaseEffect *iface, D3DXHANDLE parameter, LPCVOID data, UINT bytes)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, data %p, bytes %u\n", This, parameter, data, bytes);

    if (!param)
    {
        //WARN("Invalid parameter %p specified\n", parameter);
        return D3DERR_INVALIDCALL;
    }

    /* samplers don't touch data */
    if (param->class2 == D3DXPC_OBJECT && (param->type == D3DXPT_SAMPLER
            || param->type == D3DXPT_SAMPLER1D || param->type == D3DXPT_SAMPLER2D
            || param->type == D3DXPT_SAMPLER3D || param->type == D3DXPT_SAMPLERCUBE))
    {
        //TRACE("Sampler: returning E_FAIL\n");
        return E_FAIL;
    }

    if (data && param->bytes <= bytes)
    {
        switch (param->type)
        {
            case D3DXPT_VOID:
            case D3DXPT_BOOL:
            case D3DXPT_INT:
            case D3DXPT_FLOAT:
                //TRACE("Copy %u bytes\n", param->bytes);
                memcpy(param->data, data, param->bytes);
                break;

            default:
                //FIXME("Unhandled type %s\n", debug_d3dxparameter_type(param->type));
                break;
        }

        return D3D_OK;
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_GetValue(ID3DXBaseEffect *iface, D3DXHANDLE parameter, LPVOID data, UINT bytes)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, data %p, bytes %u\n", This, parameter, data, bytes);

    if (!param)
    {
        //WARN("Invalid parameter %p specified\n", parameter);
        return D3DERR_INVALIDCALL;
    }

    /* samplers don't touch data */
    if (param->class2 == D3DXPC_OBJECT && (param->type == D3DXPT_SAMPLER
            || param->type == D3DXPT_SAMPLER1D || param->type == D3DXPT_SAMPLER2D
            || param->type == D3DXPT_SAMPLER3D || param->type == D3DXPT_SAMPLERCUBE))
    {
        //TRACE("Sampler: returning E_FAIL\n");
        return E_FAIL;
    }

    if (data && param->bytes <= bytes)
    {
        //TRACE("Type %s\n", debug_d3dxparameter_type(param->type));

        switch (param->type)
        {
            case D3DXPT_VOID:
            case D3DXPT_BOOL:
            case D3DXPT_INT:
            case D3DXPT_FLOAT:
            case D3DXPT_STRING:
                break;

            case D3DXPT_VERTEXSHADER:
            case D3DXPT_PIXELSHADER:
            case D3DXPT_TEXTURE:
            case D3DXPT_TEXTURE1D:
            case D3DXPT_TEXTURE2D:
            case D3DXPT_TEXTURE3D:
            case D3DXPT_TEXTURECUBE:
            {
                UINT i;

                for (i = 0; i < (param->element_count ? param->element_count : 1); ++i)
                {
                    IUnknown *unk = ((IUnknown **)param->data)[i];
                    if (unk) IUnknown_AddRef(unk);
                }
                break;
            }

            default:
                //FIXME("Unhandled type %s\n", debug_d3dxparameter_type(param->type));
                break;
        }

        //TRACE("Copy %u bytes\n", param->bytes);
        memcpy(data, param->data, param->bytes);
        return D3D_OK;
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_SetBool(ID3DXBaseEffect *iface, D3DXHANDLE parameter, BOOL b)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, b %s\n", This, parameter, b ? "TRUE" : "FALSE");

    if (param && !param->element_count && param->rows == 1 && param->columns == 1)
    {
        set_number(param->data, param->type, &b, D3DXPT_BOOL);
        return D3D_OK;
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_GetBool(ID3DXBaseEffect *iface, D3DXHANDLE parameter, BOOL *b)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, b %p\n", This, parameter, b);

    if (b && param && !param->element_count && param->rows == 1 && param->columns == 1)
    {
        set_number(b, D3DXPT_BOOL, param->data, param->type);
        //TRACE("Returning %s\n", *b ? "TRUE" : "FALSE");
        return D3D_OK;
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_SetBoolArray(ID3DXBaseEffect *iface, D3DXHANDLE parameter, CONST BOOL *b, UINT count)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, b %p, count %u\n", This, parameter, b, count);

    if (param)
    {
        UINT i, size = min(count, param->bytes / sizeof(DWORD));

        //TRACE("Class %s\n", debug_d3dxparameter_class(param->class2));

        switch (param->class2)
        {
            case D3DXPC_SCALAR:
            case D3DXPC_VECTOR:
            case D3DXPC_MATRIX_ROWS:
                for (i = 0; i < size; ++i)
                {
                    /* don't crop the input, use D3DXPT_INT instead of D3DXPT_BOOL */
                    set_number((DWORD *)param->data + i, param->type, &b[i], D3DXPT_INT);
                }
                return D3D_OK;

            case D3DXPC_OBJECT:
            case D3DXPC_STRUCT:
                break;

            default:
                //FIXME("Unhandled class %s\n", debug_d3dxparameter_class(param->class2));
                break;
        }
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_GetBoolArray(ID3DXBaseEffect *iface, D3DXHANDLE parameter, BOOL *b, UINT count)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, b %p, count %u\n", This, parameter, b, count);

    if (b && param && (param->class2 == D3DXPC_SCALAR
            || param->class2 == D3DXPC_VECTOR
            || param->class2 == D3DXPC_MATRIX_ROWS
            || param->class2 == D3DXPC_MATRIX_COLUMNS))
    {
        UINT i, size = min(count, param->bytes / sizeof(DWORD));

        for (i = 0; i < size; ++i)
        {
            set_number(&b[i], D3DXPT_BOOL, (DWORD *)param->data + i, param->type);
        }
        return D3D_OK;
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_SetInt(ID3DXBaseEffect *iface, D3DXHANDLE parameter, INT n)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, n %i\n", This, parameter, n);

    if (param && !param->element_count)
    {
        if (param->rows == 1 && param->columns == 1)
        {
            set_number(param->data, param->type, &n, D3DXPT_INT);
            return D3D_OK;
        }

        /*
         * Split the value, if parameter is a vector with dimension 3 or 4.
         */
        if (param->type == D3DXPT_FLOAT &&
            ((param->class2 == D3DXPC_VECTOR && param->columns != 2) ||
            (param->class2 == D3DXPC_MATRIX_ROWS && param->rows != 2 && param->columns == 1)))
        {
            //TRACE("Vector fixup\n");

            *(FLOAT *)param->data = ((n & 0xff0000) >> 16) * INT_FLOAT_MULTI_INVERSE;
            ((FLOAT *)param->data)[1] = ((n & 0xff00) >> 8) * INT_FLOAT_MULTI_INVERSE;
            ((FLOAT *)param->data)[2] = (n & 0xff) * INT_FLOAT_MULTI_INVERSE;
            if (param->rows * param->columns > 3)
            {
                ((FLOAT *)param->data)[3] = ((n & 0xff000000) >> 24) * INT_FLOAT_MULTI_INVERSE;
            }
            return D3D_OK;
        }
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_GetInt(ID3DXBaseEffect *iface, D3DXHANDLE parameter, INT *n)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, n %p\n", This, parameter, n);

    if (n && param && !param->element_count)
    {
        if (param->columns == 1 && param->rows == 1)
        {
            set_number(n, D3DXPT_INT, param->data, param->type);
            //TRACE("Returning %i\n", *n);
            return D3D_OK;
        }

        if (param->type == D3DXPT_FLOAT &&
                ((param->class2 == D3DXPC_VECTOR && param->columns != 2)
                || (param->class2 == D3DXPC_MATRIX_ROWS && param->rows != 2 && param->columns == 1)))
        {
            //TRACE("Vector fixup\n");

            /* all components (3,4) are clamped (0,255) and put in the INT */
            *n = (INT)(min(max(0.0f, *((FLOAT *)param->data + 2)), 1.0f) * INT_FLOAT_MULTI);
            *n += ((INT)(min(max(0.0f, *((FLOAT *)param->data + 1)), 1.0f) * INT_FLOAT_MULTI)) << 8;
            *n += ((INT)(min(max(0.0f, *((FLOAT *)param->data + 0)), 1.0f) * INT_FLOAT_MULTI)) << 16;
            if (param->columns * param->rows > 3)
            {
                *n += ((INT)(min(max(0.0f, *((FLOAT *)param->data + 3)), 1.0f) * INT_FLOAT_MULTI)) << 24;
            }

            //TRACE("Returning %i\n", *n);
            return D3D_OK;
        }
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_SetIntArray(ID3DXBaseEffect *iface, D3DXHANDLE parameter, CONST INT *n, UINT count)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, n %p, count %u\n", This, parameter, n, count);

    if (param)
    {
        UINT i, size = min(count, param->bytes / sizeof(DWORD));

        //TRACE("Class %s\n", debug_d3dxparameter_class(param->class2));

        switch (param->class2)
        {
            case D3DXPC_SCALAR:
            case D3DXPC_VECTOR:
            case D3DXPC_MATRIX_ROWS:
                for (i = 0; i < size; ++i)
                {
                    set_number((DWORD *)param->data + i, param->type, &n[i], D3DXPT_INT);
                }
                return D3D_OK;

            case D3DXPC_OBJECT:
            case D3DXPC_STRUCT:
                break;

            default:
                //FIXME("Unhandled class %s\n", debug_d3dxparameter_class(param->class2));
                break;
        }
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_GetIntArray(ID3DXBaseEffect *iface, D3DXHANDLE parameter, INT *n, UINT count)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, n %p, count %u\n", This, parameter, n, count);

    if (n && param && (param->class2 == D3DXPC_SCALAR
            || param->class2 == D3DXPC_VECTOR
            || param->class2 == D3DXPC_MATRIX_ROWS
            || param->class2 == D3DXPC_MATRIX_COLUMNS))
    {
        UINT i, size = min(count, param->bytes / sizeof(DWORD));

        for (i = 0; i < size; ++i)
        {
            set_number(&n[i], D3DXPT_INT, (DWORD *)param->data + i, param->type);
        }
        return D3D_OK;
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_SetFloat(ID3DXBaseEffect *iface, D3DXHANDLE parameter, FLOAT f)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, f %f\n", This, parameter, f);

    if (param && !param->element_count && param->rows == 1 && param->columns == 1)
    {
        set_number((DWORD *)param->data, param->type, &f, D3DXPT_FLOAT);
        return D3D_OK;
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_GetFloat(ID3DXBaseEffect *iface, D3DXHANDLE parameter, FLOAT *f)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, f %p\n", This, parameter, f);

    if (f && param && !param->element_count && param->columns == 1 && param->rows == 1)
    {
        set_number(f, D3DXPT_FLOAT, (DWORD *)param->data, param->type);
        //TRACE("Returning %f\n", *f);
        return D3D_OK;
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_SetFloatArray(ID3DXBaseEffect *iface, D3DXHANDLE parameter, CONST FLOAT *f, UINT count)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, f %p, count %u\n", This, parameter, f, count);

    if (param)
    {
        UINT i, size = min(count, param->bytes / sizeof(DWORD));

        //TRACE("Class %s\n", debug_d3dxparameter_class(param->class2));

        switch (param->class2)
        {
            case D3DXPC_SCALAR:
            case D3DXPC_VECTOR:
            case D3DXPC_MATRIX_ROWS:
                for (i = 0; i < size; ++i)
                {
                    set_number((DWORD *)param->data + i, param->type, &f[i], D3DXPT_FLOAT);
                }
                return D3D_OK;

            case D3DXPC_OBJECT:
            case D3DXPC_STRUCT:
                break;

            default:
                //FIXME("Unhandled class %s\n", debug_d3dxparameter_class(param->class2));
                break;
        }
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_GetFloatArray(ID3DXBaseEffect *iface, D3DXHANDLE parameter, FLOAT *f, UINT count)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, f %p, count %u\n", This, parameter, f, count);

    if (f && param && (param->class2 == D3DXPC_SCALAR
            || param->class2 == D3DXPC_VECTOR
            || param->class2 == D3DXPC_MATRIX_ROWS
            || param->class2 == D3DXPC_MATRIX_COLUMNS))
    {
        UINT i, size = min(count, param->bytes / sizeof(DWORD));

        for (i = 0; i < size; ++i)
        {
            set_number(&f[i], D3DXPT_FLOAT, (DWORD *)param->data + i, param->type);
        }
        return D3D_OK;
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_SetVector(ID3DXBaseEffect *iface, D3DXHANDLE parameter, CONST D3DXVECTOR4 *vector)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, vector %p\n", This, parameter, vector);

    if (param && !param->element_count)
    {
        //TRACE("Class %s\n", debug_d3dxparameter_class(param->class2));

        switch (param->class2)
        {
            case D3DXPC_SCALAR:
            case D3DXPC_VECTOR:
                if (param->type == D3DXPT_INT && param->bytes == 4)
                {
                    DWORD tmp;

                    //TRACE("INT fixup\n");
                    tmp = (DWORD)(max(min(vector->z, 1.0f), 0.0f) * INT_FLOAT_MULTI);
                    tmp += ((DWORD)(max(min(vector->y, 1.0f), 0.0f) * INT_FLOAT_MULTI)) << 8;
                    tmp += ((DWORD)(max(min(vector->x, 1.0f), 0.0f) * INT_FLOAT_MULTI)) << 16;
                    tmp += ((DWORD)(max(min(vector->w, 1.0f), 0.0f) * INT_FLOAT_MULTI)) << 24;

                    *(INT *)param->data = tmp;
                    return D3D_OK;
                }
                set_vector(param, vector);
                return D3D_OK;

            case D3DXPC_MATRIX_ROWS:
            case D3DXPC_OBJECT:
            case D3DXPC_STRUCT:
                break;

            default:
                //FIXME("Unhandled class %s\n", debug_d3dxparameter_class(param->class2));
                break;
        }
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_GetVector(ID3DXBaseEffect *iface, D3DXHANDLE parameter, D3DXVECTOR4 *vector)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, vector %p\n", This, parameter, vector);

    if (vector && param && !param->element_count)
    {
        //TRACE("Class %s\n", debug_d3dxparameter_class(param->class2));

        switch (param->class2)
        {
            case D3DXPC_SCALAR:
            case D3DXPC_VECTOR:
                if (param->type == D3DXPT_INT && param->bytes == 4)
                {
                    //TRACE("INT fixup\n");
                    vector->x = (((*(INT *)param->data) & 0xff0000) >> 16) * INT_FLOAT_MULTI_INVERSE;
                    vector->y = (((*(INT *)param->data) & 0xff00) >> 8) * INT_FLOAT_MULTI_INVERSE;
                    vector->z = ((*(INT *)param->data) & 0xff) * INT_FLOAT_MULTI_INVERSE;
                    vector->w = (((*(INT *)param->data) & 0xff000000) >> 24) * INT_FLOAT_MULTI_INVERSE;
                    return D3D_OK;
                }
                get_vector(param, vector);
                return D3D_OK;

            case D3DXPC_MATRIX_ROWS:
            case D3DXPC_OBJECT:
            case D3DXPC_STRUCT:
                break;

            default:
                //FIXME("Unhandled class %s\n", debug_d3dxparameter_class(param->class2));
                break;
        }
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_SetVectorArray(ID3DXBaseEffect *iface, D3DXHANDLE parameter, CONST D3DXVECTOR4 *vector, UINT count)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, vector %p, count %u stub\n", This, parameter, vector, count);

    if (param && param->element_count && param->element_count >= count)
    {
        UINT i;

        //TRACE("Class %s\n", debug_d3dxparameter_class(param->class2));

        switch (param->class2)
        {
            case D3DXPC_VECTOR:
                for (i = 0; i < count; ++i)
                {
                    set_vector(get_parameter_struct(param->member_handles[i]), &vector[i]);
                }
                return D3D_OK;

            case D3DXPC_SCALAR:
            case D3DXPC_MATRIX_ROWS:
            case D3DXPC_OBJECT:
            case D3DXPC_STRUCT:
                break;

            default:
                //FIXME("Unhandled class %s\n", debug_d3dxparameter_class(param->class2));
                break;
        }
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_GetVectorArray(ID3DXBaseEffect *iface, D3DXHANDLE parameter, D3DXVECTOR4 *vector, UINT count)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, vector %p, count %u\n", This, parameter, vector, count);

    if (!count) return D3D_OK;

    if (vector && param && count <= param->element_count)
    {
        UINT i;

        //TRACE("Class %s\n", debug_d3dxparameter_class(param->class2));

        switch (param->class2)
        {
            case D3DXPC_VECTOR:
                for (i = 0; i < count; ++i)
                {
                    get_vector(get_parameter_struct(param->member_handles[i]), &vector[i]);
                }
                return D3D_OK;

            case D3DXPC_SCALAR:
            case D3DXPC_MATRIX_ROWS:
            case D3DXPC_OBJECT:
            case D3DXPC_STRUCT:
                break;

            default:
                //FIXME("Unhandled class %s\n", debug_d3dxparameter_class(param->class2));
                break;
        }
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_SetMatrix(ID3DXBaseEffect *iface, D3DXHANDLE parameter, CONST D3DXMATRIX *matrix)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, matrix %p\n", This, parameter, matrix);

    if (param && !param->element_count)
    {
        //TRACE("Class %s\n", debug_d3dxparameter_class(param->class2));

        switch (param->class2)
        {
            case D3DXPC_MATRIX_ROWS:
                set_matrix(param, matrix, FALSE);
                return D3D_OK;

            case D3DXPC_SCALAR:
            case D3DXPC_VECTOR:
            case D3DXPC_OBJECT:
            case D3DXPC_STRUCT:
                break;

            default:
                //FIXME("Unhandled class %s\n", debug_d3dxparameter_class(param->class2));
                break;
        }
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_GetMatrix(ID3DXBaseEffect *iface, D3DXHANDLE parameter, D3DXMATRIX *matrix)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, matrix %p\n", This, parameter, matrix);

    if (matrix && param && !param->element_count)
    {
        //TRACE("Class %s\n", debug_d3dxparameter_class(param->class2));

        switch (param->class2)
        {
            case D3DXPC_MATRIX_ROWS:
                get_matrix(param, matrix, FALSE);
                return D3D_OK;

            case D3DXPC_SCALAR:
            case D3DXPC_VECTOR:
            case D3DXPC_OBJECT:
            case D3DXPC_STRUCT:
                break;

            default:
                //FIXME("Unhandled class %s\n", debug_d3dxparameter_class(param->class2));
                break;
        }
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_SetMatrixArray(ID3DXBaseEffect *iface, D3DXHANDLE parameter, CONST D3DXMATRIX *matrix, UINT count)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, matrix %p, count %u\n", This, parameter, matrix, count);

    if (param && param->element_count >= count)
    {
        UINT i;

        //TRACE("Class %s\n", debug_d3dxparameter_class(param->class2));

        switch (param->class2)
        {
            case D3DXPC_MATRIX_ROWS:
                for (i = 0; i < count; ++i)
                {
                    set_matrix(get_parameter_struct(param->member_handles[i]), &matrix[i], FALSE);
                }
                return D3D_OK;

            case D3DXPC_SCALAR:
            case D3DXPC_VECTOR:
            case D3DXPC_OBJECT:
            case D3DXPC_STRUCT:
                break;

            default:
                //FIXME("Unhandled class %s\n", debug_d3dxparameter_class(param->class2));
                break;
        }
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_GetMatrixArray(ID3DXBaseEffect *iface, D3DXHANDLE parameter, D3DXMATRIX *matrix, UINT count)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, matrix %p, count %u\n", This, parameter, matrix, count);

    if (!count) return D3D_OK;

    if (matrix && param && count <= param->element_count)
    {
        UINT i;

        //TRACE("Class %s\n", debug_d3dxparameter_class(param->class2));

        switch (param->class2)
        {
            case D3DXPC_MATRIX_ROWS:
                for (i = 0; i < count; ++i)
                {
                    get_matrix(get_parameter_struct(param->member_handles[i]), &matrix[i], FALSE);
                }
                return D3D_OK;

            case D3DXPC_SCALAR:
            case D3DXPC_VECTOR:
            case D3DXPC_OBJECT:
            case D3DXPC_STRUCT:
                break;

            default:
                //FIXME("Unhandled class %s\n", debug_d3dxparameter_class(param->class2));
                break;
        }
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_SetMatrixPointerArray(ID3DXBaseEffect *iface, D3DXHANDLE parameter, CONST D3DXMATRIX **matrix, UINT count)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, matrix %p, count %u\n", This, parameter, matrix, count);

    if (param && count <= param->element_count)
    {
        UINT i;

        switch (param->class2)
        {
            case D3DXPC_MATRIX_ROWS:
                for (i = 0; i < count; ++i)
                {
                    set_matrix(get_parameter_struct(param->member_handles[i]), matrix[i], FALSE);
                }
                return D3D_OK;

            case D3DXPC_SCALAR:
            case D3DXPC_VECTOR:
            case D3DXPC_OBJECT:
                break;

            default:
                //FIXME("Unhandled class %s\n", debug_d3dxparameter_class(param->class2));
                break;
        }
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_GetMatrixPointerArray(ID3DXBaseEffect *iface, D3DXHANDLE parameter, D3DXMATRIX **matrix, UINT count)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, matrix %p, count %u\n", This, parameter, matrix, count);

    if (!count) return D3D_OK;

    if (param && matrix && count <= param->element_count)
    {
        UINT i;

        //TRACE("Class %s\n", debug_d3dxparameter_class(param->class2));

        switch (param->class2)
        {
            case D3DXPC_MATRIX_ROWS:
                for (i = 0; i < count; ++i)
                {
                    get_matrix(get_parameter_struct(param->member_handles[i]), matrix[i], FALSE);
                }
                return D3D_OK;

            case D3DXPC_SCALAR:
            case D3DXPC_VECTOR:
            case D3DXPC_OBJECT:
                break;

            default:
                //FIXME("Unhandled class %s\n", debug_d3dxparameter_class(param->class2));
                break;
        }
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_SetMatrixTranspose(ID3DXBaseEffect *iface, D3DXHANDLE parameter, CONST D3DXMATRIX *matrix)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, matrix %p\n", This, parameter, matrix);

    if (param && !param->element_count)
    {
        //TRACE("Class %s\n", debug_d3dxparameter_class(param->class2));

        switch (param->class2)
        {
            case D3DXPC_MATRIX_ROWS:
                set_matrix(param, matrix, TRUE);
                return D3D_OK;

            case D3DXPC_SCALAR:
            case D3DXPC_VECTOR:
            case D3DXPC_OBJECT:
            case D3DXPC_STRUCT:
                break;

            default:
                //FIXME("Unhandled class %s\n", debug_d3dxparameter_class(param->class2));
                break;
        }
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_GetMatrixTranspose(ID3DXBaseEffect *iface, D3DXHANDLE parameter, D3DXMATRIX *matrix)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, matrix %p\n", This, parameter, matrix);

    if (matrix && param && !param->element_count)
    {
        //TRACE("Class %s\n", debug_d3dxparameter_class(param->class2));

        switch (param->class2)
        {
            case D3DXPC_SCALAR:
            case D3DXPC_VECTOR:
                get_matrix(param, matrix, FALSE);
                return D3D_OK;

            case D3DXPC_MATRIX_ROWS:
                get_matrix(param, matrix, TRUE);
                return D3D_OK;

            case D3DXPC_OBJECT:
            case D3DXPC_STRUCT:
                break;

            default:
                //FIXME("Unhandled class %s\n", debug_d3dxparameter_class(param->class2));
                break;
        }
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_SetMatrixTransposeArray(ID3DXBaseEffect *iface, D3DXHANDLE parameter, CONST D3DXMATRIX *matrix, UINT count)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, matrix %p, count %u\n", This, parameter, matrix, count);

    if (param && param->element_count >= count)
    {
        UINT i;

        //TRACE("Class %s\n", debug_d3dxparameter_class(param->class2));

        switch (param->class2)
        {
            case D3DXPC_MATRIX_ROWS:
                for (i = 0; i < count; ++i)
                {
                    set_matrix(get_parameter_struct(param->member_handles[i]), &matrix[i], TRUE);
                }
                return D3D_OK;

            case D3DXPC_SCALAR:
            case D3DXPC_VECTOR:
            case D3DXPC_OBJECT:
            case D3DXPC_STRUCT:
                break;

            default:
                //FIXME("Unhandled class %s\n", debug_d3dxparameter_class(param->class2));
                break;
        }
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_GetMatrixTransposeArray(ID3DXBaseEffect *iface, D3DXHANDLE parameter, D3DXMATRIX *matrix, UINT count)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, matrix %p, count %u\n", This, parameter, matrix, count);

    if (!count) return D3D_OK;

    if (matrix && param && count <= param->element_count)
    {
        UINT i;

        //TRACE("Class %s\n", debug_d3dxparameter_class(param->class2));

        switch (param->class2)
        {
            case D3DXPC_MATRIX_ROWS:
                for (i = 0; i < count; ++i)
                {
                    get_matrix(get_parameter_struct(param->member_handles[i]), &matrix[i], TRUE);
                }
                return D3D_OK;

            case D3DXPC_SCALAR:
            case D3DXPC_VECTOR:
            case D3DXPC_OBJECT:
            case D3DXPC_STRUCT:
                break;

            default:
                //FIXME("Unhandled class %s\n", debug_d3dxparameter_class(param->class2));
                break;
        }
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_SetMatrixTransposePointerArray(ID3DXBaseEffect *iface, D3DXHANDLE parameter, CONST D3DXMATRIX **matrix, UINT count)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, matrix %p, count %u\n", This, parameter, matrix, count);

    if (param && count <= param->element_count)
    {
        UINT i;

        switch (param->class2)
        {
            case D3DXPC_MATRIX_ROWS:
                for (i = 0; i < count; ++i)
                {
                    set_matrix(get_parameter_struct(param->member_handles[i]), matrix[i], TRUE);
                }
                return D3D_OK;

            case D3DXPC_SCALAR:
            case D3DXPC_VECTOR:
            case D3DXPC_OBJECT:
                break;

            default:
                //FIXME("Unhandled class %s\n", debug_d3dxparameter_class(param->class2));
                break;
        }
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_GetMatrixTransposePointerArray(ID3DXBaseEffect *iface, D3DXHANDLE parameter, D3DXMATRIX **matrix, UINT count)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, matrix %p, count %u\n", This, parameter, matrix, count);

    if (!count) return D3D_OK;

    if (matrix && param && count <= param->element_count)
    {
        UINT i;

        //TRACE("Class %s\n", debug_d3dxparameter_class(param->class2));

        switch (param->class2)
        {
            case D3DXPC_MATRIX_ROWS:
                for (i = 0; i < count; ++i)
                {
                    get_matrix(get_parameter_struct(param->member_handles[i]), matrix[i], TRUE);
                }
                return D3D_OK;

            case D3DXPC_SCALAR:
            case D3DXPC_VECTOR:
            case D3DXPC_OBJECT:
                break;

            default:
                //FIXME("Unhandled class %s\n", debug_d3dxparameter_class(param->class2));
                break;
        }
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_SetString(ID3DXBaseEffect *iface, D3DXHANDLE parameter, LPCSTR string)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);

    //FIXME("iface %p, parameter %p, string %p stub\n", This, parameter, string);

    return E_NOTIMPL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_GetString(ID3DXBaseEffect *iface, D3DXHANDLE parameter, LPCSTR *string)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, string %p\n", This, parameter, string);

    if (string && param && !param->element_count && param->type == D3DXPT_STRING)
    {
        *string = *(LPCSTR *)param->data;
        //TRACE("Returning %s\n", debugstr_a(*string));
        return D3D_OK;
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_SetTexture(ID3DXBaseEffect *iface, D3DXHANDLE parameter, LPDIRECT3DBASETEXTURE9 texture)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, texture %p\n", This, parameter, texture);

    if (param && !param->element_count &&
            (param->type == D3DXPT_TEXTURE || param->type == D3DXPT_TEXTURE1D
            || param->type == D3DXPT_TEXTURE2D || param->type ==  D3DXPT_TEXTURE3D
            || param->type == D3DXPT_TEXTURECUBE))
    {
        LPDIRECT3DBASETEXTURE9 oltexture = *(LPDIRECT3DBASETEXTURE9 *)param->data;

        if (texture) IDirect3DBaseTexture9_AddRef(texture);
        if (oltexture) IDirect3DBaseTexture9_Release(oltexture);

        *(LPDIRECT3DBASETEXTURE9 *)param->data = texture;

        return D3D_OK;
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_GetTexture(ID3DXBaseEffect *iface, D3DXHANDLE parameter, LPDIRECT3DBASETEXTURE9 *texture)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, texture %p\n", This, parameter, texture);

    if (texture && param && !param->element_count &&
            (param->type == D3DXPT_TEXTURE || param->type == D3DXPT_TEXTURE1D
            || param->type == D3DXPT_TEXTURE2D || param->type ==  D3DXPT_TEXTURE3D
            || param->type == D3DXPT_TEXTURECUBE))
    {
        *texture = *(LPDIRECT3DBASETEXTURE9 *)param->data;
        if (*texture) IDirect3DBaseTexture9_AddRef(*texture);
        //TRACE("Returning %p\n", *texture);
        return D3D_OK;
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_GetPixelShader(ID3DXBaseEffect *iface, D3DXHANDLE parameter, LPDIRECT3DPIXELSHADER9 *pshader)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, pshader %p\n", This, parameter, pshader);

    if (pshader && param && !param->element_count && param->type == D3DXPT_PIXELSHADER)
    {
        *pshader = *(LPDIRECT3DPIXELSHADER9 *)param->data;
        if (*pshader) IDirect3DPixelShader9_AddRef(*pshader);
        //TRACE("Returning %p\n", *pshader);
        return D3D_OK;
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_GetVertexShader(ID3DXBaseEffect *iface, D3DXHANDLE parameter, LPDIRECT3DVERTEXSHADER9 *vshader)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);
    struct d3dx_parameter *param = get_valid_parameter(This, parameter);

    //TRACE("iface %p, parameter %p, vshader %p\n", This, parameter, vshader);

    if (vshader && param && !param->element_count && param->type == D3DXPT_VERTEXSHADER)
    {
        *vshader = *(LPDIRECT3DVERTEXSHADER9 *)param->data;
        if (*vshader) IDirect3DVertexShader9_AddRef(*vshader);
        //TRACE("Returning %p\n", *vshader);
        return D3D_OK;
    }

    //WARN("Invalid argument specified\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXBaseEffectImpl_SetArrayRange(ID3DXBaseEffect *iface, D3DXHANDLE parameter, UINT start, UINT end)
{
    struct ID3DXBaseEffectImpl *This = impl_from_ID3DXBaseEffect(iface);

    //FIXME("iface %p, parameter %p, start %u, end %u stub\n", This, parameter, start, end);

    return E_NOTIMPL;
}
/*
static const struct ID3DXBaseEffectVtbl ID3DXBaseEffect_Vtbl = {
    ID3DXBaseEffectImpl_QueryInterface,
    ID3DXBaseEffectImpl_AddRef,
    ID3DXBaseEffectImpl_Release,
    ID3DXBaseEffectImpl_GetDesc,
    ID3DXBaseEffectImpl_GetParameterDesc,
    ID3DXBaseEffectImpl_GetTechniqueDesc,
    ID3DXBaseEffectImpl_GetPassDesc,
    ID3DXBaseEffectImpl_GetFunctionDesc,
    ID3DXBaseEffectImpl_GetParameter,
    ID3DXBaseEffectImpl_GetParameterByName,
    ID3DXBaseEffectImpl_GetParameterBySemantic,
    ID3DXBaseEffectImpl_GetParameterElement,
    ID3DXBaseEffectImpl_GetTechnique,
    ID3DXBaseEffectImpl_GetTechniqueByName,
    ID3DXBaseEffectImpl_GetPass,
    ID3DXBaseEffectImpl_GetPassByName,
    ID3DXBaseEffectImpl_GetFunction,
    ID3DXBaseEffectImpl_GetFunctionByName,
    ID3DXBaseEffectImpl_GetAnnotation,
    ID3DXBaseEffectImpl_GetAnnotationByName,
    ID3DXBaseEffectImpl_SetValue,
    ID3DXBaseEffectImpl_GetValue,
    ID3DXBaseEffectImpl_SetBool,
    ID3DXBaseEffectImpl_GetBool,
    ID3DXBaseEffectImpl_SetBoolArray,
    ID3DXBaseEffectImpl_GetBoolArray,
    ID3DXBaseEffectImpl_SetInt,
    ID3DXBaseEffectImpl_GetInt,
    ID3DXBaseEffectImpl_SetIntArray,
    ID3DXBaseEffectImpl_GetIntArray,
    ID3DXBaseEffectImpl_SetFloat,
    ID3DXBaseEffectImpl_GetFloat,
    ID3DXBaseEffectImpl_SetFloatArray,
    ID3DXBaseEffectImpl_GetFloatArray,
    ID3DXBaseEffectImpl_SetVector,
    ID3DXBaseEffectImpl_GetVector,
    ID3DXBaseEffectImpl_SetVectorArray,
    ID3DXBaseEffectImpl_GetVectorArray,
    ID3DXBaseEffectImpl_SetMatrix,
    ID3DXBaseEffectImpl_GetMatrix,
    ID3DXBaseEffectImpl_SetMatrixArray,
    ID3DXBaseEffectImpl_GetMatrixArray,
    ID3DXBaseEffectImpl_SetMatrixPointerArray,
    ID3DXBaseEffectImpl_GetMatrixPointerArray,
    ID3DXBaseEffectImpl_SetMatrixTranspose,
    ID3DXBaseEffectImpl_GetMatrixTranspose,
    ID3DXBaseEffectImpl_SetMatrixTransposeArray,
    ID3DXBaseEffectImpl_GetMatrixTransposeArray,
    ID3DXBaseEffectImpl_SetMatrixTransposePointerArray,
    ID3DXBaseEffectImpl_GetMatrixTransposePointerArray,
    ID3DXBaseEffectImpl_SetString,
    ID3DXBaseEffectImpl_GetString,
    ID3DXBaseEffectImpl_SetTexture,
    ID3DXBaseEffectImpl_GetTexture,
    ID3DXBaseEffectImpl_GetPixelShader,
    ID3DXBaseEffectImpl_GetVertexShader,
    ID3DXBaseEffectImpl_SetArrayRange,
};
*/
static inline struct ID3DXEffectImpl *impl_from_ID3DXEffect(ID3DXEffect *iface)
{
    return CONTAINING_RECORD(iface, struct ID3DXEffectImpl, ID3DXEffect_iface);
}

/*** IUnknown methods ***/
static HRESULT WINAPI ID3DXEffectImpl_QueryInterface(ID3DXEffect *iface, REFIID riid, void **object)
{
    //TRACE("(%p)->(%s, %p)\n", iface, debugstr_guid(riid), object);

    if (IsEqualGUID(riid, (const GUID &)IID_IUnknown) ||
        IsEqualGUID(riid, (const GUID &)IID_ID3DXEffect))
    {
        //iface->lpVtbl->AddRef(iface);
		iface->AddRef();
        *object = iface;
        return S_OK;
    }

    //ERR("Interface %s not found\n", debugstr_guid(riid));

    return E_NOINTERFACE;
}

static ULONG WINAPI ID3DXEffectImpl_AddRef(ID3DXEffect *iface)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);

    //TRACE("(%p)->(): AddRef from %u\n", This, This->ref);

    return InterlockedIncrement(&This->ref);
}

static ULONG WINAPI ID3DXEffectImpl_Release(ID3DXEffect *iface)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    //TRACE("(%p)->(): Release from %u\n", This, ref + 1);

    if (!ref)
    {
        free_effect(This);
        HeapFree(GetProcessHeap(), 0, This);
    }

    return ref;
}

/*** ID3DXBaseEffect methods ***/
static HRESULT WINAPI ID3DXEffectImpl_GetDesc(ID3DXEffect *iface, D3DXEFFECT_DESC *desc)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetDesc(base, desc);
}

static HRESULT WINAPI ID3DXEffectImpl_GetParameterDesc(ID3DXEffect *iface, D3DXHANDLE parameter, D3DXPARAMETER_DESC *desc)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetParameterDesc(base, parameter, desc);
}

static HRESULT WINAPI ID3DXEffectImpl_GetTechniqueDesc(ID3DXEffect *iface, D3DXHANDLE technique, D3DXTECHNIQUE_DESC *desc)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetTechniqueDesc(base, technique, desc);
}

static HRESULT WINAPI ID3DXEffectImpl_GetPassDesc(ID3DXEffect *iface, D3DXHANDLE pass, D3DXPASS_DESC *desc)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetPassDesc(base, pass, desc);
}

static HRESULT WINAPI ID3DXEffectImpl_GetFunctionDesc(ID3DXEffect *iface, D3DXHANDLE shader, D3DXFUNCTION_DESC *desc)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetFunctionDesc(base, shader, desc);
}

static D3DXHANDLE WINAPI ID3DXEffectImpl_GetParameter(ID3DXEffect *iface, D3DXHANDLE parameter, UINT index)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetParameter(base, parameter, index);
}

static D3DXHANDLE WINAPI ID3DXEffectImpl_GetParameterByName(ID3DXEffect *iface, D3DXHANDLE parameter, LPCSTR name)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetParameterByName(base, parameter, name);
}

static D3DXHANDLE WINAPI ID3DXEffectImpl_GetParameterBySemantic(ID3DXEffect *iface, D3DXHANDLE parameter, LPCSTR semantic)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetParameterBySemantic(base, parameter, semantic);
}

static D3DXHANDLE WINAPI ID3DXEffectImpl_GetParameterElement(ID3DXEffect *iface, D3DXHANDLE parameter, UINT index)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetParameterElement(base, parameter, index);
}

static D3DXHANDLE WINAPI ID3DXEffectImpl_GetTechnique(ID3DXEffect *iface, UINT index)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetTechnique(base, index);
}

static D3DXHANDLE WINAPI ID3DXEffectImpl_GetTechniqueByName(ID3DXEffect *iface, LPCSTR name)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetTechniqueByName(base, name);
}

static D3DXHANDLE WINAPI ID3DXEffectImpl_GetPass(ID3DXEffect *iface, D3DXHANDLE technique, UINT index)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetPass(base, technique, index);
}

static D3DXHANDLE WINAPI ID3DXEffectImpl_GetPassByName(ID3DXEffect *iface, D3DXHANDLE technique, LPCSTR name)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetPassByName(base, technique, name);
}

static D3DXHANDLE WINAPI ID3DXEffectImpl_GetFunction(ID3DXEffect *iface, UINT index)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetFunction(base, index);
}

static D3DXHANDLE WINAPI ID3DXEffectImpl_GetFunctionByName(ID3DXEffect *iface, LPCSTR name)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetFunctionByName(base, name);
}

static D3DXHANDLE WINAPI ID3DXEffectImpl_GetAnnotation(ID3DXEffect *iface, D3DXHANDLE object, UINT index)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetAnnotation(base, object, index);
}

static D3DXHANDLE WINAPI ID3DXEffectImpl_GetAnnotationByName(ID3DXEffect *iface, D3DXHANDLE object, LPCSTR name)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetAnnotationByName(base, object, name);
}

static HRESULT WINAPI ID3DXEffectImpl_SetValue(ID3DXEffect *iface, D3DXHANDLE parameter, LPCVOID data, UINT bytes)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetValue(base, parameter, data, bytes);
}

static HRESULT WINAPI ID3DXEffectImpl_GetValue(ID3DXEffect *iface, D3DXHANDLE parameter, LPVOID data, UINT bytes)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetValue(base, parameter, data, bytes);
}

static HRESULT WINAPI ID3DXEffectImpl_SetBool(ID3DXEffect *iface, D3DXHANDLE parameter, BOOL b)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetBool(base, parameter, b);
}

static HRESULT WINAPI ID3DXEffectImpl_GetBool(ID3DXEffect *iface, D3DXHANDLE parameter, BOOL *b)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetBool(base, parameter, b);
}

static HRESULT WINAPI ID3DXEffectImpl_SetBoolArray(ID3DXEffect *iface, D3DXHANDLE parameter, CONST BOOL *b, UINT count)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetBoolArray(base, parameter, b, count);
}

static HRESULT WINAPI ID3DXEffectImpl_GetBoolArray(ID3DXEffect *iface, D3DXHANDLE parameter, BOOL *b, UINT count)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetBoolArray(base, parameter, b, count);
}

static HRESULT WINAPI ID3DXEffectImpl_SetInt(ID3DXEffect *iface, D3DXHANDLE parameter, INT n)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetInt(base, parameter, n);
}

static HRESULT WINAPI ID3DXEffectImpl_GetInt(ID3DXEffect *iface, D3DXHANDLE parameter, INT *n)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetInt(base, parameter, n);
}

static HRESULT WINAPI ID3DXEffectImpl_SetIntArray(ID3DXEffect *iface, D3DXHANDLE parameter, CONST INT *n, UINT count)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetIntArray(base, parameter, n, count);
}

static HRESULT WINAPI ID3DXEffectImpl_GetIntArray(ID3DXEffect *iface, D3DXHANDLE parameter, INT *n, UINT count)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetIntArray(base, parameter, n, count);
}

static HRESULT WINAPI ID3DXEffectImpl_SetFloat(ID3DXEffect *iface, D3DXHANDLE parameter, FLOAT f)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetFloat(base, parameter, f);
}

static HRESULT WINAPI ID3DXEffectImpl_GetFloat(ID3DXEffect *iface, D3DXHANDLE parameter, FLOAT *f)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetFloat(base, parameter, f);
}

static HRESULT WINAPI ID3DXEffectImpl_SetFloatArray(ID3DXEffect *iface, D3DXHANDLE parameter, CONST FLOAT *f, UINT count)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetFloatArray(base, parameter, f, count);
}

static HRESULT WINAPI ID3DXEffectImpl_GetFloatArray(ID3DXEffect *iface, D3DXHANDLE parameter, FLOAT *f, UINT count)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetFloatArray(base, parameter, f, count);
}

static HRESULT WINAPI ID3DXEffectImpl_SetVector(ID3DXEffect *iface, D3DXHANDLE parameter, CONST D3DXVECTOR4 *vector)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetVector(base, parameter, vector);
}

static HRESULT WINAPI ID3DXEffectImpl_GetVector(ID3DXEffect *iface, D3DXHANDLE parameter, D3DXVECTOR4 *vector)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetVector(base, parameter, vector);
}

static HRESULT WINAPI ID3DXEffectImpl_SetVectorArray(ID3DXEffect *iface, D3DXHANDLE parameter, CONST D3DXVECTOR4 *vector, UINT count)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetVectorArray(base, parameter, vector, count);
}

static HRESULT WINAPI ID3DXEffectImpl_GetVectorArray(ID3DXEffect *iface, D3DXHANDLE parameter, D3DXVECTOR4 *vector, UINT count)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetVectorArray(base, parameter, vector, count);
}

static HRESULT WINAPI ID3DXEffectImpl_SetMatrix(ID3DXEffect *iface, D3DXHANDLE parameter, CONST D3DXMATRIX *matrix)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetMatrix(base, parameter, matrix);
}

static HRESULT WINAPI ID3DXEffectImpl_GetMatrix(ID3DXEffect *iface, D3DXHANDLE parameter, D3DXMATRIX *matrix)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetMatrix(base, parameter, matrix);
}

static HRESULT WINAPI ID3DXEffectImpl_SetMatrixArray(ID3DXEffect *iface, D3DXHANDLE parameter, CONST D3DXMATRIX *matrix, UINT count)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetMatrixArray(base, parameter, matrix, count);
}

static HRESULT WINAPI ID3DXEffectImpl_GetMatrixArray(ID3DXEffect *iface, D3DXHANDLE parameter, D3DXMATRIX *matrix, UINT count)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetMatrixArray(base, parameter, matrix, count);
}

static HRESULT WINAPI ID3DXEffectImpl_SetMatrixPointerArray(ID3DXEffect *iface, D3DXHANDLE parameter, CONST D3DXMATRIX **matrix, UINT count)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetMatrixPointerArray(base, parameter, matrix, count);
}

static HRESULT WINAPI ID3DXEffectImpl_GetMatrixPointerArray(ID3DXEffect *iface, D3DXHANDLE parameter, D3DXMATRIX **matrix, UINT count)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetMatrixPointerArray(base, parameter, matrix, count);
}

static HRESULT WINAPI ID3DXEffectImpl_SetMatrixTranspose(ID3DXEffect *iface, D3DXHANDLE parameter, CONST D3DXMATRIX *matrix)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetMatrixTranspose(base, parameter, matrix);
}

static HRESULT WINAPI ID3DXEffectImpl_GetMatrixTranspose(ID3DXEffect *iface, D3DXHANDLE parameter, D3DXMATRIX *matrix)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetMatrixTranspose(base, parameter, matrix);
}

static HRESULT WINAPI ID3DXEffectImpl_SetMatrixTransposeArray(ID3DXEffect *iface, D3DXHANDLE parameter, CONST D3DXMATRIX *matrix, UINT count)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetMatrixTransposeArray(base, parameter, matrix, count);
}

static HRESULT WINAPI ID3DXEffectImpl_GetMatrixTransposeArray(ID3DXEffect *iface, D3DXHANDLE parameter, D3DXMATRIX *matrix, UINT count)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetMatrixTransposeArray(base, parameter, matrix, count);
}

static HRESULT WINAPI ID3DXEffectImpl_SetMatrixTransposePointerArray(ID3DXEffect *iface, D3DXHANDLE parameter, CONST D3DXMATRIX **matrix, UINT count)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetMatrixTransposePointerArray(base, parameter, matrix, count);
}

static HRESULT WINAPI ID3DXEffectImpl_GetMatrixTransposePointerArray(ID3DXEffect *iface, D3DXHANDLE parameter, D3DXMATRIX **matrix, UINT count)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetMatrixTransposePointerArray(base, parameter, matrix, count);
}

static HRESULT WINAPI ID3DXEffectImpl_SetString(ID3DXEffect *iface, D3DXHANDLE parameter, LPCSTR string)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetString(base, parameter, string);
}

static HRESULT WINAPI ID3DXEffectImpl_GetString(ID3DXEffect *iface, D3DXHANDLE parameter, LPCSTR *string)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetString(base, parameter, string);
}

static HRESULT WINAPI ID3DXEffectImpl_SetTexture(ID3DXEffect *iface, D3DXHANDLE parameter, LPDIRECT3DBASETEXTURE9 texture)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetTexture(base, parameter, texture);
}

static HRESULT WINAPI ID3DXEffectImpl_GetTexture(ID3DXEffect *iface, D3DXHANDLE parameter, LPDIRECT3DBASETEXTURE9 *texture)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetTexture(base, parameter, texture);
}

static HRESULT WINAPI ID3DXEffectImpl_GetPixelShader(ID3DXEffect *iface, D3DXHANDLE parameter, LPDIRECT3DPIXELSHADER9 *pshader)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetPixelShader(base, parameter, pshader);
}

static HRESULT WINAPI ID3DXEffectImpl_GetVertexShader(ID3DXEffect *iface, D3DXHANDLE parameter, LPDIRECT3DVERTEXSHADER9 *vshader)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetVertexShader(base, parameter, vshader);
}

static HRESULT WINAPI ID3DXEffectImpl_SetArrayRange(ID3DXEffect *iface, D3DXHANDLE parameter, UINT start, UINT end)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetArrayRange(base, parameter, start, end);
}

/*** ID3DXEffect methods ***/
static HRESULT WINAPI ID3DXEffectImpl_GetPool(ID3DXEffect *iface, LPD3DXEFFECTPOOL *pool)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);

    //TRACE("iface %p, pool %p\n", This, pool);

    if (!pool)
    {
        //WARN("Invalid argument supplied.\n");
        return D3DERR_INVALIDCALL;
    }

    if (This->pool)
    {
        //This->pool->lpVtbl->AddRef(This->pool);
		This->pool->AddRef();
    }

    *pool = This->pool;

    //TRACE("Returning pool %p\n", *pool);

    return S_OK;
}

static HRESULT WINAPI ID3DXEffectImpl_SetTechnique(ID3DXEffect *iface, D3DXHANDLE technique)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    struct ID3DXBaseEffectImpl *base = impl_from_ID3DXBaseEffect(This->base_effect);
    struct d3dx_technique *tech = is_valid_technique(base, technique);

    //TRACE("iface %p, technique %p\n", This, technique);

    if (tech)
    {
        This->active_technique = get_technique_handle(tech);
        //TRACE("Technique %p\n", tech);
        return D3D_OK;
    }

    //WARN("Invalid argument supplied.\n");

    return D3DERR_INVALIDCALL;
}

static D3DXHANDLE WINAPI ID3DXEffectImpl_GetCurrentTechnique(ID3DXEffect *iface)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);

    //TRACE("iface %p\n", This);

    return This->active_technique;
}

static HRESULT WINAPI ID3DXEffectImpl_ValidateTechnique(ID3DXEffect* iface, D3DXHANDLE technique)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);

    //FIXME("(%p)->(%p): stub\n", This, technique);

    return D3D_OK;
}

static HRESULT WINAPI ID3DXEffectImpl_FindNextValidTechnique(ID3DXEffect* iface, D3DXHANDLE technique, D3DXHANDLE* next_technique)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);

    //FIXME("(%p)->(%p, %p): stub\n", This, technique, next_technique);

    return E_NOTIMPL;
}

static BOOL WINAPI ID3DXEffectImpl_IsParameterUsed(ID3DXEffect* iface, D3DXHANDLE parameter, D3DXHANDLE technique)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);

    //FIXME("(%p)->(%p, %p): stub\n", This, parameter, technique);

    return FALSE;
}

static HRESULT WINAPI ID3DXEffectImpl_Begin(ID3DXEffect *iface, UINT *passes, DWORD flags)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    struct d3dx_technique *technique = get_technique_struct(This->active_technique);

    //FIXME("iface %p, passes %p, flags %#x partial stub\n", This, passes, flags);

    if (passes && technique)
    {
        if (This->manager || flags & D3DXFX_DONOTSAVESTATE)
        {
            //TRACE("State capturing disabled.\n");
        }
        else
        {
            //FIXME("State capturing not supported, yet!\n");
        }

        *passes = technique->pass_count;

        return D3D_OK;
    }

    //WARN("Invalid argument supplied.\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXEffectImpl_BeginPass(ID3DXEffect *iface, UINT pass)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);
    struct d3dx_technique *technique = get_technique_struct(This->active_technique);

    //TRACE("iface %p, pass %u\n", This, pass);

    if (technique && pass < technique->pass_count && !This->active_pass)
    {
        This->active_pass = technique->pass_handles[pass];

        //FIXME("No states applied, yet!\n");

        return D3D_OK;
    }

    //WARN("Invalid argument supplied.\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXEffectImpl_CommitChanges(ID3DXEffect* iface)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);

    //FIXME("(%p)->(): stub\n", This);

    return E_NOTIMPL;
}

static HRESULT WINAPI ID3DXEffectImpl_EndPass(ID3DXEffect *iface)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);

    //TRACE("iface %p\n", This);

    if (This->active_pass)
    {
         This->active_pass = NULL;
         return D3D_OK;
    }

    //WARN("Invalid call.\n");

    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI ID3DXEffectImpl_End(ID3DXEffect* iface)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);

    //FIXME("(%p)->(): stub\n", This);

    return E_NOTIMPL;
}

static HRESULT WINAPI ID3DXEffectImpl_GetDevice(ID3DXEffect *iface, LPDIRECT3DDEVICE9 *device)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);

    //TRACE("iface %p, device %p\n", This, device);

    if (!device)
    {
        //WARN("Invalid argument supplied.\n");
        return D3DERR_INVALIDCALL;
    }

    IDirect3DDevice9_AddRef(This->device);

    *device = This->device;

    //TRACE("Returning device %p\n", *device);

    return S_OK;
}

static HRESULT WINAPI ID3DXEffectImpl_OnLostDevice(ID3DXEffect* iface)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);

    //FIXME("(%p)->(): stub\n", This);

    return E_NOTIMPL;
}

static HRESULT WINAPI ID3DXEffectImpl_OnResetDevice(ID3DXEffect* iface)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);

    //FIXME("(%p)->(): stub\n", This);

    return E_NOTIMPL;
}

static HRESULT WINAPI ID3DXEffectImpl_SetStateManager(ID3DXEffect *iface, LPD3DXEFFECTSTATEMANAGER manager)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);

    //TRACE("iface %p, manager %p\n", This, manager);

    if (manager) IUnknown_AddRef(manager);
    if (This->manager) IUnknown_Release(This->manager);

    This->manager = manager;

    return D3D_OK;
}

static HRESULT WINAPI ID3DXEffectImpl_GetStateManager(ID3DXEffect *iface, LPD3DXEFFECTSTATEMANAGER *manager)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);

    //TRACE("iface %p, manager %p\n", This, manager);

    if (!manager)
    {
        //WARN("Invalid argument supplied.\n");
        return D3DERR_INVALIDCALL;
    }

    if (This->manager) IUnknown_AddRef(This->manager);
    *manager = This->manager;

    return D3D_OK;
}

static HRESULT WINAPI ID3DXEffectImpl_BeginParameterBlock(ID3DXEffect* iface)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);

    //FIXME("(%p)->(): stub\n", This);

    return E_NOTIMPL;
}

static D3DXHANDLE WINAPI ID3DXEffectImpl_EndParameterBlock(ID3DXEffect* iface)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);

    //FIXME("(%p)->(): stub\n", This);

    return NULL;
}

static HRESULT WINAPI ID3DXEffectImpl_ApplyParameterBlock(ID3DXEffect* iface, D3DXHANDLE parameter_block)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);

    //FIXME("(%p)->(%p): stub\n", This, parameter_block);

    return E_NOTIMPL;
}

static HRESULT WINAPI ID3DXEffectImpl_DeleteParameterBlock(ID3DXEffect* iface, D3DXHANDLE parameter_block)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);

    //FIXME("(%p)->(%p): stub\n", This, parameter_block);

    return E_NOTIMPL;
}

static HRESULT WINAPI ID3DXEffectImpl_CloneEffect(ID3DXEffect* iface, LPDIRECT3DDEVICE9 device, LPD3DXEFFECT* effect)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);

    //FIXME("(%p)->(%p, %p): stub\n", This, device, effect);

    return E_NOTIMPL;
}

static HRESULT WINAPI ID3DXEffectImpl_SetRawValue(ID3DXEffect* iface, D3DXHANDLE parameter, LPCVOID data, UINT byte_offset, UINT bytes)
{
    struct ID3DXEffectImpl *This = impl_from_ID3DXEffect(iface);

    //FIXME("(%p)->(%p, %p, %u, %u): stub\n", This, parameter, data, byte_offset, bytes);

    return E_NOTIMPL;
}
/*
static const struct ID3DXEffectVtbl ID3DXEffect_Vtbl =
{
    ID3DXEffectImpl_QueryInterface,
    ID3DXEffectImpl_AddRef,
    ID3DXEffectImpl_Release,
    ID3DXEffectImpl_GetDesc,
    ID3DXEffectImpl_GetParameterDesc,
    ID3DXEffectImpl_GetTechniqueDesc,
    ID3DXEffectImpl_GetPassDesc,
    ID3DXEffectImpl_GetFunctionDesc,
    ID3DXEffectImpl_GetParameter,
    ID3DXEffectImpl_GetParameterByName,
    ID3DXEffectImpl_GetParameterBySemantic,
    ID3DXEffectImpl_GetParameterElement,
    ID3DXEffectImpl_GetTechnique,
    ID3DXEffectImpl_GetTechniqueByName,
    ID3DXEffectImpl_GetPass,
    ID3DXEffectImpl_GetPassByName,
    ID3DXEffectImpl_GetFunction,
    ID3DXEffectImpl_GetFunctionByName,
    ID3DXEffectImpl_GetAnnotation,
    ID3DXEffectImpl_GetAnnotationByName,
    ID3DXEffectImpl_SetValue,
    ID3DXEffectImpl_GetValue,
    ID3DXEffectImpl_SetBool,
    ID3DXEffectImpl_GetBool,
    ID3DXEffectImpl_SetBoolArray,
    ID3DXEffectImpl_GetBoolArray,
    ID3DXEffectImpl_SetInt,
    ID3DXEffectImpl_GetInt,
    ID3DXEffectImpl_SetIntArray,
    ID3DXEffectImpl_GetIntArray,
    ID3DXEffectImpl_SetFloat,
    ID3DXEffectImpl_GetFloat,
    ID3DXEffectImpl_SetFloatArray,
    ID3DXEffectImpl_GetFloatArray,
    ID3DXEffectImpl_SetVector,
    ID3DXEffectImpl_GetVector,
    ID3DXEffectImpl_SetVectorArray,
    ID3DXEffectImpl_GetVectorArray,
    ID3DXEffectImpl_SetMatrix,
    ID3DXEffectImpl_GetMatrix,
    ID3DXEffectImpl_SetMatrixArray,
    ID3DXEffectImpl_GetMatrixArray,
    ID3DXEffectImpl_SetMatrixPointerArray,
    ID3DXEffectImpl_GetMatrixPointerArray,
    ID3DXEffectImpl_SetMatrixTranspose,
    ID3DXEffectImpl_GetMatrixTranspose,
    ID3DXEffectImpl_SetMatrixTransposeArray,
    ID3DXEffectImpl_GetMatrixTransposeArray,
    ID3DXEffectImpl_SetMatrixTransposePointerArray,
    ID3DXEffectImpl_GetMatrixTransposePointerArray,
    ID3DXEffectImpl_SetString,
    ID3DXEffectImpl_GetString,
    ID3DXEffectImpl_SetTexture,
    ID3DXEffectImpl_GetTexture,
    ID3DXEffectImpl_GetPixelShader,
    ID3DXEffectImpl_GetVertexShader,
    ID3DXEffectImpl_SetArrayRange,
    ID3DXEffectImpl_GetPool,
    ID3DXEffectImpl_SetTechnique,
    ID3DXEffectImpl_GetCurrentTechnique,
    ID3DXEffectImpl_ValidateTechnique,
    ID3DXEffectImpl_FindNextValidTechnique,
    ID3DXEffectImpl_IsParameterUsed,
    ID3DXEffectImpl_Begin,
    ID3DXEffectImpl_BeginPass,
    ID3DXEffectImpl_CommitChanges,
    ID3DXEffectImpl_EndPass,
    ID3DXEffectImpl_End,
    ID3DXEffectImpl_GetDevice,
    ID3DXEffectImpl_OnLostDevice,
    ID3DXEffectImpl_OnResetDevice,
    ID3DXEffectImpl_SetStateManager,
    ID3DXEffectImpl_GetStateManager,
    ID3DXEffectImpl_BeginParameterBlock,
    ID3DXEffectImpl_EndParameterBlock,
    ID3DXEffectImpl_ApplyParameterBlock,
    ID3DXEffectImpl_DeleteParameterBlock,
    ID3DXEffectImpl_CloneEffect,
    ID3DXEffectImpl_SetRawValue
};
*/
static inline struct ID3DXEffectCompilerImpl *impl_from_ID3DXEffectCompiler(ID3DXEffectCompiler *iface)
{
    return CONTAINING_RECORD(iface, struct ID3DXEffectCompilerImpl, ID3DXEffectCompiler_iface);
}

/*** IUnknown methods ***/
static HRESULT WINAPI ID3DXEffectCompilerImpl_QueryInterface(ID3DXEffectCompiler *iface, REFIID riid, void **object)
{
    //TRACE("iface %p, riid %s, object %p.\n", iface, debugstr_guid(riid), object);

    if (IsEqualGUID(riid, (const GUID &)IID_IUnknown) ||
        IsEqualGUID(riid, (const GUID &)IID_ID3DXEffectCompiler))
    {
        //iface->lpVtbl->AddRef(iface);
		iface->AddRef();
        *object = iface;
        return S_OK;
    }

    //ERR("Interface %s not found\n", debugstr_guid(riid));

    return E_NOINTERFACE;
}

static ULONG WINAPI ID3DXEffectCompilerImpl_AddRef(ID3DXEffectCompiler *iface)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);

    //TRACE("iface %p: AddRef from %u\n", iface, This->ref);

    return InterlockedIncrement(&This->ref);
}

static ULONG WINAPI ID3DXEffectCompilerImpl_Release(ID3DXEffectCompiler *iface)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    //TRACE("iface %p: Release from %u\n", iface, ref + 1);

    if (!ref)
    {
        free_effect_compiler(This);
        HeapFree(GetProcessHeap(), 0, This);
    }

    return ref;
}

/*** ID3DXBaseEffect methods ***/
static HRESULT WINAPI ID3DXEffectCompilerImpl_GetDesc(ID3DXEffectCompiler *iface, D3DXEFFECT_DESC *desc)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetDesc(base, desc);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetParameterDesc(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, D3DXPARAMETER_DESC *desc)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetParameterDesc(base, parameter, desc);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetTechniqueDesc(ID3DXEffectCompiler *iface, D3DXHANDLE technique, D3DXTECHNIQUE_DESC *desc)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetTechniqueDesc(base, technique, desc);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetPassDesc(ID3DXEffectCompiler *iface, D3DXHANDLE pass, D3DXPASS_DESC *desc)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetPassDesc(base, pass, desc);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetFunctionDesc(ID3DXEffectCompiler *iface, D3DXHANDLE shader, D3DXFUNCTION_DESC *desc)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetFunctionDesc(base, shader, desc);
}

static D3DXHANDLE WINAPI ID3DXEffectCompilerImpl_GetParameter(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, UINT index)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetParameter(base, parameter, index);
}

static D3DXHANDLE WINAPI ID3DXEffectCompilerImpl_GetParameterByName(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, LPCSTR name)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetParameterByName(base, parameter, name);
}

static D3DXHANDLE WINAPI ID3DXEffectCompilerImpl_GetParameterBySemantic(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, LPCSTR semantic)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetParameterBySemantic(base, parameter, semantic);
}

static D3DXHANDLE WINAPI ID3DXEffectCompilerImpl_GetParameterElement(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, UINT index)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetParameterElement(base, parameter, index);
}

static D3DXHANDLE WINAPI ID3DXEffectCompilerImpl_GetTechnique(ID3DXEffectCompiler *iface, UINT index)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetTechnique(base, index);
}

static D3DXHANDLE WINAPI ID3DXEffectCompilerImpl_GetTechniqueByName(ID3DXEffectCompiler *iface, LPCSTR name)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetTechniqueByName(base, name);
}

static D3DXHANDLE WINAPI ID3DXEffectCompilerImpl_GetPass(ID3DXEffectCompiler *iface, D3DXHANDLE technique, UINT index)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetPass(base, technique, index);
}

static D3DXHANDLE WINAPI ID3DXEffectCompilerImpl_GetPassByName(ID3DXEffectCompiler *iface, D3DXHANDLE technique, LPCSTR name)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetPassByName(base, technique, name);
}

static D3DXHANDLE WINAPI ID3DXEffectCompilerImpl_GetFunction(ID3DXEffectCompiler *iface, UINT index)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetFunction(base, index);
}

static D3DXHANDLE WINAPI ID3DXEffectCompilerImpl_GetFunctionByName(ID3DXEffectCompiler *iface, LPCSTR name)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetFunctionByName(base, name);
}

static D3DXHANDLE WINAPI ID3DXEffectCompilerImpl_GetAnnotation(ID3DXEffectCompiler *iface, D3DXHANDLE object, UINT index)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetAnnotation(base, object, index);
}

static D3DXHANDLE WINAPI ID3DXEffectCompilerImpl_GetAnnotationByName(ID3DXEffectCompiler *iface, D3DXHANDLE object, LPCSTR name)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetAnnotationByName(base, object, name);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_SetValue(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, LPCVOID data, UINT bytes)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetValue(base, parameter, data, bytes);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetValue(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, LPVOID data, UINT bytes)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetValue(base, parameter, data, bytes);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_SetBool(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, BOOL b)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetBool(base, parameter, b);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetBool(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, BOOL *b)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetBool(base, parameter, b);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_SetBoolArray(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, CONST BOOL *b, UINT count)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetBoolArray(base, parameter, b, count);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetBoolArray(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, BOOL *b, UINT count)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetBoolArray(base, parameter, b, count);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_SetInt(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, INT n)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetInt(base, parameter, n);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetInt(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, INT *n)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetInt(base, parameter, n);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_SetIntArray(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, CONST INT *n, UINT count)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetIntArray(base, parameter, n, count);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetIntArray(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, INT *n, UINT count)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetIntArray(base, parameter, n, count);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_SetFloat(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, FLOAT f)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetFloat(base, parameter, f);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetFloat(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, FLOAT *f)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetFloat(base, parameter, f);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_SetFloatArray(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, CONST FLOAT *f, UINT count)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetFloatArray(base, parameter, f, count);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetFloatArray(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, FLOAT *f, UINT count)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetFloatArray(base, parameter, f, count);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_SetVector(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, CONST D3DXVECTOR4 *vector)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetVector(base, parameter, vector);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetVector(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, D3DXVECTOR4 *vector)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetVector(base, parameter, vector);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_SetVectorArray(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, CONST D3DXVECTOR4 *vector, UINT count)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetVectorArray(base, parameter, vector, count);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetVectorArray(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, D3DXVECTOR4 *vector, UINT count)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetVectorArray(base, parameter, vector, count);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_SetMatrix(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, CONST D3DXMATRIX *matrix)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetMatrix(base, parameter, matrix);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetMatrix(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, D3DXMATRIX *matrix)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetMatrix(base, parameter, matrix);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_SetMatrixArray(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, CONST D3DXMATRIX *matrix, UINT count)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetMatrixArray(base, parameter, matrix, count);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetMatrixArray(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, D3DXMATRIX *matrix, UINT count)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetMatrixArray(base, parameter, matrix, count);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_SetMatrixPointerArray(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, CONST D3DXMATRIX **matrix, UINT count)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetMatrixPointerArray(base, parameter, matrix, count);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetMatrixPointerArray(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, D3DXMATRIX **matrix, UINT count)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetMatrixPointerArray(base, parameter, matrix, count);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_SetMatrixTranspose(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, CONST D3DXMATRIX *matrix)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetMatrixTranspose(base, parameter, matrix);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetMatrixTranspose(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, D3DXMATRIX *matrix)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetMatrixTranspose(base, parameter, matrix);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_SetMatrixTransposeArray(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, CONST D3DXMATRIX *matrix, UINT count)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetMatrixTransposeArray(base, parameter, matrix, count);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetMatrixTransposeArray(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, D3DXMATRIX *matrix, UINT count)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetMatrixTransposeArray(base, parameter, matrix, count);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_SetMatrixTransposePointerArray(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, CONST D3DXMATRIX **matrix, UINT count)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetMatrixTransposePointerArray(base, parameter, matrix, count);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetMatrixTransposePointerArray(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, D3DXMATRIX **matrix, UINT count)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetMatrixTransposePointerArray(base, parameter, matrix, count);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_SetString(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, LPCSTR string)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetString(base, parameter, string);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetString(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, LPCSTR *string)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetString(base, parameter, string);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_SetTexture(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, LPDIRECT3DBASETEXTURE9 texture)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetTexture(base, parameter, texture);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetTexture(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, LPDIRECT3DBASETEXTURE9 *texture)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetTexture(base, parameter, texture);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetPixelShader(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, LPDIRECT3DPIXELSHADER9 *pshader)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetPixelShader(base, parameter, pshader);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetVertexShader(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, LPDIRECT3DVERTEXSHADER9 *vshader)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_GetVertexShader(base, parameter, vshader);
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_SetArrayRange(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, UINT start, UINT end)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);
    ID3DXBaseEffect *base = This->base_effect;

    //TRACE("Forward iface %p, base %p\n", This, base);

    return ID3DXBaseEffectImpl_SetArrayRange(base, parameter, start, end);
}

/*** ID3DXEffectCompiler methods ***/
static HRESULT WINAPI ID3DXEffectCompilerImpl_SetLiteral(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, BOOL literal)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);

    //FIXME("iface %p, parameter %p, literal %u\n", This, parameter, literal);

    return E_NOTIMPL;
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_GetLiteral(ID3DXEffectCompiler *iface, D3DXHANDLE parameter, BOOL *literal)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);

    //FIXME("iface %p, parameter %p, literal %p\n", This, parameter, literal);

    return E_NOTIMPL;
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_CompileEffect(ID3DXEffectCompiler *iface, DWORD flags,
        LPD3DXBUFFER *effect, LPD3DXBUFFER *error_msgs)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);

    //FIXME("iface %p, flags %#x, effect %p, error_msgs %p stub\n", This, flags, effect, error_msgs);

    return E_NOTIMPL;
}

static HRESULT WINAPI ID3DXEffectCompilerImpl_CompileShader(ID3DXEffectCompiler *iface, D3DXHANDLE function,
        LPCSTR target, DWORD flags, LPD3DXBUFFER *shader, LPD3DXBUFFER *error_msgs, LPD3DXCONSTANTTABLE *constant_table)
{
    struct ID3DXEffectCompilerImpl *This = impl_from_ID3DXEffectCompiler(iface);

    //FIXME("iface %p, function %p, target %p, flags %#x, shader %p, error_msgs %p, constant_table %p stub\n",
    //        This, function, target, flags, shader, error_msgs, constant_table);

    return E_NOTIMPL;
}
/*
static const struct ID3DXEffectCompilerVtbl ID3DXEffectCompiler_Vtbl =
{
    ID3DXEffectCompilerImpl_QueryInterface,
    ID3DXEffectCompilerImpl_AddRef,
    ID3DXEffectCompilerImpl_Release,
    ID3DXEffectCompilerImpl_GetDesc,
    ID3DXEffectCompilerImpl_GetParameterDesc,
    ID3DXEffectCompilerImpl_GetTechniqueDesc,
    ID3DXEffectCompilerImpl_GetPassDesc,
    ID3DXEffectCompilerImpl_GetFunctionDesc,
    ID3DXEffectCompilerImpl_GetParameter,
    ID3DXEffectCompilerImpl_GetParameterByName,
    ID3DXEffectCompilerImpl_GetParameterBySemantic,
    ID3DXEffectCompilerImpl_GetParameterElement,
    ID3DXEffectCompilerImpl_GetTechnique,
    ID3DXEffectCompilerImpl_GetTechniqueByName,
    ID3DXEffectCompilerImpl_GetPass,
    ID3DXEffectCompilerImpl_GetPassByName,
    ID3DXEffectCompilerImpl_GetFunction,
    ID3DXEffectCompilerImpl_GetFunctionByName,
    ID3DXEffectCompilerImpl_GetAnnotation,
    ID3DXEffectCompilerImpl_GetAnnotationByName,
    ID3DXEffectCompilerImpl_SetValue,
    ID3DXEffectCompilerImpl_GetValue,
    ID3DXEffectCompilerImpl_SetBool,
    ID3DXEffectCompilerImpl_GetBool,
    ID3DXEffectCompilerImpl_SetBoolArray,
    ID3DXEffectCompilerImpl_GetBoolArray,
    ID3DXEffectCompilerImpl_SetInt,
    ID3DXEffectCompilerImpl_GetInt,
    ID3DXEffectCompilerImpl_SetIntArray,
    ID3DXEffectCompilerImpl_GetIntArray,
    ID3DXEffectCompilerImpl_SetFloat,
    ID3DXEffectCompilerImpl_GetFloat,
    ID3DXEffectCompilerImpl_SetFloatArray,
    ID3DXEffectCompilerImpl_GetFloatArray,
    ID3DXEffectCompilerImpl_SetVector,
    ID3DXEffectCompilerImpl_GetVector,
    ID3DXEffectCompilerImpl_SetVectorArray,
    ID3DXEffectCompilerImpl_GetVectorArray,
    ID3DXEffectCompilerImpl_SetMatrix,
    ID3DXEffectCompilerImpl_GetMatrix,
    ID3DXEffectCompilerImpl_SetMatrixArray,
    ID3DXEffectCompilerImpl_GetMatrixArray,
    ID3DXEffectCompilerImpl_SetMatrixPointerArray,
    ID3DXEffectCompilerImpl_GetMatrixPointerArray,
    ID3DXEffectCompilerImpl_SetMatrixTranspose,
    ID3DXEffectCompilerImpl_GetMatrixTranspose,
    ID3DXEffectCompilerImpl_SetMatrixTransposeArray,
    ID3DXEffectCompilerImpl_GetMatrixTransposeArray,
    ID3DXEffectCompilerImpl_SetMatrixTransposePointerArray,
    ID3DXEffectCompilerImpl_GetMatrixTransposePointerArray,
    ID3DXEffectCompilerImpl_SetString,
    ID3DXEffectCompilerImpl_GetString,
    ID3DXEffectCompilerImpl_SetTexture,
    ID3DXEffectCompilerImpl_GetTexture,
    ID3DXEffectCompilerImpl_GetPixelShader,
    ID3DXEffectCompilerImpl_GetVertexShader,
    ID3DXEffectCompilerImpl_SetArrayRange,
    ID3DXEffectCompilerImpl_SetLiteral,
    ID3DXEffectCompilerImpl_GetLiteral,
    ID3DXEffectCompilerImpl_CompileEffect,
    ID3DXEffectCompilerImpl_CompileShader,
};
*/
static HRESULT d3dx9_parse_sampler(struct d3dx_sampler *sampler, const char *data, const char **ptr, D3DXHANDLE *objects)
{
    HRESULT hr;
    UINT i;
    struct d3dx_state *states;

    read_dword(ptr, (DWORD *)&sampler->state_count);
    //TRACE("Count: %u\n", sampler->state_count);

    states = (d3dx_state *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*states) * sampler->state_count);
    if (!states)
    {
        //ERR("Out of memory\n");
        return E_OUTOFMEMORY;
    }

    for (i = 0; i < sampler->state_count; ++i)
    {
        hr = d3dx9_parse_state(&states[i], data, ptr, objects);
        if (hr != D3D_OK)
        {
            //WARN("Failed to parse state\n");
            goto err_out;
        }
    }

    sampler->states = states;

    return D3D_OK;

err_out:

    for (i = 0; i < sampler->state_count; ++i)
    {
        free_state(&states[i]);
    }

    HeapFree(GetProcessHeap(), 0, states);

    return hr;
}

static HRESULT d3dx9_parse_value(struct d3dx_parameter *param, void *value, const char *data, const char **ptr, D3DXHANDLE *objects)
{
    unsigned int i;
    HRESULT hr;
    UINT old_size = 0;
    DWORD id;

    if (param->element_count)
    {
        param->data = value;

        for (i = 0; i < param->element_count; ++i)
        {
            struct d3dx_parameter *member = get_parameter_struct(param->member_handles[i]);

            hr = d3dx9_parse_value(member, value ? (char *)value + old_size : NULL, data, ptr, objects);
            if (hr != D3D_OK)
            {
                //WARN("Failed to parse value\n");
                return hr;
            }

            old_size += member->bytes;
        }

        return D3D_OK;
    }

    switch(param->class2)
    {
        case D3DXPC_SCALAR:
        case D3DXPC_VECTOR:
        case D3DXPC_MATRIX_ROWS:
        case D3DXPC_MATRIX_COLUMNS:
            param->data = value;
            break;

        case D3DXPC_STRUCT:
            param->data = value;

            for (i = 0; i < param->member_count; ++i)
            {
                struct d3dx_parameter *member = get_parameter_struct(param->member_handles[i]);

                hr = d3dx9_parse_value(member, (char *)value + old_size, data, ptr, objects);
                if (hr != D3D_OK)
                {
                    //WARN("Failed to parse value\n");
                    return hr;
                }

                old_size += member->bytes;
            }
            break;

        case D3DXPC_OBJECT:
            switch (param->type)
            {
                case D3DXPT_STRING:
                case D3DXPT_TEXTURE:
                case D3DXPT_TEXTURE1D:
                case D3DXPT_TEXTURE2D:
                case D3DXPT_TEXTURE3D:
                case D3DXPT_TEXTURECUBE:
                case D3DXPT_PIXELSHADER:
                case D3DXPT_VERTEXSHADER:
                    read_dword(ptr, &id);
                    //TRACE("Id: %u\n", id);
                    objects[id] = get_parameter_handle(param);
                    param->data = value;
                    break;

                case D3DXPT_SAMPLER:
                case D3DXPT_SAMPLER1D:
                case D3DXPT_SAMPLER2D:
                case D3DXPT_SAMPLER3D:
                case D3DXPT_SAMPLERCUBE:
                {
                    struct d3dx_sampler *sampler;

                    sampler = (d3dx_sampler *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*sampler));
                    if (!sampler)
                    {
                        //ERR("Out of memory\n");
                        return E_OUTOFMEMORY;
                    }

                    hr = d3dx9_parse_sampler(sampler, data, ptr, objects);
                    if (hr != D3D_OK)
                    {
                        HeapFree(GetProcessHeap(), 0, sampler);
                        //WARN("Failed to parse sampler\n");
                        return hr;
                    }

                    param->data = sampler;
                    break;
                }

                default:
                    //FIXME("Unhandled type %s\n", debug_d3dxparameter_type(param->type));
                    break;
            }
            break;

        default:
            //FIXME("Unhandled class %s\n", debug_d3dxparameter_class(param->class2));
            break;
    }

    return D3D_OK;
}

static HRESULT d3dx9_parse_init_value(struct d3dx_parameter *param, const char *data, const char *ptr, D3DXHANDLE *objects)
{
    UINT size = param->bytes;
    HRESULT hr;
    void *value = NULL;

    //TRACE("param size: %u\n", size);

    if (size)
    {
        value = HeapAlloc(GetProcessHeap(), 0, size);
        if (!value)
        {
            //ERR("Failed to allocate data memory.\n");
            return E_OUTOFMEMORY;
        }

        //TRACE("Data: %s.\n", debugstr_an(ptr, size));
        memcpy(value, ptr, size);
    }

    hr = d3dx9_parse_value(param, value, data, &ptr, objects);
    if (hr != D3D_OK)
    {
        //WARN("Failed to parse value\n");
        HeapFree(GetProcessHeap(), 0, value);
        return hr;
    }

    return D3D_OK;
}

static HRESULT d3dx9_parse_name(char **name, const char *ptr)
{
    DWORD size;

    read_dword(&ptr, &size);
    //TRACE("Name size: %#x\n", size);

    if (!size)
    {
        return D3D_OK;
    }

    *name = (char *)HeapAlloc(GetProcessHeap(), 0, size);
    if (!*name)
    {
        //ERR("Failed to allocate name memory.\n");
        return E_OUTOFMEMORY;
    }

    //TRACE("Name: %s.\n", debugstr_an(ptr, size));
    memcpy(*name, ptr, size);

    return D3D_OK;
}

static HRESULT d3dx9_copy_data(char **str, const char **ptr)
{
    DWORD size;

    read_dword(ptr, &size);
    //TRACE("Data size: %#x\n", size);

    *str = (char *)HeapAlloc(GetProcessHeap(), 0, size);
    if (!*str)
    {
        //ERR("Failed to allocate name memory.\n");
        return E_OUTOFMEMORY;
    }

    //TRACE("Data: %s.\n", debugstr_an(*ptr, size));
    memcpy(*str, *ptr, size);

    *ptr += ((size + 3) & ~3);

    return D3D_OK;
}

static HRESULT d3dx9_parse_data(struct d3dx_parameter *param, const char **ptr, LPDIRECT3DDEVICE9 device)
{
    DWORD size;
    HRESULT hr;

    //TRACE("Parse data for parameter %s, type %s\n", debugstr_a(param->name), debug_d3dxparameter_type(param->type));

    read_dword(ptr, &size);
    //TRACE("Data size: %#x\n", size);

    if (!size)
    {
        //TRACE("Size is 0\n");
        *(void **)param->data = NULL;
        return D3D_OK;
    }

    switch (param->type)
    {
        case D3DXPT_STRING:
            /* re-read with size (sizeof(DWORD) = 4) */
            hr = d3dx9_parse_name((LPSTR *)param->data, *ptr - 4);
            if (hr != D3D_OK)
            {
                //WARN("Failed to parse string data\n");
                return hr;
            }
            break;

        case D3DXPT_VERTEXSHADER:
            hr = IDirect3DDevice9_CreateVertexShader(device, (DWORD *)*ptr, (LPDIRECT3DVERTEXSHADER9 *)param->data);
            if (hr != D3D_OK)
            {
                //WARN("Failed to create vertex shader\n");
                return hr;
            }
            break;

        case D3DXPT_PIXELSHADER:
            hr = IDirect3DDevice9_CreatePixelShader(device, (DWORD *)*ptr, (LPDIRECT3DPIXELSHADER9 *)param->data);
            if (hr != D3D_OK)
            {
                //WARN("Failed to create pixel shader\n");
                return hr;
            }
            break;

        default:
            //FIXME("Unhandled type %s\n", debug_d3dxparameter_type(param->type));
            break;
    }


    *ptr += ((size + 3) & ~3);

    return D3D_OK;
}

static HRESULT d3dx9_parse_effect_typedef(struct d3dx_parameter *param, const char *data, const char **ptr,
        struct d3dx_parameter *parent, UINT flags)
{
    DWORD offset;
    HRESULT hr;
    D3DXHANDLE *member_handles = NULL;
    UINT i;

    param->flags = flags;

    if (!parent)
    {
        read_dword(ptr, (DWORD *)&param->type);
        //TRACE("Type: %s\n", debug_d3dxparameter_type(param->type));

        read_dword(ptr, (DWORD *)&param->class2);
        //TRACE("Class: %s\n", debug_d3dxparameter_class(param->class2));

        read_dword(ptr, &offset);
        //TRACE("Type name offset: %#x\n", offset);
        hr = d3dx9_parse_name(&param->name, data + offset);
        if (hr != D3D_OK)
        {
            //WARN("Failed to parse name\n");
            goto err_out;
        }

        read_dword(ptr, &offset);
        //TRACE("Type semantic offset: %#x\n", offset);
        hr = d3dx9_parse_name(&param->semantic, data + offset);
        if (hr != D3D_OK)
        {
            //WARN("Failed to parse semantic\n");
            goto err_out;
        }

        read_dword(ptr, (DWORD *)&param->element_count);
        //TRACE("Elements: %u\n", param->element_count);

        switch (param->class2)
        {
            case D3DXPC_VECTOR:
                read_dword(ptr, (DWORD *)&param->columns);
                //TRACE("Columns: %u\n", param->columns);

                read_dword(ptr, (DWORD *)&param->rows);
                //TRACE("Rows: %u\n", param->rows);

                /* sizeof(DWORD) * rows * columns */
                param->bytes = 4 * param->rows * param->columns;
                break;

            case D3DXPC_SCALAR:
            case D3DXPC_MATRIX_ROWS:
            case D3DXPC_MATRIX_COLUMNS:
                read_dword(ptr, (DWORD *)&param->rows);
                //TRACE("Rows: %u\n", param->rows);

                read_dword(ptr, (DWORD *)&param->columns);
                //TRACE("Columns: %u\n", param->columns);

                /* sizeof(DWORD) * rows * columns */
                param->bytes = 4 * param->rows * param->columns;
                break;

            case D3DXPC_STRUCT:
                read_dword(ptr, (DWORD *)&param->member_count);
                //TRACE("Members: %u\n", param->member_count);
                break;

            case D3DXPC_OBJECT:
                switch (param->type)
                {
                    case D3DXPT_STRING:
                        param->bytes = sizeof(LPCSTR);
                        break;

                    case D3DXPT_PIXELSHADER:
                        param->bytes = sizeof(LPDIRECT3DPIXELSHADER9);
                        break;

                    case D3DXPT_VERTEXSHADER:
                        param->bytes = sizeof(LPDIRECT3DVERTEXSHADER9);
                        break;

                    case D3DXPT_TEXTURE:
                    case D3DXPT_TEXTURE1D:
                    case D3DXPT_TEXTURE2D:
                    case D3DXPT_TEXTURE3D:
                    case D3DXPT_TEXTURECUBE:
                        param->bytes = sizeof(LPDIRECT3DBASETEXTURE9);
                        break;

                    case D3DXPT_SAMPLER:
                    case D3DXPT_SAMPLER1D:
                    case D3DXPT_SAMPLER2D:
                    case D3DXPT_SAMPLER3D:
                    case D3DXPT_SAMPLERCUBE:
                        param->bytes = 0;
                        break;

                    default:
                        //FIXME("Unhandled type %s\n", debug_d3dxparameter_type(param->type));
                        break;
                }
                break;

            default:
                //FIXME("Unhandled class %s\n", debug_d3dxparameter_class(param->class2));
                break;
        }
    }
    else
    {
        /* elements */
        param->type = parent->type;
        param->class2 = parent->class2;
        param->name = parent->name;
        param->semantic = parent->semantic;
        param->element_count = 0;
        param->annotation_count = 0;
        param->member_count = parent->member_count;
        param->bytes = parent->bytes;
        param->rows = parent->rows;
        param->columns = parent->columns;
    }

    if (param->element_count)
    {
        unsigned int param_bytes = 0;
        const char *save_ptr = *ptr;

        member_handles = (D3DXHANDLE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*member_handles) * param->element_count);
        if (!member_handles)
        {
            //ERR("Out of memory\n");
            hr = E_OUTOFMEMORY;
            goto err_out;
        }

        for (i = 0; i < param->element_count; ++i)
        {
            struct d3dx_parameter *member;
            *ptr = save_ptr;

            member = (d3dx_parameter *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*member));
            if (!member)
            {
                //ERR("Out of memory\n");
                hr = E_OUTOFMEMORY;
                goto err_out;
            }

            member_handles[i] = get_parameter_handle(member);

            hr = d3dx9_parse_effect_typedef(member, data, ptr, param, flags);
            if (hr != D3D_OK)
            {
                //WARN("Failed to parse member\n");
                goto err_out;
            }

            param_bytes += member->bytes;
        }

        param->bytes = param_bytes;
    }
    else if (param->member_count)
    {
        member_handles = (D3DXHANDLE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*member_handles) * param->member_count);
        if (!member_handles)
        {
            //ERR("Out of memory\n");
            hr = E_OUTOFMEMORY;
            goto err_out;
        }

        for (i = 0; i < param->member_count; ++i)
        {
            struct d3dx_parameter *member;

            member = (d3dx_parameter *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*member));
            if (!member)
            {
                //ERR("Out of memory\n");
                hr = E_OUTOFMEMORY;
                goto err_out;
            }

            member_handles[i] = get_parameter_handle(member);

            hr = d3dx9_parse_effect_typedef(member, data, ptr, NULL, flags);
            if (hr != D3D_OK)
            {
                //WARN("Failed to parse member\n");
                goto err_out;
            }

            param->bytes += member->bytes;
        }
    }

    param->member_handles = member_handles;

    return D3D_OK;

err_out:

    if (member_handles)
    {
        unsigned int count;

        if (param->element_count) count = param->element_count;
        else count = param->member_count;

        for (i = 0; i < count; ++i)
        {
            free_parameter(member_handles[i], param->element_count != 0, TRUE);
        }
        HeapFree(GetProcessHeap(), 0, member_handles);
    }

    if (!parent)
    {
        HeapFree(GetProcessHeap(), 0, param->name);
        HeapFree(GetProcessHeap(), 0, param->semantic);
    }
    param->name = NULL;
    param->semantic = NULL;

    return hr;
}

static HRESULT d3dx9_parse_effect_annotation(struct d3dx_parameter *anno, const char *data, const char **ptr, D3DXHANDLE *objects)
{
    DWORD offset;
    const char *ptr2;
    HRESULT hr;

    anno->flags = D3DX_PARAMETER_ANNOTATION;

    read_dword(ptr, &offset);
    //TRACE("Typedef offset: %#x\n", offset);
    ptr2 = data + offset;
    hr = d3dx9_parse_effect_typedef(anno, data, &ptr2, NULL, D3DX_PARAMETER_ANNOTATION);
    if (hr != D3D_OK)
    {
        //WARN("Failed to parse type definition\n");
        return hr;
    }

    read_dword(ptr, &offset);
    //TRACE("Value offset: %#x\n", offset);
    hr = d3dx9_parse_init_value(anno, data, data + offset, objects);
    if (hr != D3D_OK)
    {
        //WARN("Failed to parse value\n");
        return hr;
    }

    return D3D_OK;
}

static HRESULT d3dx9_parse_state(struct d3dx_state *state, const char *data, const char **ptr, D3DXHANDLE *objects)
{
    DWORD offset;
    const char *ptr2;
    HRESULT hr;
    struct d3dx_parameter *parameter;

    parameter = (d3dx_parameter *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*parameter));
    if (!parameter)
    {
        //ERR("Out of memory\n");
        return E_OUTOFMEMORY;
    }

    state->type = ST_CONSTANT;

    read_dword(ptr, (DWORD *)&state->operation);
    //TRACE("Operation: %#x (%s)\n", state->operation, state_table[state->operation].name);

    read_dword(ptr, (DWORD *)&state->index);
    //TRACE("Index: %#x\n", state->index);

    read_dword(ptr, &offset);
    //TRACE("Typedef offset: %#x\n", offset);
    ptr2 = data + offset;
    hr = d3dx9_parse_effect_typedef(parameter, data, &ptr2, NULL, 0);
    if (hr != D3D_OK)
    {
        //WARN("Failed to parse type definition\n");
        goto err_out;
    }

    read_dword(ptr, &offset);
    //TRACE("Value offset: %#x\n", offset);
    hr = d3dx9_parse_init_value(parameter, data, data + offset, objects);
    if (hr != D3D_OK)
    {
        //WARN("Failed to parse value\n");
        goto err_out;
    }

    state->parameter = get_parameter_handle(parameter);

    return D3D_OK;

err_out:

    free_parameter(get_parameter_handle(parameter), FALSE, FALSE);

    return hr;
}

static HRESULT d3dx9_parse_effect_parameter(struct d3dx_parameter *param, const char *data, const char **ptr, D3DXHANDLE *objects)
{
    DWORD offset;
    HRESULT hr;
    unsigned int i;
    D3DXHANDLE *annotation_handles = NULL;
    const char *ptr2;

    read_dword(ptr, &offset);
    //TRACE("Typedef offset: %#x\n", offset);
    ptr2 = data + offset;

    read_dword(ptr, &offset);
    //TRACE("Value offset: %#x\n", offset);

    read_dword(ptr, &param->flags);
    //TRACE("Flags: %#x\n", param->flags);

    read_dword(ptr, (DWORD *)&param->annotation_count);
    //TRACE("Annotation count: %u\n", param->annotation_count);

    hr = d3dx9_parse_effect_typedef(param, data, &ptr2, NULL, param->flags);
    if (hr != D3D_OK)
    {
        //WARN("Failed to parse type definition\n");
        return hr;
    }

    hr = d3dx9_parse_init_value(param, data, data + offset, objects);
    if (hr != D3D_OK)
    {
        //WARN("Failed to parse value\n");
        return hr;
    }

    if (param->annotation_count)
    {
        annotation_handles = (D3DXHANDLE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*annotation_handles) * param->annotation_count);
        if (!annotation_handles)
        {
            //ERR("Out of memory\n");
            hr = E_OUTOFMEMORY;
            goto err_out;
        }

        for (i = 0; i < param->annotation_count; ++i)
        {
            struct d3dx_parameter *annotation;

            annotation = (d3dx_parameter *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*annotation));
            if (!annotation)
            {
                //ERR("Out of memory\n");
                hr = E_OUTOFMEMORY;
                goto err_out;
            }

            annotation_handles[i] = get_parameter_handle(annotation);

            hr = d3dx9_parse_effect_annotation(annotation, data, ptr, objects);
            if (hr != D3D_OK)
            {
                //WARN("Failed to parse annotation\n");
                goto err_out;
            }
        }
    }

    param->annotation_handles = annotation_handles;

    return D3D_OK;

err_out:

    if (annotation_handles)
    {
        for (i = 0; i < param->annotation_count; ++i)
        {
            free_parameter(annotation_handles[i], FALSE, FALSE);
        }
        HeapFree(GetProcessHeap(), 0, annotation_handles);
    }

    return hr;
}

static HRESULT d3dx9_parse_effect_pass(struct d3dx_pass *pass, const char *data, const char **ptr, D3DXHANDLE *objects)
{
    DWORD offset;
    HRESULT hr;
    unsigned int i;
    D3DXHANDLE *annotation_handles = NULL;
    struct d3dx_state *states = NULL;
    char *name = NULL;

    read_dword(ptr, &offset);
    //TRACE("Pass name offset: %#x\n", offset);
    hr = d3dx9_parse_name(&name, data + offset);
    if (hr != D3D_OK)
    {
        //WARN("Failed to parse name\n");
        goto err_out;
    }

    read_dword(ptr, (DWORD *)&pass->annotation_count);
    //TRACE("Annotation count: %u\n", pass->annotation_count);

    read_dword(ptr, (DWORD *)&pass->state_count);
    //TRACE("State count: %u\n", pass->state_count);

    if (pass->annotation_count)
    {
        annotation_handles = (D3DXHANDLE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*annotation_handles) * pass->annotation_count);
        if (!annotation_handles)
        {
            //ERR("Out of memory\n");
            hr = E_OUTOFMEMORY;
            goto err_out;
        }

        for (i = 0; i < pass->annotation_count; ++i)
        {
            struct d3dx_parameter *annotation;

            annotation = (d3dx_parameter *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*annotation));
            if (!annotation)
            {
                //ERR("Out of memory\n");
                hr = E_OUTOFMEMORY;
                goto err_out;
            }

            annotation_handles[i] = get_parameter_handle(annotation);

            hr = d3dx9_parse_effect_annotation(annotation, data, ptr, objects);
            if (hr != D3D_OK)
            {
                //WARN("Failed to parse annotations\n");
                goto err_out;
            }
        }
    }

    if (pass->state_count)
    {
        states = (d3dx_state *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*states) * pass->state_count);
        if (!states)
        {
            //ERR("Out of memory\n");
            hr = E_OUTOFMEMORY;
            goto err_out;
        }

        for (i = 0; i < pass->state_count; ++i)
        {
            hr = d3dx9_parse_state(&states[i], data, ptr, objects);
            if (hr != D3D_OK)
            {
                //WARN("Failed to parse annotations\n");
                goto err_out;
            }
        }
    }

    pass->name = name;
    pass->annotation_handles = annotation_handles;
    pass->states = states;

    return D3D_OK;

err_out:

    if (annotation_handles)
    {
        for (i = 0; i < pass->annotation_count; ++i)
        {
            free_parameter(annotation_handles[i], FALSE, FALSE);
        }
        HeapFree(GetProcessHeap(), 0, annotation_handles);
    }

    if (states)
    {
        for (i = 0; i < pass->state_count; ++i)
        {
            free_state(&states[i]);
        }
        HeapFree(GetProcessHeap(), 0, states);
    }

    HeapFree(GetProcessHeap(), 0, name);

    return hr;
}

static HRESULT d3dx9_parse_effect_technique(struct d3dx_technique *technique, const char *data, const char **ptr, D3DXHANDLE *objects)
{
    DWORD offset;
    HRESULT hr;
    unsigned int i;
    D3DXHANDLE *annotation_handles = NULL;
    D3DXHANDLE *pass_handles = NULL;
    char *name = NULL;

    read_dword(ptr, &offset);
    //TRACE("Technique name offset: %#x\n", offset);
    hr = d3dx9_parse_name(&name, data + offset);
    if (hr != D3D_OK)
    {
        //WARN("Failed to parse name\n");
        goto err_out;
    }

    read_dword(ptr, (DWORD*)&technique->annotation_count);
    //TRACE("Annotation count: %u\n", technique->annotation_count);

    read_dword(ptr, (DWORD*)&technique->pass_count);
    //TRACE("Pass count: %u\n", technique->pass_count);

    if (technique->annotation_count)
    {
        annotation_handles = (D3DXHANDLE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*annotation_handles) * technique->annotation_count);
        if (!annotation_handles)
        {
            //ERR("Out of memory\n");
            hr = E_OUTOFMEMORY;
            goto err_out;
        }

        for (i = 0; i < technique->annotation_count; ++i)
        {
            struct d3dx_parameter *annotation;

            annotation = (d3dx_parameter *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*annotation));
            if (!annotation)
            {
                //ERR("Out of memory\n");
                hr = E_OUTOFMEMORY;
                goto err_out;
            }

            annotation_handles[i] = get_parameter_handle(annotation);

            hr = d3dx9_parse_effect_annotation(annotation, data, ptr, objects);
            if (hr != D3D_OK)
            {
                //WARN("Failed to parse annotations\n");
                goto err_out;
            }
        }
    }

    if (technique->pass_count)
    {
        pass_handles = (D3DXHANDLE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*pass_handles) * technique->pass_count);
        if (!pass_handles)
        {
            //ERR("Out of memory\n");
            hr = E_OUTOFMEMORY;
            goto err_out;
        }

        for (i = 0; i < technique->pass_count; ++i)
        {
            struct d3dx_pass *pass;

            pass = (d3dx_pass *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*pass));
            if (!pass)
            {
                //ERR("Out of memory\n");
                hr = E_OUTOFMEMORY;
                goto err_out;
            }

            pass_handles[i] = get_pass_handle(pass);

            hr = d3dx9_parse_effect_pass(pass, data, ptr, objects);
            if (hr != D3D_OK)
            {
                //WARN("Failed to parse passes\n");
                goto err_out;
            }
        }
    }

    technique->name = name;
    technique->pass_handles = pass_handles;
    technique->annotation_handles = annotation_handles;

    return D3D_OK;

err_out:

    if (pass_handles)
    {
        for (i = 0; i < technique->pass_count; ++i)
        {
            free_pass(pass_handles[i]);
        }
        HeapFree(GetProcessHeap(), 0, pass_handles);
    }

    if (annotation_handles)
    {
        for (i = 0; i < technique->annotation_count; ++i)
        {
            free_parameter(annotation_handles[i], FALSE, FALSE);
        }
        HeapFree(GetProcessHeap(), 0, annotation_handles);
    }

    HeapFree(GetProcessHeap(), 0, name);

    return hr;
}

static HRESULT d3dx9_parse_resource(struct ID3DXBaseEffectImpl *base, const char *data, const char **ptr)
{
    DWORD technique_index;
    DWORD index, state_index, usage, element_index;
    struct d3dx_state *state;
    struct d3dx_parameter *param;
    HRESULT hr = E_FAIL;

    read_dword(ptr, &technique_index);
    //TRACE("techn: %u\n", technique_index);

    read_dword(ptr, &index);
    //TRACE("index: %u\n", index);

    read_dword(ptr, &element_index);
    //TRACE("element_index: %u\n", element_index);

    read_dword(ptr, &state_index);
    //TRACE("state_index: %u\n", state_index);

    read_dword(ptr, &usage);
    //TRACE("usage: %u\n", usage);

    if (technique_index == 0xffffffff)
    {
        struct d3dx_parameter *parameter;
        struct d3dx_sampler *sampler;

        if (index >= base->parameter_count)
        {
            //FIXME("Index out of bounds: index %u >= parameter_count %u\n", index, base->parameter_count);
            return E_FAIL;
        }

        parameter = get_parameter_struct(base->parameter_handles[index]);
        if (element_index != 0xffffffff)
        {
            if (element_index >= parameter->element_count && parameter->element_count != 0)
            {
                //FIXME("Index out of bounds: element_index %u >= element_count %u\n", element_index, parameter->element_count);
                return E_FAIL;
            }

            if (parameter->element_count != 0) parameter = get_parameter_struct(parameter->member_handles[element_index]);
        }

        sampler = (d3dx_sampler *)parameter->data;
        if (state_index >= sampler->state_count)
        {
            //FIXME("Index out of bounds: state_index %u >= state_count %u\n", state_index, sampler->state_count);
            return E_FAIL;
        }

        state = &sampler->states[state_index];
    }
    else
    {
        struct d3dx_technique *technique;
        struct d3dx_pass *pass;

        if (technique_index >= base->technique_count)
        {
            //FIXME("Index out of bounds: technique_index %u >= technique_count %u\n", technique_index, base->technique_count);
            return E_FAIL;
        }

        technique = get_technique_struct(base->technique_handles[technique_index]);
        if (index >= technique->pass_count)
        {
            //FIXME("Index out of bounds: index %u >= pass_count %u\n", index, technique->pass_count);
            return E_FAIL;
        }

        pass = get_pass_struct(technique->pass_handles[index]);
        if (state_index >= pass->state_count)
        {
            //FIXME("Index out of bounds: state_index %u >= state_count %u\n", state_index, pass->state_count);
            return E_FAIL;
        }

        state = &pass->states[state_index];
    }

    param = get_parameter_struct(state->parameter);

    switch (usage)
    {
        case 0:
            //TRACE("usage 0: type %s\n", debug_d3dxparameter_type(param->type));
            switch (param->type)
            {
                case D3DXPT_VERTEXSHADER:
                case D3DXPT_PIXELSHADER:
                    state->type = ST_CONSTANT;
                    hr = d3dx9_parse_data(param, ptr, base->effect->device);
                    break;

                case D3DXPT_BOOL:
                case D3DXPT_INT:
                case D3DXPT_FLOAT:
                case D3DXPT_STRING:
                    state->type = ST_FXLC;
                    hr = d3dx9_copy_data((char **)param->data, ptr);
                    break;

                default:
                    //FIXME("Unhandled type %s\n", debug_d3dxparameter_type(param->type));
                    break;
            }
            break;

        case 1:
            state->type = ST_PARAMETER;
            hr = d3dx9_copy_data((char **)param->data, ptr);
            if (hr == D3D_OK)
            {
                //TRACE("Mapping to parameter %s\n", *(char **)param->data);
            }
            break;

        default:
            //FIXME("Unknown usage %x\n", usage);
            break;
    }

    return hr;
}

static HRESULT d3dx9_parse_effect(struct ID3DXBaseEffectImpl *base, const char *data, UINT data_size, DWORD start)
{
    const char *ptr = data + start;
    D3DXHANDLE *parameter_handles = NULL;
    D3DXHANDLE *technique_handles = NULL;
    D3DXHANDLE *objects = NULL;
    UINT stringcount, objectcount, resourcecount;
    HRESULT hr;
    UINT i;

    read_dword(&ptr, (DWORD *)&base->parameter_count);
    //TRACE("Parameter count: %u\n", base->parameter_count);

    read_dword(&ptr, (DWORD *)&base->technique_count);
    //TRACE("Technique count: %u\n", base->technique_count);

    skip_dword_unknown(&ptr, 1);

    read_dword(&ptr, (DWORD *)&objectcount);
    //TRACE("Object count: %u\n", objectcount);

    objects = (D3DXHANDLE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*objects) * objectcount);
    if (!objects)
    {
        //ERR("Out of memory\n");
        hr = E_OUTOFMEMORY;
        goto err_out;
    }

    if (base->parameter_count)
    {
        parameter_handles = (D3DXHANDLE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*parameter_handles) * base->parameter_count);
        if (!parameter_handles)
        {
            //ERR("Out of memory\n");
            hr = E_OUTOFMEMORY;
            goto err_out;
        }

        for (i = 0; i < base->parameter_count; ++i)
        {
            struct d3dx_parameter *parameter;

            parameter = (d3dx_parameter *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*parameter));
            if (!parameter)
            {
                //ERR("Out of memory\n");
                hr = E_OUTOFMEMORY;
                goto err_out;
            }

            parameter_handles[i] = get_parameter_handle(parameter);

            hr = d3dx9_parse_effect_parameter(parameter, data, &ptr, objects);
            if (hr != D3D_OK)
            {
                //WARN("Failed to parse parameter\n");
                goto err_out;
            }
        }
    }

    if (base->technique_count)
    {
        technique_handles = (D3DXHANDLE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*technique_handles) * base->technique_count);
        if (!technique_handles)
        {
            //ERR("Out of memory\n");
            hr = E_OUTOFMEMORY;
            goto err_out;
        }

        for (i = 0; i < base->technique_count; ++i)
        {
            struct d3dx_technique *technique;

            technique = (d3dx_technique *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*technique));
            if (!technique)
            {
                //ERR("Out of memory\n");
                hr = E_OUTOFMEMORY;
                goto err_out;
            }

            technique_handles[i] = get_technique_handle(technique);

            hr = d3dx9_parse_effect_technique(technique, data, &ptr, objects);
            if (hr != D3D_OK)
            {
                //WARN("Failed to parse technique\n");
                goto err_out;
            }
        }
    }

    /* needed for further parsing */
    base->technique_handles = technique_handles;
    base->parameter_handles = parameter_handles;

    read_dword(&ptr, (DWORD *)&stringcount);
    //TRACE("String count: %u\n", stringcount);

    read_dword(&ptr, (DWORD *)&resourcecount);
    //TRACE("Resource count: %u\n", resourcecount);

    for (i = 0; i < stringcount; ++i)
    {
        DWORD id;
        struct d3dx_parameter *param;

        read_dword(&ptr, &id);
        //TRACE("Id: %u\n", id);

        param = get_parameter_struct(objects[id]);

        hr = d3dx9_parse_data(param, &ptr, base->effect->device);
        if (hr != D3D_OK)
        {
            //WARN("Failed to parse data\n");
            goto err_out;
        }
    }

    for (i = 0; i < resourcecount; ++i)
    {
        //TRACE("parse resource %u\n", i);

        hr = d3dx9_parse_resource(base, data, &ptr);
        if (hr != D3D_OK)
        {
            //WARN("Failed to parse data\n");
            goto err_out;
        }
    }

    HeapFree(GetProcessHeap(), 0, objects);

    return D3D_OK;

err_out:

    if (technique_handles)
    {
        for (i = 0; i < base->technique_count; ++i)
        {
            free_technique(technique_handles[i]);
        }
        HeapFree(GetProcessHeap(), 0, technique_handles);
    }

    if (parameter_handles)
    {
        for (i = 0; i < base->parameter_count; ++i)
        {
            free_parameter(parameter_handles[i], FALSE, FALSE);
        }
        HeapFree(GetProcessHeap(), 0, parameter_handles);
    }

    base->technique_handles = NULL;
    base->parameter_handles = NULL;

    HeapFree(GetProcessHeap(), 0, objects);

    return hr;
}

static HRESULT d3dx9_base_effect_init(struct ID3DXBaseEffectImpl *base,
        const char *data, SIZE_T data_size, struct ID3DXEffectImpl *effect)
{
    DWORD tag, offset;
    const char *ptr = data;
    HRESULT hr;

    //TRACE("base %p, data %p, data_size %lu, effect %p\n", base, data, data_size, effect);

    //base->ID3DXBaseEffect_iface.lpVtbl = &ID3DXBaseEffect_Vtbl; // dunno
    base->ref = 1;
    base->effect = effect;

    read_dword(&ptr, &tag);
    //TRACE("Tag: %x\n", tag);

    if (tag != d3dx9_effect_version(9, 1))
    {
        /* todo: compile hlsl ascii code */
        //FIXME("HLSL ascii effects not supported, yet\n");

        /* Show the start of the shader for debugging info. */
        //TRACE("effect:\n%s\n", debugstr_an(data, data_size > 40 ? 40 : data_size));
    }
    else
    {
        read_dword(&ptr, &offset);
        //TRACE("Offset: %x\n", offset);

        hr = d3dx9_parse_effect(base, ptr, data_size, offset);
        if (hr != D3D_OK)
        {
            //FIXME("Failed to parse effect.\n");
            return hr;
        }
    }

    return D3D_OK;
}

static HRESULT d3dx9_effect_init(struct ID3DXEffectImpl *effect, LPDIRECT3DDEVICE9 device,
        const char *data, SIZE_T data_size, LPD3DXEFFECTPOOL pool)
{
    HRESULT hr;
    struct ID3DXBaseEffectImpl *object = NULL;

    //TRACE("effect %p, device %p, data %p, data_size %lu, pool %p\n", effect, device, data, data_size, pool);

    //effect->ID3DXEffect_iface.lpVtbl = &ID3DXEffect_Vtbl; //dunno
    effect->ref = 1;

    if (pool) {
	    //pool->lpVtbl->AddRef(pool);
		pool->AddRef();
	}
    effect->pool = pool;

    IDirect3DDevice9_AddRef(device);
    effect->device = device;

    object = (ID3DXBaseEffectImpl *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));
    if (!object)
    {
        //ERR("Out of memory\n");
        hr = E_OUTOFMEMORY;
        goto err_out;
    }

    hr = d3dx9_base_effect_init(object, data, data_size, effect);
    if (hr != D3D_OK)
    {
        //FIXME("Failed to parse effect.\n");
        goto err_out;
    }

    effect->base_effect = (ID3DXBaseEffect *)&object->ID3DXBaseEffect_iface;

    /* initialize defaults - check because of unsupported ascii effects */
    if (object->technique_handles)
    {
        effect->active_technique = object->technique_handles[0];
        effect->active_pass = NULL;
    }

    return D3D_OK;

err_out:

    HeapFree(GetProcessHeap(), 0, object);
    free_effect(effect);

    return hr;
}

HRESULT WINAPI D3DXCreateEffectEx(LPDIRECT3DDEVICE9 device,
                                  LPCVOID srcdata,
                                  UINT srcdatalen,
                                  CONST D3DXMACRO* defines,
                                  LPD3DXINCLUDE include,
                                  LPCSTR skip_constants,
                                  DWORD flags,
                                  LPD3DXEFFECTPOOL pool,
                                  LPD3DXEFFECT* effect,
                                  LPD3DXBUFFER* compilation_errors)
{
    struct ID3DXEffectImpl *object;
    HRESULT hr;

    //FIXME("(%p, %p, %u, %p, %p, %p, %#x, %p, %p, %p): semi-stub\n", device, srcdata, srcdatalen, defines, include,
    //    skip_constants, flags, pool, effect, compilation_errors);

    if (!device || !srcdata)
        return D3DERR_INVALIDCALL;

    if (!srcdatalen)
        return E_FAIL;

    /* Native dll allows effect to be null so just return D3D_OK after doing basic checks */
    if (!effect)
        return D3D_OK;

    object = (ID3DXEffectImpl *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));
    if (!object)
    {
        //ERR("Out of memory\n");
        return E_OUTOFMEMORY;
    }

    hr = d3dx9_effect_init(object, device, (const char *)srcdata, srcdatalen, pool);
    if (FAILED(hr))
    {
        //WARN("Failed to initialize shader reflection\n");
        HeapFree(GetProcessHeap(), 0, object);
        return hr;
    }

    *effect = (LPD3DXEFFECT)&object->ID3DXEffect_iface;

    //TRACE("Created ID3DXEffect %p\n", object);

    return D3D_OK;
}

static HRESULT d3dx9_effect_compiler_init(struct ID3DXEffectCompilerImpl *compiler, const char *data, SIZE_T data_size)
{
    HRESULT hr;
    struct ID3DXBaseEffectImpl *object = NULL;

    //TRACE("effect %p, data %p, data_size %lu\n", compiler, data, data_size);

    //compiler->ID3DXEffectCompiler_iface.lpVtbl = &ID3DXEffectCompiler_Vtbl; // dunno
    compiler->ref = 1;

    object = (ID3DXBaseEffectImpl *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));
    if (!object)
    {
        //ERR("Out of memory\n");
        hr = E_OUTOFMEMORY;
        goto err_out;
    }

    hr = d3dx9_base_effect_init(object, data, data_size, NULL);
    if (hr != D3D_OK)
    {
        //FIXME("Failed to parse effect.\n");
        goto err_out;
    }

    compiler->base_effect = (ID3DXBaseEffect *)&object->ID3DXBaseEffect_iface;

    return D3D_OK;

err_out:

    HeapFree(GetProcessHeap(), 0, object);
    free_effect_compiler(compiler);

    return hr;
}

HRESULT WINAPI D3DXCreateEffectCompiler(LPCSTR srcdata,
                                        UINT srcdatalen,
                                        CONST D3DXMACRO *defines,
                                        LPD3DXINCLUDE include,
                                        DWORD flags,
                                        LPD3DXEFFECTCOMPILER *compiler,
                                        LPD3DXBUFFER *parse_errors)
{
    struct ID3DXEffectCompilerImpl *object;
    HRESULT hr;

    //TRACE("srcdata %p, srcdatalen %u, defines %p, include %p, flags %#x, compiler %p, parse_errors %p\n",
    //        srcdata, srcdatalen, defines, include, flags, compiler, parse_errors);

    if (!srcdata || !compiler)
    {
        //WARN("Invalid arguments supplied\n");
        return D3DERR_INVALIDCALL;
    }

    object = (ID3DXEffectCompilerImpl *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));
    if (!object)
    {
        //ERR("Out of memory\n");
        return E_OUTOFMEMORY;
    }

    hr = d3dx9_effect_compiler_init(object, srcdata, srcdatalen);
    if (FAILED(hr))
    {
        //WARN("Failed to initialize effect compiler\n");
        HeapFree(GetProcessHeap(), 0, object);
        return hr;
    }

    *compiler = (LPD3DXEFFECTCOMPILER)&object->ID3DXEffectCompiler_iface;

    //TRACE("Created ID3DXEffectCompiler %p\n", object);

    return D3D_OK;
}

struct ID3DXEffectPoolImpl
{
    ID3DXEffectPool * ID3DXEffectPool_iface;
    LONG ref;
};

static inline struct ID3DXEffectPoolImpl *impl_from_ID3DXEffectPool(ID3DXEffectPool *iface)
{
    return CONTAINING_RECORD(iface, struct ID3DXEffectPoolImpl, ID3DXEffectPool_iface);
}

/*** IUnknown methods ***/
static HRESULT WINAPI ID3DXEffectPoolImpl_QueryInterface(ID3DXEffectPool *iface, REFIID riid, void **object)
{
    //TRACE("(%p)->(%s, %p)\n", iface, debugstr_guid(riid), object);

    if (IsEqualGUID(riid, (const GUID &)IID_IUnknown) ||
        IsEqualGUID(riid, (const GUID &)IID_ID3DXEffectPool))
    {
        //iface->lpVtbl->AddRef(iface);
		iface->AddRef();
        *object = iface;
        return S_OK;
    }

    //WARN("Interface %s not found\n", debugstr_guid(riid));

    return E_NOINTERFACE;
}

static ULONG WINAPI ID3DXEffectPoolImpl_AddRef(ID3DXEffectPool *iface)
{
    struct ID3DXEffectPoolImpl *This = impl_from_ID3DXEffectPool(iface);

    //TRACE("(%p)->(): AddRef from %u\n", This, This->ref);

    return InterlockedIncrement(&This->ref);
}

static ULONG WINAPI ID3DXEffectPoolImpl_Release(ID3DXEffectPool *iface)
{
    struct ID3DXEffectPoolImpl *This = impl_from_ID3DXEffectPool(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    //TRACE("(%p)->(): Release from %u\n", This, ref + 1);

    if (!ref)
        HeapFree(GetProcessHeap(), 0, This);

    return ref;
}

/*
static const struct ID3DXEffectPoolVtbl ID3DXEffectPool_Vtbl =
{
    ID3DXEffectPoolImpl_QueryInterface,
    ID3DXEffectPoolImpl_AddRef,
    ID3DXEffectPoolImpl_Release
};
*/
HRESULT WINAPI D3DXCreateEffectPool(LPD3DXEFFECTPOOL *pool)
{
    struct ID3DXEffectPoolImpl *object;

    //TRACE("(%p)\n", pool);

    if (!pool)
        return D3DERR_INVALIDCALL;

    object = (ID3DXEffectPoolImpl *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));
    if (!object)
    {
        //ERR("Out of memory\n");
        return E_OUTOFMEMORY;
    }

    // object->ID3DXEffectPool_iface.lpVtbl = &ID3DXEffectPool_Vtbl; // dunno
    object->ref = 1;

    *pool = (LPD3DXEFFECTPOOL)&object->ID3DXEffectPool_iface;

    return S_OK;
}

HRESULT WINAPI D3DXCreateEffectFromFileExW(LPDIRECT3DDEVICE9 device, LPCWSTR srcfile,
    const D3DXMACRO *defines, LPD3DXINCLUDE include, LPCSTR skipconstants, DWORD flags,
    LPD3DXEFFECTPOOL pool, LPD3DXEFFECT *effect, LPD3DXBUFFER *compilationerrors)
{
    LPVOID buffer;
    HRESULT ret;
    DWORD size;

    //TRACE("(%s): relay\n", debugstr_w(srcfile));

    if (!device || !srcfile)
        return D3DERR_INVALIDCALL;

    ret = map_view_of_file(srcfile, &buffer, &size);

    if (FAILED(ret))
        return D3DXERR_INVALIDDATA;

    ret = D3DXCreateEffectEx(device, buffer, size, defines, include, skipconstants, flags, pool, effect, compilationerrors);
    UnmapViewOfFile(buffer);

    return ret;
}

HRESULT WINAPI D3DXCreateEffectFromFileExA(LPDIRECT3DDEVICE9 device, LPCSTR srcfile,
    const D3DXMACRO *defines, LPD3DXINCLUDE include, LPCSTR skipconstants, DWORD flags,
    LPD3DXEFFECTPOOL pool, LPD3DXEFFECT *effect, LPD3DXBUFFER *compilationerrors)
{
    LPWSTR srcfileW;
    HRESULT ret;
    DWORD len;

    //TRACE("(void): relay\n");

    if (!srcfile)
        return D3DERR_INVALIDCALL;

    len = MultiByteToWideChar(CP_ACP, 0, srcfile, -1, NULL, 0);
    srcfileW = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, len * sizeof(*srcfileW));
    MultiByteToWideChar(CP_ACP, 0, srcfile, -1, srcfileW, len);

    ret = D3DXCreateEffectFromFileExW(device, srcfileW, defines, include, skipconstants, flags, pool, effect, compilationerrors);
    HeapFree(GetProcessHeap(), 0, srcfileW);

    return ret;
}

HRESULT WINAPI D3DXCreateEffectFromFileW(LPDIRECT3DDEVICE9 device, LPCWSTR srcfile,
    const D3DXMACRO *defines, LPD3DXINCLUDE include, DWORD flags, LPD3DXEFFECTPOOL pool,
    LPD3DXEFFECT *effect, LPD3DXBUFFER *compilationerrors)
{
    //TRACE("(void): relay\n");
    return D3DXCreateEffectFromFileExW(device, srcfile, defines, include, NULL, flags, pool, effect, compilationerrors);
}

HRESULT WINAPI D3DXCreateEffectFromFileA(LPDIRECT3DDEVICE9 device, LPCSTR srcfile,
    const D3DXMACRO *defines, LPD3DXINCLUDE include, DWORD flags, LPD3DXEFFECTPOOL pool,
    LPD3DXEFFECT *effect, LPD3DXBUFFER *compilationerrors)
{
    //TRACE("(void): relay\n");
    return D3DXCreateEffectFromFileExA(device, srcfile, defines, include, NULL, flags, pool, effect, compilationerrors);
}

HRESULT WINAPI D3DXCreateEffectFromResourceExW(LPDIRECT3DDEVICE9 device, HMODULE srcmodule, LPCWSTR srcresource,
    const D3DXMACRO *defines, LPD3DXINCLUDE include, LPCSTR skipconstants, DWORD flags,
    LPD3DXEFFECTPOOL pool, LPD3DXEFFECT *effect, LPD3DXBUFFER *compilationerrors)
{
    HRSRC resinfo;

    //TRACE("(%p, %s): relay\n", srcmodule, debugstr_w(srcresource));

    if (!device)
        return D3DERR_INVALIDCALL;

    resinfo = FindResourceW(srcmodule, srcresource, (LPCWSTR) RT_RCDATA);

    if (resinfo)
    {
        LPVOID buffer;
        HRESULT ret;
        DWORD size;

        ret = load_resource_into_memory(srcmodule, resinfo, &buffer, &size);

        if (FAILED(ret))
            return D3DXERR_INVALIDDATA;

        return D3DXCreateEffectEx(device, buffer, size, defines, include, skipconstants, flags, pool, effect, compilationerrors);
    }

    return D3DXERR_INVALIDDATA;
}

HRESULT WINAPI D3DXCreateEffectFromResourceExA(LPDIRECT3DDEVICE9 device, HMODULE srcmodule, LPCSTR srcresource,
    const D3DXMACRO *defines, LPD3DXINCLUDE include, LPCSTR skipconstants, DWORD flags,
    LPD3DXEFFECTPOOL pool, LPD3DXEFFECT *effect, LPD3DXBUFFER *compilationerrors)
{
    HRSRC resinfo;

    //TRACE("(%p, %s): relay\n", srcmodule, debugstr_a(srcresource));

    if (!device)
        return D3DERR_INVALIDCALL;

    resinfo = FindResourceA(srcmodule, srcresource, (LPCSTR) RT_RCDATA);

    if (resinfo)
    {
        LPVOID buffer;
        HRESULT ret;
        DWORD size;

        ret = load_resource_into_memory(srcmodule, resinfo, &buffer, &size);

        if (FAILED(ret))
            return D3DXERR_INVALIDDATA;

        return D3DXCreateEffectEx(device, buffer, size, defines, include, skipconstants, flags, pool, effect, compilationerrors);
    }

    return D3DXERR_INVALIDDATA;
}

HRESULT WINAPI D3DXCreateEffectFromResourceW(LPDIRECT3DDEVICE9 device, HMODULE srcmodule, LPCWSTR srcresource,
    const D3DXMACRO *defines, LPD3DXINCLUDE include, DWORD flags, LPD3DXEFFECTPOOL pool,
    LPD3DXEFFECT *effect, LPD3DXBUFFER *compilationerrors)
{
    //TRACE("(void): relay\n");
    return D3DXCreateEffectFromResourceExW(device, srcmodule, srcresource, defines, include, NULL, flags, pool, effect, compilationerrors);
}

HRESULT WINAPI D3DXCreateEffectFromResourceA(LPDIRECT3DDEVICE9 device, HMODULE srcmodule, LPCSTR srcresource,
    const D3DXMACRO *defines, LPD3DXINCLUDE include, DWORD flags, LPD3DXEFFECTPOOL pool,
    LPD3DXEFFECT *effect, LPD3DXBUFFER *compilationerrors)
{
    //TRACE("(void): relay\n");
    return D3DXCreateEffectFromResourceExA(device, srcmodule, srcresource, defines, include, NULL, flags, pool, effect, compilationerrors);
}

HRESULT WINAPI D3DXCreateEffectCompilerFromFileW(LPCWSTR srcfile, const D3DXMACRO *defines, LPD3DXINCLUDE include,
    DWORD flags, LPD3DXEFFECTCOMPILER *effectcompiler, LPD3DXBUFFER *parseerrors)
{
    LPVOID buffer;
    HRESULT ret;
    DWORD size;

    //TRACE("(%s): relay\n", debugstr_w(srcfile));

    if (!srcfile)
        return D3DERR_INVALIDCALL;

    ret = map_view_of_file(srcfile, &buffer, &size);

    if (FAILED(ret))
        return D3DXERR_INVALIDDATA;

    ret = D3DXCreateEffectCompiler((LPCSTR)buffer, size, defines, include, flags, effectcompiler, parseerrors);
    UnmapViewOfFile(buffer);

    return ret;
}

HRESULT WINAPI D3DXCreateEffectCompilerFromFileA(LPCSTR srcfile, const D3DXMACRO *defines, LPD3DXINCLUDE include,
    DWORD flags, LPD3DXEFFECTCOMPILER *effectcompiler, LPD3DXBUFFER *parseerrors)
{
    LPWSTR srcfileW;
    HRESULT ret;
    DWORD len;

    //TRACE("(void): relay\n");

    if (!srcfile)
        return D3DERR_INVALIDCALL;

    len = MultiByteToWideChar(CP_ACP, 0, srcfile, -1, NULL, 0);
    srcfileW = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, len * sizeof(*srcfileW));
    MultiByteToWideChar(CP_ACP, 0, srcfile, -1, srcfileW, len);

    ret = D3DXCreateEffectCompilerFromFileW(srcfileW, defines, include, flags, effectcompiler, parseerrors);
    HeapFree(GetProcessHeap(), 0, srcfileW);

    return ret;
}

HRESULT WINAPI D3DXCreateEffectCompilerFromResourceA(HMODULE srcmodule, LPCSTR srcresource, const D3DXMACRO *defines,
    LPD3DXINCLUDE include, DWORD flags, LPD3DXEFFECTCOMPILER *effectcompiler, LPD3DXBUFFER *parseerrors)
{
    HRSRC resinfo;

    //TRACE("(%p, %s): relay\n", srcmodule, debugstr_a(srcresource));

    resinfo = FindResourceA(srcmodule, srcresource, (LPCSTR) RT_RCDATA);

    if (resinfo)
    {
        LPVOID buffer;
        HRESULT ret;
        DWORD size;

        ret = load_resource_into_memory(srcmodule, resinfo, &buffer, &size);

        if (FAILED(ret))
            return D3DXERR_INVALIDDATA;

        return D3DXCreateEffectCompiler((LPCSTR)buffer, size, defines, include, flags, effectcompiler, parseerrors);
    }

    return D3DXERR_INVALIDDATA;
}

HRESULT WINAPI D3DXCreateEffectCompilerFromResourceW(HMODULE srcmodule, LPCWSTR srcresource, const D3DXMACRO *defines,
    LPD3DXINCLUDE include, DWORD flags, LPD3DXEFFECTCOMPILER *effectcompiler, LPD3DXBUFFER *parseerrors)
{
    HRSRC resinfo;

    //TRACE("(%p, %s): relay\n", srcmodule, debugstr_w(srcresource));

    resinfo = FindResourceW(srcmodule, srcresource, (LPCWSTR) RT_RCDATA);

    if (resinfo)
    {
        LPVOID buffer;
        HRESULT ret;
        DWORD size;

        ret = load_resource_into_memory(srcmodule, resinfo, &buffer, &size);

        if (FAILED(ret))
            return D3DXERR_INVALIDDATA;

        return D3DXCreateEffectCompiler((LPCSTR)buffer, size, defines, include, flags, effectcompiler, parseerrors);
    }

    return D3DXERR_INVALIDDATA;
}













/* Returns TRUE if num is a power of 2, FALSE if not, or if 0 */

static BOOL is_pow2(UINT num)
{
    return !(num & (num - 1));
}

/* Returns the smallest power of 2 which is greater than or equal to num */
static UINT make_pow2(UINT num)
{
    UINT result = 1;

    /* In the unlikely event somebody passes a large value, make sure we don't enter an infinite loop */
    if (num >= 0x80000000)
        return 0x80000000;

    while (result < num)
        result <<= 1;

    return result;
}

// texture.c
/*
static HRESULT get_surface(D3DRESOURCETYPE type, LPDIRECT3DBASETEXTURE9 tex,
                           int face, UINT level, LPDIRECT3DSURFACE9 *surf)
{
    switch (type)
    {
        case D3DRTYPE_TEXTURE:
            return IDirect3DTexture9_GetSurfaceLevel((IDirect3DTexture9*) tex, level, surf);
        case D3DRTYPE_CUBETEXTURE:
            //return IDirect3DCubeTexture9_GetCubeMapSurface((IDirect3DCubeTexture9*) tex, face, level, surf);
            return IDirect3DCubeTexture9_GetCubeMapSurface(((IDirect3DCubeTexture9*) tex, face, level, surf);
        default:
            //ERR("Unexpected texture type\n");
            return E_NOTIMPL;
    }
}
*/
static void la_from_rgba(const struct vec4 *rgba, struct vec4 *la)
{
    la->x = rgba->x * 0.2125f + rgba->y * 0.7154f + rgba->z * 0.0721f;
    la->w = rgba->w;
}

static void la_to_rgba(const struct vec4 *la, struct vec4 *rgba)
{
    rgba->x = la->x;
    rgba->y = la->x;
    rgba->z = la->x;
    rgba->w = la->w;
}

/************************************************************
 * pixel format table providing info about number of bytes per pixel,
 * number of bits per channel and format type.
 *
 * Call get_format_info to request information about a specific format.
 */
static const struct pixel_format_desc formats[] =
{
    /* format            bpc              shifts            bpp blocks   type            from_rgba     to_rgba */
    {D3DFMT_R8G8B8,      {0,  8,  8,  8}, { 0, 16,  8,  0}, 3, 1, 1,  3, FORMAT_ARGB,    NULL,         NULL      },
    {D3DFMT_A8R8G8B8,    {8,  8,  8,  8}, {24, 16,  8,  0}, 4, 1, 1,  4, FORMAT_ARGB,    NULL,         NULL      },
    {D3DFMT_X8R8G8B8,    {0,  8,  8,  8}, { 0, 16,  8,  0}, 4, 1, 1,  4, FORMAT_ARGB,    NULL,         NULL      },
    {D3DFMT_A8B8G8R8,    {8,  8,  8,  8}, {24,  0,  8, 16}, 4, 1, 1,  4, FORMAT_ARGB,    NULL,         NULL      },
    {D3DFMT_X8B8G8R8,    {0,  8,  8,  8}, { 0,  0,  8, 16}, 4, 1, 1,  4, FORMAT_ARGB,    NULL,         NULL      },
    {D3DFMT_R5G6B5,      {0,  5,  6,  5}, { 0, 11,  5,  0}, 2, 1, 1,  2, FORMAT_ARGB,    NULL,         NULL      },
    {D3DFMT_X1R5G5B5,    {0,  5,  5,  5}, { 0, 10,  5,  0}, 2, 1, 1,  2, FORMAT_ARGB,    NULL,         NULL      },
    {D3DFMT_A1R5G5B5,    {1,  5,  5,  5}, {15, 10,  5,  0}, 2, 1, 1,  2, FORMAT_ARGB,    NULL,         NULL      },
    {D3DFMT_R3G3B2,      {0,  3,  3,  2}, { 0,  5,  2,  0}, 1, 1, 1,  1, FORMAT_ARGB,    NULL,         NULL      },
    {D3DFMT_A8R3G3B2,    {8,  3,  3,  2}, { 8,  5,  2,  0}, 2, 1, 1,  2, FORMAT_ARGB,    NULL,         NULL      },
    {D3DFMT_A4R4G4B4,    {4,  4,  4,  4}, {12,  8,  4,  0}, 2, 1, 1,  2, FORMAT_ARGB,    NULL,         NULL      },
    {D3DFMT_X4R4G4B4,    {0,  4,  4,  4}, { 0,  8,  4,  0}, 2, 1, 1,  2, FORMAT_ARGB,    NULL,         NULL      },
    {D3DFMT_A2R10G10B10, {2, 10, 10, 10}, {30, 20, 10,  0}, 4, 1, 1,  4, FORMAT_ARGB,    NULL,         NULL      },
    {D3DFMT_A2B10G10R10, {2, 10, 10, 10}, {30,  0, 10, 20}, 4, 1, 1,  4, FORMAT_ARGB,    NULL,         NULL      },
    {D3DFMT_G16R16,      {0, 16, 16,  0}, { 0,  0, 16,  0}, 4, 1, 1,  4, FORMAT_ARGB,    NULL,         NULL      },
    {D3DFMT_A8,          {8,  0,  0,  0}, { 0,  0,  0,  0}, 1, 1, 1,  1, FORMAT_ARGB,    NULL,         NULL      },
    {D3DFMT_A8L8,        {8,  8,  0,  0}, { 8,  0,  0,  0}, 2, 1, 1,  2, FORMAT_ARGB,    la_from_rgba, la_to_rgba},
    {D3DFMT_A4L4,        {4,  4,  0,  0}, { 4,  0,  0,  0}, 1, 1, 1,  1, FORMAT_ARGB,    la_from_rgba, la_to_rgba},
    {D3DFMT_L8,          {0,  8,  0,  0}, { 0,  0,  0,  0}, 1, 1, 1,  1, FORMAT_ARGB,    la_from_rgba, la_to_rgba},
    {D3DFMT_L16,         {0, 16,  0,  0}, { 0,  0,  0,  0}, 2, 1, 1,  2, FORMAT_ARGB,    la_from_rgba, la_to_rgba},
    {D3DFMT_DXT1,        {0,  0,  0,  0}, { 0,  0,  0,  0}, 1, 4, 4,  8, FORMAT_ARGB,    NULL,         NULL      },
    {D3DFMT_DXT2,        {0,  0,  0,  0}, { 0,  0,  0,  0}, 1, 4, 4, 16, FORMAT_ARGB,    NULL,         NULL      },
    {D3DFMT_DXT3,        {0,  0,  0,  0}, { 0,  0,  0,  0}, 1, 4, 4, 16, FORMAT_ARGB,    NULL,         NULL      },
    {D3DFMT_DXT4,        {0,  0,  0,  0}, { 0,  0,  0,  0}, 1, 4, 4, 16, FORMAT_ARGB,    NULL,         NULL      },
    {D3DFMT_DXT5,        {0,  0,  0,  0}, { 0,  0,  0,  0}, 1, 4, 4, 16, FORMAT_ARGB,    NULL,         NULL      },
    /* marks last element */
    {D3DFMT_UNKNOWN,     {0,  0,  0,  0}, { 0,  0,  0,  0}, 0, 1, 1,  0, FORMAT_UNKNOWN, NULL,         NULL      },
};


/************************************************************
 * map_view_of_file
 *
 * Loads a file into buffer and stores the number of read bytes in length.
 *
 * PARAMS
 *   filename [I] name of the file to be loaded
 *   buffer   [O] pointer to destination buffer
 *   length   [O] size of the obtained data
 *
 * RETURNS
 *   Success: D3D_OK
 *   Failure:
 *     see error codes for CreateFileW, GetFileSize, CreateFileMapping and MapViewOfFile
 *
 * NOTES
 *   The caller must UnmapViewOfFile when it doesn't need the data anymore
 *
 */
HRESULT map_view_of_file(LPCWSTR filename, LPVOID *buffer, DWORD *length)
{
    HANDLE hfile, hmapping = NULL;

    hfile = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if(hfile == INVALID_HANDLE_VALUE) goto error;

    *length = GetFileSize(hfile, NULL);
    if(*length == INVALID_FILE_SIZE) goto error;

    hmapping = CreateFileMappingW(hfile, NULL, PAGE_READONLY, 0, 0, NULL);
    if(!hmapping) goto error;

    *buffer = MapViewOfFile(hmapping, FILE_MAP_READ, 0, 0, 0);
    if(*buffer == NULL) goto error;

    CloseHandle(hmapping);
    CloseHandle(hfile);

    return S_OK;

error:
    CloseHandle(hmapping);
    CloseHandle(hfile);
    return HRESULT_FROM_WIN32(GetLastError());
}


const struct pixel_format_desc *get_format_info(D3DFORMAT format)
{
    unsigned int i = 0;
    while(formats[i].format != format && formats[i].format != D3DFMT_UNKNOWN) i++;
    //if (formats[i].format == D3DFMT_UNKNOWN)
    //    FIXME("Unknown format %#x (as FOURCC %s).\n", format, debugstr_an((const char *)&format, 4));
    return &formats[i];
}

const struct pixel_format_desc *get_format_info_idx(int idx)
{
    if(idx >= sizeof(formats) / sizeof(formats[0]))
        return NULL;
    if(formats[idx].format == D3DFMT_UNKNOWN)
        return NULL;
    return &formats[idx];
}

HRESULT WINAPI D3DXCheckTextureRequirements(LPDIRECT3DDEVICE9 device,
                                            UINT* width,
                                            UINT* height,
                                            UINT* miplevels,
                                            DWORD usage,
                                            D3DFORMAT* format,
                                            D3DPOOL pool)
{
    UINT w = (width && *width) ? *width : 1;
    UINT h = (height && *height) ? *height : 1;
    D3DCAPS9 caps;
    D3DDEVICE_CREATION_PARAMETERS params;
    IDirect3D9 *d3d = NULL;
    D3DDISPLAYMODE mode;
    HRESULT hr;
    D3DFORMAT usedformat = D3DFMT_UNKNOWN;

    //TRACE("(%p, %p, %p, %p, %u, %p, %u)\n", device, width, height, miplevels, usage, format, pool);

    if (!device)
        return D3DERR_INVALIDCALL;

    /* usage */
    if (usage == D3DX_DEFAULT)
        usage = 0;
    if (usage & (D3DUSAGE_WRITEONLY | D3DUSAGE_DONOTCLIP | D3DUSAGE_POINTS | D3DUSAGE_RTPATCHES | D3DUSAGE_NPATCHES))
        return D3DERR_INVALIDCALL;

    /* pool */
    if ((pool != D3DPOOL_DEFAULT) && (pool != D3DPOOL_MANAGED) && (pool != D3DPOOL_SYSTEMMEM) && (pool != D3DPOOL_SCRATCH))
        return D3DERR_INVALIDCALL;

    /* width and height */
    if (FAILED(IDirect3DDevice9_GetDeviceCaps(device, &caps)))
        return D3DERR_INVALIDCALL;

    /* 256 x 256 default width/height */
    if ((w == D3DX_DEFAULT) && (h == D3DX_DEFAULT))
        w = h = 256;
    else if (w == D3DX_DEFAULT)
        w = (height ? h : 256);
    else if (h == D3DX_DEFAULT)
        h = (width ? w : 256);

    /* ensure width/height is power of 2 */
    if ((caps.TextureCaps & D3DPTEXTURECAPS_POW2) && (!is_pow2(w)))
        w = make_pow2(w);

    if (w > caps.MaxTextureWidth)
        w = caps.MaxTextureWidth;

    if ((caps.TextureCaps & D3DPTEXTURECAPS_POW2) && (!is_pow2(h)))
        h = make_pow2(h);

    if (h > caps.MaxTextureHeight)
        h = caps.MaxTextureHeight;

    /* texture must be square? */
    if (caps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY)
    {
        if (w > h)
            h = w;
        else
            w = h;
    }

    if (width)
        *width = w;

    if (height)
        *height = h;

    /* miplevels */
    if (miplevels && (usage & D3DUSAGE_AUTOGENMIPMAP))
    {
        if (*miplevels > 1)
            *miplevels = 0;
    }
    else if (miplevels)
    {
        UINT max_mipmaps = 1;

        if (!width && !height)
            max_mipmaps = 9;    /* number of mipmaps in a 256x256 texture */
        else
        {
            UINT max_dimen = max(w, h);

            while (max_dimen > 1)
            {
                max_dimen >>= 1;
                max_mipmaps++;
            }
        }

        if (*miplevels == 0 || *miplevels > max_mipmaps)
            *miplevels = max_mipmaps;
    }

    /* format */
    if (format)
    {
        //TRACE("Requested format %x\n", *format);
        usedformat = *format;
    }

    hr = IDirect3DDevice9_GetDirect3D(device, &d3d);

    if (FAILED(hr))
        goto cleanup;

    hr = IDirect3DDevice9_GetCreationParameters(device, &params);

    if (FAILED(hr))
        goto cleanup;

    hr = IDirect3DDevice9_GetDisplayMode(device, 0, &mode);

    if (FAILED(hr))
        goto cleanup;

    if ((usedformat == D3DFMT_UNKNOWN) || (usedformat == D3DX_DEFAULT))
        usedformat = D3DFMT_A8R8G8B8;

    hr = IDirect3D9_CheckDeviceFormat(d3d, params.AdapterOrdinal, params.DeviceType, mode.Format,
        usage, D3DRTYPE_TEXTURE, usedformat);

    if (FAILED(hr))
    {
        /* Heuristic to choose the fallback format */
        const struct pixel_format_desc *fmt = get_format_info(usedformat);
        BOOL allow_24bits;
        int bestscore = INT_MIN, i = 0, j;
        unsigned int channels;
        const struct pixel_format_desc *curfmt;

        if (!fmt)
        {
            //FIXME("Pixel format %x not handled\n", usedformat);
            goto cleanup;
        }

        allow_24bits = fmt->bytes_per_pixel == 3;
        channels = (fmt->bits[0] ? 1 : 0) + (fmt->bits[1] ? 1 : 0)
            + (fmt->bits[2] ? 1 : 0) + (fmt->bits[3] ? 1 : 0);
        usedformat = D3DFMT_UNKNOWN;

        while ((curfmt = get_format_info_idx(i)))
        {
            unsigned int curchannels = (curfmt->bits[0] ? 1 : 0) + (curfmt->bits[1] ? 1 : 0)
                + (curfmt->bits[2] ? 1 : 0) + (curfmt->bits[3] ? 1 : 0);
            int score;

            i++;

            if (curchannels < channels)
                continue;
            if (curfmt->bytes_per_pixel == 3 && !allow_24bits)
                continue;

            hr = IDirect3D9_CheckDeviceFormat(d3d, params.AdapterOrdinal, params.DeviceType,
                mode.Format, usage, D3DRTYPE_TEXTURE, curfmt->format);
            if (FAILED(hr))
                continue;

            /* This format can be used, let's evaluate it.
               Weights chosen quite arbitrarily... */
            score = 16 - 4 * (curchannels - channels);

            for (j = 0; j < 4; j++)
            {
                int diff = curfmt->bits[j] - fmt->bits[j];
                score += 16 - (diff < 0 ? -diff * 4 : diff);
            }

            if (score > bestscore)
            {
                bestscore = score;
                usedformat = curfmt->format;
            }
        }
        hr = D3D_OK;
    }

cleanup:

    if (d3d)
        IDirect3D9_Release(d3d);

    if (FAILED(hr))
        return hr;

    if (usedformat == D3DFMT_UNKNOWN)
    {
        //WARN("Couldn't find a suitable pixel format\n");
        return D3DERR_NOTAVAILABLE;
    }

    //TRACE("Format chosen: %x\n", usedformat);
    if (format)
        *format = usedformat;

    return D3D_OK;
}

HRESULT WINAPI D3DXCheckCubeTextureRequirements(LPDIRECT3DDEVICE9 device,
                                                UINT *size,
                                                UINT *miplevels,
                                                DWORD usage,
                                                D3DFORMAT *format,
                                                D3DPOOL pool)
{
    D3DCAPS9 caps;
    UINT s = (size && *size) ? *size : 256;
    HRESULT hr;

    //TRACE("(%p, %p, %p, %u, %p, %u)\n", device, size, miplevels, usage, format, pool);

    if (s == D3DX_DEFAULT)
        s = 256;

    if (!device || FAILED(IDirect3DDevice9_GetDeviceCaps(device, &caps)))
        return D3DERR_INVALIDCALL;

    if (!(caps.TextureCaps & D3DPTEXTURECAPS_CUBEMAP))
        return D3DERR_NOTAVAILABLE;

    /* ensure width/height is power of 2 */
    if ((caps.TextureCaps & D3DPTEXTURECAPS_CUBEMAP_POW2) && (!is_pow2(s)))
        s = make_pow2(s);

    hr = D3DXCheckTextureRequirements(device, &s, &s, miplevels, usage, format, pool);

    if (!(caps.TextureCaps & D3DPTEXTURECAPS_MIPCUBEMAP))
    {
        if(miplevels)
            *miplevels = 1;
    }

    if (size)
        *size = s;

    return hr;
}

HRESULT WINAPI D3DXCheckVolumeTextureRequirements(LPDIRECT3DDEVICE9 device,
                                                  UINT *width,
                                                  UINT *height,
                                                  UINT *depth,
                                                  UINT *miplevels,
                                                  DWORD usage,
                                                  D3DFORMAT *format,
                                                  D3DPOOL pool)
{
    D3DCAPS9 caps;
    UINT w = width ? *width : D3DX_DEFAULT;
    UINT h = height ? *height : D3DX_DEFAULT;
    UINT d = (depth && *depth) ? *depth : 1;
    HRESULT hr;

    //TRACE("(%p, %p, %p, %p, %p, %u, %p, %u)\n", device, width, height, depth, miplevels,
    //      usage, format, pool);

    if (!device || FAILED(IDirect3DDevice9_GetDeviceCaps(device, &caps)))
        return D3DERR_INVALIDCALL;

    if (!(caps.TextureCaps & D3DPTEXTURECAPS_VOLUMEMAP))
        return D3DERR_NOTAVAILABLE;

    hr = D3DXCheckTextureRequirements(device, &w, &h, NULL, usage, format, pool);
    if (d == D3DX_DEFAULT)
        d = 1;

    /* ensure width/height is power of 2 */
    if ((caps.TextureCaps & D3DPTEXTURECAPS_VOLUMEMAP_POW2) &&
        (!is_pow2(w) || !is_pow2(h) || !is_pow2(d)))
    {
        w = make_pow2(w);
        h = make_pow2(h);
        d = make_pow2(d);
    }

    if (w > caps.MaxVolumeExtent)
        w = caps.MaxVolumeExtent;
    if (h > caps.MaxVolumeExtent)
        h = caps.MaxVolumeExtent;
    if (d > caps.MaxVolumeExtent)
        d = caps.MaxVolumeExtent;

    if (miplevels)
    {
        if (!(caps.TextureCaps & D3DPTEXTURECAPS_MIPVOLUMEMAP))
            *miplevels = 1;
        else if ((usage & D3DUSAGE_AUTOGENMIPMAP))
        {
            if (*miplevels > 1)
                *miplevels = 0;
        }
        else
        {
            UINT max_mipmaps = 1;
            UINT max_dimen = max(max(w, h), d);

            while (max_dimen > 1)
            {
                max_dimen >>= 1;
                max_mipmaps++;
            }

            if (*miplevels == 0 || *miplevels > max_mipmaps)
                *miplevels = max_mipmaps;
        }
    }

    if (width)
        *width = w;
    if (height)
        *height = h;
    if (depth)
        *depth = d;

    return hr;
}

HRESULT WINAPI D3DXCreateVolumeTexture(LPDIRECT3DDEVICE9 device,
                                       UINT width,
                                       UINT height,
                                       UINT depth,
                                       UINT miplevels,
                                       DWORD usage,
                                       D3DFORMAT format,
                                       D3DPOOL pool,
                                       LPDIRECT3DVOLUMETEXTURE9 *texture) {
    HRESULT hr;

    //TRACE("(%p, %u, %u, %u, %u, %#x, %#x, %#x, %p)\n", device, width, height, depth,
    //      miplevels, usage, format, pool, texture);

    if (!device || !texture)
        return D3DERR_INVALIDCALL;

    hr = D3DXCheckVolumeTextureRequirements(device, &width, &height, &depth,
                                            &miplevels, usage, &format, pool);

    if (FAILED(hr))
    {
    //    TRACE("D3DXCheckVolumeTextureRequirements failed\n");
        return hr;
    }

    return IDirect3DDevice9_CreateVolumeTexture(device, width, height, depth, miplevels,
                                                usage, format, pool, texture, NULL);
}

HRESULT WINAPI D3DXCreateCubeTexture(LPDIRECT3DDEVICE9 device,
                                     UINT size,
                                     UINT miplevels,
                                     DWORD usage,
                                    D3DFORMAT format,
                                     D3DPOOL pool,
                                     LPDIRECT3DCUBETEXTURE9 *texture) {
    HRESULT hr;

    //TRACE("(%p, %u, %u, %#x, %#x, %#x, %p)\n", device, size, miplevels, usage, format,
    //    pool, texture);

    if (!device || !texture)
        return D3DERR_INVALIDCALL;

    hr = D3DXCheckCubeTextureRequirements(device, &size, &miplevels, usage, &format, pool);

    if (FAILED(hr))
    {
        //TRACE("D3DXCheckCubeTextureRequirements failed\n");
        return hr;
    }

    return IDirect3DDevice9_CreateCubeTexture(device, size, miplevels, usage, format, pool, texture, NULL);
}

HRESULT WINAPI D3DXCreateTexture(LPDIRECT3DDEVICE9 pDevice,
                                 UINT width,
                                 UINT height,
                                 UINT miplevels,
                                  DWORD usage,
                                 D3DFORMAT format,
                                  D3DPOOL pool,
                                LPDIRECT3DTEXTURE9 *ppTexture) {
    HRESULT hr; 

   // TRACE("(%p, %u, %u, %u, %x, %x, %x, %p)\n", pDevice, width, height, miplevels, usage, format, 
    //     pool, ppTexture); 
  
    if (!pDevice || !ppTexture) 
        return D3DERR_INVALIDCALL; 

    hr = D3DXCheckTextureRequirements(pDevice, &width, &height, &miplevels, usage, &format, pool); 
 
    if (FAILED(hr)) 
        return hr; 

     return IDirect3DDevice9_CreateTexture(pDevice, width, height, miplevels, usage, format, pool, ppTexture, NULL); 
 } 

HRESULT WINAPI D3DXCreateEffect(LPDIRECT3DDEVICE9 device,
                                LPCVOID srcdata,
                                UINT srcdatalen,
                                CONST D3DXMACRO* defines,
                                LPD3DXINCLUDE include,
                                DWORD flags,
                                LPD3DXEFFECTPOOL pool,
                                LPD3DXEFFECT* effect,
                                LPD3DXBUFFER* compilation_errors)
{
    //TRACE("(%p, %p, %u, %p, %p, %#x, %p, %p, %p): Forwarded to D3DXCreateEffectEx\n", device, srcdata, srcdatalen, defines,
    //    include, flags, pool, effect, compilation_errors);

    return D3DXCreateEffectEx(device, srcdata, srcdatalen, defines, include, NULL, flags, pool, effect, compilation_errors);
}

#else

typedef HRESULT (WINAPI * pD3DXCreateEffect)(LPDIRECT3DDEVICE9 device,
                                LPCVOID srcdata,
                                UINT srcdatalen,
                                CONST D3DXMACRO* defines,
                                LPD3DXINCLUDE include,
                                DWORD flags,
                                LPD3DXEFFECTPOOL pool,
                                LPD3DXEFFECT* effect,
                                LPD3DXBUFFER* compilation_errors);

/* extern "C" */HRESULT WINAPI D3DXCreateEffect(LPDIRECT3DDEVICE9 device,
                                LPCVOID srcdata,
                                UINT srcdatalen,
                                CONST D3DXMACRO* defines,
                                LPD3DXINCLUDE include,
                                DWORD flags,
                                LPD3DXEFFECTPOOL pool,
                                LPD3DXEFFECT* effect,
                                LPD3DXBUFFER* compilation_errors) {
	static pD3DXCreateEffect D3DXCreateEffect_p = NULL;
	HMODULE mod = LoadLibrary(_T("D3DX9_43.DLL"));
	if (mod)
		D3DXCreateEffect_p = (pD3DXCreateEffect) GetProcAddress(mod, "D3DXCreateEffect");
	if(NULL != D3DXCreateEffect_p)
		return D3DXCreateEffect_p(device, srcdata, srcdatalen, defines, include, flags, pool, effect, compilation_errors);
	else
		return D3DERR_NOTAVAILABLE;
}

typedef HRESULT (WINAPI * pD3DXCreateEffectCompilerFromFileA)(LPCSTR srcfile, const D3DXMACRO *defines, LPD3DXINCLUDE include,
    DWORD flags, LPD3DXEFFECTCOMPILER *effectcompiler, LPD3DXBUFFER *parseerrors);

/* extern "C" */HRESULT WINAPI D3DXCreateEffectCompilerFromFileA(LPCSTR srcfile, const D3DXMACRO *defines, LPD3DXINCLUDE include,
    DWORD flags, LPD3DXEFFECTCOMPILER *effectcompiler, LPD3DXBUFFER *parseerrors) {
	static pD3DXCreateEffectCompilerFromFileA D3DXCreateEffectCompilerFromFileA_p = NULL;
	HMODULE mod = LoadLibrary(_T("D3DX9_43.DLL"));
	if (mod)
		D3DXCreateEffectCompilerFromFileA_p = (pD3DXCreateEffectCompilerFromFileA) GetProcAddress(mod, "D3DXCreateEffectCompilerFromFileA");
	if(NULL != D3DXCreateEffectCompilerFromFileA_p)
		return D3DXCreateEffectCompilerFromFileA_p(srcfile, defines, include, flags, effectcompiler, parseerrors);
	else
		return D3DERR_NOTAVAILABLE;
}

typedef HRESULT (WINAPI * pD3DXCreateTexture)(LPDIRECT3DDEVICE9 pDevice,
                                 UINT width,
                                 UINT height,
                                 UINT miplevels,
                                  DWORD usage,
                                 D3DFORMAT format,
                                  D3DPOOL pool,
                                LPDIRECT3DTEXTURE9 *ppTexture);

/* extern "C" */HRESULT WINAPI D3DXCreateTexture(LPDIRECT3DDEVICE9 pDevice,
                                 UINT width,
                                 UINT height,
                                 UINT miplevels,
                                  DWORD usage,
                                 D3DFORMAT format,
                                  D3DPOOL pool,
                                LPDIRECT3DTEXTURE9 *ppTexture) {
	static pD3DXCreateTexture D3DXCreateTexture_p = NULL;
	HMODULE mod = LoadLibrary(_T("D3DX9_43.DLL"));
	if (mod) 
		D3DXCreateTexture_p = (pD3DXCreateTexture) GetProcAddress(mod, "D3DXCreateTexture");
	if(NULL != D3DXCreateTexture_p)
		return D3DXCreateTexture_p(pDevice, width, height, miplevels, usage, format, pool, ppTexture);
	else
		return D3DERR_NOTAVAILABLE;
}

typedef HRESULT (WINAPI * pD3DXCreateCubeTexture)(LPDIRECT3DDEVICE9 device,
                                     UINT size,
                                     UINT miplevels,
                                     DWORD usage,
                                    D3DFORMAT format,
                                     D3DPOOL pool,
                                     LPDIRECT3DCUBETEXTURE9 *texture);

/* extern "C" */HRESULT WINAPI D3DXCreateCubeTexture(LPDIRECT3DDEVICE9 device,
                                     UINT size,
                                     UINT miplevels,
                                     DWORD usage,
                                    D3DFORMAT format,
                                     D3DPOOL pool,
                                     LPDIRECT3DCUBETEXTURE9 *texture) {
	static pD3DXCreateCubeTexture D3DXCreateCubeTexture_p = NULL;
	HMODULE mod = LoadLibrary(_T("D3DX9_43.DLL"));
	if (mod) 
		D3DXCreateCubeTexture_p = (pD3DXCreateCubeTexture) GetProcAddress(mod, "D3DXCreateCubeTexture");
	if(NULL != D3DXCreateCubeTexture_p)
		return D3DXCreateCubeTexture_p(device, size, miplevels, usage, format, pool, texture);
	else
		return D3DERR_NOTAVAILABLE;
}

typedef HRESULT (WINAPI * pD3DXCreateVolumeTexture)(LPDIRECT3DDEVICE9 device,
                                       UINT width,
                                       UINT height,
                                       UINT depth,
                                       UINT miplevels,
                                       DWORD usage,
                                       D3DFORMAT format,
                                       D3DPOOL pool,
                                       LPDIRECT3DVOLUMETEXTURE9 *texture);

/* extern "C" */HRESULT WINAPI D3DXCreateVolumeTexture(LPDIRECT3DDEVICE9 device,
                                       UINT width,
                                       UINT height,
                                       UINT depth,
                                       UINT miplevels,
                                       DWORD usage,
                                       D3DFORMAT format,
                                       D3DPOOL pool,
                                       LPDIRECT3DVOLUMETEXTURE9 *texture) {
	static pD3DXCreateVolumeTexture D3DXCreateVolumeTexture_p = NULL;
	HMODULE mod = LoadLibrary(_T("D3DX9_43.DLL"));
	if (mod) 
		D3DXCreateVolumeTexture_p = (pD3DXCreateVolumeTexture) GetProcAddress(mod, "D3DXCreateVolumeTexture");
	if(NULL != D3DXCreateVolumeTexture_p)
		return D3DXCreateVolumeTexture_p(device, width, height, depth, miplevels, usage, format, pool, texture);
	else
		return D3DERR_NOTAVAILABLE;
}

#endif

typedef HRESULT (WINAPI * pD3DXFillVolumeTextureTX)(LPDIRECT3DVOLUMETEXTURE9 pVolumeTexture,
                                        LPD3DXTEXTURESHADER pTextureShader);

/* extern "C" */HRESULT WINAPI D3DXFillVolumeTextureTX(LPDIRECT3DVOLUMETEXTURE9 pVolumeTexture,
                                        LPD3DXTEXTURESHADER pTextureShader) {
	static pD3DXFillVolumeTextureTX D3DXFillVolumeTextureTX_p = NULL;
	HMODULE mod = LoadLibrary(_T("D3DX9_43.DLL"));
	if (mod) 
		D3DXFillVolumeTextureTX_p = (pD3DXFillVolumeTextureTX) GetProcAddress(mod, "D3DXFillVolumeTextureTX");
	//FreeLibrary(mod);
	if(NULL != D3DXFillVolumeTextureTX_p)
		return D3DXFillVolumeTextureTX_p(pVolumeTexture, pTextureShader);
	else
		return D3DERR_NOTAVAILABLE;
}

typedef HRESULT (WINAPI * pD3DXFillCubeTextureTX)(
        LPDIRECT3DCUBETEXTURE9    pCubeTexture,
        LPD3DXTEXTURESHADER       pTextureShader);

/* extern "C" */HRESULT WINAPI
    D3DXFillCubeTextureTX(
        LPDIRECT3DCUBETEXTURE9    pCubeTexture,
        LPD3DXTEXTURESHADER       pTextureShader) {
	static pD3DXFillCubeTextureTX D3DXFillCubeTextureTX_p = NULL;
	HMODULE mod = LoadLibrary(_T("D3DX9_43.DLL"));
	if (mod) 
		D3DXFillCubeTextureTX_p = (pD3DXFillCubeTextureTX) GetProcAddress(mod, "D3DXFillCubeTextureTX");
	//FreeLibrary(mod);
	if(NULL != D3DXFillCubeTextureTX_p)
		return D3DXFillCubeTextureTX_p(pCubeTexture, pTextureShader);
	else
		return D3DERR_NOTAVAILABLE;
}

typedef HRESULT (WINAPI * pD3DXFillTextureTX)(
        LPDIRECT3DTEXTURE9        pTexture,
        LPD3DXTEXTURESHADER       pTextureShader);

/* extern "C" */HRESULT WINAPI 
    D3DXFillTextureTX(
        LPDIRECT3DTEXTURE9        pTexture,
        LPD3DXTEXTURESHADER       pTextureShader) {
	static pD3DXFillTextureTX D3DXFillTextureTX_p = NULL;
	HMODULE mod = LoadLibrary(_T("D3DX9_43.DLL"));
	//FreeLibrary(mod);
	if (mod) 
		D3DXFillTextureTX_p = (pD3DXFillTextureTX) GetProcAddress(mod, "D3DXFillTextureTX");
	if(NULL != D3DXFillTextureTX_p)
		return D3DXFillTextureTX_p(pTexture, pTextureShader);
	else
		return D3DERR_NOTAVAILABLE;
}

typedef HRESULT (WINAPI * pD3DXCreateTextureShader)(
  const DWORD *pFunction,
  LPD3DXTEXTURESHADER *ppTextureShader
);

/* extern "C" */HRESULT WINAPI D3DXCreateTextureShader(
  const DWORD *pFunction,
  LPD3DXTEXTURESHADER *ppTextureShader) {
	static pD3DXCreateTextureShader D3DXCreateTextureShader_p = NULL;
	HMODULE mod = LoadLibrary(_T("D3DX9_43.DLL"));
	//FreeLibrary(mod);
	if (mod) 
		D3DXCreateTextureShader_p = (pD3DXCreateTextureShader) GetProcAddress(mod, "D3DXCreateTextureShader");
	if(NULL != D3DXCreateTextureShader_p)
		return D3DXCreateTextureShader_p(pFunction, ppTextureShader);
	else
		return D3DERR_NOTAVAILABLE;
}

D3DXMATRIX* WINAPI D3DXMatrixMultiply(D3DXMATRIX *pout, CONST D3DXMATRIX *pm1, CONST D3DXMATRIX *pm2) {
    int i,j;

    for (i=0; i<4; i++)
    {
     for (j=0; j<4; j++)
     {
      pout->m[i][j] = pm1->m[i][0] * pm2->m[0][j] + pm1->m[i][1] * pm2->m[1][j] + pm1->m[i][2] * pm2->m[2][j] + pm1->m[i][3] * pm2->m[3][j];
     }
    }
    return pout;
}

D3DXMATRIX* WINAPI D3DXMatrixTranslation(D3DXMATRIX *pOut, FLOAT tx, FLOAT ty, FLOAT tz) {
    pOut->_11=1.0f;   pOut->_12=0.0f;   pOut->_13=0.0f;  pOut->_14=0.0f;
    pOut->_21=0.0f;   pOut->_22=1.0f;   pOut->_23=0.0f;  pOut->_24=0.0f;
    pOut->_31=0.0f;   pOut->_32=0.0f;   pOut->_33=1.0f;  pOut->_34=0.0f;
    pOut->_41=tx;     pOut->_42=ty;     pOut->_43=tz;    pOut->_44=1.0f;

    return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixOrthoOffCenterLH(D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn, FLOAT zf) {
    pOut->_11=2.0f/r; pOut->_12=0.0f;   pOut->_13=0.0f;  pOut->_14=0.0f;
    pOut->_21=0.0f;   pOut->_22=2.0f/t; pOut->_23=0.0f;  pOut->_24=0.0f;
    pOut->_31=0.0f;   pOut->_32=0.0f;   pOut->_33=1.0f;  pOut->_34=0.0f;
    pOut->_41=-1.0f;  pOut->_42=-1.0f;  pOut->_43=0.0f;  pOut->_44=1.0f;

    return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixScaling(D3DXMATRIX *pOut, FLOAT sx, FLOAT sy, FLOAT sz) {
    pOut->_11=sx;     pOut->_12=0.0f;   pOut->_13=0.0f;  pOut->_14=0.0f;
    pOut->_21=0.0f;   pOut->_22=sy;     pOut->_23=0.0f;  pOut->_24=0.0f;
    pOut->_31=0.0f;   pOut->_32=0.0f;   pOut->_33=sz;    pOut->_34=0.0f;
    pOut->_41=0.0f;   pOut->_42=0.0f;   pOut->_43=0.0f;  pOut->_44=1.0f;

    return pOut;
}
