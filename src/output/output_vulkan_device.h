#ifndef DOSBOX_OUTPUT_VULKAN_DEVICE_H
#define DOSBOX_OUTPUT_VULKAN_DEVICE_H

#include "dosbox.h"

/* The Vulkan output's device, for the Vulkan 3dfx emulation, which renders with it and
 * has the output show its frame (an image on the same device) in place of the emulated
 * video card's. Everything is submitted to the one queue, in order. */

#if C_VULKAN_OUTPUT
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

struct OUTPUT_VULKAN_Device {
    VkInstance              instance;
    VkPhysicalDevice        physical;
    VkDevice                device;
    VkQueue                 queue;
    uint32_t                queue_family;
    PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
};

/* false if the output is not Vulkan (or has no device) */
bool OUTPUT_VULKAN_GetDevice(OUTPUT_VULKAN_Device &dev);

/* show this image (in VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL whenever the output draws)
 * instead of the emulated video card's frame, VK_NULL_HANDLE to stop */
void OUTPUT_VULKAN_SetExternalImage(VkImageView view, uint32_t width, uint32_t height);

/* draw the window now, with the image (after its rendering was submitted) */
void OUTPUT_VULKAN_PresentExternal(void);

/* the 3dfx emulation must let go of the device, the output is about to destroy it */
void voodoo_vk_device_lost(void);
#endif

bool OUTPUT_VULKAN_Active(void);

#endif
