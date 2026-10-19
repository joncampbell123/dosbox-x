#include <sys/types.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#include "control.h"
#include "dosbox.h"
#include "logging.h"
#include "menudef.h"
#include "../ints/int10.h"
#include <output/output_vulkan.h>
#include <output/output_surface.h>
#include <output/output_tools.h>
#include <output/output_tools_xbrz.h>

#include <algorithm>
#include <vector>

#include "sdlmain.h"
#include "render.h"

#if C_VULKAN_OUTPUT

/* Vulkan is loaded at runtime through SDL (SDL_Vulkan_LoadLibrary), so the
 * executable does not link against, or require, the Vulkan loader. If it is
 * missing, the output falls back to surface.
 *
 * SDL1 builds need the in-tree SDL 1.2 (vs/sdl), which has the SDL_VULKAN
 * video mode flag and an SDL_vulkan.h modeled after SDL2's. */
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "SDL_vulkan.h"

using namespace std;

extern Bitu frames;
extern Bitu userResizeWindowWidth;
extern Bitu userResizeWindowHeight;
extern Bitu currentWindowWidth;
extern Bitu currentWindowHeight;

bool setSizeButNotResize();
int Voodoo_OGL_GetWidth();
int Voodoo_OGL_GetHeight();
bool Voodoo_OGL_Active();

/* The two shaders, hand assembled to SPIR-V 1.0 (no shader compiler is needed
 * to build DOSBox-X). Equivalent GLSL:
 *
 *   // vertex: draws a rectangle as a 4 vertex triangle strip
 *   layout(push_constant) uniform PC { vec4 pos; vec4 uv; } pc;
 *   layout(location=0) out vec2 v_uv;
 *   void main() {
 *       vec2 c = vec2(float(gl_VertexIndex & 1), float(gl_VertexIndex >> 1));
 *       gl_Position = vec4(pc.pos.xy + pc.pos.zw * c, 0.0, 1.0);
 *       v_uv = pc.uv.xy + pc.uv.zw * c;
 *   }
 *
 *   // fragment
 *   layout(set=0, binding=0) uniform sampler2D tex;
 *   layout(location=0) in vec2 v_uv;
 *   layout(location=0) out vec4 o;
 *   void main() { o = texture(tex, v_uv); }
 */
static const uint32_t vulkan_vert_spv[] = {
    0x07230203, 0x00010000, 0x00000000, 0x0000002c, 0x00000000, 0x00020011,
    0x00000001, 0x0003000e, 0x00000000, 0x00000001, 0x0008000f, 0x00000000,
    0x00000015, 0x6e69616d, 0x00000000, 0x00000008, 0x0000000a, 0x0000000c,
    0x00040047, 0x00000008, 0x0000000b, 0x0000002a, 0x00040047, 0x0000000a,
    0x0000000b, 0x00000000, 0x00040047, 0x0000000c, 0x0000001e, 0x00000000,
    0x00030047, 0x0000000d, 0x00000002, 0x00050048, 0x0000000d, 0x00000000,
    0x00000023, 0x00000000, 0x00050048, 0x0000000d, 0x00000001, 0x00000023,
    0x00000010, 0x00020013, 0x00000001, 0x00030021, 0x00000002, 0x00000001,
    0x00030016, 0x00000003, 0x00000020, 0x00040017, 0x00000004, 0x00000003,
    0x00000002, 0x00040017, 0x00000005, 0x00000003, 0x00000004, 0x00040015,
    0x00000006, 0x00000020, 0x00000001, 0x00040020, 0x00000007, 0x00000001,
    0x00000006, 0x0004003b, 0x00000007, 0x00000008, 0x00000001, 0x00040020,
    0x00000009, 0x00000003, 0x00000005, 0x0004003b, 0x00000009, 0x0000000a,
    0x00000003, 0x00040020, 0x0000000b, 0x00000003, 0x00000004, 0x0004003b,
    0x0000000b, 0x0000000c, 0x00000003, 0x0004001e, 0x0000000d, 0x00000005,
    0x00000005, 0x00040020, 0x0000000e, 0x00000009, 0x0000000d, 0x0004003b,
    0x0000000e, 0x0000000f, 0x00000009, 0x00040020, 0x00000010, 0x00000009,
    0x00000005, 0x0004002b, 0x00000006, 0x00000011, 0x00000000, 0x0004002b,
    0x00000006, 0x00000012, 0x00000001, 0x0004002b, 0x00000003, 0x00000013,
    0x00000000, 0x0004002b, 0x00000003, 0x00000014, 0x3f800000, 0x00050036,
    0x00000001, 0x00000015, 0x00000000, 0x00000002, 0x000200f8, 0x00000016,
    0x0004003d, 0x00000006, 0x00000017, 0x00000008, 0x000500c7, 0x00000006,
    0x00000018, 0x00000017, 0x00000012, 0x000500c3, 0x00000006, 0x00000019,
    0x00000017, 0x00000012, 0x0004006f, 0x00000003, 0x0000001a, 0x00000018,
    0x0004006f, 0x00000003, 0x0000001b, 0x00000019, 0x00050050, 0x00000004,
    0x0000001c, 0x0000001a, 0x0000001b, 0x00050041, 0x00000010, 0x0000001d,
    0x0000000f, 0x00000011, 0x0004003d, 0x00000005, 0x0000001e, 0x0000001d,
    0x0007004f, 0x00000004, 0x0000001f, 0x0000001e, 0x0000001e, 0x00000000,
    0x00000001, 0x0007004f, 0x00000004, 0x00000020, 0x0000001e, 0x0000001e,
    0x00000002, 0x00000003, 0x00050085, 0x00000004, 0x00000021, 0x00000020,
    0x0000001c, 0x00050081, 0x00000004, 0x00000022, 0x0000001f, 0x00000021,
    0x00050051, 0x00000003, 0x00000023, 0x00000022, 0x00000000, 0x00050051,
    0x00000003, 0x00000024, 0x00000022, 0x00000001, 0x00070050, 0x00000005,
    0x00000025, 0x00000023, 0x00000024, 0x00000013, 0x00000014, 0x0003003e,
    0x0000000a, 0x00000025, 0x00050041, 0x00000010, 0x00000026, 0x0000000f,
    0x00000012, 0x0004003d, 0x00000005, 0x00000027, 0x00000026, 0x0007004f,
    0x00000004, 0x00000028, 0x00000027, 0x00000027, 0x00000000, 0x00000001,
    0x0007004f, 0x00000004, 0x00000029, 0x00000027, 0x00000027, 0x00000002,
    0x00000003, 0x00050085, 0x00000004, 0x0000002a, 0x00000029, 0x0000001c,
    0x00050081, 0x00000004, 0x0000002b, 0x00000028, 0x0000002a, 0x0003003e,
    0x0000000c, 0x0000002b, 0x000100fd, 0x00010038,
};

static const uint32_t vulkan_frag_spv[] = {
    0x07230203, 0x00010000, 0x00000000, 0x00000013, 0x00000000, 0x00020011,
    0x00000001, 0x0003000e, 0x00000000, 0x00000001, 0x0007000f, 0x00000004,
    0x0000000e, 0x6e69616d, 0x00000000, 0x0000000b, 0x0000000d, 0x00030010,
    0x0000000e, 0x00000007, 0x00040047, 0x00000009, 0x00000022, 0x00000000,
    0x00040047, 0x00000009, 0x00000021, 0x00000000, 0x00040047, 0x0000000b,
    0x0000001e, 0x00000000, 0x00040047, 0x0000000d, 0x0000001e, 0x00000000,
    0x00020013, 0x00000001, 0x00030021, 0x00000002, 0x00000001, 0x00030016,
    0x00000003, 0x00000020, 0x00040017, 0x00000004, 0x00000003, 0x00000002,
    0x00040017, 0x00000005, 0x00000003, 0x00000004, 0x00090019, 0x00000006,
    0x00000003, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000001,
    0x00000000, 0x0003001b, 0x00000007, 0x00000006, 0x00040020, 0x00000008,
    0x00000000, 0x00000007, 0x0004003b, 0x00000008, 0x00000009, 0x00000000,
    0x00040020, 0x0000000a, 0x00000001, 0x00000004, 0x0004003b, 0x0000000a,
    0x0000000b, 0x00000001, 0x00040020, 0x0000000c, 0x00000003, 0x00000005,
    0x0004003b, 0x0000000c, 0x0000000d, 0x00000003, 0x00050036, 0x00000001,
    0x0000000e, 0x00000000, 0x00000002, 0x000200f8, 0x0000000f, 0x0004003d,
    0x00000007, 0x00000010, 0x00000009, 0x0004003d, 0x00000004, 0x00000011,
    0x0000000b, 0x00050057, 0x00000005, 0x00000012, 0x00000010, 0x00000011,
    0x0003003e, 0x0000000d, 0x00000012, 0x000100fd, 0x00010038,
};

#define VK_GLOBAL_FUNCS \
    VKF(vkCreateInstance) \
    VKF(vkEnumerateInstanceExtensionProperties)

#define VK_INSTANCE_FUNCS \
    VKF(vkDestroyInstance) \
    VKF(vkEnumeratePhysicalDevices) \
    VKF(vkGetPhysicalDeviceProperties) \
    VKF(vkGetPhysicalDeviceQueueFamilyProperties) \
    VKF(vkGetPhysicalDeviceMemoryProperties) \
    VKF(vkGetPhysicalDeviceSurfaceSupportKHR) \
    VKF(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) \
    VKF(vkGetPhysicalDeviceSurfaceFormatsKHR) \
    VKF(vkGetPhysicalDeviceSurfacePresentModesKHR) \
    VKF(vkEnumerateDeviceExtensionProperties) \
    VKF(vkDestroySurfaceKHR) \
    VKF(vkCreateDevice) \
    VKF(vkGetDeviceProcAddr)

#define VK_DEVICE_FUNCS \
    VKF(vkDestroyDevice) \
    VKF(vkGetDeviceQueue) \
    VKF(vkDeviceWaitIdle) \
    VKF(vkCreateSwapchainKHR) \
    VKF(vkDestroySwapchainKHR) \
    VKF(vkGetSwapchainImagesKHR) \
    VKF(vkAcquireNextImageKHR) \
    VKF(vkQueuePresentKHR) \
    VKF(vkQueueSubmit) \
    VKF(vkCreateImageView) \
    VKF(vkDestroyImageView) \
    VKF(vkCreateRenderPass) \
    VKF(vkDestroyRenderPass) \
    VKF(vkCreateFramebuffer) \
    VKF(vkDestroyFramebuffer) \
    VKF(vkCreateShaderModule) \
    VKF(vkDestroyShaderModule) \
    VKF(vkCreatePipelineLayout) \
    VKF(vkDestroyPipelineLayout) \
    VKF(vkCreateGraphicsPipelines) \
    VKF(vkDestroyPipeline) \
    VKF(vkCreateDescriptorSetLayout) \
    VKF(vkDestroyDescriptorSetLayout) \
    VKF(vkCreateDescriptorPool) \
    VKF(vkDestroyDescriptorPool) \
    VKF(vkAllocateDescriptorSets) \
    VKF(vkUpdateDescriptorSets) \
    VKF(vkCreateSampler) \
    VKF(vkDestroySampler) \
    VKF(vkCreateImage) \
    VKF(vkDestroyImage) \
    VKF(vkGetImageMemoryRequirements) \
    VKF(vkBindImageMemory) \
    VKF(vkCreateBuffer) \
    VKF(vkDestroyBuffer) \
    VKF(vkGetBufferMemoryRequirements) \
    VKF(vkBindBufferMemory) \
    VKF(vkAllocateMemory) \
    VKF(vkFreeMemory) \
    VKF(vkMapMemory) \
    VKF(vkUnmapMemory) \
    VKF(vkCreateCommandPool) \
    VKF(vkDestroyCommandPool) \
    VKF(vkAllocateCommandBuffers) \
    VKF(vkResetCommandBuffer) \
    VKF(vkBeginCommandBuffer) \
    VKF(vkEndCommandBuffer) \
    VKF(vkCmdPipelineBarrier) \
    VKF(vkCmdCopyBufferToImage) \
    VKF(vkCmdBeginRenderPass) \
    VKF(vkCmdEndRenderPass) \
    VKF(vkCmdBindPipeline) \
    VKF(vkCmdBindDescriptorSets) \
    VKF(vkCmdPushConstants) \
    VKF(vkCmdSetViewport) \
    VKF(vkCmdSetScissor) \
    VKF(vkCmdDraw) \
    VKF(vkCreateSemaphore) \
    VKF(vkDestroySemaphore) \
    VKF(vkCreateFence) \
    VKF(vkDestroyFence) \
    VKF(vkWaitForFences) \
    VKF(vkResetFences)

#define VKF(name) static PFN_##name name = NULL;
static PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = NULL;
VK_GLOBAL_FUNCS
VK_INSTANCE_FUNCS
VK_DEVICE_FUNCS
#undef VKF

/* A texture sampled by the fragment shader, with a persistently mapped
 * staging buffer that the CPU copies the new contents into. */
struct VulkanTexture {
    uint32_t        width = 0, height = 0;
    VkImage         image = VK_NULL_HANDLE;
    VkDeviceMemory  image_memory = VK_NULL_HANDLE;
    VkImageView     view = VK_NULL_HANDLE;
    VkBuffer        staging = VK_NULL_HANDLE;
    VkDeviceMemory  staging_memory = VK_NULL_HANDLE;
    uint8_t*        staging_ptr = NULL;
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkSampler       bound_sampler = VK_NULL_HANDLE;
    bool            initialized = false; // image contents are valid (layout is SHADER_READ_ONLY_OPTIMAL)
};

static struct {
    bool                library_loaded = false;
    VkInstance          instance = VK_NULL_HANDLE;
    VkPhysicalDevice    physical = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties memory_properties = {};
    VkDevice            device = VK_NULL_HANDLE;
    uint32_t            queue_family = 0;
    VkQueue             queue = VK_NULL_HANDLE;

    VkCommandPool       command_pool = VK_NULL_HANDLE;
    VkCommandBuffer     cmd = VK_NULL_HANDLE;
    VkSemaphore         image_acquired = VK_NULL_HANDLE;
    VkFence             frame_fence = VK_NULL_HANDLE;

    VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
    VkDescriptorPool    descriptor_pool = VK_NULL_HANDLE;
    VkPipelineLayout    pipeline_layout = VK_NULL_HANDLE;
    VkSampler           sampler_nearest = VK_NULL_HANDLE;
    VkSampler           sampler_linear = VK_NULL_HANDLE;

    /* per window */
    void*               surface_window = NULL; // the window the surface was made for
    VkSurfaceKHR        surface = VK_NULL_HANDLE;
    VkSwapchainKHR      swapchain = VK_NULL_HANDLE;
    VkFormat            swapchain_format = VK_FORMAT_UNDEFINED;
    VkExtent2D          swapchain_extent = {};
    int                 swapchain_drawable_w = 0, swapchain_drawable_h = 0; // SDL's idea of the size, when created
    VkPresentModeKHR    present_mode = VK_PRESENT_MODE_FIFO_KHR;
    bool                swapchain_dirty = false;
    vector<VkImage>     swapchain_images;
    vector<VkImageView> swapchain_views;
    vector<VkFramebuffer> framebuffers;
    vector<VkSemaphore> render_done; // one per swapchain image
    VkRenderPass        render_pass = VK_NULL_HANDLE;
    VkFormat            render_pass_format = VK_FORMAT_UNDEFINED;
    VkPipeline          pipeline_opaque = VK_NULL_HANDLE;
    VkPipeline          pipeline_blend = VK_NULL_HANDLE;

    /* the emulator image, and the window sized overlay that SDL drawn menus go to */
    VulkanTexture       frame;
    VulkanTexture       overlay;
    vector<uint32_t>    framebuf;   // what the renderer draws into
    uint32_t            framebuf_pitch = 0;
    bool                frame_linear = true;
    SDL_Surface*        overlay_surface = NULL;
    bool                overlay_dirty = false;
} vk;

static bool VK_Check(VkResult res, const char *what) {
    if (res != VK_SUCCESS) {
        LOG_MSG("VULKAN: %s failed (VkResult %d)", what, (int)res);
        return false;
    }
    return true;
}

/* the window helpers below hide the differences between SDL1 and SDL2 */
static void* VK_CurrentWindow(void) {
#if defined(C_SDL2)
    return sdl.window;
#else
    /* SDL1 has a single window. It tells us before destroying it (see
     * VK_WindowDestroyed) so a surface is good for as long as it exists */
    static int sdl1_window;
    return SDL_GetVideoSurface() != NULL ? &sdl1_window : NULL;
#endif
}

static bool VK_WindowIsVulkan(void) {
#if defined(C_SDL2)
    return sdl.window != NULL && (SDL_GetWindowFlags(sdl.window) & SDL_WINDOW_VULKAN);
#else
    SDL_Surface *video = SDL_GetVideoSurface();
    return video != NULL && (video->flags & SDL_VULKAN);
#endif
}

/* in window coordinates, which is what sdl.clip and the menus use */
static void VK_GetWindowSize(int &w, int &h) {
#if defined(C_SDL2)
    SDL_GetWindowSize(sdl.window, &w, &h);
#else
    SDL_Surface *video = SDL_GetVideoSurface();
    w = video ? video->w : 0;
    h = video ? video->h : 0;
#endif
}

/* in pixels, may differ from the window size on high DPI displays */
static void VK_GetDrawableSize(int &w, int &h) {
#if defined(C_SDL2)
    SDL_Vulkan_GetDrawableSize(sdl.window, &w, &h);
#else
    SDL_Vulkan_GetDrawableSize(&w, &h);
#endif
}

#if !defined(C_SDL2)
/* SDL1: SDL_SetVideoMode() may recreate the window */
static void SDLCALL VK_WindowDestroyed(void *userdata) {
    (void)userdata;
    OUTPUT_VULKAN_ReleaseWindow();
}

/* SDL1: (re)size the window, or make it a Vulkan window */
static bool VK_SetVideoMode(int w, int h) {
    Uint32 flags = SDL_VULKAN;
    if (sdl.desktop.fullscreen)
        flags |= SDL_FULLSCREEN;
    else
        flags |= SDL_RESIZABLE;
#if defined(SDL_DOSBOX_X_SPECIAL)
    if (setSizeButNotResize()) flags |= SDL_HAX_NORESIZEWINDOW;
#endif
    SDL_Surface *video = SDL_SetVideoMode(w, h, 0, flags);
    if (video == NULL) {
        LOG_MSG("VULKAN: SDL_SetVideoMode failed: %s", SDL_GetError());
        return false;
    }
    sdl.deferred_resize = false;
    return true;
}
#endif

static bool VK_LoadLibrary(void) {
    if (vk.library_loaded) return true;

    if (SDL_Vulkan_LoadLibrary(NULL) != 0) {
        LOG_MSG("VULKAN: Unable to load the Vulkan library: %s", SDL_GetError());
        return false;
    }

    vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
    if (vkGetInstanceProcAddr == NULL) {
        LOG_MSG("VULKAN: vkGetInstanceProcAddr not available: %s", SDL_GetError());
        SDL_Vulkan_UnloadLibrary();
        return false;
    }

#define VKF(name) name = (PFN_##name)vkGetInstanceProcAddr(VK_NULL_HANDLE, #name); if (name == NULL) { LOG_MSG("VULKAN: missing " #name); SDL_Vulkan_UnloadLibrary(); return false; }
    VK_GLOBAL_FUNCS
#undef VKF

#if !defined(C_SDL2)
    SDL_Vulkan_SetWindowDestroyCallback(VK_WindowDestroyed, NULL);
#endif

    vk.library_loaded = true;
    return true;
}

static bool VK_CreateInstance(void) {
    unsigned int count = 0;
#if defined(C_SDL2)
#define VK_GETEXTENSIONS(c,n) SDL_Vulkan_GetInstanceExtensions(sdl.window, c, n)
#else
#define VK_GETEXTENSIONS(c,n) SDL_Vulkan_GetInstanceExtensions(c, n)
#endif
    if (!VK_GETEXTENSIONS(&count, NULL)) {
        LOG_MSG("VULKAN: SDL_Vulkan_GetInstanceExtensions failed: %s", SDL_GetError());
        return false;
    }
    vector<const char*> extensions(count);
    if (!VK_GETEXTENSIONS(&count, extensions.data())) {
        LOG_MSG("VULKAN: SDL_Vulkan_GetInstanceExtensions failed: %s", SDL_GetError());
        return false;
    }
    extensions.resize(count);
#undef VK_GETEXTENSIONS

    VkInstanceCreateFlags flags = 0;
#ifdef VK_KHR_portability_enumeration
    /* MoltenVK (macOS) is only enumerated if the application asks for portability drivers */
    uint32_t avail_count = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &avail_count, NULL);
    vector<VkExtensionProperties> avail(avail_count);
    vkEnumerateInstanceExtensionProperties(NULL, &avail_count, avail.data());
    for (const auto &e : avail) {
        if (!strcmp(e.extensionName, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
            extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }
    }
#endif

    VkApplicationInfo app = {};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "DOSBox-X";
    app.pEngineName = "DOSBox-X";
    app.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.flags = flags;
    ci.pApplicationInfo = &app;
    ci.enabledExtensionCount = (uint32_t)extensions.size();
    ci.ppEnabledExtensionNames = extensions.data();
    if (!VK_Check(vkCreateInstance(&ci, NULL, &vk.instance), "vkCreateInstance"))
        return false;

#define VKF(name) name = (PFN_##name)vkGetInstanceProcAddr(vk.instance, #name); if (name == NULL) { LOG_MSG("VULKAN: missing " #name); return false; }
    VK_INSTANCE_FUNCS
#undef VKF

    return true;
}

static bool VK_DeviceHasExtension(VkPhysicalDevice dev, const char *name) {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(dev, NULL, &count, NULL);
    vector<VkExtensionProperties> props(count);
    vkEnumerateDeviceExtensionProperties(dev, NULL, &count, props.data());
    for (const auto &p : props)
        if (!strcmp(p.extensionName, name)) return true;
    return false;
}

/* pick a device that can draw and present to the given surface, preferring real GPUs */
static bool VK_CreateDevice(void) {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(vk.instance, &count, NULL);
    vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(vk.instance, &count, devices.data());

    int best_score = -1;
    for (VkPhysicalDevice dev : devices) {
        if (!VK_DeviceHasExtension(dev, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) continue;

        uint32_t qcount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qcount, NULL);
        vector<VkQueueFamilyProperties> qprops(qcount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qcount, qprops.data());

        for (uint32_t q = 0; q < qcount; q++) {
            VkBool32 present = VK_FALSE;
            if (!(qprops[q].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;
            if (vkGetPhysicalDeviceSurfaceSupportKHR(dev, q, vk.surface, &present) != VK_SUCCESS || !present) continue;

            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(dev, &props);
            int score = 1;
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score = 4;
            else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score = 3;
            else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) score = 2;
            if (score > best_score) {
                best_score = score;
                vk.physical = dev;
                vk.queue_family = q;
            }
            break;
        }
    }

    if (best_score < 0) {
        LOG_MSG("VULKAN: No device can present to the window");
        return false;
    }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(vk.physical, &props);
    LOG_MSG("VULKAN: Using device %s", props.deviceName);
    vkGetPhysicalDeviceMemoryProperties(vk.physical, &vk.memory_properties);

    vector<const char*> extensions;
    extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if (VK_DeviceHasExtension(vk.physical, "VK_KHR_portability_subset"))
        extensions.push_back("VK_KHR_portability_subset");

    const float priority = 1.0f;
    VkDeviceQueueCreateInfo qci = {};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = vk.queue_family;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;

    VkDeviceCreateInfo dci = {};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = (uint32_t)extensions.size();
    dci.ppEnabledExtensionNames = extensions.data();
    if (!VK_Check(vkCreateDevice(vk.physical, &dci, NULL, &vk.device), "vkCreateDevice"))
        return false;

#define VKF(name) name = (PFN_##name)vkGetDeviceProcAddr(vk.device, #name); if (name == NULL) { LOG_MSG("VULKAN: missing " #name); return false; }
    VK_DEVICE_FUNCS
#undef VKF

    vkGetDeviceQueue(vk.device, vk.queue_family, 0, &vk.queue);
    return true;
}

/* device level objects that do not depend on the window */
static bool VK_CreateDeviceObjects(void) {
    VkCommandPoolCreateInfo pci = {};
    pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = vk.queue_family;
    if (!VK_Check(vkCreateCommandPool(vk.device, &pci, NULL, &vk.command_pool), "vkCreateCommandPool")) return false;

    VkCommandBufferAllocateInfo cai = {};
    cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cai.commandPool = vk.command_pool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    if (!VK_Check(vkAllocateCommandBuffers(vk.device, &cai, &vk.cmd), "vkAllocateCommandBuffers")) return false;

    VkSemaphoreCreateInfo sci = {};
    sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (!VK_Check(vkCreateSemaphore(vk.device, &sci, NULL, &vk.image_acquired), "vkCreateSemaphore")) return false;

    VkFenceCreateInfo fci = {};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (!VK_Check(vkCreateFence(vk.device, &fci, NULL, &vk.frame_fence), "vkCreateFence")) return false;

    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo dslci = {};
    dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslci.bindingCount = 1;
    dslci.pBindings = &binding;
    if (!VK_Check(vkCreateDescriptorSetLayout(vk.device, &dslci, NULL, &vk.set_layout), "vkCreateDescriptorSetLayout")) return false;

    VkDescriptorPoolSize pool_size = {};
    pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 2;
    VkDescriptorPoolCreateInfo dpci = {};
    dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.maxSets = 2;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes = &pool_size;
    if (!VK_Check(vkCreateDescriptorPool(vk.device, &dpci, NULL, &vk.descriptor_pool), "vkCreateDescriptorPool")) return false;

    VkDescriptorSetLayout layouts[2] = { vk.set_layout, vk.set_layout };
    VkDescriptorSet sets[2];
    VkDescriptorSetAllocateInfo dsai = {};
    dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool = vk.descriptor_pool;
    dsai.descriptorSetCount = 2;
    dsai.pSetLayouts = layouts;
    if (!VK_Check(vkAllocateDescriptorSets(vk.device, &dsai, sets), "vkAllocateDescriptorSets")) return false;
    vk.frame.set = sets[0];
    vk.overlay.set = sets[1];

    VkPushConstantRange pcr = {};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(float) * 8;
    VkPipelineLayoutCreateInfo plci = {};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &vk.set_layout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;
    if (!VK_Check(vkCreatePipelineLayout(vk.device, &plci, NULL, &vk.pipeline_layout), "vkCreatePipelineLayout")) return false;

    VkSamplerCreateInfo smp = {};
    smp.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    smp.magFilter = smp.minFilter = VK_FILTER_NEAREST;
    smp.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    smp.addressModeU = smp.addressModeV = smp.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    smp.maxLod = 0.0f;
    if (!VK_Check(vkCreateSampler(vk.device, &smp, NULL, &vk.sampler_nearest), "vkCreateSampler")) return false;
    smp.magFilter = smp.minFilter = VK_FILTER_LINEAR;
    if (!VK_Check(vkCreateSampler(vk.device, &smp, NULL, &vk.sampler_linear), "vkCreateSampler")) return false;

    return true;
}

static int VK_FindMemoryType(uint32_t type_bits, VkMemoryPropertyFlags want) {
    for (uint32_t i = 0; i < vk.memory_properties.memoryTypeCount; i++) {
        if ((type_bits & (1u << i)) && (vk.memory_properties.memoryTypes[i].propertyFlags & want) == want)
            return (int)i;
    }
    return -1;
}

static void VK_DestroyTexture(VulkanTexture &t) {
    if (t.staging_ptr) vkUnmapMemory(vk.device, t.staging_memory);
    if (t.staging != VK_NULL_HANDLE) vkDestroyBuffer(vk.device, t.staging, NULL);
    if (t.staging_memory != VK_NULL_HANDLE) vkFreeMemory(vk.device, t.staging_memory, NULL);
    if (t.view != VK_NULL_HANDLE) vkDestroyImageView(vk.device, t.view, NULL);
    if (t.image != VK_NULL_HANDLE) vkDestroyImage(vk.device, t.image, NULL);
    if (t.image_memory != VK_NULL_HANDLE) vkFreeMemory(vk.device, t.image_memory, NULL);
    t.staging_ptr = NULL;
    t.staging = VK_NULL_HANDLE;
    t.staging_memory = VK_NULL_HANDLE;
    t.view = VK_NULL_HANDLE;
    t.image = VK_NULL_HANDLE;
    t.image_memory = VK_NULL_HANDLE;
    t.bound_sampler = VK_NULL_HANDLE;
    t.width = t.height = 0;
    t.initialized = false;
}

/* (re)create a BGRA texture, the caller must make sure the GPU is idle */
static bool VK_CreateTexture(VulkanTexture &t, uint32_t w, uint32_t h) {
    if (t.image != VK_NULL_HANDLE && t.width == w && t.height == h) return true;
    VK_DestroyTexture(t);

    VkImageCreateInfo ici = {};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = VK_FORMAT_B8G8R8A8_UNORM;
    ici.extent.width = w;
    ici.extent.height = h;
    ici.extent.depth = 1;
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!VK_Check(vkCreateImage(vk.device, &ici, NULL, &t.image), "vkCreateImage")) return false;

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(vk.device, t.image, &req);
    int type = VK_FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (type < 0) type = VK_FindMemoryType(req.memoryTypeBits, 0);
    VkMemoryAllocateInfo mai = {};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = (uint32_t)type;
    if (type < 0 || !VK_Check(vkAllocateMemory(vk.device, &mai, NULL, &t.image_memory), "vkAllocateMemory")) return false;
    if (!VK_Check(vkBindImageMemory(vk.device, t.image, t.image_memory, 0), "vkBindImageMemory")) return false;

    VkImageViewCreateInfo vci = {};
    vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image = t.image;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = VK_FORMAT_B8G8R8A8_UNORM;
    vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vci.subresourceRange.levelCount = 1;
    vci.subresourceRange.layerCount = 1;
    if (!VK_Check(vkCreateImageView(vk.device, &vci, NULL, &t.view), "vkCreateImageView")) return false;

    VkBufferCreateInfo bci = {};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = (VkDeviceSize)w * h * 4;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (!VK_Check(vkCreateBuffer(vk.device, &bci, NULL, &t.staging), "vkCreateBuffer")) return false;

    vkGetBufferMemoryRequirements(vk.device, t.staging, &req);
    type = VK_FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = (uint32_t)type;
    if (type < 0 || !VK_Check(vkAllocateMemory(vk.device, &mai, NULL, &t.staging_memory), "vkAllocateMemory")) return false;
    if (!VK_Check(vkBindBufferMemory(vk.device, t.staging, t.staging_memory, 0), "vkBindBufferMemory")) return false;
    void *ptr = NULL;
    if (!VK_Check(vkMapMemory(vk.device, t.staging_memory, 0, VK_WHOLE_SIZE, 0, &ptr), "vkMapMemory")) return false;
    t.staging_ptr = (uint8_t*)ptr;

    t.width = w;
    t.height = h;
    t.initialized = false;
    return true;
}

static void VK_BindTexture(VulkanTexture &t, VkSampler sampler) {
    if (t.bound_sampler == sampler) return;

    VkDescriptorImageInfo dii = {};
    dii.sampler = sampler;
    dii.imageView = t.view;
    dii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet w = {};
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = t.set;
    w.dstBinding = 0;
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo = &dii;
    vkUpdateDescriptorSets(vk.device, 1, &w, 0, NULL);
    t.bound_sampler = sampler;
}

static VkShaderModule VK_CreateShader(const uint32_t *code, size_t size) {
    VkShaderModule mod = VK_NULL_HANDLE;
    VkShaderModuleCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = size;
    ci.pCode = code;
    if (!VK_Check(vkCreateShaderModule(vk.device, &ci, NULL, &mod), "vkCreateShaderModule")) return VK_NULL_HANDLE;
    return mod;
}

static void VK_DestroyRenderPass(void) {
    if (vk.pipeline_opaque != VK_NULL_HANDLE) vkDestroyPipeline(vk.device, vk.pipeline_opaque, NULL);
    if (vk.pipeline_blend != VK_NULL_HANDLE) vkDestroyPipeline(vk.device, vk.pipeline_blend, NULL);
    if (vk.render_pass != VK_NULL_HANDLE) vkDestroyRenderPass(vk.device, vk.render_pass, NULL);
    vk.pipeline_opaque = vk.pipeline_blend = VK_NULL_HANDLE;
    vk.render_pass = VK_NULL_HANDLE;
    vk.render_pass_format = VK_FORMAT_UNDEFINED;
}

/* the render pass and pipelines depend only on the swapchain format */
static bool VK_CreateRenderPass(VkFormat format) {
    if (vk.render_pass != VK_NULL_HANDLE && vk.render_pass_format == format) return true;
    VK_DestroyRenderPass();

    VkAttachmentDescription att = {};
    att.format = format;
    att.samples = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference ref = {};
    ref.attachment = 0;
    ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkSubpassDescription sub = {};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &ref;
    /* wait for the swapchain image to be acquired before writing to it */
    VkSubpassDependency dep = {};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo rpci = {};
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = 1;
    rpci.pAttachments = &att;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &sub;
    rpci.dependencyCount = 1;
    rpci.pDependencies = &dep;
    if (!VK_Check(vkCreateRenderPass(vk.device, &rpci, NULL, &vk.render_pass), "vkCreateRenderPass")) return false;
    vk.render_pass_format = format;

    VkShaderModule vs = VK_CreateShader(vulkan_vert_spv, sizeof(vulkan_vert_spv));
    VkShaderModule fs = VK_CreateShader(vulkan_frag_spv, sizeof(vulkan_frag_spv));
    bool ok = false;
    if (vs != VK_NULL_HANDLE && fs != VK_NULL_HANDLE) {
        VkPipelineShaderStageCreateInfo stages[2] = {};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vs;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fs;
        stages[1].pName = "main";

        VkPipelineVertexInputStateCreateInfo vin = {};
        vin.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VkPipelineInputAssemblyStateCreateInfo ia = {};
        ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        VkPipelineViewportStateCreateInfo vps = {};
        vps.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vps.viewportCount = 1;
        vps.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo rs = {};
        rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.cullMode = VK_CULL_MODE_NONE;
        rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rs.lineWidth = 1.0f;
        VkPipelineMultisampleStateCreateInfo ms = {};
        ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState cba = {};
        cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo cb = {};
        cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.attachmentCount = 1;
        cb.pAttachments = &cba;
        VkDynamicState dyn_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dyn = {};
        dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dyn.dynamicStateCount = 2;
        dyn.pDynamicStates = dyn_states;

        VkGraphicsPipelineCreateInfo gpci = {};
        gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gpci.stageCount = 2;
        gpci.pStages = stages;
        gpci.pVertexInputState = &vin;
        gpci.pInputAssemblyState = &ia;
        gpci.pViewportState = &vps;
        gpci.pRasterizationState = &rs;
        gpci.pMultisampleState = &ms;
        gpci.pColorBlendState = &cb;
        gpci.pDynamicState = &dyn;
        gpci.layout = vk.pipeline_layout;
        gpci.renderPass = vk.render_pass;
        gpci.subpass = 0;

        /* opaque: the emulator image */
        ok = VK_Check(vkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &gpci, NULL, &vk.pipeline_opaque), "vkCreateGraphicsPipelines");

        /* alpha blended: the menu overlay */
        cba.blendEnable = VK_TRUE;
        cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        cba.colorBlendOp = VK_BLEND_OP_ADD;
        cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        cba.alphaBlendOp = VK_BLEND_OP_ADD;
        if (ok) ok = VK_Check(vkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &gpci, NULL, &vk.pipeline_blend), "vkCreateGraphicsPipelines");
    }
    if (vs != VK_NULL_HANDLE) vkDestroyShaderModule(vk.device, vs, NULL);
    if (fs != VK_NULL_HANDLE) vkDestroyShaderModule(vk.device, fs, NULL);
    return ok;
}

static void VK_DestroySwapchainResources(void) {
    for (auto fb : vk.framebuffers) vkDestroyFramebuffer(vk.device, fb, NULL);
    for (auto v : vk.swapchain_views) vkDestroyImageView(vk.device, v, NULL);
    for (auto s : vk.render_done) vkDestroySemaphore(vk.device, s, NULL);
    vk.framebuffers.clear();
    vk.swapchain_views.clear();
    vk.render_done.clear();
    vk.swapchain_images.clear();
}

static VkPresentModeKHR VK_ChoosePresentMode(void) {
    bool vsync = false;
    Section_prop* sec = static_cast<Section_prop*>(control->GetSection("vsync"));
    if (sec) vsync = !strcmp(sec->Get_string("vsyncmode"), "host");
    if (vsync) return VK_PRESENT_MODE_FIFO_KHR;

    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vk.physical, vk.surface, &count, NULL);
    vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(vk.physical, vk.surface, &count, modes.data());
    if (find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.end()) return VK_PRESENT_MODE_MAILBOX_KHR;
    if (find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end()) return VK_PRESENT_MODE_IMMEDIATE_KHR;
    return VK_PRESENT_MODE_FIFO_KHR; // always supported
}

/* (re)create the swapchain at the window's current drawable size */
static bool VK_CreateSwapchain(void) {
    if (vk.surface == VK_NULL_HANDLE) return false;
    vkDeviceWaitIdle(vk.device);
    VK_DestroySwapchainResources();

    VkSurfaceCapabilitiesKHR caps;
    if (!VK_Check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.physical, vk.surface, &caps), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR")) return false;

    VkExtent2D extent = caps.currentExtent;
    int draw_w = 0, draw_h = 0;
    VK_GetDrawableSize(draw_w, draw_h);
    if (extent.width == 0xFFFFFFFFu) {
        extent.width = (uint32_t)max(draw_w, 1);
        extent.height = (uint32_t)max(draw_h, 1);
    }
    extent.width = max(caps.minImageExtent.width, min(caps.maxImageExtent.width, extent.width));
    extent.height = max(caps.minImageExtent.height, min(caps.maxImageExtent.height, extent.height));
    if (extent.width == 0 || extent.height == 0) return false; // minimized

    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vk.physical, vk.surface, &count, NULL);
    vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vk.physical, vk.surface, &count, formats.data());
    if (formats.empty()) return false;
    /* UNORM, so that the emulator's colors are not gamma corrected again */
    VkSurfaceFormatKHR format = formats[0];
    for (const auto &f : formats) {
        if ((f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_UNORM) && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            format = f;
            break;
        }
    }
    if (format.format == VK_FORMAT_UNDEFINED) format.format = VK_FORMAT_B8G8R8A8_UNORM;

    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) image_count = caps.maxImageCount;

    VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (!(caps.supportedCompositeAlpha & alpha)) {
        if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) alpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        else if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) alpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
        else alpha = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
    }

    vk.present_mode = VK_ChoosePresentMode();

    VkSwapchainKHR old = vk.swapchain;
    VkSwapchainCreateInfoKHR sci = {};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = vk.surface;
    sci.minImageCount = image_count;
    sci.imageFormat = format.format;
    sci.imageColorSpace = format.colorSpace;
    sci.imageExtent = extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = (caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : caps.currentTransform;
    sci.compositeAlpha = alpha;
    sci.presentMode = vk.present_mode;
    sci.clipped = VK_TRUE;
    sci.oldSwapchain = old;
    VkResult res = vkCreateSwapchainKHR(vk.device, &sci, NULL, &vk.swapchain);
    if (old != VK_NULL_HANDLE) vkDestroySwapchainKHR(vk.device, old, NULL);
    if (!VK_Check(res, "vkCreateSwapchainKHR")) {
        vk.swapchain = VK_NULL_HANDLE;
        return false;
    }
    vk.swapchain_format = format.format;
    vk.swapchain_extent = extent;
    vk.swapchain_drawable_w = draw_w;
    vk.swapchain_drawable_h = draw_h;
    vk.swapchain_dirty = false;

    if (!VK_CreateRenderPass(format.format)) return false;

    vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &count, NULL);
    vk.swapchain_images.resize(count);
    vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &count, vk.swapchain_images.data());

    for (VkImage img : vk.swapchain_images) {
        VkImageViewCreateInfo vci = {};
        vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vci.image = img;
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = format.format;
        vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.layerCount = 1;
        VkImageView view = VK_NULL_HANDLE;
        if (!VK_Check(vkCreateImageView(vk.device, &vci, NULL, &view), "vkCreateImageView")) return false;
        vk.swapchain_views.push_back(view);

        VkFramebufferCreateInfo fci = {};
        fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.renderPass = vk.render_pass;
        fci.attachmentCount = 1;
        fci.pAttachments = &view;
        fci.width = extent.width;
        fci.height = extent.height;
        fci.layers = 1;
        VkFramebuffer fb = VK_NULL_HANDLE;
        if (!VK_Check(vkCreateFramebuffer(vk.device, &fci, NULL, &fb), "vkCreateFramebuffer")) return false;
        vk.framebuffers.push_back(fb);

        VkSemaphoreCreateInfo semci = {};
        semci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkSemaphore sem = VK_NULL_HANDLE;
        if (!VK_Check(vkCreateSemaphore(vk.device, &semci, NULL, &sem), "vkCreateSemaphore")) return false;
        vk.render_done.push_back(sem);
    }

    LOG(LOG_MISC,LOG_DEBUG)("VULKAN: swapchain %ux%u, %u images, present mode %d",
        extent.width, extent.height, (unsigned int)count, (int)vk.present_mode);
    return true;
}

void OUTPUT_VULKAN_ReleaseWindow() {
    if (vk.device == VK_NULL_HANDLE) return;

    vkDeviceWaitIdle(vk.device);
    VK_DestroySwapchainResources();
    if (vk.swapchain != VK_NULL_HANDLE) vkDestroySwapchainKHR(vk.device, vk.swapchain, NULL);
    if (vk.surface != VK_NULL_HANDLE) vkDestroySurfaceKHR(vk.instance, vk.surface, NULL);
    vk.swapchain = VK_NULL_HANDLE;
    vk.surface = VK_NULL_HANDLE;
    vk.surface_window = NULL;
}

/* make sure there is a Vulkan surface for the window, creating the instance
 * and device the first time around */
static bool VK_AttachWindow(void) {
    if (VK_CurrentWindow() == NULL) return false;
    if (!VK_WindowIsVulkan()) {
        LOG_MSG("VULKAN: window was not created for Vulkan");
        return false;
    }
    if (vk.surface != VK_NULL_HANDLE && vk.surface_window == VK_CurrentWindow()) return true;
    if (vk.surface != VK_NULL_HANDLE) OUTPUT_VULKAN_ReleaseWindow();

    if (vk.instance == VK_NULL_HANDLE && !VK_CreateInstance()) return false;

#if defined(C_SDL2)
    if (!SDL_Vulkan_CreateSurface(sdl.window, vk.instance, &vk.surface)) {
#else
    if (!SDL_Vulkan_CreateSurface(vk.instance, &vk.surface)) {
#endif
        LOG_MSG("VULKAN: SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
        vk.surface = VK_NULL_HANDLE;
        return false;
    }
    vk.surface_window = VK_CurrentWindow();

    if (vk.device == VK_NULL_HANDLE) {
        if (!VK_CreateDevice() || !VK_CreateDeviceObjects()) return false;
    }
    else {
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(vk.physical, vk.queue_family, vk.surface, &present);
        if (!present) {
            LOG_MSG("VULKAN: The device can not present to the new window");
            return false;
        }
    }

    return VK_CreateSwapchain();
}

static void VK_RecordUpload(VulkanTexture &t, const vector<VkBufferImageCopy> &regions) {
    VkImageMemoryBarrier b = {};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.srcAccessMask = t.initialized ? VK_ACCESS_SHADER_READ_BIT : 0;
    b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    b.oldLayout = t.initialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
    b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = t.image;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(vk.cmd,
        t.initialized ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &b);

    vkCmdCopyBufferToImage(vk.cmd, t.staging, t.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)regions.size(), regions.data());

    b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(vk.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, NULL, 0, NULL, 1, &b);
    t.initialized = true;
}

static VkBufferImageCopy VK_RowsRegion(const VulkanTexture &t, uint32_t y, uint32_t h) {
    VkBufferImageCopy r = {};
    r.bufferOffset = (VkDeviceSize)y * t.width * 4;
    r.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    r.imageSubresource.layerCount = 1;
    r.imageOffset.y = (int32_t)y;
    r.imageExtent.width = t.width;
    r.imageExtent.height = h;
    r.imageExtent.depth = 1;
    return r;
}

static void VK_DrawRect(VulkanTexture &t, VkPipeline pipeline, const SDL_Rect &rect, int win_w, int win_h) {
    /* window coordinates to normalized device coordinates, the drawable may be
     * larger than the window on high DPI displays */
    const float pc[8] = {
        ((float)rect.x * 2.0f / (float)win_w) - 1.0f,
        ((float)rect.y * 2.0f / (float)win_h) - 1.0f,
        (float)rect.w * 2.0f / (float)win_w,
        (float)rect.h * 2.0f / (float)win_h,
        0.0f, 0.0f, 1.0f, 1.0f
    };
    vkCmdBindPipeline(vk.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(vk.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipeline_layout, 0, 1, &t.set, 0, NULL);
    vkCmdPushConstants(vk.cmd, vk.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), pc);
    vkCmdDraw(vk.cmd, 4, 1, 0, 0);
}

/* Draw the emulator image and the menu overlay to the window. If upload is
 * set, the rows of the CPU frame buffer listed in changedLines (or all of
 * them, if NULL) are sent to the GPU first. */
static void VK_Present(bool upload, const uint16_t *changedLines) {
    if (vk.device == VK_NULL_HANDLE || vk.surface == VK_NULL_HANDLE || vk.surface_window != VK_CurrentWindow()) return;

    int dw = 0, dh = 0;
    VK_GetDrawableSize(dw, dh);
    if (vk.swapchain == VK_NULL_HANDLE || vk.swapchain_dirty ||
        dw != vk.swapchain_drawable_w || dh != vk.swapchain_drawable_h) {
        if (!VK_CreateSwapchain()) return;
    }

    vkWaitForFences(vk.device, 1, &vk.frame_fence, VK_TRUE, UINT64_MAX);

    uint32_t index = 0;
    VkResult res = vkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX, vk.image_acquired, VK_NULL_HANDLE, &index);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        if (!VK_CreateSwapchain()) return;
        res = vkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX, vk.image_acquired, VK_NULL_HANDLE, &index);
    }
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        vk.swapchain_dirty = true;
        return;
    }

    VkCommandBufferBeginInfo bi = {};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkResetCommandBuffer(vk.cmd, 0);
    vkBeginCommandBuffer(vk.cmd, &bi);

    VulkanTexture &frame = vk.frame;
    if (upload && frame.image != VK_NULL_HANDLE && !vk.framebuf.empty()) {
        const uint32_t pitch = frame.width * 4;
        vector<VkBufferImageCopy> regions;
        if (changedLines != NULL && frame.initialized && frame.height == sdl.draw.height) {
            uint32_t y = 0;
            for (unsigned int i = 0; y < frame.height; i++) {
                uint32_t h = changedLines[i];
                if (i & 1) {
                    if (y + h > frame.height) h = frame.height - y;
                    memcpy(frame.staging_ptr + y * pitch, (uint8_t*)vk.framebuf.data() + y * pitch, (size_t)h * pitch);
                    regions.push_back(VK_RowsRegion(frame, y, h));
                }
                y += h;
            }
        }
        else {
            memcpy(frame.staging_ptr, vk.framebuf.data(), (size_t)frame.height * pitch);
            regions.push_back(VK_RowsRegion(frame, 0, frame.height));
        }
        if (!regions.empty()) VK_RecordUpload(frame, regions);
    }

    VulkanTexture &overlay = vk.overlay;
    if (vk.overlay_dirty && vk.overlay_surface != NULL && overlay.image != VK_NULL_HANDLE) {
        SDL_Surface *s = vk.overlay_surface;
        const uint32_t rows = min((uint32_t)s->h, overlay.height);
        const uint32_t row_bytes = min((uint32_t)s->w, overlay.width) * 4;
        for (uint32_t y = 0; y < rows; y++)
            memcpy(overlay.staging_ptr + y * overlay.width * 4, (uint8_t*)s->pixels + y * s->pitch, row_bytes);
        vector<VkBufferImageCopy> regions(1, VK_RowsRegion(overlay, 0, overlay.height));
        VK_RecordUpload(overlay, regions);
        vk.overlay_dirty = false;
    }

    VkClearValue clear = {};
    clear.color.float32[3] = 1.0f;
    VkRenderPassBeginInfo rpbi = {};
    rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpbi.renderPass = vk.render_pass;
    rpbi.framebuffer = vk.framebuffers[index];
    rpbi.renderArea.extent = vk.swapchain_extent;
    rpbi.clearValueCount = 1;
    rpbi.pClearValues = &clear;
    vkCmdBeginRenderPass(vk.cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp = {};
    vp.width = (float)vk.swapchain_extent.width;
    vp.height = (float)vk.swapchain_extent.height;
    vp.maxDepth = 1.0f;
    VkRect2D scissor = {};
    scissor.extent = vk.swapchain_extent;
    vkCmdSetViewport(vk.cmd, 0, 1, &vp);
    vkCmdSetScissor(vk.cmd, 0, 1, &scissor);

    int win_w = 0, win_h = 0;
    VK_GetWindowSize(win_w, win_h);
    if (vk.overlay_surface != NULL) {
        /* everything is laid out in the coordinates of the surface sdl.clip was computed for */
        win_w = vk.overlay_surface->w;
        win_h = vk.overlay_surface->h;
    }
    if (win_w > 0 && win_h > 0) {
        if (frame.initialized) {
            VK_BindTexture(frame, vk.frame_linear ? vk.sampler_linear : vk.sampler_nearest);
            VK_DrawRect(frame, vk.pipeline_opaque, sdl.clip, win_w, win_h);
        }
        if (overlay.initialized) {
            SDL_Rect all;
            all.x = all.y = 0;
            all.w = win_w;
            all.h = win_h;
            VK_BindTexture(overlay, vk.sampler_nearest);
            VK_DrawRect(overlay, vk.pipeline_blend, all, win_w, win_h);
        }
    }

    vkCmdEndRenderPass(vk.cmd);
    vkEndCommandBuffer(vk.cmd);

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si = {};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &vk.image_acquired;
    si.pWaitDstStageMask = &wait_stage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &vk.cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &vk.render_done[index];
    vkResetFences(vk.device, 1, &vk.frame_fence);
    if (!VK_Check(vkQueueSubmit(vk.queue, 1, &si, vk.frame_fence), "vkQueueSubmit")) {
        /* the fence was not submitted, signal it again so the next wait does not hang */
        vkDeviceWaitIdle(vk.device);
        VkFenceCreateInfo fci = {};
        fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkDestroyFence(vk.device, vk.frame_fence, NULL);
        vkCreateFence(vk.device, &fci, NULL, &vk.frame_fence);
        vk.swapchain_dirty = true;
        return;
    }

    VkPresentInfoKHR pi = {};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &vk.render_done[index];
    pi.swapchainCount = 1;
    pi.pSwapchains = &vk.swapchain;
    pi.pImageIndices = &index;
    res = vkQueuePresentKHR(vk.queue, &pi);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
        vk.swapchain_dirty = true;
}

/* sdl.surface becomes a window sized overlay that SDL drawn menus (and
 * anything else that draws to sdl.surface) go to. Transparent where nothing
 * is drawn. */
static bool VK_SetupOverlay(int w, int h) {
    if (vk.overlay_surface == NULL || vk.overlay_surface->w != w || vk.overlay_surface->h != h) {
        if (vk.overlay_surface != NULL) {
            if (sdl.surface == vk.overlay_surface) sdl.surface = NULL;
            SDL_FreeSurface(vk.overlay_surface);
        }
#if defined(C_SDL2)
        vk.overlay_surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
#else
        vk.overlay_surface = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
#endif
        if (vk.overlay_surface == NULL) {
            LOG_MSG("VULKAN: Unable to create overlay surface: %s", SDL_GetError());
            return false;
        }
    }
    SDL_FillRect(vk.overlay_surface, NULL, 0);
#if !defined(C_SDL2)
    /* SDL1 code looks at sdl.surface for the window's flags */
    SDL_Surface *video = SDL_GetVideoSurface();
    vk.overlay_surface->flags &= ~(SDL_RESIZABLE|SDL_FULLSCREEN);
    if (video) vk.overlay_surface->flags |= video->flags & (SDL_RESIZABLE|SDL_FULLSCREEN);
#endif
    sdl.surface = vk.overlay_surface;
    vk.overlay_dirty = true;
    return true;
}

// output API below

void OUTPUT_VULKAN_Select()
{
    sdl.desktop.want_type = SCREEN_VULKAN;
    render.aspectOffload = true;

#if defined(C_SDL2)
    void GFX_SetResizeable(bool enable);
    GFX_SetResizeable(true);
#endif

    /* load the library first, SDL can not create a Vulkan window without it */
    if (VK_LoadLibrary()) {
#if defined(C_SDL2)
        sdl.window = GFX_SetSDLWindowMode(640,400, SCREEN_VULKAN);
#else
        VK_SetVideoMode(640, 400);
#endif
        if (VK_AttachWindow()) {
            int w = 0, h = 0;
            VK_GetWindowSize(w, h);
            if (VK_SetupOverlay(w, h)) {
                ApplyPreventCap();
                return;
            }
        }
    }

    LOG_MSG("Could not initialize Vulkan, switching back to surface");
    sdl.desktop.want_type = SCREEN_SURFACE;
}

Bitu OUTPUT_VULKAN_GetBestMode(Bitu flags)
{
    if (!(flags & GFX_CAN_32)) return 0; // Vulkan output uses 32-bit BGRA
    flags |= GFX_SCALING;
    flags &= ~(GFX_CAN_8 | GFX_CAN_15 | GFX_CAN_16);
    return flags;
}

/* same window sizing rules as the OpenGL output */
static bool SetupWindowScaledVulkan(void)
{
    uint16_t fixedWidth;
    uint16_t fixedHeight;
    uint16_t windowWidth;
    uint16_t windowHeight;

    if (sdl.desktop.fullscreen)
    {
        fixedWidth = sdl.desktop.full.fixed ? sdl.desktop.full.width : 0;
        fixedHeight = sdl.desktop.full.fixed ? sdl.desktop.full.height : 0;
    }
    else
    {
        fixedWidth = sdl.desktop.window.width;
        fixedHeight = sdl.desktop.window.height;
    }

    if (vga.draw.modeswitch_set)
        fixedHeight = 0;
    else if (fixedWidth == 0 || fixedHeight == 0)
    {
        Bitu consider_height = menu.maxwindow ? currentWindowHeight : 0;
        Bitu consider_width = menu.maxwindow ? currentWindowWidth : 0;
        int final_height = (int)max(consider_height, userResizeWindowHeight);
        int final_width = (int)max(consider_width, userResizeWindowWidth);

        fixedWidth = final_width;
        fixedHeight = final_height;
    }

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    /* scale the menu bar if the window is large enough */
    {
        int cw = fixedWidth, ch = fixedHeight;
        int scale = 1;

        if (cw == 0)
            cw = (uint16_t)(sdl.draw.width*sdl.draw.scalex);
        if (ch == 0)
            ch = (uint16_t)(sdl.draw.height*sdl.draw.scaley);

        while ((cw / scale) >= (640 * 2) && (ch / scale) >= (400 * 2))
            scale++;

        mainMenu.setScale((unsigned int)scale);

        if (mainMenu.isVisible() && !sdl.desktop.fullscreen && fixedHeight)
            fixedHeight -= mainMenu.menuBox.h;
    }
#endif

    sdl.clip.x = 0; sdl.clip.y = 0;
    if (fixedWidth && fixedHeight)
    {
        windowWidth  = fixedWidth;
        windowHeight = fixedHeight;
        sdl.clip.w = windowWidth;
        sdl.clip.h = windowHeight;
        if (render.aspect) aspectCorrectFitClip(sdl.clip.w, sdl.clip.h, sdl.clip.x, sdl.clip.y, fixedWidth, fixedHeight);
    }
    else
    {
        windowWidth = (uint16_t)(sdl.draw.width * sdl.draw.scalex);
        windowHeight = (uint16_t)(sdl.draw.height * sdl.draw.scaley);
        if (render.aspect) aspectCorrectExtend(windowWidth, windowHeight);
        sdl.clip.w = windowWidth; sdl.clip.h = windowHeight;
    }

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    if (mainMenu.isVisible() && !sdl.desktop.fullscreen)
    {
        windowHeight += mainMenu.menuBox.h;
        sdl.clip.y += mainMenu.menuBox.h;
    }
#endif

    LOG(LOG_MISC, LOG_DEBUG)("GFX_SetSize Vulkan window=%ux%u clip=x,y,w,h=%d,%d,%d,%d",
        (unsigned int)windowWidth,
        (unsigned int)windowHeight,
        (unsigned int)sdl.clip.x,
        (unsigned int)sdl.clip.y,
        (unsigned int)sdl.clip.w,
        (unsigned int)sdl.clip.h);

#if defined(C_SDL2)
    sdl.window = GFX_SetSDLWindowMode(windowWidth, windowHeight, SCREEN_VULKAN);
    if (sdl.window == NULL && sdl.desktop.fullscreen) {
        LOG_MSG("Fullscreen not supported: %s", SDL_GetError());
        sdl.desktop.fullscreen = false;
        GFX_CaptureMouse();
        sdl.window = GFX_SetSDLWindowMode(windowWidth, windowHeight, SCREEN_VULKAN);
    }
    return sdl.window != NULL;
#else
    if (!VK_SetVideoMode(windowWidth, windowHeight) && sdl.desktop.fullscreen) {
        LOG_MSG("Fullscreen not supported: %s", SDL_GetError());
        sdl.desktop.fullscreen = false;
        GFX_CaptureMouse();
        return VK_SetVideoMode(windowWidth, windowHeight);
    }
    return VK_WindowIsVulkan();
#endif
}

Bitu OUTPUT_VULKAN_SetSize()
{
    if (!VK_LoadLibrary() || !SetupWindowScaledVulkan() || !VK_AttachWindow()) {
        LOG_MSG("VULKAN: Unable to set up the window, falling back to surface");
        return 0;
    }

    vkDeviceWaitIdle(vk.device);

    int win_w = 0, win_h = 0;
    VK_GetWindowSize(win_w, win_h);
    if (win_w <= 0 || win_h <= 0 || !VK_SetupOverlay(win_w, win_h)) return 0;

    uint32_t texWidth = sdl.draw.width;
    uint32_t texHeight = sdl.draw.height;
#if C_XBRZ
    if (sdl_xbrz.enable && xBRZ_SetScaleParameters((int)texWidth, (int)texHeight, (int)sdl.clip.w, (int)sdl.clip.h))
    {
        texWidth = texWidth * (unsigned int)sdl_xbrz.scale_factor;
        texHeight = texHeight * (unsigned int)sdl_xbrz.scale_factor;
    }
#endif

    if (!VK_CreateTexture(vk.frame, texWidth, texHeight) ||
        !VK_CreateTexture(vk.overlay, (uint32_t)win_w, (uint32_t)win_h)) {
        LOG_MSG("VULKAN: Unable to create textures, falling back to surface");
        return 0;
    }
    vk.frame.initialized = false;
    vk.framebuf.assign((size_t)texWidth * texHeight, 0);
    vk.framebuf_pitch = texWidth * 4;

    /* nearest neighbor if asked to, or when it would not make a difference anyway */
    Section_prop* section = static_cast<Section_prop*>(control->GetSection("render"));
    const bool want_nearest = section && !strcmp(section->Get_string("aspect"), "nearest");
    vk.frame_linear = !want_nearest && !(sdl.clip.w % texWidth == 0 && sdl.clip.h % texHeight == 0);

    sdl.deferred_resize = false;
    sdl.must_redraw_all = true;

    UpdateWindowDimensions();
    GFX_LogSDLState();
    ApplyPreventCap();

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    mainMenu.screenWidth = (size_t)(sdl.surface->w);
    mainMenu.screenHeight = (size_t)(sdl.surface->h);
    mainMenu.updateRect();
    mainMenu.setRedraw();
    GFX_DrawSDLMenu(mainMenu, mainMenu.display_list);
#endif

    /* show the menu (and black where the emulator image will be) right away */
    VK_Present(false, NULL);

    return GFX_CAN_32 | GFX_SCALING;
}

bool OUTPUT_VULKAN_StartUpdate(uint8_t* &pixels, Bitu &pitch)
{
    if (vk.framebuf.empty()) return false;

#if C_XBRZ
    if (sdl_xbrz.enable && sdl_xbrz.scale_on)
    {
        sdl_xbrz.renderbuf.resize(sdl.draw.width * sdl.draw.height);
        pixels = sdl_xbrz.renderbuf.empty() ? nullptr : reinterpret_cast<uint8_t*>(&sdl_xbrz.renderbuf[0]);
        pitch = sdl.draw.width * sizeof(uint32_t);
    }
    else
#endif
    {
        pixels = (uint8_t*)vk.framebuf.data();
        pitch = vk.framebuf_pitch;
    }

    sdl.updating = true;
    return true;
}

void OUTPUT_VULKAN_EndUpdate(const uint16_t *changedLines)
{
    if (sdl.must_redraw_all && changedLines == NULL) return;

#if C_XBRZ
    if (sdl_xbrz.enable && sdl_xbrz.scale_on)
    {
        const uint32_t srcWidth = sdl.draw.width;
        const uint32_t srcHeight = sdl.draw.height;
        if (changedLines == NULL || sdl_xbrz.renderbuf.size() != (size_t)srcWidth * srcHeight || srcWidth == 0 || srcHeight == 0)
            return;
        // the frame texture is sized for the xBRZ output, render buffer is not scaled
        xBRZ_Render(&sdl_xbrz.renderbuf[0], vk.framebuf.data(), changedLines, (int)srcWidth, (int)srcHeight, sdl_xbrz.scale_factor);
        VK_Present(true, NULL);
    }
    else
#endif
    if (changedLines != NULL)
    {
        if (changedLines[0] == sdl.draw.height) {
            /* nothing changed, but the menu might have */
            if (vk.overlay_dirty) VK_Present(false, NULL);
            return;
        }
        VK_Present(true, changedLines);
    }
    else
        return;

    if (!menu.hidecycles && !sdl.desktop.fullscreen) frames++;
}

bool OUTPUT_VULKAN_UpdateOverlay()
{
    if (vk.overlay_surface == NULL || sdl.surface != vk.overlay_surface) return false;

    vk.overlay_dirty = true;
    /* while the emulator is drawing a frame, the menu is shown along with it
     * when it is done */
    if (!sdl.updating) VK_Present(false, NULL);
    return true;
}

void OUTPUT_VULKAN_Shutdown()
{
    if (vk.device == VK_NULL_HANDLE) return;

    OUTPUT_VULKAN_ReleaseWindow();
#if !defined(C_SDL2)
    SDL_Vulkan_SetWindowDestroyCallback(NULL, NULL);
#endif
    if (vk.overlay_surface != NULL) {
        if (sdl.surface == vk.overlay_surface) sdl.surface = NULL;
        SDL_FreeSurface(vk.overlay_surface);
        vk.overlay_surface = NULL;
    }
    VK_DestroyTexture(vk.frame);
    VK_DestroyTexture(vk.overlay);
    VK_DestroyRenderPass();
    vk.framebuf.clear();
    if (vk.sampler_nearest != VK_NULL_HANDLE) vkDestroySampler(vk.device, vk.sampler_nearest, NULL);
    if (vk.sampler_linear != VK_NULL_HANDLE) vkDestroySampler(vk.device, vk.sampler_linear, NULL);
    if (vk.pipeline_layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(vk.device, vk.pipeline_layout, NULL);
    if (vk.descriptor_pool != VK_NULL_HANDLE) vkDestroyDescriptorPool(vk.device, vk.descriptor_pool, NULL);
    if (vk.set_layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(vk.device, vk.set_layout, NULL);
    if (vk.frame_fence != VK_NULL_HANDLE) vkDestroyFence(vk.device, vk.frame_fence, NULL);
    if (vk.image_acquired != VK_NULL_HANDLE) vkDestroySemaphore(vk.device, vk.image_acquired, NULL);
    if (vk.command_pool != VK_NULL_HANDLE) vkDestroyCommandPool(vk.device, vk.command_pool, NULL);
    vkDestroyDevice(vk.device, NULL);
    vkDestroyInstance(vk.instance, NULL);

    vk.sampler_nearest = vk.sampler_linear = VK_NULL_HANDLE;
    vk.pipeline_layout = VK_NULL_HANDLE;
    vk.descriptor_pool = VK_NULL_HANDLE;
    vk.set_layout = VK_NULL_HANDLE;
    vk.frame.set = vk.overlay.set = VK_NULL_HANDLE;
    vk.frame_fence = VK_NULL_HANDLE;
    vk.image_acquired = VK_NULL_HANDLE;
    vk.command_pool = VK_NULL_HANDLE;
    vk.cmd = VK_NULL_HANDLE;
    vk.queue = VK_NULL_HANDLE;
    vk.device = VK_NULL_HANDLE;
    vk.physical = VK_NULL_HANDLE;
    vk.instance = VK_NULL_HANDLE;
}

#endif /*C_VULKAN_OUTPUT*/
