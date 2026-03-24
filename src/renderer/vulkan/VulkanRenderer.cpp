#include "renderer/Renderer.h"
#include "renderer/font/GeneratedUiFontData.h"
#include "renderer/vulkan/MeshRuntime.h"

#include "platform/Window.h"
#include "util/FileSystem.h"
#include "util/Log.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RIGHT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <spdlog/spdlog.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#ifdef _WIN32
#include <SDL3/SDL_vulkan.h>
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <vulkan/vulkan.h>

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mycsg::renderer {

namespace {

class VulkanDispatch {
public:
    bool loadLoader() {
#ifdef _WIN32
        module_ = LoadLibraryA("vulkan-1.dll");
        if (module_ == nullptr) {
            spdlog::error("Failed to load vulkan-1.dll.");
            return false;
        }
        vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(module_, "vkGetInstanceProcAddr"));
#else
        module_ = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
        if (module_ == nullptr) {
            spdlog::error("Failed to load libvulkan.so.1.");
            return false;
        }
        vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(module_, "vkGetInstanceProcAddr"));
#endif
        if (vkGetInstanceProcAddr == nullptr) {
            spdlog::error("Failed to resolve vkGetInstanceProcAddr.");
            unloadLoader();
            return false;
        }

        vkCreateInstance = loadGlobal<PFN_vkCreateInstance>("vkCreateInstance");
        return vkCreateInstance != nullptr;
    }

    bool loadInstanceFunctions(const VkInstance instance) {
        instance_ = instance;
        vkDestroyInstance = loadInstance<PFN_vkDestroyInstance>("vkDestroyInstance");
        vkEnumeratePhysicalDevices = loadInstance<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");
        vkGetPhysicalDeviceQueueFamilyProperties = loadInstance<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
        vkGetPhysicalDeviceMemoryProperties = loadInstance<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties");
        vkGetPhysicalDeviceFormatProperties = loadInstance<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties");
        vkGetPhysicalDeviceSurfaceSupportKHR = loadInstance<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>("vkGetPhysicalDeviceSurfaceSupportKHR");
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR = loadInstance<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>("vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        vkGetPhysicalDeviceSurfaceFormatsKHR = loadInstance<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>("vkGetPhysicalDeviceSurfaceFormatsKHR");
        vkGetPhysicalDeviceSurfacePresentModesKHR = loadInstance<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>("vkGetPhysicalDeviceSurfacePresentModesKHR");
        vkCreateDevice = loadInstance<PFN_vkCreateDevice>("vkCreateDevice");
        vkDestroySurfaceKHR = loadInstance<PFN_vkDestroySurfaceKHR>("vkDestroySurfaceKHR");
        vkGetDeviceProcAddr = loadInstance<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
        return vkDestroyInstance != nullptr &&
               vkEnumeratePhysicalDevices != nullptr &&
               vkGetPhysicalDeviceQueueFamilyProperties != nullptr &&
               vkGetPhysicalDeviceMemoryProperties != nullptr &&
               vkGetPhysicalDeviceFormatProperties != nullptr &&
               vkGetPhysicalDeviceSurfaceSupportKHR != nullptr &&
               vkGetPhysicalDeviceSurfaceCapabilitiesKHR != nullptr &&
               vkGetPhysicalDeviceSurfaceFormatsKHR != nullptr &&
               vkGetPhysicalDeviceSurfacePresentModesKHR != nullptr &&
               vkCreateDevice != nullptr &&
               vkDestroySurfaceKHR != nullptr &&
               vkGetDeviceProcAddr != nullptr;
    }

    bool loadDeviceFunctions(const VkDevice device) {
        device_ = device;
        vkDestroyDevice = loadDevice<PFN_vkDestroyDevice>("vkDestroyDevice");
        vkGetDeviceQueue = loadDevice<PFN_vkGetDeviceQueue>("vkGetDeviceQueue");
        vkCreateSwapchainKHR = loadDevice<PFN_vkCreateSwapchainKHR>("vkCreateSwapchainKHR");
        vkDestroySwapchainKHR = loadDevice<PFN_vkDestroySwapchainKHR>("vkDestroySwapchainKHR");
        vkGetSwapchainImagesKHR = loadDevice<PFN_vkGetSwapchainImagesKHR>("vkGetSwapchainImagesKHR");
        vkCreateImageView = loadDevice<PFN_vkCreateImageView>("vkCreateImageView");
        vkDestroyImageView = loadDevice<PFN_vkDestroyImageView>("vkDestroyImageView");
        vkCreateRenderPass = loadDevice<PFN_vkCreateRenderPass>("vkCreateRenderPass");
        vkDestroyRenderPass = loadDevice<PFN_vkDestroyRenderPass>("vkDestroyRenderPass");
        vkCreateFramebuffer = loadDevice<PFN_vkCreateFramebuffer>("vkCreateFramebuffer");
        vkDestroyFramebuffer = loadDevice<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer");
        vkCreateCommandPool = loadDevice<PFN_vkCreateCommandPool>("vkCreateCommandPool");
        vkDestroyCommandPool = loadDevice<PFN_vkDestroyCommandPool>("vkDestroyCommandPool");
        vkAllocateCommandBuffers = loadDevice<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers");
        vkFreeCommandBuffers = loadDevice<PFN_vkFreeCommandBuffers>("vkFreeCommandBuffers");
        vkCreateBuffer = loadDevice<PFN_vkCreateBuffer>("vkCreateBuffer");
        vkDestroyBuffer = loadDevice<PFN_vkDestroyBuffer>("vkDestroyBuffer");
        vkGetBufferMemoryRequirements = loadDevice<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements");
        vkCreateImage = loadDevice<PFN_vkCreateImage>("vkCreateImage");
        vkDestroyImage = loadDevice<PFN_vkDestroyImage>("vkDestroyImage");
        vkGetImageMemoryRequirements = loadDevice<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements");
        vkAllocateMemory = loadDevice<PFN_vkAllocateMemory>("vkAllocateMemory");
        vkFreeMemory = loadDevice<PFN_vkFreeMemory>("vkFreeMemory");
        vkBindBufferMemory = loadDevice<PFN_vkBindBufferMemory>("vkBindBufferMemory");
        vkBindImageMemory = loadDevice<PFN_vkBindImageMemory>("vkBindImageMemory");
        vkMapMemory = loadDevice<PFN_vkMapMemory>("vkMapMemory");
        vkUnmapMemory = loadDevice<PFN_vkUnmapMemory>("vkUnmapMemory");
        vkResetCommandBuffer = loadDevice<PFN_vkResetCommandBuffer>("vkResetCommandBuffer");
        vkBeginCommandBuffer = loadDevice<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer");
        vkEndCommandBuffer = loadDevice<PFN_vkEndCommandBuffer>("vkEndCommandBuffer");
        vkCmdBeginRenderPass = loadDevice<PFN_vkCmdBeginRenderPass>("vkCmdBeginRenderPass");
        vkCmdEndRenderPass = loadDevice<PFN_vkCmdEndRenderPass>("vkCmdEndRenderPass");
        vkCmdClearAttachments = loadDevice<PFN_vkCmdClearAttachments>("vkCmdClearAttachments");
        vkCreateShaderModule = loadDevice<PFN_vkCreateShaderModule>("vkCreateShaderModule");
        vkDestroyShaderModule = loadDevice<PFN_vkDestroyShaderModule>("vkDestroyShaderModule");
        vkCreateDescriptorSetLayout = loadDevice<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout");
        vkDestroyDescriptorSetLayout = loadDevice<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout");
        vkCreateDescriptorPool = loadDevice<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool");
        vkDestroyDescriptorPool = loadDevice<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool");
        vkAllocateDescriptorSets = loadDevice<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets");
        vkUpdateDescriptorSets = loadDevice<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets");
        vkCreatePipelineLayout = loadDevice<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout");
        vkDestroyPipelineLayout = loadDevice<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout");
        vkCreateGraphicsPipelines = loadDevice<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines");
        vkDestroyPipeline = loadDevice<PFN_vkDestroyPipeline>("vkDestroyPipeline");
        vkCmdBindPipeline = loadDevice<PFN_vkCmdBindPipeline>("vkCmdBindPipeline");
        vkCmdBindDescriptorSets = loadDevice<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets");
        vkCmdSetViewport = loadDevice<PFN_vkCmdSetViewport>("vkCmdSetViewport");
        vkCmdSetScissor = loadDevice<PFN_vkCmdSetScissor>("vkCmdSetScissor");
        vkCmdBindVertexBuffers = loadDevice<PFN_vkCmdBindVertexBuffers>("vkCmdBindVertexBuffers");
        vkCmdPushConstants = loadDevice<PFN_vkCmdPushConstants>("vkCmdPushConstants");
        vkCmdDraw = loadDevice<PFN_vkCmdDraw>("vkCmdDraw");
        vkCmdPipelineBarrier = loadDevice<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
        vkCmdCopyBufferToImage = loadDevice<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage");
        vkCreateSampler = loadDevice<PFN_vkCreateSampler>("vkCreateSampler");
        vkDestroySampler = loadDevice<PFN_vkDestroySampler>("vkDestroySampler");
        vkCreateSemaphore = loadDevice<PFN_vkCreateSemaphore>("vkCreateSemaphore");
        vkDestroySemaphore = loadDevice<PFN_vkDestroySemaphore>("vkDestroySemaphore");
        vkCreateFence = loadDevice<PFN_vkCreateFence>("vkCreateFence");
        vkDestroyFence = loadDevice<PFN_vkDestroyFence>("vkDestroyFence");
        vkWaitForFences = loadDevice<PFN_vkWaitForFences>("vkWaitForFences");
        vkResetFences = loadDevice<PFN_vkResetFences>("vkResetFences");
        vkAcquireNextImageKHR = loadDevice<PFN_vkAcquireNextImageKHR>("vkAcquireNextImageKHR");
        vkQueueSubmit = loadDevice<PFN_vkQueueSubmit>("vkQueueSubmit");
        vkQueuePresentKHR = loadDevice<PFN_vkQueuePresentKHR>("vkQueuePresentKHR");
        vkDeviceWaitIdle = loadDevice<PFN_vkDeviceWaitIdle>("vkDeviceWaitIdle");
        return vkDestroyDevice != nullptr &&
               vkGetDeviceQueue != nullptr &&
               vkCreateSwapchainKHR != nullptr &&
               vkDestroySwapchainKHR != nullptr &&
               vkGetSwapchainImagesKHR != nullptr &&
               vkCreateImageView != nullptr &&
               vkDestroyImageView != nullptr &&
               vkCreateRenderPass != nullptr &&
               vkDestroyRenderPass != nullptr &&
               vkCreateFramebuffer != nullptr &&
               vkDestroyFramebuffer != nullptr &&
               vkCreateCommandPool != nullptr &&
               vkDestroyCommandPool != nullptr &&
               vkAllocateCommandBuffers != nullptr &&
               vkFreeCommandBuffers != nullptr &&
               vkCreateBuffer != nullptr &&
               vkDestroyBuffer != nullptr &&
               vkGetBufferMemoryRequirements != nullptr &&
               vkCreateImage != nullptr &&
               vkDestroyImage != nullptr &&
               vkGetImageMemoryRequirements != nullptr &&
               vkAllocateMemory != nullptr &&
               vkFreeMemory != nullptr &&
               vkBindBufferMemory != nullptr &&
               vkBindImageMemory != nullptr &&
               vkMapMemory != nullptr &&
               vkUnmapMemory != nullptr &&
               vkResetCommandBuffer != nullptr &&
               vkBeginCommandBuffer != nullptr &&
               vkEndCommandBuffer != nullptr &&
               vkCmdBeginRenderPass != nullptr &&
               vkCmdEndRenderPass != nullptr &&
               vkCmdClearAttachments != nullptr &&
               vkCreateShaderModule != nullptr &&
               vkDestroyShaderModule != nullptr &&
               vkCreateDescriptorSetLayout != nullptr &&
               vkDestroyDescriptorSetLayout != nullptr &&
               vkCreateDescriptorPool != nullptr &&
               vkDestroyDescriptorPool != nullptr &&
               vkAllocateDescriptorSets != nullptr &&
               vkUpdateDescriptorSets != nullptr &&
               vkCreatePipelineLayout != nullptr &&
               vkDestroyPipelineLayout != nullptr &&
               vkCreateGraphicsPipelines != nullptr &&
               vkDestroyPipeline != nullptr &&
               vkCmdBindPipeline != nullptr &&
               vkCmdBindDescriptorSets != nullptr &&
               vkCmdSetViewport != nullptr &&
               vkCmdSetScissor != nullptr &&
               vkCmdBindVertexBuffers != nullptr &&
               vkCmdPushConstants != nullptr &&
               vkCmdDraw != nullptr &&
               vkCmdPipelineBarrier != nullptr &&
               vkCmdCopyBufferToImage != nullptr &&
               vkCreateSampler != nullptr &&
               vkDestroySampler != nullptr &&
               vkCreateSemaphore != nullptr &&
               vkDestroySemaphore != nullptr &&
               vkCreateFence != nullptr &&
               vkDestroyFence != nullptr &&
               vkWaitForFences != nullptr &&
               vkResetFences != nullptr &&
               vkAcquireNextImageKHR != nullptr &&
               vkQueueSubmit != nullptr &&
               vkQueuePresentKHR != nullptr &&
               vkDeviceWaitIdle != nullptr;
    }

    void unloadLoader() {
        instance_ = VK_NULL_HANDLE;
        device_ = VK_NULL_HANDLE;
        vkCreateInstance = nullptr;
        vkGetInstanceProcAddr = nullptr;
        vkGetDeviceProcAddr = nullptr;
#ifdef _WIN32
        if (module_ != nullptr) {
            FreeLibrary(module_);
            module_ = nullptr;
        }
#else
        if (module_ != nullptr) {
            dlclose(module_);
            module_ = nullptr;
        }
#endif
    }

    template <typename Fn>
    Fn loadGlobal(const std::string_view name) const {
        return reinterpret_cast<Fn>(vkGetInstanceProcAddr(VK_NULL_HANDLE, name.data()));
    }

    template <typename Fn>
    Fn loadInstance(const std::string_view name) const {
        return reinterpret_cast<Fn>(vkGetInstanceProcAddr(instance_, name.data()));
    }

    template <typename Fn>
    Fn loadDevice(const std::string_view name) const {
        return reinterpret_cast<Fn>(vkGetDeviceProcAddr(device_, name.data()));
    }

#ifdef _WIN32
    HMODULE module_ = nullptr;
#else
    void* module_ = nullptr;
#endif
    VkInstance instance_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;

    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
    PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = nullptr;

    PFN_vkCreateInstance vkCreateInstance = nullptr;
    PFN_vkDestroyInstance vkDestroyInstance = nullptr;
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = nullptr;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
    PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties = nullptr;
    PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
    PFN_vkCreateDevice vkCreateDevice = nullptr;
    PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR = nullptr;

    PFN_vkDestroyDevice vkDestroyDevice = nullptr;
    PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;
    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR = nullptr;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR = nullptr;
    PFN_vkCreateImageView vkCreateImageView = nullptr;
    PFN_vkDestroyImageView vkDestroyImageView = nullptr;
    PFN_vkCreateRenderPass vkCreateRenderPass = nullptr;
    PFN_vkDestroyRenderPass vkDestroyRenderPass = nullptr;
    PFN_vkCreateFramebuffer vkCreateFramebuffer = nullptr;
    PFN_vkDestroyFramebuffer vkDestroyFramebuffer = nullptr;
    PFN_vkCreateCommandPool vkCreateCommandPool = nullptr;
    PFN_vkDestroyCommandPool vkDestroyCommandPool = nullptr;
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = nullptr;
    PFN_vkFreeCommandBuffers vkFreeCommandBuffers = nullptr;
    PFN_vkCreateBuffer vkCreateBuffer = nullptr;
    PFN_vkDestroyBuffer vkDestroyBuffer = nullptr;
    PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements = nullptr;
    PFN_vkCreateImage vkCreateImage = nullptr;
    PFN_vkDestroyImage vkDestroyImage = nullptr;
    PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements = nullptr;
    PFN_vkAllocateMemory vkAllocateMemory = nullptr;
    PFN_vkFreeMemory vkFreeMemory = nullptr;
    PFN_vkBindBufferMemory vkBindBufferMemory = nullptr;
    PFN_vkBindImageMemory vkBindImageMemory = nullptr;
    PFN_vkMapMemory vkMapMemory = nullptr;
    PFN_vkUnmapMemory vkUnmapMemory = nullptr;
    PFN_vkResetCommandBuffer vkResetCommandBuffer = nullptr;
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer = nullptr;
    PFN_vkEndCommandBuffer vkEndCommandBuffer = nullptr;
    PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass = nullptr;
    PFN_vkCmdEndRenderPass vkCmdEndRenderPass = nullptr;
    PFN_vkCmdClearAttachments vkCmdClearAttachments = nullptr;
    PFN_vkCreateShaderModule vkCreateShaderModule = nullptr;
    PFN_vkDestroyShaderModule vkDestroyShaderModule = nullptr;
    PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout = nullptr;
    PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout = nullptr;
    PFN_vkCreateDescriptorPool vkCreateDescriptorPool = nullptr;
    PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool = nullptr;
    PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets = nullptr;
    PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets = nullptr;
    PFN_vkCreatePipelineLayout vkCreatePipelineLayout = nullptr;
    PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout = nullptr;
    PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines = nullptr;
    PFN_vkDestroyPipeline vkDestroyPipeline = nullptr;
    PFN_vkCmdBindPipeline vkCmdBindPipeline = nullptr;
    PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets = nullptr;
    PFN_vkCmdSetViewport vkCmdSetViewport = nullptr;
    PFN_vkCmdSetScissor vkCmdSetScissor = nullptr;
    PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers = nullptr;
    PFN_vkCmdPushConstants vkCmdPushConstants = nullptr;
    PFN_vkCmdDraw vkCmdDraw = nullptr;
    PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier = nullptr;
    PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage = nullptr;
    PFN_vkCreateSampler vkCreateSampler = nullptr;
    PFN_vkDestroySampler vkDestroySampler = nullptr;
    PFN_vkCreateSemaphore vkCreateSemaphore = nullptr;
    PFN_vkDestroySemaphore vkDestroySemaphore = nullptr;
    PFN_vkCreateFence vkCreateFence = nullptr;
    PFN_vkDestroyFence vkDestroyFence = nullptr;
    PFN_vkWaitForFences vkWaitForFences = nullptr;
    PFN_vkResetFences vkResetFences = nullptr;
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR = nullptr;
    PFN_vkQueueSubmit vkQueueSubmit = nullptr;
    PFN_vkQueuePresentKHR vkQueuePresentKHR = nullptr;
    PFN_vkDeviceWaitIdle vkDeviceWaitIdle = nullptr;
};

#ifdef _WIN32

struct FrameResources {
    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
};

struct GpuBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    std::uint32_t vertexCount = 0;
    util::Vec3 center{};
    float radius = 1.0f;
};

struct TextureResource {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool valid = false;
};

struct StreamingBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    std::size_t capacityBytes = 0;
};

struct MeshPushConstants {
    std::array<float, 16> mvp{};
    std::array<float, 16> model{};
};

struct TextVertex {
    float x = 0.0f;
    float y = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct UiFontGlyphPlacement {
    std::uint16_t atlasX = 0;
    std::uint16_t atlasY = 0;
    std::uint16_t atlasWidth = 0;
    std::uint16_t atlasHeight = 0;
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 0.0f;
    float v1 = 0.0f;
};

MeshPushConstants makePushConstants(const glm::mat4& projectionView, const glm::mat4& model) {
    const glm::mat4 mvp = projectionView * model;
    MeshPushConstants push{};
    std::memcpy(push.mvp.data(), glm::value_ptr(mvp), sizeof(push.mvp));
    std::memcpy(push.model.data(), glm::value_ptr(model), sizeof(push.model));
    return push;
}

VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return formats.front();
}

VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes) {
    for (const auto mode : modes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D queryExtent(HWND hwnd) {
    RECT rect{};
    GetClientRect(hwnd, &rect);
    VkExtent2D extent{};
    extent.width = static_cast<std::uint32_t>(std::max<LONG>(1, rect.right - rect.left));
    extent.height = static_cast<std::uint32_t>(std::max<LONG>(1, rect.bottom - rect.top));
    return extent;
}

VkClearColorValue backgroundColor(app::AppFlow flow) {
    VkClearColorValue color{};
    switch (flow) {
        case app::AppFlow::MainMenu: color.float32[0] = 0.07f; color.float32[1] = 0.11f; color.float32[2] = 0.16f; break;
        case app::AppFlow::MapBrowser: color.float32[0] = 0.08f; color.float32[1] = 0.10f; color.float32[2] = 0.14f; break;
        case app::AppFlow::SinglePlayerLobby: color.float32[0] = 0.07f; color.float32[1] = 0.15f; color.float32[2] = 0.10f; break;
        case app::AppFlow::MultiPlayerLobby: color.float32[0] = 0.10f; color.float32[1] = 0.08f; color.float32[2] = 0.17f; break;
        case app::AppFlow::MapEditor: color.float32[0] = 0.17f; color.float32[1] = 0.12f; color.float32[2] = 0.08f; break;
        case app::AppFlow::Settings: color.float32[0] = 0.12f; color.float32[1] = 0.12f; color.float32[2] = 0.12f; break;
        case app::AppFlow::Exit: color.float32[0] = 0.05f; color.float32[1] = 0.05f; color.float32[2] = 0.05f; break;
    }
    color.float32[3] = 1.0f;
    return color;
}

std::filesystem::path rendererAssetRootPath() {
#ifdef _WIN32
    return "assets";
#elif defined(MYCSGO_ASSET_ROOT)
    return MYCSGO_ASSET_ROOT;
#else
    return "assets";
#endif
}

std::filesystem::path resolveAssetPath(const std::filesystem::path& path) {
    if (path.empty()) {
        return {};
    }
    if (path.is_absolute()) {
        return path;
    }

    const auto assetRoot = rendererAssetRootPath();
    const auto normalized = path.lexically_normal();
    const auto normalizedText = normalized.generic_string();
    const auto assetRootText = assetRoot.lexically_normal().generic_string();
    if (!assetRootText.empty() &&
        normalizedText.size() >= assetRootText.size() &&
        normalizedText.compare(0, assetRootText.size(), assetRootText) == 0) {
        return normalized;
    }
    return (assetRoot / normalized).lexically_normal();
}

std::filesystem::path parseMaterialAlbedoPath(const std::filesystem::path& materialPath) {
    if (materialPath.empty()) {
        return {};
    }

    const std::string content = util::FileSystem::readText(resolveAssetPath(materialPath));
    if (content.empty()) {
        return {};
    }

    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind("albedo=", 0) == 0) {
            const std::filesystem::path rawPath = line.substr(7);
            if (rawPath.is_absolute()) {
                return rawPath;
            }

            const auto assetResolved = resolveAssetPath(rawPath);
            if (util::FileSystem::exists(assetResolved)) {
                return assetResolved;
            }
            const auto materialRelative = (resolveAssetPath(materialPath).parent_path() / rawPath).lexically_normal();
            if (util::FileSystem::exists(materialRelative)) {
                return materialRelative;
            }
            return assetResolved;
        }
    }

    return {};
}

struct DecodedTexturePixels {
    std::vector<unsigned char> rgba;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

bool readPpmToken(const std::vector<std::byte>& bytes, std::size_t& cursor, std::string& token) {
    token.clear();
    while (cursor < bytes.size()) {
        const char ch = static_cast<char>(bytes[cursor]);
        if (std::isspace(static_cast<unsigned char>(ch))) {
            ++cursor;
            continue;
        }
        if (ch == '#') {
            while (cursor < bytes.size() && static_cast<char>(bytes[cursor]) != '\n') {
                ++cursor;
            }
            continue;
        }
        break;
    }

    while (cursor < bytes.size()) {
        const char ch = static_cast<char>(bytes[cursor]);
        if (std::isspace(static_cast<unsigned char>(ch)) || ch == '#') {
            break;
        }
        token.push_back(ch);
        ++cursor;
    }
    return !token.empty();
}

std::optional<DecodedTexturePixels> decodePpmTexture(const std::vector<std::byte>& bytes) {
    if (bytes.size() < 3) {
        return std::nullopt;
    }

    std::size_t cursor = 0;
    std::string token;
    if (!readPpmToken(bytes, cursor, token) || (token != "P3" && token != "P6")) {
        return std::nullopt;
    }

    std::string widthToken;
    std::string heightToken;
    std::string maxValueToken;
    if (!readPpmToken(bytes, cursor, widthToken) ||
        !readPpmToken(bytes, cursor, heightToken) ||
        !readPpmToken(bytes, cursor, maxValueToken)) {
        return std::nullopt;
    }

    const int widthValue = std::stoi(widthToken);
    const int heightValue = std::stoi(heightToken);
    const int maxValue = std::stoi(maxValueToken);
    if (widthValue <= 0 || heightValue <= 0 || maxValue <= 0 || maxValue > 255) {
        return std::nullopt;
    }

    DecodedTexturePixels decoded{};
    decoded.width = static_cast<std::uint32_t>(widthValue);
    decoded.height = static_cast<std::uint32_t>(heightValue);
    decoded.rgba.resize(static_cast<std::size_t>(decoded.width) * decoded.height * 4, 255);

    const auto scaleChannel = [maxValue](const int value) -> unsigned char {
        const int clamped = std::clamp(value, 0, maxValue);
        return static_cast<unsigned char>((clamped * 255 + maxValue / 2) / maxValue);
    };

    if (token == "P3") {
        for (std::size_t pixelIndex = 0; pixelIndex < decoded.width * decoded.height; ++pixelIndex) {
            std::string rToken;
            std::string gToken;
            std::string bToken;
            if (!readPpmToken(bytes, cursor, rToken) ||
                !readPpmToken(bytes, cursor, gToken) ||
                !readPpmToken(bytes, cursor, bToken)) {
                return std::nullopt;
            }

            const std::size_t base = pixelIndex * 4;
            decoded.rgba[base + 0] = scaleChannel(std::stoi(rToken));
            decoded.rgba[base + 1] = scaleChannel(std::stoi(gToken));
            decoded.rgba[base + 2] = scaleChannel(std::stoi(bToken));
        }
        return decoded;
    }

    while (cursor < bytes.size() && std::isspace(static_cast<unsigned char>(bytes[cursor]))) {
        ++cursor;
    }
    const std::size_t rgbBytes = static_cast<std::size_t>(decoded.width) * decoded.height * 3;
    if (cursor + rgbBytes > bytes.size()) {
        return std::nullopt;
    }

    for (std::size_t pixelIndex = 0; pixelIndex < decoded.width * decoded.height; ++pixelIndex) {
        const std::size_t src = cursor + pixelIndex * 3;
        const std::size_t dst = pixelIndex * 4;
        decoded.rgba[dst + 0] = scaleChannel(static_cast<unsigned char>(bytes[src + 0]));
        decoded.rgba[dst + 1] = scaleChannel(static_cast<unsigned char>(bytes[src + 1]));
        decoded.rgba[dst + 2] = scaleChannel(static_cast<unsigned char>(bytes[src + 2]));
    }
    return decoded;
}

VkClearAttachment makeAttachment(float r, float g, float b) {
    VkClearAttachment attachment{};
    attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    attachment.colorAttachment = 0;
    attachment.clearValue.color.float32[0] = r;
    attachment.clearValue.color.float32[1] = g;
    attachment.clearValue.color.float32[2] = b;
    attachment.clearValue.color.float32[3] = 1.0f;
    return attachment;
}

VkClearAttachment makeAttachment(const COLORREF color) {
    return makeAttachment(
        static_cast<float>(GetRValue(color)) / 255.0f,
        static_cast<float>(GetGValue(color)) / 255.0f,
        static_cast<float>(GetBValue(color)) / 255.0f);
}

struct UiRectRatio {
    float x0;
    float y0;
    float x1;
    float y1;
};

UiRectRatio mainMenuHeaderRatio() {
    return {0.06f, 0.08f, 0.46f, 0.18f};
}

UiRectRatio mainMenuSidebarRatio() {
    return {0.56f, 0.12f, 0.92f, 0.82f};
}

UiRectRatio mainMenuSidebarAccentRatio() {
    return {0.60f, 0.18f, 0.88f, 0.28f};
}

UiRectRatio mainMenuPreviewRatio() {
    return {0.62f, 0.36f, 0.86f, 0.72f};
}

UiRectRatio mainMenuCardRatio(const std::size_t index) {
    const float top = 0.24f + static_cast<float>(index) * 0.11f;
    return {0.06f, top, 0.44f, top + 0.085f};
}

UiRectRatio mainMenuCardGlowRatio(const std::size_t index) {
    const auto card = mainMenuCardRatio(index);
    return {0.045f, card.y0, 0.055f, card.y1};
}

VkClearRect makeRect(const VkExtent2D extent, float x0, float y0, float x1, float y1) {
    const auto left = static_cast<std::int32_t>(extent.width * x0);
    const auto top = static_cast<std::int32_t>(extent.height * y0);
    const auto right = static_cast<std::int32_t>(extent.width * x1);
    const auto bottom = static_cast<std::int32_t>(extent.height * y1);

    VkClearRect rect{};
    rect.rect.offset.x = left;
    rect.rect.offset.y = top;
    rect.rect.extent.width = static_cast<std::uint32_t>(std::max(1, right - left));
    rect.rect.extent.height = static_cast<std::uint32_t>(std::max(1, bottom - top));
    rect.baseArrayLayer = 0;
    rect.layerCount = 1;
    return rect;
}

VkClearRect makeRect(const VkExtent2D extent, const UiRectRatio rect) {
    return makeRect(extent, rect.x0, rect.y0, rect.x1, rect.y1);
}

std::wstring widenUtf8(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), size);
    if (!result.empty() && result.back() == L'\0') {
        result.pop_back();
    }
    return result;
}

std::wstring formatFixedValue(const float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1) << value;
    return widenUtf8(stream.str());
}

std::wstring formatPitchDegrees(const float value) {
    std::ostringstream stream;
    stream << static_cast<int>(std::round(value)) << " 度";
    return widenUtf8(stream.str());
}

void clearRect(VulkanDispatch& dispatch,
               const VkCommandBuffer commandBuffer,
               const VkClearAttachment& attachment,
               const VkClearRect& rect);

const font::UiFontGlyph* fallbackUiFontGlyph() {
    static const font::UiFontGlyph* glyph = [] {
        for (std::size_t index = 0; index < font::kUiFontGlyphCount; ++index) {
            if (font::kUiFontGlyphs[index].codepoint == static_cast<std::uint32_t>('?')) {
                return &font::kUiFontGlyphs[index];
            }
        }
        return static_cast<const font::UiFontGlyph*>(nullptr);
    }();
    return glyph;
}

const font::UiFontGlyph* findUiFontGlyph(const std::uint32_t codepoint) {
    std::size_t left = 0;
    std::size_t right = font::kUiFontGlyphCount;
    while (left < right) {
        const std::size_t middle = left + (right - left) / 2;
        const std::uint32_t current = font::kUiFontGlyphs[middle].codepoint;
        if (current == codepoint) {
            return &font::kUiFontGlyphs[middle];
        }
        if (current < codepoint) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    return fallbackUiFontGlyph();
}

float measureBitmapTextWidth(std::wstring_view text, const float scale, const float tracking = 0.0f) {
    float width = 0.0f;
    float maxWidth = 0.0f;
    for (const wchar_t character : text) {
        if (character == L'\n') {
            maxWidth = std::max(maxWidth, width);
            width = 0.0f;
            continue;
        }

        const auto* glyph = findUiFontGlyph(static_cast<std::uint32_t>(character));
        if (glyph == nullptr) {
            continue;
        }
        width += static_cast<float>(glyph->advance) * scale + tracking;
    }
    return std::max(maxWidth, width);
}

VkClearRect makePixelRect(const VkExtent2D extent, const float leftPx, const float topPx, const float rightPx, const float bottomPx) {
    const float width = static_cast<float>(extent.width);
    const float height = static_cast<float>(extent.height);
    return makeRect(
        extent,
        std::clamp(leftPx / width, 0.0f, 1.0f),
        std::clamp(topPx / height, 0.0f, 1.0f),
        std::clamp(rightPx / width, 0.0f, 1.0f),
        std::clamp(bottomPx / height, 0.0f, 1.0f));
}

void clearRect(VulkanDispatch& dispatch,
               const VkCommandBuffer commandBuffer,
               const VkClearAttachment& attachment,
               const VkClearRect& rect) {
    dispatch.vkCmdClearAttachments(commandBuffer, 1, &attachment, 1, &rect);
}

bool isBlockedCell(const gameplay::MapData& map, const int cellX, const int cellZ) {
    if (cellX < 0 || cellZ < 0 || cellX >= map.width || cellZ >= map.depth) {
        return true;
    }

    for (const auto& block : map.blocks) {
        if (block.solid && block.cell.x == cellX && block.cell.z == cellZ && block.cell.y >= 1) {
            return true;
        }
    }

    return false;
}

bool hasRampCell(const gameplay::MapData& map, const int cellX, const int cellZ) {
    for (const auto& block : map.blocks) {
        if (block.cell.x == cellX && block.cell.z == cellZ && gameplay::isRampMaterial(block.materialId)) {
            return true;
        }
    }
    return false;
}

std::string toLowerAscii(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

struct PropVisualProfile {
    enum class Shape {
        Generic,
        Crate,
        Barrel,
    };

    Shape shape = Shape::Generic;
    float spriteHeightScale = 0.92f;
    float spriteWidthScale = 0.72f;
    COLORREF body = RGB(156, 118, 82);
    COLORREF accent = RGB(198, 164, 116);
    COLORREF detail = RGB(92, 68, 52);
};

PropVisualProfile describeProp(const gameplay::MapProp& prop) {
    const std::string key = toLowerAscii(prop.id + " " + prop.modelPath.generic_string() + " " + prop.materialPath.generic_string());
    if (key.find("barrel") != std::string::npos) {
        return PropVisualProfile{
            .shape = PropVisualProfile::Shape::Barrel,
            .spriteHeightScale = 0.98f,
            .spriteWidthScale = 0.52f,
            .body = RGB(148, 82, 64),
            .accent = RGB(196, 156, 122),
            .detail = RGB(88, 48, 36),
        };
    }
    if (key.find("crate") != std::string::npos) {
        return PropVisualProfile{
            .shape = PropVisualProfile::Shape::Crate,
            .spriteHeightScale = 0.90f,
            .spriteWidthScale = 0.76f,
            .body = RGB(154, 116, 78),
            .accent = RGB(204, 170, 120),
            .detail = RGB(94, 68, 46),
        };
    }
    return {};
}

struct PreviewTriangle {
    std::uint32_t a = 0;
    std::uint32_t b = 0;
    std::uint32_t c = 0;
    util::Vec3 color{0.82f, 0.86f, 0.90f};
};

struct PreviewMeshModel {
    std::vector<util::Vec3> vertices;
    std::vector<PreviewTriangle> triangles;
    util::Vec3 center{};
    float radius = 1.0f;
    bool valid = false;
};

PreviewMeshModel loadPreviewMeshModel(const std::filesystem::path& path) {
    PreviewMeshModel model;
    const vulkan::CpuMesh cpuMesh = vulkan::loadMeshAsset(rendererAssetRootPath(), path);
    if (!cpuMesh.valid || cpuMesh.vertices.size() < 3) {
        return model;
    }

    model.vertices.reserve(cpuMesh.vertices.size());
    for (const auto& vertex : cpuMesh.vertices) {
        model.vertices.push_back({vertex.px, vertex.py, vertex.pz});
    }
    for (std::size_t index = 0; index + 2 < cpuMesh.vertices.size(); index += 3) {
        model.triangles.push_back({
            static_cast<std::uint32_t>(index),
            static_cast<std::uint32_t>(index + 1),
            static_cast<std::uint32_t>(index + 2),
            {cpuMesh.vertices[index].r, cpuMesh.vertices[index].g, cpuMesh.vertices[index].b},
        });
    }

    model.center = cpuMesh.center;
    model.radius = cpuMesh.radius;
    model.valid = !model.triangles.empty();
    return model;
}

struct ProjectedVertex {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

VkClearAttachment makeAttachment(const util::Vec3& color, const float intensity = 1.0f) {
    return makeAttachment(
        std::clamp(color.x * intensity, 0.0f, 1.0f),
        std::clamp(color.y * intensity, 0.0f, 1.0f),
        std::clamp(color.z * intensity, 0.0f, 1.0f));
}

void recordFilledTriangle(VulkanDispatch& dispatch,
                          const VkCommandBuffer commandBuffer,
                          const VkExtent2D extent,
                          const VkClearAttachment& attachment,
                          const ProjectedVertex& v0,
                          const ProjectedVertex& v1,
                          const ProjectedVertex& v2) {
    const float minY = std::min({v0.y, v1.y, v2.y});
    const float maxY = std::max({v0.y, v1.y, v2.y});
    const int startY = std::max(0, static_cast<int>(std::floor(minY)));
    const int endY = std::min(static_cast<int>(extent.height) - 1, static_cast<int>(std::ceil(maxY)));
    if (endY < startY) {
        return;
    }

    for (int y = startY; y <= endY; ++y) {
        const float scanY = static_cast<float>(y) + 0.5f;
        std::array<float, 3> intersections{};
        int count = 0;
        const auto addEdge = [&](const ProjectedVertex& a, const ProjectedVertex& b) {
            if (std::abs(a.y - b.y) < 0.0001f) {
                return;
            }
            const float edgeMinY = std::min(a.y, b.y);
            const float edgeMaxY = std::max(a.y, b.y);
            if (scanY < edgeMinY || scanY >= edgeMaxY) {
                return;
            }
            const float t = (scanY - a.y) / (b.y - a.y);
            intersections[count++] = a.x + (b.x - a.x) * t;
        };
        addEdge(v0, v1);
        addEdge(v1, v2);
        addEdge(v2, v0);
        if (count < 2) {
            continue;
        }
        std::sort(intersections.begin(), intersections.begin() + count);
        clearRect(dispatch, commandBuffer, attachment, makePixelRect(
            extent,
            intersections[0],
            static_cast<float>(y),
            intersections[count - 1],
            static_cast<float>(y + 1)));
    }
}

class VulkanRenderer final : public IRenderer {
public:
    bool initialize(platform::IWindow& window) override {
        hwnd_ = static_cast<HWND>(window.nativeHandle());
        sdlWindow_ = static_cast<SDL_Window*>(window.nativeWindowObject());
        if (hwnd_ == nullptr) {
            spdlog::error("Missing Win32 window handle.");
            return false;
        }
        if (sdlWindow_ == nullptr) {
            spdlog::error("Missing SDL window object.");
            return false;
        }

        if (!dispatch_.loadLoader()) {
            return false;
        }

        if (!createInstance() || !createSurface() || !pickPhysicalDevice() || !createDevice() || !createAllocator() || !createSwapchainResources()) {
            shutdown();
            return false;
        }

        return true;
    }

    void render(const RenderFrame& frame) override {
        if (frame.mainMenu != nullptr && !menuPrinted_) {
            spdlog::info("[MainMenu] {}", frame.mainMenu->title());
            for (const auto& section : frame.mainMenu->sections()) {
                spdlog::info("  [{}]", section.title);
                for (const auto& item : section.items) {
                    spdlog::info("    - {} : {}", item.label, item.description);
                }
            }
            menuPrinted_ = true;
        }
        if (frame.editingMap != nullptr && !editorPrinted_) {
            spdlog::info("[MapEditor] editing {}", frame.editingMap->name);
            editorPrinted_ = true;
        }
        if (frame.world != nullptr && !worldPrinted_) {
            spdlog::info("[Simulation] players={}", frame.world->players().size());
            worldPrinted_ = true;
        }

        drawFrame(frame);

        if (!bitmapUiLogged_) {
            spdlog::info("[Renderer] UI text uses textured glyph atlas batching.");
            bitmapUiLogged_ = true;
        }
    }

    void shutdown() override {
        if (device_ != VK_NULL_HANDLE && dispatch_.vkDeviceWaitIdle != nullptr) {
            dispatch_.vkDeviceWaitIdle(device_);
        }

        destroyMeshResources();
        destroySwapchainResources();

        if (renderFence_ != VK_NULL_HANDLE && dispatch_.vkDestroyFence != nullptr) {
            dispatch_.vkDestroyFence(device_, renderFence_, nullptr);
            renderFence_ = VK_NULL_HANDLE;
        }
        if (renderFinishedSemaphore_ != VK_NULL_HANDLE && dispatch_.vkDestroySemaphore != nullptr) {
            dispatch_.vkDestroySemaphore(device_, renderFinishedSemaphore_, nullptr);
            renderFinishedSemaphore_ = VK_NULL_HANDLE;
        }
        if (imageAvailableSemaphore_ != VK_NULL_HANDLE && dispatch_.vkDestroySemaphore != nullptr) {
            dispatch_.vkDestroySemaphore(device_, imageAvailableSemaphore_, nullptr);
            imageAvailableSemaphore_ = VK_NULL_HANDLE;
        }
        if (allocator_ != VK_NULL_HANDLE) {
            vmaDestroyAllocator(allocator_);
            allocator_ = VK_NULL_HANDLE;
        }
        if (device_ != VK_NULL_HANDLE && dispatch_.vkDestroyDevice != nullptr) {
            dispatch_.vkDestroyDevice(device_, nullptr);
            device_ = VK_NULL_HANDLE;
        }
        if (surface_ != VK_NULL_HANDLE) {
            SDL_Vulkan_DestroySurface(instance_, surface_, nullptr);
            surface_ = VK_NULL_HANDLE;
        }
        if (instance_ != VK_NULL_HANDLE && dispatch_.vkDestroyInstance != nullptr) {
            dispatch_.vkDestroyInstance(instance_, nullptr);
            instance_ = VK_NULL_HANDLE;
        }
        dispatch_.unloadLoader();
    }

private:
    struct CachedPreviewMesh {
        std::string path;
        PreviewMeshModel model;
    };

    struct CachedGpuMesh {
        std::string path;
        GpuBuffer buffer;
    };

    struct CachedTexture {
        std::string path;
        TextureResource texture;
    };

    std::array<float, 4> textColor(const VkClearAttachment& attachment) const {
        return {
            attachment.clearValue.color.float32[0],
            attachment.clearValue.color.float32[1],
            attachment.clearValue.color.float32[2],
            attachment.clearValue.color.float32[3],
        };
    }

    bool buildUiFontAtlas(std::vector<unsigned char>& rgbaPixels,
                          std::uint32_t& atlasWidth,
                          std::uint32_t& atlasHeight) {
        constexpr std::uint32_t kPadding = 1;
        constexpr std::uint32_t kAtlasWidth = 1024;

        atlasWidth = kAtlasWidth;
        atlasHeight = 0;
        uiFontGlyphPlacements_.assign(font::kUiFontGlyphCount, {});

        std::uint32_t cursorX = kPadding;
        std::uint32_t cursorY = kPadding;
        std::uint32_t rowHeight = 0;
        for (std::size_t glyphIndex = 0; glyphIndex < font::kUiFontGlyphCount; ++glyphIndex) {
            const auto& glyph = font::kUiFontGlyphs[glyphIndex];
            auto& placement = uiFontGlyphPlacements_[glyphIndex];
            placement.atlasWidth = glyph.width;
            placement.atlasHeight = glyph.height;

            if (glyph.width == 0 || glyph.height == 0) {
                continue;
            }

            const std::uint32_t glyphWidth = static_cast<std::uint32_t>(glyph.width) + kPadding * 2;
            const std::uint32_t glyphHeight = static_cast<std::uint32_t>(glyph.height) + kPadding * 2;
            if (cursorX + glyphWidth > atlasWidth) {
                cursorX = kPadding;
                cursorY += rowHeight + kPadding;
                rowHeight = 0;
            }

            placement.atlasX = static_cast<std::uint16_t>(cursorX + kPadding);
            placement.atlasY = static_cast<std::uint16_t>(cursorY + kPadding);
            cursorX += glyphWidth + kPadding;
            rowHeight = std::max(rowHeight, glyphHeight);
            atlasHeight = std::max(atlasHeight, cursorY + glyphHeight + kPadding);
        }

        atlasHeight = std::max<std::uint32_t>(atlasHeight, 4);
        std::vector<unsigned char> alphaPixels(static_cast<std::size_t>(atlasWidth) * atlasHeight, 0);
        for (std::size_t glyphIndex = 0; glyphIndex < font::kUiFontGlyphCount; ++glyphIndex) {
            const auto& glyph = font::kUiFontGlyphs[glyphIndex];
            const auto& placement = uiFontGlyphPlacements_[glyphIndex];
            if (glyph.width == 0 || glyph.height == 0) {
                continue;
            }

            for (std::uint32_t spanIndex = 0; spanIndex < glyph.spanCount; ++spanIndex) {
                const auto& span = font::kUiFontSpans[glyph.spanOffset + spanIndex];
                const std::uint32_t row = static_cast<std::uint32_t>(placement.atlasY) + span.y;
                const std::uint32_t startX = static_cast<std::uint32_t>(placement.atlasX) + span.x0;
                const std::uint32_t endX = static_cast<std::uint32_t>(placement.atlasX) + span.x1;
                if (row >= atlasHeight) {
                    continue;
                }
                for (std::uint32_t x = startX; x < endX && x < atlasWidth; ++x) {
                    alphaPixels[static_cast<std::size_t>(row) * atlasWidth + x] = 255;
                }
            }
        }

        const float atlasWidthFloat = static_cast<float>(atlasWidth);
        const float atlasHeightFloat = static_cast<float>(atlasHeight);
        for (auto& placement : uiFontGlyphPlacements_) {
            if (placement.atlasWidth == 0 || placement.atlasHeight == 0) {
                continue;
            }
            placement.u0 = static_cast<float>(placement.atlasX) / atlasWidthFloat;
            placement.v0 = static_cast<float>(placement.atlasY) / atlasHeightFloat;
            placement.u1 = static_cast<float>(placement.atlasX + placement.atlasWidth) / atlasWidthFloat;
            placement.v1 = static_cast<float>(placement.atlasY + placement.atlasHeight) / atlasHeightFloat;
        }

        rgbaPixels.resize(alphaPixels.size() * 4, 255);
        for (std::size_t index = 0; index < alphaPixels.size(); ++index) {
            rgbaPixels[index * 4 + 3] = alphaPixels[index];
        }
        return true;
    }

    bool createUiFontAtlasTexture() {
        if (uiFontAtlasTexture_.valid) {
            return true;
        }

        std::vector<unsigned char> rgbaPixels;
        std::uint32_t atlasWidth = 0;
        std::uint32_t atlasHeight = 0;
        if (!buildUiFontAtlas(rgbaPixels, atlasWidth, atlasHeight)) {
            return false;
        }
        return createTextureFromRgba(rgbaPixels.data(), atlasWidth, atlasHeight, uiFontAtlasTexture_);
    }

    void destroyStreamingBuffer(StreamingBuffer& buffer) {
        if (buffer.buffer != VK_NULL_HANDLE && buffer.allocation != VK_NULL_HANDLE && allocator_ != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator_, buffer.buffer, buffer.allocation);
        }
        buffer = {};
    }

    bool ensureTextVertexCapacity(const std::size_t vertexCount) {
        const std::size_t requiredBytes = std::max<std::size_t>(vertexCount, 1) * sizeof(TextVertex);
        if (textVertexBuffer_.buffer != VK_NULL_HANDLE && textVertexBuffer_.capacityBytes >= requiredBytes) {
            return true;
        }

        destroyStreamingBuffer(textVertexBuffer_);
        if (!createBuffer(requiredBytes,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                textVertexBuffer_.buffer,
                textVertexBuffer_.allocation)) {
            return false;
        }
        textVertexBuffer_.capacityBytes = requiredBytes;
        return true;
    }

    const UiFontGlyphPlacement* glyphPlacement(const font::UiFontGlyph& glyph) const {
        const auto* glyphBegin = font::kUiFontGlyphs;
        const auto* glyphEnd = font::kUiFontGlyphs + font::kUiFontGlyphCount;
        if (&glyph < glyphBegin || &glyph >= glyphEnd) {
            return nullptr;
        }
        const std::size_t index = static_cast<std::size_t>(&glyph - glyphBegin);
        if (index >= uiFontGlyphPlacements_.size()) {
            return nullptr;
        }
        return &uiFontGlyphPlacements_[index];
    }

    void appendTextQuad(const float leftPx,
                        const float topPx,
                        const float rightPx,
                        const float bottomPx,
                        const UiFontGlyphPlacement& placement,
                        const std::array<float, 4>& color) {
        if (placement.atlasWidth == 0 || placement.atlasHeight == 0) {
            return;
        }

        const float width = std::max(1.0f, static_cast<float>(swapchainExtent_.width));
        const float height = std::max(1.0f, static_cast<float>(swapchainExtent_.height));
        const auto toNdcX = [width](const float value) { return value / width * 2.0f - 1.0f; };
        const auto toNdcY = [height](const float value) { return value / height * 2.0f - 1.0f; };

        const TextVertex topLeft{
            toNdcX(leftPx), toNdcY(topPx), placement.u0, placement.v0,
            color[0], color[1], color[2], color[3]
        };
        const TextVertex topRight{
            toNdcX(rightPx), toNdcY(topPx), placement.u1, placement.v0,
            color[0], color[1], color[2], color[3]
        };
        const TextVertex bottomLeft{
            toNdcX(leftPx), toNdcY(bottomPx), placement.u0, placement.v1,
            color[0], color[1], color[2], color[3]
        };
        const TextVertex bottomRight{
            toNdcX(rightPx), toNdcY(bottomPx), placement.u1, placement.v1,
            color[0], color[1], color[2], color[3]
        };

        textVertices_.push_back(topLeft);
        textVertices_.push_back(bottomLeft);
        textVertices_.push_back(topRight);
        textVertices_.push_back(topRight);
        textVertices_.push_back(bottomLeft);
        textVertices_.push_back(bottomRight);
    }

    void recordBitmapText(const VkClearAttachment& attachment,
                          const float startXPx,
                          const float baselineYPx,
                          std::wstring_view text,
                          const float scale,
                          const float tracking = 0.0f,
                          const float lineSpacing = 1.0f) {
        const auto color = textColor(attachment);
        float penX = startXPx;
        float baseline = baselineYPx;
        const float scaledLineHeight = static_cast<float>(font::kUiFontLineHeight) * scale * lineSpacing;

        for (const wchar_t character : text) {
            if (character == L'\n') {
                penX = startXPx;
                baseline += scaledLineHeight;
                continue;
            }

            const auto* glyph = findUiFontGlyph(static_cast<std::uint32_t>(character));
            if (glyph == nullptr) {
                continue;
            }
            const auto* placement = glyphPlacement(*glyph);
            if (placement == nullptr) {
                continue;
            }

            const float glyphLeft = penX + static_cast<float>(glyph->bearingX) * scale;
            const float glyphTop = baseline - static_cast<float>(glyph->bearingY) * scale;
            appendTextQuad(
                glyphLeft,
                glyphTop,
                glyphLeft + static_cast<float>(glyph->width) * scale,
                glyphTop + static_cast<float>(glyph->height) * scale,
                *placement,
                color);
            penX += static_cast<float>(glyph->advance) * scale + tracking;
        }
    }

    void recordCenteredBitmapText(const VkClearAttachment& attachment,
                                  const float centerXPx,
                                  const float baselineYPx,
                                  std::wstring_view text,
                                  const float scale,
                                  const float tracking = 0.0f) {
        const float width = measureBitmapTextWidth(text, scale, tracking);
        recordBitmapText(attachment, centerXPx - width * 0.5f, baselineYPx, text, scale, tracking);
    }

    void recordBitmapText(VulkanDispatch&,
                          const VkCommandBuffer,
                          const VkExtent2D,
                          const VkClearAttachment& attachment,
                          const float startXPx,
                          const float baselineYPx,
                          std::wstring_view text,
                          const float scale,
                          const float tracking = 0.0f,
                          const float lineSpacing = 1.0f) {
        recordBitmapText(attachment, startXPx, baselineYPx, text, scale, tracking, lineSpacing);
    }

    void recordCenteredBitmapText(VulkanDispatch&,
                                  const VkCommandBuffer,
                                  const VkExtent2D,
                                  const VkClearAttachment& attachment,
                                  const float centerXPx,
                                  const float baselineYPx,
                                  std::wstring_view text,
                                  const float scale,
                                  const float tracking = 0.0f) {
        recordCenteredBitmapText(attachment, centerXPx, baselineYPx, text, scale, tracking);
    }

    bool createTextPipeline() {
        if (!createTexturePipelineResources() || !createUiFontAtlasTexture()) {
            return false;
        }

        const auto shaderRoot = rendererAssetRootPath() / "generated" / "shaders";
        VkShaderModule vertexShader = VK_NULL_HANDLE;
        VkShaderModule fragmentShader = VK_NULL_HANDLE;
        if (!createShaderModuleFromFile(shaderRoot / "text.vert.spv", vertexShader) ||
            !createShaderModuleFromFile(shaderRoot / "text.frag.spv", fragmentShader)) {
            if (vertexShader != VK_NULL_HANDLE) {
                dispatch_.vkDestroyShaderModule(device_, vertexShader, nullptr);
            }
            if (fragmentShader != VK_NULL_HANDLE) {
                dispatch_.vkDestroyShaderModule(device_, fragmentShader, nullptr);
            }
            return false;
        }

        VkPipelineShaderStageCreateInfo vertexStage{};
        vertexStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexStage.module = vertexShader;
        vertexStage.pName = "main";

        VkPipelineShaderStageCreateInfo fragmentStage{};
        fragmentStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragmentStage.module = fragmentShader;
        fragmentStage.pName = "main";
        const std::array stages{vertexStage, fragmentStage};

        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(TextVertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        const std::array<VkVertexInputAttributeDescription, 3> attributes{{
            {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(TextVertex, x)},
            {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(TextVertex, u)},
            {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(TextVertex, r)},
        }};

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
        vertexInput.pVertexAttributeDescriptions = attributes.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlend{};
        colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlend.attachmentCount = 1;
        colorBlend.pAttachments = &colorBlendAttachment;

        constexpr std::array dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &textureDescriptorSetLayout_;
        if (dispatch_.vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &textPipelineLayout_) != VK_SUCCESS) {
            dispatch_.vkDestroyShaderModule(device_, vertexShader, nullptr);
            dispatch_.vkDestroyShaderModule(device_, fragmentShader, nullptr);
            return false;
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = static_cast<std::uint32_t>(stages.size());
        pipelineInfo.pStages = stages.data();
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlend;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = textPipelineLayout_;
        pipelineInfo.renderPass = renderPass_;
        pipelineInfo.subpass = 0;

        const bool ok = dispatch_.vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &textPipeline_) == VK_SUCCESS;
        dispatch_.vkDestroyShaderModule(device_, vertexShader, nullptr);
        dispatch_.vkDestroyShaderModule(device_, fragmentShader, nullptr);
        return ok;
    }

    void flushTextBatch(const VkCommandBuffer commandBuffer) {
        if (textVertices_.empty() || textPipeline_ == VK_NULL_HANDLE || uiFontAtlasTexture_.descriptorSet == VK_NULL_HANDLE) {
            return;
        }
        if (!ensureTextVertexCapacity(textVertices_.size())) {
            return;
        }

        void* mapped = nullptr;
        if (vmaMapMemory(allocator_, textVertexBuffer_.allocation, &mapped) != VK_SUCCESS) {
            return;
        }
        std::memcpy(mapped, textVertices_.data(), textVertices_.size() * sizeof(TextVertex));
        vmaUnmapMemory(allocator_, textVertexBuffer_.allocation);

        VkViewport viewport{};
        viewport.width = static_cast<float>(swapchainExtent_.width);
        viewport.height = static_cast<float>(swapchainExtent_.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.extent = swapchainExtent_;

        dispatch_.vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, textPipeline_);
        dispatch_.vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        dispatch_.vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        dispatch_.vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            textPipelineLayout_, 0, 1, &uiFontAtlasTexture_.descriptorSet, 0, nullptr);
        const VkDeviceSize offset = 0;
        dispatch_.vkCmdBindVertexBuffers(commandBuffer, 0, 1, &textVertexBuffer_.buffer, &offset);
        dispatch_.vkCmdDraw(commandBuffer, static_cast<std::uint32_t>(textVertices_.size()), 1, 0, 0);
    }

    bool createAllocator() {
        if (allocator_ != VK_NULL_HANDLE) {
            return true;
        }

        VmaVulkanFunctions functions{};
        functions.vkGetInstanceProcAddr = dispatch_.vkGetInstanceProcAddr;
        functions.vkGetDeviceProcAddr = dispatch_.vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo createInfo{};
        createInfo.instance = instance_;
        createInfo.physicalDevice = physicalDevice_;
        createInfo.device = device_;
        createInfo.vulkanApiVersion = VK_API_VERSION_1_0;
        createInfo.pVulkanFunctions = &functions;
        return vmaCreateAllocator(&createInfo, &allocator_) == VK_SUCCESS;
    }

    VkFormat chooseDepthFormat() const {
        constexpr std::array<VkFormat, 3> candidates{
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
        };
        for (const auto format : candidates) {
            VkFormatProperties properties{};
            dispatch_.vkGetPhysicalDeviceFormatProperties(physicalDevice_, format, &properties);
            if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
                return format;
            }
        }
        return VK_FORMAT_D32_SFLOAT;
    }

    bool createBuffer(const VkDeviceSize size,
                      const VkBufferUsageFlags usage,
                      const VkMemoryPropertyFlags properties,
                      VkBuffer& buffer,
                      VmaAllocation& allocation) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0
            ? VMA_MEMORY_USAGE_AUTO_PREFER_HOST
            : VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        allocInfo.requiredFlags = properties;
        allocInfo.flags = (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0
            ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
            : 0u;
        return vmaCreateBuffer(allocator_, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr) == VK_SUCCESS;
    }

    bool createDepthResources() {
        depthFormat_ = chooseDepthFormat();

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = swapchainExtent_.width;
        imageInfo.extent.height = swapchainExtent_.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = depthFormat_;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        if (vmaCreateImage(allocator_, &imageInfo, &allocInfo, &depthImage_, &depthAllocation_, nullptr) != VK_SUCCESS) {
            return false;
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = depthImage_;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = depthFormat_;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        if (dispatch_.vkCreateImageView(device_, &viewInfo, nullptr, &depthImageView_) != VK_SUCCESS) {
            return false;
        }
        return true;
    }

    bool createShaderModuleFromFile(const std::filesystem::path& path, VkShaderModule& shaderModule) {
        const std::vector<std::byte> bytes = util::FileSystem::readBinary(path);
        if (bytes.empty() || (bytes.size() % 4) != 0) {
            spdlog::error("Missing or invalid shader binary: {}", path.string());
            return false;
        }

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = bytes.size();
        createInfo.pCode = reinterpret_cast<const std::uint32_t*>(bytes.data());
        return dispatch_.vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule) == VK_SUCCESS;
    }

    bool createTexturePipelineResources() {
        if (textureDescriptorSetLayout_ == VK_NULL_HANDLE) {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = 0;
            binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount = 1;
            binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = 1;
            layoutInfo.pBindings = &binding;
            if (dispatch_.vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &textureDescriptorSetLayout_) != VK_SUCCESS) {
                return false;
            }
        }

        if (textureDescriptorPool_ == VK_NULL_HANDLE) {
            VkDescriptorPoolSize poolSize{};
            poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            poolSize.descriptorCount = 256;

            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.maxSets = 256;
            poolInfo.poolSizeCount = 1;
            poolInfo.pPoolSizes = &poolSize;
            if (dispatch_.vkCreateDescriptorPool(device_, &poolInfo, nullptr, &textureDescriptorPool_) != VK_SUCCESS) {
                return false;
            }
        }

        if (textureSampler_ == VK_NULL_HANDLE) {
            VkSamplerCreateInfo samplerInfo{};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.maxLod = 0.0f;
            if (dispatch_.vkCreateSampler(device_, &samplerInfo, nullptr, &textureSampler_) != VK_SUCCESS) {
                return false;
            }
        }

        return true;
    }

    bool createMeshPipeline() {
        if (!createTexturePipelineResources()) {
            return false;
        }

        const auto shaderRoot = rendererAssetRootPath() / "generated" / "shaders";
        VkShaderModule vertexShader = VK_NULL_HANDLE;
        VkShaderModule fragmentShader = VK_NULL_HANDLE;
        if (!createShaderModuleFromFile(shaderRoot / "mesh.vert.spv", vertexShader) ||
            !createShaderModuleFromFile(shaderRoot / "mesh.frag.spv", fragmentShader)) {
            if (vertexShader != VK_NULL_HANDLE) {
                dispatch_.vkDestroyShaderModule(device_, vertexShader, nullptr);
            }
            if (fragmentShader != VK_NULL_HANDLE) {
                dispatch_.vkDestroyShaderModule(device_, fragmentShader, nullptr);
            }
            return false;
        }

        VkPipelineShaderStageCreateInfo vertexStage{};
        vertexStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexStage.module = vertexShader;
        vertexStage.pName = "main";

        VkPipelineShaderStageCreateInfo fragmentStage{};
        fragmentStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragmentStage.module = fragmentShader;
        fragmentStage.pName = "main";
        const std::array stages{vertexStage, fragmentStage};

        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(vulkan::MeshVertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        const std::array<VkVertexInputAttributeDescription, 4> attributes{{
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vulkan::MeshVertex, px)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vulkan::MeshVertex, nx)},
            {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vulkan::MeshVertex, r)},
            {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(vulkan::MeshVertex, u)},
        }};

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
        vertexInput.pVertexAttributeDescriptions = attributes.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlend{};
        colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlend.attachmentCount = 1;
        colorBlend.pAttachments = &colorBlendAttachment;

        constexpr std::array dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(MeshPushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &textureDescriptorSetLayout_;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushRange;
        if (dispatch_.vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &meshPipelineLayout_) != VK_SUCCESS) {
            return false;
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = static_cast<std::uint32_t>(stages.size());
        pipelineInfo.pStages = stages.data();
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlend;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = meshPipelineLayout_;
        pipelineInfo.renderPass = renderPass_;
        pipelineInfo.subpass = 0;

        const bool ok = dispatch_.vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &meshPipeline_) == VK_SUCCESS;
        dispatch_.vkDestroyShaderModule(device_, vertexShader, nullptr);
        dispatch_.vkDestroyShaderModule(device_, fragmentShader, nullptr);
        return ok;
    }

    void destroyBuffer(GpuBuffer& buffer) {
        if (buffer.buffer != VK_NULL_HANDLE && buffer.allocation != VK_NULL_HANDLE && allocator_ != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator_, buffer.buffer, buffer.allocation);
        }
        buffer = {};
    }

    bool uploadMesh(const vulkan::CpuMesh& source, GpuBuffer& target) {
        destroyBuffer(target);
        if (!source.valid || source.vertices.empty()) {
            return false;
        }

        const VkDeviceSize size = sizeof(vulkan::MeshVertex) * source.vertices.size();
        if (!createBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                target.buffer, target.allocation)) {
            return false;
        }

        void* mapped = nullptr;
        if (vmaMapMemory(allocator_, target.allocation, &mapped) != VK_SUCCESS) {
            destroyBuffer(target);
            return false;
        }
        std::memcpy(mapped, source.vertices.data(), static_cast<std::size_t>(size));
        vmaUnmapMemory(allocator_, target.allocation);
        target.vertexCount = static_cast<std::uint32_t>(source.vertices.size());
        target.center = source.center;
        target.radius = source.radius;
        return true;
    }

    void destroyTexture(TextureResource& texture) {
        if (texture.view != VK_NULL_HANDLE && dispatch_.vkDestroyImageView != nullptr) {
            dispatch_.vkDestroyImageView(device_, texture.view, nullptr);
        }
        if (texture.image != VK_NULL_HANDLE && texture.allocation != VK_NULL_HANDLE && allocator_ != VK_NULL_HANDLE) {
            vmaDestroyImage(allocator_, texture.image, texture.allocation);
        }
        texture = {};
    }

    bool beginImmediateCommands(VkCommandBuffer& commandBuffer) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool_;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        if (dispatch_.vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            return false;
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (dispatch_.vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            dispatch_.vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
            commandBuffer = VK_NULL_HANDLE;
            return false;
        }

        return true;
    }

    void endImmediateCommands(VkCommandBuffer commandBuffer) {
        if (commandBuffer == VK_NULL_HANDLE) {
            return;
        }

        dispatch_.vkEndCommandBuffer(commandBuffer);
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        dispatch_.vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
        dispatch_.vkDeviceWaitIdle(device_);
        dispatch_.vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
    }

    bool createTextureDescriptor(TextureResource& texture) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = textureDescriptorPool_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &textureDescriptorSetLayout_;
        if (dispatch_.vkAllocateDescriptorSets(device_, &allocInfo, &texture.descriptorSet) != VK_SUCCESS) {
            return false;
        }

        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = textureSampler_;
        imageInfo.imageView = texture.view;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = texture.descriptorSet;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &imageInfo;
        dispatch_.vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
        return true;
    }

    bool createTextureFromRgba(const unsigned char* pixels,
                               const std::uint32_t width,
                               const std::uint32_t height,
                               TextureResource& texture) {
        destroyTexture(texture);
        if (pixels == nullptr || width == 0 || height == 0) {
            return false;
        }

        const VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4;
        GpuBuffer staging{};
        if (!createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                staging.buffer, staging.allocation)) {
            return false;
        }

        void* mapped = nullptr;
        if (vmaMapMemory(allocator_, staging.allocation, &mapped) != VK_SUCCESS) {
            destroyBuffer(staging);
            return false;
        }
        std::memcpy(mapped, pixels, static_cast<std::size_t>(imageSize));
        vmaUnmapMemory(allocator_, staging.allocation);

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {width, height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        if (vmaCreateImage(allocator_, &imageInfo, &allocInfo, &texture.image, &texture.allocation, nullptr) != VK_SUCCESS) {
            destroyTexture(texture);
            destroyBuffer(staging);
            return false;
        }

        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        if (!beginImmediateCommands(commandBuffer)) {
            destroyTexture(texture);
            destroyBuffer(staging);
            return false;
        }

        VkImageMemoryBarrier toTransfer{};
        toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image = texture.image;
        toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toTransfer.subresourceRange.levelCount = 1;
        toTransfer.subresourceRange.layerCount = 1;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dispatch_.vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &toTransfer);

        VkBufferImageCopy copyRegion{};
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = {width, height, 1};
        dispatch_.vkCmdCopyBufferToImage(commandBuffer, staging.buffer, texture.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        VkImageMemoryBarrier toShaderRead = toTransfer;
        toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dispatch_.vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &toShaderRead);
        endImmediateCommands(commandBuffer);
        destroyBuffer(staging);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = texture.image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        if (dispatch_.vkCreateImageView(device_, &viewInfo, nullptr, &texture.view) != VK_SUCCESS ||
            !createTextureDescriptor(texture)) {
            destroyTexture(texture);
            return false;
        }

        texture.width = width;
        texture.height = height;
        texture.valid = true;
        return true;
    }

    bool loadTextureFromFile(const std::filesystem::path& texturePath, TextureResource& texture) {
        const auto resolvedPath = resolveAssetPath(texturePath);
        const std::vector<std::byte> bytes = util::FileSystem::readBinary(resolvedPath);
        if (bytes.empty()) {
            return false;
        }

        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char* pixels = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(bytes.data()),
            static_cast<int>(bytes.size()),
            &width,
            &height,
            &channels,
            STBI_rgb_alpha);
        if (pixels == nullptr) {
            const auto ppmDecoded = decodePpmTexture(bytes);
            if (!ppmDecoded.has_value()) {
                spdlog::warn("[Renderer] Failed to decode texture: {}", resolvedPath.generic_string());
                return false;
            }
            spdlog::info("[Renderer] stb decode failed, using PPM fallback: {}", resolvedPath.generic_string());
            return createTextureFromRgba(
                ppmDecoded->rgba.data(),
                ppmDecoded->width,
                ppmDecoded->height,
                texture);
        }

        const bool ok = createTextureFromRgba(pixels, static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), texture);
        stbi_image_free(pixels);
        return ok;
    }

    bool ensureDefaultTexture() {
        if (defaultTexture_.valid) {
            return true;
        }

        constexpr unsigned char kWhitePixel[4] = {255, 255, 255, 255};
        return createTextureFromRgba(kWhitePixel, 1, 1, defaultTexture_);
    }

    std::filesystem::path resolveTexturePath(const std::filesystem::path& explicitAlbedoPath,
                                             const std::filesystem::path& materialPath) const {
        if (!explicitAlbedoPath.empty()) {
            return resolveAssetPath(explicitAlbedoPath);
        }
        return parseMaterialAlbedoPath(materialPath);
    }

    const TextureResource* cachedTexture(const std::filesystem::path& explicitAlbedoPath,
                                         const std::filesystem::path& materialPath) {
        const std::filesystem::path texturePath = resolveTexturePath(explicitAlbedoPath, materialPath);
        if (texturePath.empty()) {
            return ensureDefaultTexture() ? &defaultTexture_ : nullptr;
        }

        const std::string key = texturePath.generic_string();
        for (auto& cached : textureCache_) {
            if (cached.path == key) {
                return cached.texture.valid ? &cached.texture : (ensureDefaultTexture() ? &defaultTexture_ : nullptr);
            }
        }

        CachedTexture cached{.path = key, .texture = {}};
        if (!loadTextureFromFile(texturePath, cached.texture)) {
            textureCache_.push_back(std::move(cached));
            return ensureDefaultTexture() ? &defaultTexture_ : nullptr;
        }
        textureCache_.push_back(std::move(cached));
        return &textureCache_.back().texture;
    }

    const GpuBuffer* cachedSourceMesh(const std::filesystem::path& path) {
        if (path.empty()) {
            return nullptr;
        }
        const std::string key = path.generic_string();
        for (auto& cached : gpuMeshCache_) {
            if (cached.path == key) {
                return cached.buffer.vertexCount > 0 ? &cached.buffer : nullptr;
            }
        }

        vulkan::CpuMesh cpuMesh = vulkan::loadMeshAsset(rendererAssetRootPath(), path);
        CachedGpuMesh cached{.path = key, .buffer = {}};
        if (!uploadMesh(cpuMesh, cached.buffer)) {
            gpuMeshCache_.push_back(std::move(cached));
            return nullptr;
        }
        gpuMeshCache_.push_back(std::move(cached));
        return &gpuMeshCache_.back().buffer;
    }

    bool ensureStaticWorldMesh(const gameplay::MapData& map) {
        const std::string key = map.name + ":" + std::to_string(map.width) + "x" + std::to_string(map.depth);
        if (staticWorldMeshKey_ == key && staticWorldMesh_.vertexCount > 0) {
            return true;
        }
        vulkan::CpuMesh cpuMesh = vulkan::buildStaticWorldMesh(map);
        if (!uploadMesh(cpuMesh, staticWorldMesh_)) {
            return false;
        }
        staticWorldMeshKey_ = key;
        return true;
    }

    void destroyMeshResources() {
        destroyBuffer(staticWorldMesh_);
        staticWorldMeshKey_.clear();
        for (auto& cached : gpuMeshCache_) {
            destroyBuffer(cached.buffer);
        }
        gpuMeshCache_.clear();
        destroyStreamingBuffer(textVertexBuffer_);
        destroyTexture(uiFontAtlasTexture_);
        uiFontGlyphPlacements_.clear();
        textVertices_.clear();
        destroyTexture(defaultTexture_);
        for (auto& cached : textureCache_) {
            destroyTexture(cached.texture);
        }
        textureCache_.clear();
    }

    void drawMesh(const VkCommandBuffer commandBuffer,
                  const GpuBuffer& mesh,
                  const TextureResource* texture,
                  const glm::mat4& projectionView,
                  const glm::mat4& model,
                  const VkViewport& viewport,
                  const VkRect2D& scissor) {
        if (mesh.buffer == VK_NULL_HANDLE || mesh.vertexCount == 0 || meshPipeline_ == VK_NULL_HANDLE) {
            return;
        }

        dispatch_.vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipeline_);
        dispatch_.vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        dispatch_.vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        if (texture != nullptr && texture->descriptorSet != VK_NULL_HANDLE) {
            dispatch_.vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                meshPipelineLayout_, 0, 1, &texture->descriptorSet, 0, nullptr);
        }

        const VkDeviceSize offset = 0;
        dispatch_.vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mesh.buffer, &offset);
        const MeshPushConstants push = makePushConstants(projectionView, model);
        dispatch_.vkCmdPushConstants(commandBuffer, meshPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);
        dispatch_.vkCmdDraw(commandBuffer, mesh.vertexCount, 1, 0, 0);
    }

    bool createInstance() {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "my-cs-go";
        appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
        appInfo.pEngineName = "my-cs-go engine";
        appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        Uint32 extensionCount = 0;
        const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
        if (extensions == nullptr || extensionCount == 0) {
            spdlog::error("Failed to query SDL Vulkan instance extensions.");
            return false;
        }

        VkInstanceCreateInfo instanceInfo{};
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceInfo.pApplicationInfo = &appInfo;
        instanceInfo.enabledExtensionCount = extensionCount;
        instanceInfo.ppEnabledExtensionNames = extensions;

        if (dispatch_.vkCreateInstance(&instanceInfo, nullptr, &instance_) != VK_SUCCESS) {
            spdlog::error("Failed to create Vulkan instance.");
            return false;
        }

        if (!dispatch_.loadInstanceFunctions(instance_)) {
            spdlog::error("Failed to load Vulkan instance functions.");
            return false;
        }
        return true;
    }

    bool createSurface() {
        if (!SDL_Vulkan_CreateSurface(sdlWindow_, instance_, nullptr, &surface_)) {
            spdlog::error("Failed to create SDL Vulkan surface.");
            return false;
        }
        return true;
    }

    bool pickPhysicalDevice() {
        std::uint32_t deviceCount = 0;
        dispatch_.vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
        if (deviceCount == 0) {
            spdlog::error("No Vulkan GPU found.");
            return false;
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        dispatch_.vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

        for (const auto physicalDevice : devices) {
            std::uint32_t queueFamilyCount = 0;
            dispatch_.vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
            std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
            dispatch_.vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, families.data());

            for (std::uint32_t index = 0; index < queueFamilyCount; ++index) {
                VkBool32 presentSupported = VK_FALSE;
                dispatch_.vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, index, surface_, &presentSupported);
                if ((families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && presentSupported == VK_TRUE) {
                    physicalDevice_ = physicalDevice;
                    queueFamilyIndex_ = index;
                    return true;
                }
            }
        }

        spdlog::error("No graphics+present queue family available.");
        return false;
    }

    bool createDevice() {
        constexpr float queuePriority = 1.0f;

        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = queueFamilyIndex_;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;

        constexpr std::array<const char*, 1> extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        VkDeviceCreateInfo deviceInfo{};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
        deviceInfo.ppEnabledExtensionNames = extensions.data();

        if (dispatch_.vkCreateDevice(physicalDevice_, &deviceInfo, nullptr, &device_) != VK_SUCCESS) {
            spdlog::error("Failed to create Vulkan device.");
            return false;
        }
        if (!dispatch_.loadDeviceFunctions(device_)) {
            spdlog::error("Failed to load Vulkan device functions.");
            return false;
        }

        dispatch_.vkGetDeviceQueue(device_, queueFamilyIndex_, 0, &graphicsQueue_);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if (dispatch_.vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailableSemaphore_) != VK_SUCCESS ||
            dispatch_.vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &renderFinishedSemaphore_) != VK_SUCCESS) {
            spdlog::error("Failed to create Vulkan semaphores.");
            return false;
        }

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        if (dispatch_.vkCreateFence(device_, &fenceInfo, nullptr, &renderFence_) != VK_SUCCESS) {
            spdlog::error("Failed to create Vulkan fence.");
            return false;
        }

        return true;
    }

    bool createSwapchainResources() {
        VkSurfaceCapabilitiesKHR capabilities{};
        if (dispatch_.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &capabilities) != VK_SUCCESS) {
            return false;
        }

        std::uint32_t formatCount = 0;
        dispatch_.vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, nullptr);
        if (formatCount == 0) {
            spdlog::error("No Vulkan surface formats available.");
            return false;
        }
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        dispatch_.vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, formats.data());
        surfaceFormat_ = chooseSurfaceFormat(formats);

        std::uint32_t presentModeCount = 0;
        dispatch_.vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &presentModeCount, nullptr);
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        if (presentModeCount > 0) {
            dispatch_.vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &presentModeCount, presentModes.data());
        }
        const VkPresentModeKHR presentMode = presentModes.empty() ? VK_PRESENT_MODE_FIFO_KHR : choosePresentMode(presentModes);

        const VkExtent2D clientExtent = queryExtent(hwnd_);
        if (capabilities.currentExtent.width != UINT32_MAX) {
            swapchainExtent_ = capabilities.currentExtent;
        } else {
            swapchainExtent_.width = std::clamp(clientExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            swapchainExtent_.height = std::clamp(clientExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        }

        std::uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0) {
            imageCount = std::min(imageCount, capabilities.maxImageCount);
        }

        VkSwapchainCreateInfoKHR swapchainInfo{};
        swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainInfo.surface = surface_;
        swapchainInfo.minImageCount = imageCount;
        swapchainInfo.imageFormat = surfaceFormat_.format;
        swapchainInfo.imageColorSpace = surfaceFormat_.colorSpace;
        swapchainInfo.imageExtent = swapchainExtent_;
        swapchainInfo.imageArrayLayers = 1;
        swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainInfo.preTransform = capabilities.currentTransform;
        swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchainInfo.presentMode = presentMode;
        swapchainInfo.clipped = VK_TRUE;

        if (dispatch_.vkCreateSwapchainKHR(device_, &swapchainInfo, nullptr, &swapchain_) != VK_SUCCESS) {
            spdlog::error("Failed to create Vulkan swapchain.");
            return false;
        }

        std::uint32_t swapchainImageCount = 0;
        dispatch_.vkGetSwapchainImagesKHR(device_, swapchain_, &swapchainImageCount, nullptr);
        std::vector<VkImage> images(swapchainImageCount);
        dispatch_.vkGetSwapchainImagesKHR(device_, swapchain_, &swapchainImageCount, images.data());
        frames_.resize(swapchainImageCount);
        for (std::size_t index = 0; index < frames_.size(); ++index) {
            frames_[index].image = images[index];
        }

        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = surfaceFormat_.format;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorReference{};
        colorReference.attachment = 0;
        colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        depthFormat_ = chooseDepthFormat();
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = depthFormat_;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthReference{};
        depthReference.attachment = 1;
        depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorReference;
        subpass.pDepthStencilAttachment = &depthReference;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        const std::array attachments{colorAttachment, depthAttachment};
        renderPassInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (dispatch_.vkCreateRenderPass(device_, &renderPassInfo, nullptr, &renderPass_) != VK_SUCCESS) {
            spdlog::error("Failed to create Vulkan render pass.");
            return false;
        }

        if (!createDepthResources()) {
            spdlog::error("Failed to create depth resources.");
            return false;
        }

        for (auto& frame : frames_) {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = frame.image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = surfaceFormat_.format;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.layerCount = 1;
            if (dispatch_.vkCreateImageView(device_, &viewInfo, nullptr, &frame.imageView) != VK_SUCCESS) {
                spdlog::error("Failed to create swapchain image view.");
                return false;
            }
        }

        VkCommandPoolCreateInfo commandPoolInfo{};
        commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        commandPoolInfo.queueFamilyIndex = queueFamilyIndex_;
        if (dispatch_.vkCreateCommandPool(device_, &commandPoolInfo, nullptr, &commandPool_) != VK_SUCCESS) {
            spdlog::error("Failed to create command pool.");
            return false;
        }

        std::vector<VkCommandBuffer> commandBuffers(frames_.size());
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool_;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<std::uint32_t>(frames_.size());
        if (dispatch_.vkAllocateCommandBuffers(device_, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            spdlog::error("Failed to allocate command buffers.");
            return false;
        }

        for (std::size_t index = 0; index < frames_.size(); ++index) {
            frames_[index].commandBuffer = commandBuffers[index];

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass_;
            const std::array attachments{frames_[index].imageView, depthImageView_};
            framebufferInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = swapchainExtent_.width;
            framebufferInfo.height = swapchainExtent_.height;
            framebufferInfo.layers = 1;
            if (dispatch_.vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &frames_[index].framebuffer) != VK_SUCCESS) {
                spdlog::error("Failed to create framebuffer.");
                return false;
            }
        }

        if (!createMeshPipeline()) {
            spdlog::error("Failed to create mesh graphics pipeline.");
            return false;
        }
        if (!createTextPipeline()) {
            spdlog::error("Failed to create text graphics pipeline.");
            return false;
        }

        return true;
    }

    void destroySwapchainResources() {
        if (device_ == VK_NULL_HANDLE) {
            return;
        }
        destroyStreamingBuffer(textVertexBuffer_);
        destroyTexture(uiFontAtlasTexture_);
        uiFontGlyphPlacements_.clear();
        textVertices_.clear();
        for (auto& frame : frames_) {
            if (frame.framebuffer != VK_NULL_HANDLE && dispatch_.vkDestroyFramebuffer != nullptr) {
                dispatch_.vkDestroyFramebuffer(device_, frame.framebuffer, nullptr);
                frame.framebuffer = VK_NULL_HANDLE;
            }
            if (frame.imageView != VK_NULL_HANDLE && dispatch_.vkDestroyImageView != nullptr) {
                dispatch_.vkDestroyImageView(device_, frame.imageView, nullptr);
                frame.imageView = VK_NULL_HANDLE;
            }
        }
        frames_.clear();

        if (commandPool_ != VK_NULL_HANDLE && dispatch_.vkDestroyCommandPool != nullptr) {
            dispatch_.vkDestroyCommandPool(device_, commandPool_, nullptr);
            commandPool_ = VK_NULL_HANDLE;
        }
        if (meshPipeline_ != VK_NULL_HANDLE && dispatch_.vkDestroyPipeline != nullptr) {
            dispatch_.vkDestroyPipeline(device_, meshPipeline_, nullptr);
            meshPipeline_ = VK_NULL_HANDLE;
        }
        if (meshPipelineLayout_ != VK_NULL_HANDLE && dispatch_.vkDestroyPipelineLayout != nullptr) {
            dispatch_.vkDestroyPipelineLayout(device_, meshPipelineLayout_, nullptr);
            meshPipelineLayout_ = VK_NULL_HANDLE;
        }
        if (textPipeline_ != VK_NULL_HANDLE && dispatch_.vkDestroyPipeline != nullptr) {
            dispatch_.vkDestroyPipeline(device_, textPipeline_, nullptr);
            textPipeline_ = VK_NULL_HANDLE;
        }
        if (textPipelineLayout_ != VK_NULL_HANDLE && dispatch_.vkDestroyPipelineLayout != nullptr) {
            dispatch_.vkDestroyPipelineLayout(device_, textPipelineLayout_, nullptr);
            textPipelineLayout_ = VK_NULL_HANDLE;
        }
        if (textureSampler_ != VK_NULL_HANDLE && dispatch_.vkDestroySampler != nullptr) {
            dispatch_.vkDestroySampler(device_, textureSampler_, nullptr);
            textureSampler_ = VK_NULL_HANDLE;
        }
        if (textureDescriptorPool_ != VK_NULL_HANDLE && dispatch_.vkDestroyDescriptorPool != nullptr) {
            dispatch_.vkDestroyDescriptorPool(device_, textureDescriptorPool_, nullptr);
            textureDescriptorPool_ = VK_NULL_HANDLE;
        }
        if (textureDescriptorSetLayout_ != VK_NULL_HANDLE && dispatch_.vkDestroyDescriptorSetLayout != nullptr) {
            dispatch_.vkDestroyDescriptorSetLayout(device_, textureDescriptorSetLayout_, nullptr);
            textureDescriptorSetLayout_ = VK_NULL_HANDLE;
        }
        if (depthImageView_ != VK_NULL_HANDLE && dispatch_.vkDestroyImageView != nullptr) {
            dispatch_.vkDestroyImageView(device_, depthImageView_, nullptr);
            depthImageView_ = VK_NULL_HANDLE;
        }
        if (depthImage_ != VK_NULL_HANDLE && depthAllocation_ != VK_NULL_HANDLE && allocator_ != VK_NULL_HANDLE) {
            vmaDestroyImage(allocator_, depthImage_, depthAllocation_);
            depthImage_ = VK_NULL_HANDLE;
            depthAllocation_ = VK_NULL_HANDLE;
        }
        if (renderPass_ != VK_NULL_HANDLE && dispatch_.vkDestroyRenderPass != nullptr) {
            dispatch_.vkDestroyRenderPass(device_, renderPass_, nullptr);
            renderPass_ = VK_NULL_HANDLE;
        }
        if (swapchain_ != VK_NULL_HANDLE && dispatch_.vkDestroySwapchainKHR != nullptr) {
            dispatch_.vkDestroySwapchainKHR(device_, swapchain_, nullptr);
            swapchain_ = VK_NULL_HANDLE;
        }
    }

    void drawFrame(const RenderFrame& frame) {
        if (device_ == VK_NULL_HANDLE || swapchain_ == VK_NULL_HANDLE) {
            return;
        }

        if (dispatch_.vkWaitForFences(device_, 1, &renderFence_, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
            return;
        }

        std::uint32_t imageIndex = 0;
        const VkResult acquireResult = dispatch_.vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, imageAvailableSemaphore_, VK_NULL_HANDLE, &imageIndex);
        if (acquireResult != VK_SUCCESS) {
            return;
        }

        dispatch_.vkResetFences(device_, 1, &renderFence_);
        dispatch_.vkResetCommandBuffer(frames_[imageIndex].commandBuffer, 0);
        recordCommandBuffer(frames_[imageIndex], frame);

        constexpr VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &imageAvailableSemaphore_;
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &frames_[imageIndex].commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderFinishedSemaphore_;
        if (dispatch_.vkQueueSubmit(graphicsQueue_, 1, &submitInfo, renderFence_) != VK_SUCCESS) {
            return;
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderFinishedSemaphore_;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain_;
        presentInfo.pImageIndices = &imageIndex;
        dispatch_.vkQueuePresentKHR(graphicsQueue_, &presentInfo);
    }

    const PreviewMeshModel* cachedPreviewMeshModel(const std::filesystem::path& path) {
        if (path.empty()) {
            return nullptr;
        }
        const std::string key = path.generic_string();
        for (auto& cached : previewMeshCache_) {
            if (cached.path == key) {
                return cached.model.valid ? &cached.model : nullptr;
            }
        }
        previewMeshCache_.push_back({key, loadPreviewMeshModel(path)});
        return previewMeshCache_.back().model.valid ? &previewMeshCache_.back().model : nullptr;
    }

    void recordEquipmentPreview(const VkCommandBuffer commandBuffer, const RenderFrame& renderFrame) {
        const auto previewPanel = makeRect(swapchainExtent_, 0.60f, 0.03f, 0.80f, 0.29f);
        clearRect(dispatch_, commandBuffer, makeAttachment(0.07f, 0.10f, 0.13f), previewPanel);

        if (const auto* previewMesh = cachedSourceMesh(renderFrame.activeEquipmentModelPath)) {
            const auto* previewTexture = cachedTexture(renderFrame.activeEquipmentAlbedoPath, renderFrame.activeEquipmentMaterialPath);
            const float time = renderFrame.world != nullptr ? renderFrame.world->elapsedSeconds() : 0.0f;
            const float yaw = time * 0.85f + 0.35f;
            const float pitch = -0.36f;
            glm::mat4 projection = glm::perspectiveRH_ZO(0.90f,
                static_cast<float>(previewPanel.rect.extent.width) / std::max(1.0f, static_cast<float>(previewPanel.rect.extent.height)),
                0.05f, 32.0f);
            projection[1][1] *= -1.0f;
            const glm::mat4 view = glm::lookAtRH(glm::vec3(0.0f, 0.0f, 2.8f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::mat4 projectionView = projection * view;
            const glm::mat4 centered = glm::translate(glm::mat4(1.0f), glm::vec3(-previewMesh->center.x, -previewMesh->center.y, -previewMesh->center.z));
            const glm::mat4 scaled = glm::scale(glm::mat4(1.0f), glm::vec3(0.85f / std::max(0.001f, previewMesh->radius)));
            const glm::mat4 rotated = glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0.0f, 1.0f, 0.0f)) *
                glm::rotate(glm::mat4(1.0f), pitch, glm::vec3(1.0f, 0.0f, 0.0f));
            const glm::mat4 model = rotated * scaled * centered;
            const VkViewport viewport{
                static_cast<float>(previewPanel.rect.offset.x),
                static_cast<float>(previewPanel.rect.offset.y),
                static_cast<float>(previewPanel.rect.extent.width),
                static_cast<float>(previewPanel.rect.extent.height),
                0.0f,
                1.0f,
            };
            drawMesh(commandBuffer, *previewMesh, previewTexture, projectionView, model, viewport, previewPanel.rect);
            return;
        }

        const auto* previewModel = cachedPreviewMeshModel(renderFrame.activeEquipmentModelPath);
        if (previewModel == nullptr) {
            return;
        }

        const float panelLeft = static_cast<float>(previewPanel.rect.offset.x);
        const float panelTop = static_cast<float>(previewPanel.rect.offset.y);
        const float panelWidth = static_cast<float>(previewPanel.rect.extent.width);
        const float panelHeight = static_cast<float>(previewPanel.rect.extent.height);
        const float centerX = panelLeft + panelWidth * 0.52f;
        const float centerY = panelTop + panelHeight * 0.58f;
        const float previewScale = std::min(panelWidth, panelHeight) * 0.34f / std::max(0.001f, previewModel->radius);
        const float time = renderFrame.world != nullptr ? renderFrame.world->elapsedSeconds() : 0.0f;
        const float yaw = time * 0.85f + 0.35f;
        const float pitch = -0.36f;
        const float cosYaw = std::cos(yaw);
        const float sinYaw = std::sin(yaw);
        const float cosPitch = std::cos(pitch);
        const float sinPitch = std::sin(pitch);
        const float cameraDistance = previewModel->radius * 4.2f;
        struct PreviewFace {
            ProjectedVertex a{};
            ProjectedVertex b{};
            ProjectedVertex c{};
            util::Vec3 color{};
            float depth = 0.0f;
        };

        std::vector<ProjectedVertex> projected;
        projected.reserve(previewModel->vertices.size());
        std::vector<util::Vec3> transformed;
        transformed.reserve(previewModel->vertices.size());
        for (const auto& source : previewModel->vertices) {
            util::Vec3 vertex{
                source.x - previewModel->center.x,
                source.y - previewModel->center.y,
                source.z - previewModel->center.z,
            };
            const float yawX = vertex.x * cosYaw - vertex.z * sinYaw;
            const float yawZ = vertex.x * sinYaw + vertex.z * cosYaw;
            const float pitchY = vertex.y * cosPitch - yawZ * sinPitch;
            const float pitchZ = vertex.y * sinPitch + yawZ * cosPitch + cameraDistance;
            transformed.push_back({yawX, pitchY, pitchZ});
            projected.push_back({centerX + (yawX / pitchZ) * previewScale, centerY - (pitchY / pitchZ) * previewScale, pitchZ});
        }

        std::vector<PreviewFace> faces;
        faces.reserve(previewModel->triangles.size());
        for (const auto& triangle : previewModel->triangles) {
            if (triangle.a >= projected.size() || triangle.b >= projected.size() || triangle.c >= projected.size()) {
                continue;
            }
            const auto& pa = projected[triangle.a];
            const auto& pb = projected[triangle.b];
            const auto& pc = projected[triangle.c];
            if (pa.z <= 0.01f || pb.z <= 0.01f || pc.z <= 0.01f) {
                continue;
            }
            const auto& va = transformed[triangle.a];
            const auto& vb = transformed[triangle.b];
            const auto& vc = transformed[triangle.c];
            const util::Vec3 ab{vb.x - va.x, vb.y - va.y, vb.z - va.z};
            const util::Vec3 ac{vc.x - va.x, vc.y - va.y, vc.z - va.z};
            const util::Vec3 normal{
                ab.y * ac.z - ab.z * ac.y,
                ab.z * ac.x - ab.x * ac.z,
                ab.x * ac.y - ab.y * ac.x,
            };
            if (normal.z >= 0.0f) {
                continue;
            }
            const float normalLength = std::max(0.0001f, std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z));
            const util::Vec3 lightDir{-0.35f, 0.65f, -0.68f};
            const float diffuse = std::max(0.18f, -(normal.x * lightDir.x + normal.y * lightDir.y + normal.z * lightDir.z) / normalLength);
            faces.push_back({pa, pb, pc, triangle.color, (pa.z + pb.z + pc.z) / 3.0f});
            faces.back().color.x *= diffuse;
            faces.back().color.y *= diffuse;
            faces.back().color.z *= diffuse;
        }

        std::sort(faces.begin(), faces.end(), [](const PreviewFace& lhs, const PreviewFace& rhs) {
            return lhs.depth > rhs.depth;
        });

        for (const auto& face : faces) {
            recordFilledTriangle(dispatch_, commandBuffer, swapchainExtent_, makeAttachment(face.color), face.a, face.b, face.c);
        }
    }

    void recordSinglePlayerGameplay(const VkCommandBuffer commandBuffer, const RenderFrame& renderFrame) {
        if (renderFrame.world == nullptr) {
            return;
        }

        const int width = static_cast<int>(swapchainExtent_.width);
        const int height = static_cast<int>(swapchainExtent_.height);
        const float widthPx = static_cast<float>(width);
        const float heightPx = static_cast<float>(height);

        const auto& map = renderFrame.world->map();
        const float posX = renderFrame.cameraPosition.x;
        const float posY = renderFrame.cameraPosition.y;
        const float posZ = renderFrame.cameraPosition.z;
        const float dirX = std::cos(renderFrame.cameraYawRadians);
        const float dirY = std::sin(renderFrame.cameraPitchRadians);
        const float dirZ = std::sin(renderFrame.cameraYawRadians);
        const float magnification = renderFrame.activeEquipmentSlot == RenderFrame::EquipmentSlot::Primary
            ? std::clamp(renderFrame.opticMagnification, 1.0f, 8.0f)
            : 1.0f;
        const float fovRadians = std::clamp(1.18f / std::pow(magnification, 0.85f), 0.20f, 1.18f);
        glm::mat4 projection = glm::perspectiveRH_ZO(fovRadians, widthPx / std::max(1.0f, heightPx), 0.05f, 128.0f);
        projection[1][1] *= -1.0f;
        const util::Vec3 eye{posX, posY, posZ};
        const util::Vec3 target{
            posX + std::cos(renderFrame.cameraYawRadians) * std::cos(renderFrame.cameraPitchRadians),
            posY + dirY,
            posZ + std::sin(renderFrame.cameraYawRadians) * std::cos(renderFrame.cameraPitchRadians),
        };
        const glm::mat4 view = glm::lookAtRH(
            glm::vec3(eye.x, eye.y, eye.z),
            glm::vec3(target.x, target.y, target.z),
            glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::mat4 projectionView = projection * view;
        const VkViewport worldViewport{0.0f, 0.0f, widthPx, heightPx, 0.0f, 1.0f};
        const VkRect2D worldScissor{{0, 0}, swapchainExtent_};

        if (ensureStaticWorldMesh(map)) {
            const auto* defaultTexture = ensureDefaultTexture() ? &defaultTexture_ : nullptr;
            drawMesh(commandBuffer, staticWorldMesh_, defaultTexture, projectionView, glm::mat4(1.0f), worldViewport, worldScissor);
        }
        for (const auto& prop : map.props) {
            const auto* mesh = cachedSourceMesh(prop.modelPath);
            if (mesh == nullptr) {
                continue;
            }
            const auto* texture = cachedTexture({}, prop.materialPath);
            drawMesh(commandBuffer, *mesh, texture, projectionView,
                glm::translate(glm::mat4(1.0f), glm::vec3(prop.position.x, prop.position.y, prop.position.z)),
                worldViewport, worldScissor);
        }
        // The previous box-built bot placeholder was useful for debugging,
        // but it is visually distracting in the current art pass.
        // Keep bot simulation active and wait for a real character asset.

        if (renderFrame.smokeOverlay > 0.0f) {
            clearRect(dispatch_, commandBuffer, makeAttachment(0.60f, 0.64f, 0.68f), makeRect(swapchainExtent_, 0.12f, 0.26f, 0.88f, 0.74f));
            clearRect(dispatch_, commandBuffer, makeAttachment(0.54f, 0.58f, 0.62f), makeRect(swapchainExtent_, 0.00f, 0.36f, 1.00f, 0.62f));
        }
        if (renderFrame.flashOverlay > 0.0f) {
            const float flashStrength = std::clamp(renderFrame.flashOverlay / 1.1f, 0.0f, 1.0f);
            const float tint = 0.86f + flashStrength * 0.12f;
            clearRect(dispatch_, commandBuffer, makeAttachment(tint, tint, 0.95f), makeRect(swapchainExtent_, 0.0f, 0.0f, 1.0f, 1.0f));
        }

        const auto hudPanel = makeRect(swapchainExtent_, 0.02f, 0.02f, 0.29f, 0.17f);
        const auto hintPanel = makeRect(swapchainExtent_, 0.04f, 0.84f, 0.96f, 0.95f);
        const auto minimapPanel = makeRect(swapchainExtent_, 0.82f, 0.02f, 0.98f, 0.30f);
        clearRect(dispatch_, commandBuffer, makeAttachment(0.05f, 0.07f, 0.09f), hudPanel);
        clearRect(dispatch_, commandBuffer, makeAttachment(0.05f, 0.07f, 0.09f), hintPanel);
        clearRect(dispatch_, commandBuffer, makeAttachment(0.05f, 0.07f, 0.09f), minimapPanel);
        recordEquipmentPreview(commandBuffer, renderFrame);

        const float minimapLeft = widthPx * 0.83f;
        const float minimapTop = heightPx * 0.06f;
        const float minimapRight = widthPx * 0.97f;
        const float minimapBottom = heightPx * 0.28f;
        const float cellWidth = (minimapRight - minimapLeft) / static_cast<float>(std::max(1, map.width));
        const float cellHeight = (minimapBottom - minimapTop) / static_cast<float>(std::max(1, map.depth));
        for (int z = 0; z < map.depth; ++z) {
            for (int x = 0; x < map.width; ++x) {
                COLORREF cellColor = isBlockedCell(map, x, z) ? RGB(152, 158, 164) : RGB(56, 66, 76);
                if (hasRampCell(map, x, z)) {
                    cellColor = RGB(184, 156, 86);
                }
                clearRect(dispatch_, commandBuffer, makeAttachment(cellColor), makePixelRect(
                    swapchainExtent_,
                    minimapLeft + cellWidth * static_cast<float>(x),
                    minimapTop + cellHeight * static_cast<float>(z),
                    minimapLeft + cellWidth * static_cast<float>(x + 1),
                    minimapTop + cellHeight * static_cast<float>(z + 1)));
            }
        }
        for (const auto& prop : map.props) {
            const auto profile = describeProp(prop);
            const float iconX = minimapLeft + cellWidth * prop.position.x;
            const float iconY = minimapTop + cellHeight * prop.position.z;
            clearRect(dispatch_, commandBuffer, makeAttachment(profile.body),
                makePixelRect(swapchainExtent_, iconX - 3.0f, iconY - 3.0f, iconX + 3.0f, iconY + 3.0f));
            clearRect(dispatch_, commandBuffer, makeAttachment(profile.accent),
                makePixelRect(swapchainExtent_, iconX - 1.5f, iconY - 1.5f, iconX + 1.5f, iconY + 1.5f));
        }

        const float playerX = minimapLeft + cellWidth * posX;
        const float playerY = minimapTop + cellHeight * posZ;
        clearRect(dispatch_, commandBuffer, makeAttachment(1.0f, 0.85f, 0.38f),
            makePixelRect(swapchainExtent_, playerX - 4.0f, playerY - 4.0f, playerX + 4.0f, playerY + 4.0f));
        clearRect(dispatch_, commandBuffer, makeAttachment(1.0f, 0.94f, 0.70f),
            makePixelRect(swapchainExtent_, playerX + dirX * 10.0f - 2.0f, playerY + dirZ * 10.0f - 2.0f,
                playerX + dirX * 10.0f + 2.0f, playerY + dirZ * 10.0f + 2.0f));

        const float crossGap = 8.0f + renderFrame.crosshairSpread * 3.5f + renderFrame.recoilKick * 12.0f;
        const float crossArm = renderFrame.activeEquipmentSlot == RenderFrame::EquipmentSlot::Throwable ? 16.0f
            : (renderFrame.activeEquipmentSlot == RenderFrame::EquipmentSlot::Melee ? 6.0f : 10.0f + std::min(8.0f, renderFrame.crosshairSpread * 0.8f));
        const auto crossColor = renderFrame.lastShotHit ? makeAttachment(1.0f, 0.83f, 0.36f)
            : (renderFrame.activeEquipmentSlot == RenderFrame::EquipmentSlot::Throwable ? makeAttachment(0.92f, 0.94f, 0.54f)
                : makeAttachment(0.96f, 0.96f, 0.96f));
        const float crossX = widthPx * 0.5f;
        const float crossY = heightPx * 0.5f - renderFrame.recoilKick * 28.0f;
        clearRect(dispatch_, commandBuffer, crossColor, makePixelRect(swapchainExtent_, crossX - crossGap - crossArm, crossY - 1.5f, crossX - crossGap, crossY + 1.5f));
        clearRect(dispatch_, commandBuffer, crossColor, makePixelRect(swapchainExtent_, crossX + crossGap, crossY - 1.5f, crossX + crossGap + crossArm, crossY + 1.5f));
        clearRect(dispatch_, commandBuffer, crossColor, makePixelRect(swapchainExtent_, crossX - 1.5f, crossY - crossGap - crossArm, crossX + 1.5f, crossY - crossGap));
        clearRect(dispatch_, commandBuffer, crossColor, makePixelRect(swapchainExtent_, crossX - 1.5f, crossY + crossGap, crossX + 1.5f, crossY + crossGap + crossArm));
        clearRect(dispatch_, commandBuffer, crossColor, makePixelRect(swapchainExtent_, crossX - 2.0f, crossY - 2.0f, crossX + 2.0f, crossY + 2.0f));
        if (renderFrame.activeEquipmentSlot == RenderFrame::EquipmentSlot::Melee) {
            clearRect(dispatch_, commandBuffer, makeAttachment(0.96f, 0.74f, 0.70f),
                makePixelRect(swapchainExtent_, crossX - 12.0f, crossY - 12.0f, crossX - 8.0f, crossY + 12.0f));
        }
        if (renderFrame.lastShotHit) {
            clearRect(dispatch_, commandBuffer, makeAttachment(1.0f, 0.93f, 0.70f), makePixelRect(swapchainExtent_, crossX - 12.0f, crossY - 12.0f, crossX - 8.0f, crossY - 8.0f));
            clearRect(dispatch_, commandBuffer, makeAttachment(1.0f, 0.93f, 0.70f), makePixelRect(swapchainExtent_, crossX + 8.0f, crossY - 12.0f, crossX + 12.0f, crossY - 8.0f));
            clearRect(dispatch_, commandBuffer, makeAttachment(1.0f, 0.93f, 0.70f), makePixelRect(swapchainExtent_, crossX - 12.0f, crossY + 8.0f, crossX - 8.0f, crossY + 12.0f));
            clearRect(dispatch_, commandBuffer, makeAttachment(1.0f, 0.93f, 0.70f), makePixelRect(swapchainExtent_, crossX + 8.0f, crossY + 8.0f, crossX + 12.0f, crossY + 12.0f));
        }

        const float slotTop = heightPx * 0.86f;
        const float slotHeight = heightPx * 0.045f;
        const auto primarySlotColor = renderFrame.activeEquipmentSlot == RenderFrame::EquipmentSlot::Primary
            ? makeAttachment(0.78f, 0.64f, 0.22f) : makeAttachment(0.21f, 0.24f, 0.28f);
        const auto meleeSlotColor = renderFrame.activeEquipmentSlot == RenderFrame::EquipmentSlot::Melee
            ? makeAttachment(0.78f, 0.64f, 0.22f) : makeAttachment(0.21f, 0.24f, 0.28f);
        const auto throwableSlotColor = renderFrame.activeEquipmentSlot == RenderFrame::EquipmentSlot::Throwable
            ? makeAttachment(0.78f, 0.64f, 0.22f) : makeAttachment(0.21f, 0.24f, 0.28f);
        clearRect(dispatch_, commandBuffer, primarySlotColor, makePixelRect(swapchainExtent_, widthPx * 0.10f, slotTop, widthPx * 0.40f, slotTop + slotHeight));
        clearRect(dispatch_, commandBuffer, meleeSlotColor, makePixelRect(swapchainExtent_, widthPx * 0.42f, slotTop, widthPx * 0.58f, slotTop + slotHeight));
        clearRect(dispatch_, commandBuffer, throwableSlotColor, makePixelRect(swapchainExtent_, widthPx * 0.60f, slotTop, widthPx * 0.86f, slotTop + slotHeight));

        const float ammoRatio = std::clamp(renderFrame.ammoInMagazine / 40.0f, 0.0f, 1.0f);
        const float reserveRatio = std::clamp(renderFrame.reserveAmmo / 120.0f, 0.0f, 1.0f);
        const float elimRatio = std::clamp(renderFrame.eliminations / 10.0f, 0.0f, 1.0f);
        clearRect(dispatch_, commandBuffer, makeAttachment(0.13f, 0.16f, 0.20f), makePixelRect(swapchainExtent_, widthPx * 0.10f, heightPx * 0.81f, widthPx * 0.32f, heightPx * 0.825f));
        clearRect(dispatch_, commandBuffer, makeAttachment(0.90f, 0.80f, 0.24f), makePixelRect(swapchainExtent_, widthPx * 0.10f, heightPx * 0.81f, widthPx * (0.10f + 0.22f * ammoRatio), heightPx * 0.825f));
        clearRect(dispatch_, commandBuffer, makeAttachment(0.13f, 0.16f, 0.20f), makePixelRect(swapchainExtent_, widthPx * 0.34f, heightPx * 0.81f, widthPx * 0.56f, heightPx * 0.825f));
        clearRect(dispatch_, commandBuffer, makeAttachment(0.50f, 0.72f, 0.88f), makePixelRect(swapchainExtent_, widthPx * 0.34f, heightPx * 0.81f, widthPx * (0.34f + 0.22f * reserveRatio), heightPx * 0.825f));
        clearRect(dispatch_, commandBuffer, makeAttachment(0.13f, 0.16f, 0.20f), makePixelRect(swapchainExtent_, widthPx * 0.78f, heightPx * 0.81f, widthPx * 0.90f, heightPx * 0.825f));
        clearRect(dispatch_, commandBuffer, makeAttachment(0.91f, 0.42f, 0.24f), makePixelRect(swapchainExtent_, widthPx * 0.78f, heightPx * 0.81f, widthPx * (0.78f + 0.12f * elimRatio), heightPx * 0.825f));

        if (renderFrame.activeEquipmentSlot == RenderFrame::EquipmentSlot::Primary && renderFrame.opticMagnification > 1.0f) {
            clearRect(dispatch_, commandBuffer, makeAttachment(0.09f, 0.10f, 0.12f), makeRect(swapchainExtent_, 0.00f, 0.00f, 0.10f, 1.00f));
            clearRect(dispatch_, commandBuffer, makeAttachment(0.09f, 0.10f, 0.12f), makeRect(swapchainExtent_, 0.90f, 0.00f, 1.00f, 1.00f));
        }

        const auto titleColor = makeAttachment(0.96f, 0.95f, 0.91f);
        const auto bodyColor = makeAttachment(0.89f, 0.91f, 0.93f);
        const auto accentColor = makeAttachment(0.98f, 0.88f, 0.48f);
        const auto subtleColor = makeAttachment(0.72f, 0.79f, 0.84f);
        std::ostringstream hud;
        hud << "单机训练场\n";
        hud << "武器 " << renderFrame.activeWeaponLabel << "\n";
        hud << "准镜 " << renderFrame.activeOpticLabel << "\n";
        hud << "弹药 " << renderFrame.ammoInMagazine << " / " << renderFrame.reserveAmmo << "\n";
        hud << "击倒 " << renderFrame.eliminations << "\n";
        hud << "位置 " << static_cast<int>(renderFrame.cameraPosition.x * 10.0f) / 10.0f
            << ", " << static_cast<int>(renderFrame.cameraPosition.z * 10.0f) / 10.0f;
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, titleColor, widthPx * 0.03f, heightPx * 0.055f, widenUtf8(hud.str()), 0.44f, 0.05f, 1.10f);
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, titleColor, widthPx * 0.835f, heightPx * 0.045f, L"战术小地图", 0.36f);
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, titleColor,
            widthPx * 0.615f, heightPx * 0.055f, widenUtf8(renderFrame.activeEquipmentDisplayLabel), 0.38f, 0.05f);
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, bodyColor,
            widthPx * 0.11f, heightPx * 0.892f, L"1 主武器", 0.34f);
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, bodyColor,
            widthPx * 0.43f, heightPx * 0.892f, L"3 近战", 0.34f);
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, bodyColor,
            widthPx * 0.61f, heightPx * 0.892f, L"4 投掷物", 0.34f);
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, accentColor,
            widthPx * 0.11f, heightPx * 0.928f, widenUtf8(renderFrame.activeWeaponLabel), 0.34f, 0.05f);
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, subtleColor,
            widthPx * 0.43f, heightPx * 0.928f, widenUtf8(renderFrame.meleeWeaponLabel), 0.34f, 0.05f);
        std::ostringstream throwableHud;
        throwableHud << renderFrame.selectedThrowableLabel << "  "
                     << "破片 " << renderFrame.fragCount << "  "
                     << "闪光 " << renderFrame.flashCount << "  "
                     << "烟雾 " << renderFrame.smokeCount;
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, subtleColor,
            widthPx * 0.61f, heightPx * 0.928f, widenUtf8(throwableHud.str()), 0.31f, 0.04f);
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, bodyColor,
            widthPx * 0.05f, heightPx * 0.905f,
            L"WASD 移动  鼠标查看  空格跳跃  左键开火  R 换弹  Tab 切枪  V 切准镜  G 切投掷物  Esc 返回主菜单",
            0.30f, 0.03f);
    }

    void recordCommandBuffer(const FrameResources& frame, const RenderFrame& renderFrame) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (dispatch_.vkBeginCommandBuffer(frame.commandBuffer, &beginInfo) != VK_SUCCESS) {
            return;
        }

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = backgroundColor(renderFrame.appFlow);
        clearValues[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass_;
        renderPassInfo.framebuffer = frame.framebuffer;
        renderPassInfo.renderArea.offset.x = 0;
        renderPassInfo.renderArea.offset.y = 0;
        renderPassInfo.renderArea.extent = swapchainExtent_;
        renderPassInfo.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        dispatch_.vkCmdBeginRenderPass(frame.commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        textVertices_.clear();
        recordLayout(frame.commandBuffer, renderFrame);
        flushTextBatch(frame.commandBuffer);
        dispatch_.vkCmdEndRenderPass(frame.commandBuffer);
        dispatch_.vkEndCommandBuffer(frame.commandBuffer);
    }

    void recordLayout(const VkCommandBuffer commandBuffer, const RenderFrame& renderFrame) {
        switch (renderFrame.appFlow) {
            case app::AppFlow::MainMenu:
                recordMainMenu(commandBuffer, renderFrame);
                break;
            case app::AppFlow::MapBrowser:
                recordMapBrowser(commandBuffer, renderFrame);
                break;
            case app::AppFlow::SinglePlayerLobby:
                recordSinglePlayerLobby(commandBuffer, renderFrame);
                break;
            case app::AppFlow::MultiPlayerLobby:
                recordMultiPlayerLobby(commandBuffer, renderFrame);
                break;
            case app::AppFlow::MapEditor:
                recordMapEditor(commandBuffer, renderFrame);
                break;
            case app::AppFlow::Settings:
                recordSettings(commandBuffer, renderFrame);
                break;
            case app::AppFlow::Exit:
                recordExitScreen(commandBuffer);
                break;
        }
    }

    void recordMainMenu(const VkCommandBuffer commandBuffer, const RenderFrame& renderFrame) {
        const auto header = makeAttachment(0.88f, 0.67f, 0.23f);
        const auto side = makeAttachment(0.10f, 0.15f, 0.20f);
        const auto sideAccent = makeAttachment(0.21f, 0.36f, 0.52f);

        clearRect(dispatch_, commandBuffer, header, makeRect(swapchainExtent_, mainMenuHeaderRatio()));
        clearRect(dispatch_, commandBuffer, side, makeRect(swapchainExtent_, mainMenuSidebarRatio()));
        clearRect(dispatch_, commandBuffer, sideAccent, makeRect(swapchainExtent_, mainMenuSidebarAccentRatio()));

        const std::size_t itemCount = renderFrame.mainMenu != nullptr ? renderFrame.mainMenu->items().size() : 5;
        for (std::size_t index = 0; index < itemCount; ++index) {
            const bool selected = index == renderFrame.selectedMenuIndex;
            const auto card = selected ? makeAttachment(0.23f, 0.52f, 0.78f) : makeAttachment(0.14f, 0.23f, 0.31f);
            const auto glow = selected ? makeAttachment(0.93f, 0.83f, 0.49f) : makeAttachment(0.10f, 0.15f, 0.20f);
            clearRect(dispatch_, commandBuffer, card, makeRect(swapchainExtent_, mainMenuCardRatio(index)));
            clearRect(dispatch_, commandBuffer, glow, makeRect(swapchainExtent_, mainMenuCardGlowRatio(index)));
        }

        const std::array<VkClearAttachment, 5> previewPalette{
            makeAttachment(0.12f, 0.39f, 0.21f),
            makeAttachment(0.31f, 0.22f, 0.56f),
            makeAttachment(0.49f, 0.31f, 0.13f),
            makeAttachment(0.33f, 0.33f, 0.36f),
            makeAttachment(0.39f, 0.12f, 0.12f),
        };
        clearRect(dispatch_, commandBuffer, previewPalette[renderFrame.selectedMenuIndex % previewPalette.size()], makeRect(swapchainExtent_, mainMenuPreviewRatio()));

        const float width = static_cast<float>(swapchainExtent_.width);
        const float height = static_cast<float>(swapchainExtent_.height);
        const auto titleColor = makeAttachment(0.96f, 0.95f, 0.90f);
        const auto subColor = makeAttachment(0.82f, 0.86f, 0.88f);
        const auto sideColor = makeAttachment(0.90f, 0.93f, 0.90f);
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, titleColor,
            width * 0.08f, height * 0.13f, widenUtf8(renderFrame.mainMenu != nullptr ? renderFrame.mainMenu->title() : "全民竞技实验场"), 1.32f);
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, subColor,
            width * 0.08f, height * 0.19f, widenUtf8(renderFrame.mainMenu != nullptr ? renderFrame.mainMenu->subtitle() : ""), 0.56f, 0.2f);

        if (renderFrame.mainMenu != nullptr) {
            const auto& items = renderFrame.mainMenu->items();
            for (std::size_t index = 0; index < items.size(); ++index) {
                const auto titleAttachment = index == renderFrame.selectedMenuIndex
                    ? makeAttachment(0.99f, 0.94f, 0.72f)
                    : makeAttachment(0.92f, 0.95f, 0.97f);
                const auto descAttachment = index == renderFrame.selectedMenuIndex
                    ? makeAttachment(0.97f, 0.87f, 0.56f)
                    : makeAttachment(0.70f, 0.78f, 0.84f);
                const float top = 0.24f + static_cast<float>(index) * 0.11f;
                const std::wstring prefix = index == renderFrame.selectedMenuIndex ? L"> " : L"  ";
                recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, titleAttachment,
                    width * 0.075f, height * (top + 0.030f), prefix + widenUtf8(items[index]->label), 0.78f);
                recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, descAttachment,
                    width * 0.078f, height * (top + 0.060f), widenUtf8(items[index]->description), 0.46f, 0.08f);
            }
        }

        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, titleColor,
            width * 0.60f, height * 0.23f, L"当前操作", 0.72f);
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, sideColor,
            width * 0.60f, height * 0.30f,
            L"键盘:\n方向键 / WASD 切换\nEnter / Space 确认\nEsc 退出\n\n鼠标:\n左键点击菜单项\n\n当前原型已支持:\n单机入口\n联机入口\n地图编辑器入口\n设置入口",
            0.44f, 0.05f, 1.08f);
    }

    void recordMapBrowser(const VkCommandBuffer commandBuffer, const RenderFrame& renderFrame) {
        clearRect(dispatch_, commandBuffer, makeAttachment(0.10f, 0.14f, 0.20f), makeRect(swapchainExtent_, 0.06f, 0.08f, 0.66f, 0.90f));
        clearRect(dispatch_, commandBuffer, makeAttachment(0.16f, 0.20f, 0.28f), makeRect(swapchainExtent_, 0.08f, 0.10f, 0.64f, 0.20f));
        clearRect(dispatch_, commandBuffer, makeAttachment(0.11f, 0.12f, 0.16f), makeRect(swapchainExtent_, 0.70f, 0.08f, 0.94f, 0.90f));
        clearRect(dispatch_, commandBuffer, makeAttachment(0.22f, 0.34f, 0.52f), makeRect(swapchainExtent_, 0.72f, 0.12f, 0.92f, 0.24f));

        const float width = static_cast<float>(swapchainExtent_.width);
        const float height = static_cast<float>(swapchainExtent_.height);
        const auto titleColor = makeAttachment(0.97f, 0.96f, 0.92f);
        const auto bodyColor = makeAttachment(0.88f, 0.91f, 0.94f);
        const auto mutedColor = makeAttachment(0.72f, 0.79f, 0.84f);
        const auto accentColor = makeAttachment(0.98f, 0.86f, 0.48f);

        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, titleColor,
            width * 0.10f, height * 0.15f, widenUtf8(renderFrame.mapBrowserTitle), 0.94f, 0.05f);
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, mutedColor,
            width * 0.10f, height * 0.20f, widenUtf8(renderFrame.mapBrowserSubtitle), 0.40f, 0.04f);

        if (renderFrame.mapBrowserItems.empty()) {
            recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, accentColor,
                width * 0.10f, height * 0.34f, L"没有发现可用地图文件。", 0.56f);
        } else {
            for (std::size_t index = 0; index < renderFrame.mapBrowserItems.size(); ++index) {
                const float top = 0.24f + static_cast<float>(index) * 0.10f;
                const bool selected = index == renderFrame.mapBrowserSelectedIndex;
                clearRect(dispatch_, commandBuffer,
                    selected ? makeAttachment(0.22f, 0.52f, 0.78f) : makeAttachment(0.13f, 0.18f, 0.24f),
                    makeRect(swapchainExtent_, 0.08f, top, 0.62f, top + 0.075f));
                clearRect(dispatch_, commandBuffer,
                    selected ? makeAttachment(0.94f, 0.82f, 0.42f) : makeAttachment(0.08f, 0.10f, 0.14f),
                    makeRect(swapchainExtent_, 0.08f, top, 0.09f, top + 0.075f));
                const std::wstring prefix = selected ? L"> " : L"  ";
                recordBitmapText(dispatch_, commandBuffer, swapchainExtent_,
                    selected ? titleColor : bodyColor,
                    width * 0.105f, height * (top + 0.043f),
                    prefix + widenUtf8(renderFrame.mapBrowserItems[index]), 0.54f, 0.04f);
            }
        }

        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, titleColor,
            width * 0.74f, height * 0.19f, L"操作", 0.66f);
        std::ostringstream instructions;
        instructions << "当前状态\n" << renderFrame.mapBrowserStatus << "\n\n";
        instructions << "快捷键\n";
        instructions << "W / S 选择地图\nQ / E 快速切换\nEnter / Space 进入\n左键点击直接进入\nF6 新建地图\nEsc 返回主菜单";
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, bodyColor,
            width * 0.74f, height * 0.30f, widenUtf8(instructions.str()), 0.38f, 0.04f, 1.10f);

        if (!renderFrame.mapBrowserItems.empty() && renderFrame.mapBrowserSelectedIndex < renderFrame.mapBrowserItems.size()) {
            std::ostringstream selected;
            selected << "当前选中\n" << renderFrame.mapBrowserItems[renderFrame.mapBrowserSelectedIndex] << "\n\n";
            selected << "总数 " << renderFrame.mapBrowserItems.size();
            recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, accentColor,
                width * 0.74f, height * 0.63f, widenUtf8(selected.str()), 0.42f, 0.04f, 1.08f);
        }
    }

    void recordSinglePlayerLobby(const VkCommandBuffer commandBuffer, const RenderFrame& renderFrame) {
        recordSinglePlayerGameplay(commandBuffer, renderFrame);
    }

    void recordMultiPlayerLobby(const VkCommandBuffer commandBuffer, const RenderFrame& renderFrame) {
        clearRect(dispatch_, commandBuffer, makeAttachment(0.23f, 0.18f, 0.43f), makeRect(swapchainExtent_, 0.06f, 0.10f, 0.56f, 0.88f));
        clearRect(dispatch_, commandBuffer, makeAttachment(0.12f, 0.10f, 0.22f), makeRect(swapchainExtent_, 0.62f, 0.10f, 0.92f, 0.88f));
        clearRect(dispatch_, commandBuffer, makeAttachment(0.38f, 0.28f, 0.68f), makeRect(swapchainExtent_, 0.08f, 0.14f, 0.54f, 0.24f));
        clearRect(dispatch_, commandBuffer, makeAttachment(0.20f, 0.14f, 0.38f), makeRect(swapchainExtent_, 0.66f, 0.14f, 0.88f, 0.24f));

        const float width = static_cast<float>(swapchainExtent_.width);
        const float height = static_cast<float>(swapchainExtent_.height);
        const auto titleColor = makeAttachment(0.96f, 0.95f, 0.99f);
        const auto bodyColor = makeAttachment(0.84f, 0.82f, 0.95f);
        const auto accentColor = makeAttachment(0.98f, 0.86f, 0.54f);
        const auto mutedColor = makeAttachment(0.72f, 0.70f, 0.86f);

        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, titleColor,
            width * 0.10f, height * 0.18f, L"联机房间", 1.00f);
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, bodyColor,
            width * 0.10f, height * 0.24f, widenUtf8("地图 " + renderFrame.multiplayerMapLabel), 0.42f, 0.04f);

        struct LobbyLine {
            const wchar_t* label;
            std::wstring value;
        };
        std::array<LobbyLine, 4> lines{{
            {L"会话模式", widenUtf8(renderFrame.multiplayerSessionTypeLabel)},
            {L"端口", widenUtf8(renderFrame.multiplayerEndpointLabel)},
            {L"房间人数", widenUtf8(std::to_string(renderFrame.multiplayerMaxPlayers))},
            {L"启动会话", renderFrame.multiplayerSessionActive ? L"已启动" : L"按 Enter 启动"},
        }};

        for (std::size_t index = 0; index < lines.size(); ++index) {
            const float top = 0.30f + static_cast<float>(index) * 0.12f;
            const bool selected = index == renderFrame.multiplayerSelectedIndex;
            clearRect(dispatch_, commandBuffer,
                selected ? makeAttachment(0.28f, 0.52f, 0.78f) : makeAttachment(0.16f, 0.13f, 0.30f),
                makeRect(swapchainExtent_, 0.10f, top, 0.52f, top + 0.09f));
            clearRect(dispatch_, commandBuffer,
                selected ? accentColor : makeAttachment(0.08f, 0.08f, 0.14f),
                makeRect(swapchainExtent_, 0.10f, top, 0.11f, top + 0.09f));
            recordBitmapText(dispatch_, commandBuffer, swapchainExtent_,
                selected ? titleColor : bodyColor,
                width * 0.13f, height * (top + 0.050f), lines[index].label, 0.46f, 0.04f);
            recordBitmapText(dispatch_, commandBuffer, swapchainExtent_,
                selected ? accentColor : mutedColor,
                width * 0.30f, height * (top + 0.050f), lines[index].value, 0.42f, 0.04f);
        }

        std::ostringstream summary;
        summary << "房间状态\n" << renderFrame.multiplayerStatusLabel << "\n\n";
        summary << "当前地图\n" << renderFrame.multiplayerMapLabel << "\n\n";
        summary << "基础网络\n";
        summary << "- UDP transport\n";
        summary << "- Host / Client 切换\n";
        summary << "- 会话快照入口";
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, bodyColor,
            width * 0.68f, height * 0.30f, widenUtf8(summary.str()), 0.40f, 0.04f, 1.08f);
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, mutedColor,
            width * 0.68f, height * 0.74f,
            L"W/S 选择项目  A/D 调整  Enter 启动或切换\nEsc 返回主菜单  重新进入联机模式可重新选图",
            0.34f, 0.03f, 1.08f);
    }

    void recordMapEditor(const VkCommandBuffer commandBuffer, const RenderFrame& renderFrame) {
        clearRect(dispatch_, commandBuffer, makeAttachment(0.40f, 0.24f, 0.10f), makeRect(swapchainExtent_, 0.06f, 0.10f, 0.24f, 0.86f));
        clearRect(dispatch_, commandBuffer, makeAttachment(0.15f, 0.11f, 0.08f), makeRect(swapchainExtent_, 0.26f, 0.10f, 0.94f, 0.86f));
        clearRect(dispatch_, commandBuffer, makeAttachment(0.20f, 0.15f, 0.10f), makeRect(swapchainExtent_, 0.30f, 0.10f, 0.92f, 0.86f));

        const float width = static_cast<float>(swapchainExtent_.width);
        const float height = static_cast<float>(swapchainExtent_.height);
        const float canvasLeft = width * 0.30f;
        const float canvasTop = height * 0.10f;
        const float canvasRight = width * 0.92f;
        const float canvasBottom = height * 0.86f;

        int attackSpawnCount = 0;
        int defendSpawnCount = 0;
        if (renderFrame.editingMap != nullptr && renderFrame.editingMap->width > 0 && renderFrame.editingMap->depth > 0) {
            const float cellWidth = (canvasRight - canvasLeft) / static_cast<float>(renderFrame.editingMap->width);
            const float cellHeight = (canvasBottom - canvasTop) / static_cast<float>(renderFrame.editingMap->depth);

            for (int z = 0; z < renderFrame.editingMap->depth; ++z) {
                for (int x = 0; x < renderFrame.editingMap->width; ++x) {
                    bool hasCover = false;
                    bool hasSiteA = false;
                    bool hasSiteB = false;
                    for (const auto& block : renderFrame.editingMap->blocks) {
                        if (block.cell.x != x || block.cell.z != z) {
                            continue;
                        }
                        hasCover = hasCover || (block.solid && block.cell.y >= 1);
                        hasSiteA = hasSiteA || block.materialId.find("site_a") != std::string::npos;
                        hasSiteB = hasSiteB || block.materialId.find("site_b") != std::string::npos;
                    }

                    auto cellColor = makeAttachment(((x + z) % 2) == 0 ? 0.24f : 0.21f, ((x + z) % 2) == 0 ? 0.26f : 0.23f, 0.22f);
                    if (hasSiteA) {
                        cellColor = makeAttachment(0.56f, 0.22f, 0.16f);
                    } else if (hasSiteB) {
                        cellColor = makeAttachment(0.20f, 0.30f, 0.56f);
                    } else if (hasCover) {
                        cellColor = makeAttachment(0.48f, 0.44f, 0.40f);
                    }

                    const float left = canvasLeft + cellWidth * static_cast<float>(x) + 1.0f;
                    const float top = canvasTop + cellHeight * static_cast<float>(z) + 1.0f;
                    const float right = canvasLeft + cellWidth * static_cast<float>(x + 1) - 1.0f;
                    const float bottom = canvasTop + cellHeight * static_cast<float>(z + 1) - 1.0f;
                    clearRect(dispatch_, commandBuffer, cellColor, makePixelRect(swapchainExtent_, left, top, right, bottom));

                    for (const auto& prop : renderFrame.editingMap->props) {
                        if (static_cast<int>(std::floor(prop.position.x)) != x || static_cast<int>(std::floor(prop.position.z)) != z) {
                            continue;
                        }
                        const bool isBarrel = toLowerAscii(prop.id + " " + prop.modelPath.generic_string()).find("barrel") != std::string::npos;
                        clearRect(dispatch_, commandBuffer,
                            isBarrel ? makeAttachment(0.72f, 0.42f, 0.28f) : makeAttachment(0.74f, 0.58f, 0.28f),
                            makePixelRect(swapchainExtent_,
                                left + cellWidth * 0.22f,
                                top + cellHeight * 0.22f,
                                right - cellWidth * 0.22f,
                                bottom - cellHeight * 0.22f));
                    }

                    for (const auto& spawn : renderFrame.editingMap->spawns) {
                        if (static_cast<int>(std::floor(spawn.position.x)) != x || static_cast<int>(std::floor(spawn.position.z)) != z) {
                            continue;
                        }
                        if (spawn.team == gameplay::Team::Attackers) {
                            ++attackSpawnCount;
                            clearRect(dispatch_, commandBuffer, makeAttachment(0.22f, 0.78f, 0.44f),
                                makePixelRect(swapchainExtent_, left + 4.0f, top + 4.0f, right - 4.0f, top + std::max(8.0f, cellHeight * 0.32f)));
                        } else if (spawn.team == gameplay::Team::Defenders) {
                            ++defendSpawnCount;
                            clearRect(dispatch_, commandBuffer, makeAttachment(0.34f, 0.62f, 0.94f),
                                makePixelRect(swapchainExtent_, left + 4.0f, bottom - std::max(8.0f, cellHeight * 0.32f), right - 4.0f, bottom - 4.0f));
                        }
                    }
                }
            }

            const float cursorLeft = canvasLeft + cellWidth * static_cast<float>(std::clamp(renderFrame.editorCursorX, 0, renderFrame.editingMap->width - 1));
            const float cursorTop = canvasTop + cellHeight * static_cast<float>(std::clamp(renderFrame.editorCursorZ, 0, renderFrame.editingMap->depth - 1));
            const float cursorRight = cursorLeft + cellWidth;
            const float cursorBottom = cursorTop + cellHeight;
            clearRect(dispatch_, commandBuffer, makeAttachment(0.98f, 0.88f, 0.42f),
                makePixelRect(swapchainExtent_, cursorLeft, cursorTop, cursorRight, cursorTop + 3.0f));
            clearRect(dispatch_, commandBuffer, makeAttachment(0.98f, 0.88f, 0.42f),
                makePixelRect(swapchainExtent_, cursorLeft, cursorBottom - 3.0f, cursorRight, cursorBottom));
            clearRect(dispatch_, commandBuffer, makeAttachment(0.98f, 0.88f, 0.42f),
                makePixelRect(swapchainExtent_, cursorLeft, cursorTop, cursorLeft + 3.0f, cursorBottom));
            clearRect(dispatch_, commandBuffer, makeAttachment(0.98f, 0.88f, 0.42f),
                makePixelRect(swapchainExtent_, cursorRight - 3.0f, cursorTop, cursorRight, cursorBottom));
        }

        const auto titleColor = makeAttachment(0.98f, 0.94f, 0.86f);
        const auto bodyColor = makeAttachment(0.92f, 0.84f, 0.70f);
        const auto mutedColor = makeAttachment(0.84f, 0.78f, 0.70f);
        const auto accentColor = makeAttachment(0.98f, 0.86f, 0.48f);

        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, titleColor,
            width * 0.09f, height * 0.18f, L"地图编辑器", 1.06f);
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, bodyColor,
            width * 0.09f, height * 0.27f, widenUtf8(renderFrame.editingMap != nullptr ? renderFrame.editingMap->name : "未加载地图"), 0.54f, 0.05f);
        std::ostringstream mapCatalog;
        mapCatalog << "地图文件\n" << renderFrame.editorMapFileLabel << "\n";
        if (renderFrame.editorMapCount > 0) {
            mapCatalog << "第 " << (renderFrame.editorMapIndex + 1) << " / " << renderFrame.editorMapCount << " 张";
        } else {
            mapCatalog << "暂无地图文件";
        }
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, accentColor,
            width * 0.09f, height * 0.77f, widenUtf8(mapCatalog.str()), 0.36f, 0.04f, 1.06f);

        std::ostringstream toolList;
        toolList << "当前工具\n" << renderFrame.editorToolLabel << "\n\n";
        toolList << "快捷键\n";
        toolList << "1 墙体\n2 箱体\n3 进攻出生点\n4 防守出生点\n5 擦除\nG 循环工具\nQ / E 切换地图\nF6 新建地图\nF5 / V 保存";
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, mutedColor,
            width * 0.09f, height * 0.35f, widenUtf8(toolList.str()), 0.38f, 0.04f, 1.10f);

        std::ostringstream stats;
        if (renderFrame.editingMap != nullptr) {
            stats << "地图尺寸 " << renderFrame.editingMap->width << " x " << renderFrame.editingMap->depth << "\n";
            stats << "方块 " << renderFrame.editingMap->blocks.size() << "\n";
            stats << "道具 " << renderFrame.editingMap->props.size() << "\n";
            stats << "出生点 " << renderFrame.editingMap->spawns.size() << " 进攻 " << attackSpawnCount << " 防守 " << defendSpawnCount << "\n";
        }
        stats << "光标 (" << renderFrame.editorCursorX << ", " << renderFrame.editorCursorZ << ")\n";
        stats << "状态 " << renderFrame.editorStatusLabel;
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, accentColor,
            width * 0.32f, height * 0.18f, widenUtf8(stats.str()), 0.42f, 0.04f, 1.08f);
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, bodyColor,
            width * 0.32f, height * 0.76f,
            L"左键直接选格并应用工具  回车/空格应用当前工具  Delete 清除当前格  Q/E 切图  F6 新建  Esc 返回主菜单",
            0.36f, 0.03f, 1.06f);
    }

    void recordSettings(const VkCommandBuffer commandBuffer, const RenderFrame& renderFrame) {
        clearRect(dispatch_, commandBuffer, makeAttachment(0.20f, 0.22f, 0.24f), makeRect(swapchainExtent_, 0.06f, 0.08f, 0.94f, 0.90f));
        clearRect(dispatch_, commandBuffer, makeAttachment(0.11f, 0.14f, 0.16f), makeRect(swapchainExtent_, 0.10f, 0.16f, 0.58f, 0.82f));
        clearRect(dispatch_, commandBuffer, makeAttachment(0.13f, 0.10f, 0.08f), makeRect(swapchainExtent_, 0.62f, 0.16f, 0.90f, 0.82f));

        const float width = static_cast<float>(swapchainExtent_.width);
        const float height = static_cast<float>(swapchainExtent_.height);
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, makeAttachment(0.95f, 0.95f, 0.93f),
            width * 0.11f, height * 0.17f, L"设置", 1.12f);
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, makeAttachment(0.80f, 0.84f, 0.86f),
            width * 0.11f, height * 0.24f, L"上下选择项目，左右调整数值，Enter 切换，Esc 返回主菜单。", 0.40f, 0.05f);

        struct SettingLine {
            const wchar_t* label;
            std::wstring value;
        };

        std::array<SettingLine, 4> lines{{
            {L"鼠标灵敏度", formatFixedValue(renderFrame.settingsMouseSensitivity)},
            {L"垂直灵敏度", formatFixedValue(renderFrame.settingsMouseVerticalSensitivity)},
            {L"俯仰角限制", formatPitchDegrees(renderFrame.settingsMaxLookPitchDegrees)},
            {L"自动换弹", renderFrame.settingsAutoReload ? L"开启" : L"关闭"},
        }};

        for (std::size_t index = 0; index < lines.size(); ++index) {
            const bool selected = index == renderFrame.selectedSettingsIndex;
            const float top = 0.30f + static_cast<float>(index) * 0.11f;
            clearRect(dispatch_, commandBuffer,
                selected ? makeAttachment(0.77f, 0.55f, 0.18f) : makeAttachment(0.17f, 0.20f, 0.22f),
                makeRect(swapchainExtent_, 0.12f, top, 0.56f, top + 0.08f));
            clearRect(dispatch_, commandBuffer,
                selected ? makeAttachment(0.29f, 0.21f, 0.08f) : makeAttachment(0.09f, 0.10f, 0.12f),
                makeRect(swapchainExtent_, 0.38f, top + 0.014f, 0.54f, top + 0.066f));

            recordBitmapText(dispatch_, commandBuffer, swapchainExtent_,
                selected ? makeAttachment(0.99f, 0.96f, 0.90f) : makeAttachment(0.88f, 0.90f, 0.92f),
                width * 0.14f, height * (top + 0.050f), lines[index].label, 0.48f, 0.04f);
            recordBitmapText(dispatch_, commandBuffer, swapchainExtent_,
                selected ? makeAttachment(0.99f, 0.90f, 0.66f) : makeAttachment(0.77f, 0.82f, 0.86f),
                width * 0.40f, height * (top + 0.050f), lines[index].value, 0.44f, 0.04f);
        }

        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, makeAttachment(0.95f, 0.90f, 0.82f),
            width * 0.65f, height * 0.27f, L"当前生效", 0.56f);
        recordBitmapText(dispatch_, commandBuffer, swapchainExtent_, makeAttachment(0.84f, 0.82f, 0.76f),
            width * 0.65f, height * 0.36f,
            L"这些项目会实时写入 settings.cfg。\n\n建议：\n- 鼠标灵敏度 0.8 到 1.2\n- 垂直灵敏度 1.2 到 1.8\n- 俯仰角限制 74 到 82 度",
            0.40f, 0.05f, 1.10f);
    }

    void recordExitScreen(const VkCommandBuffer commandBuffer) {
        clearRect(dispatch_, commandBuffer, makeAttachment(0.36f, 0.10f, 0.10f), makeRect(swapchainExtent_, 0.22f, 0.20f, 0.78f, 0.74f));
        clearRect(dispatch_, commandBuffer, makeAttachment(0.68f, 0.20f, 0.18f), makeRect(swapchainExtent_, 0.28f, 0.30f, 0.72f, 0.42f));
        clearRect(dispatch_, commandBuffer, makeAttachment(0.20f, 0.06f, 0.06f), makeRect(swapchainExtent_, 0.30f, 0.52f, 0.48f, 0.64f));
        clearRect(dispatch_, commandBuffer, makeAttachment(0.20f, 0.06f, 0.06f), makeRect(swapchainExtent_, 0.52f, 0.52f, 0.70f, 0.64f));

        const float width = static_cast<float>(swapchainExtent_.width);
        const float height = static_cast<float>(swapchainExtent_.height);
        recordCenteredBitmapText(dispatch_, commandBuffer, swapchainExtent_, makeAttachment(0.99f, 0.95f, 0.92f),
            width * 0.50f, height * 0.40f, L"退出游戏", 1.00f);
        recordCenteredBitmapText(dispatch_, commandBuffer, swapchainExtent_, makeAttachment(0.94f, 0.84f, 0.80f),
            width * 0.50f, height * 0.54f, L"正在退出。", 0.68f);
    }

    VulkanDispatch dispatch_;
    HWND hwnd_ = nullptr;
    SDL_Window* sdlWindow_ = nullptr;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VmaAllocator allocator_ = VK_NULL_HANDLE;
    std::uint32_t queueFamilyIndex_ = 0;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkSurfaceFormatKHR surfaceFormat_{};
    VkExtent2D swapchainExtent_{};
    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    VkImage depthImage_ = VK_NULL_HANDLE;
    VmaAllocation depthAllocation_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout textureDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool textureDescriptorPool_ = VK_NULL_HANDLE;
    VkSampler textureSampler_ = VK_NULL_HANDLE;
    VkPipelineLayout meshPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline meshPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout textPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline textPipeline_ = VK_NULL_HANDLE;
    VkSemaphore imageAvailableSemaphore_ = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSemaphore_ = VK_NULL_HANDLE;
    VkFence renderFence_ = VK_NULL_HANDLE;
    std::vector<FrameResources> frames_;
    bool menuPrinted_ = false;
    bool worldPrinted_ = false;
    bool editorPrinted_ = false;
    bool bitmapUiLogged_ = false;
    GpuBuffer staticWorldMesh_{};
    TextureResource defaultTexture_{};
    TextureResource uiFontAtlasTexture_{};
    StreamingBuffer textVertexBuffer_{};
    std::string staticWorldMeshKey_;
    std::vector<CachedGpuMesh> gpuMeshCache_{};
    std::vector<CachedTexture> textureCache_{};
    std::vector<CachedPreviewMesh> previewMeshCache_{};
    std::vector<UiFontGlyphPlacement> uiFontGlyphPlacements_{};
    std::vector<TextVertex> textVertices_{};
};

#else

class VulkanRenderer final : public IRenderer {
public:
    bool initialize(platform::IWindow&) override {
        if (!dispatch_.loadLoader()) {
            return false;
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "my-cs-go";
        appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
        appInfo.pEngineName = "my-cs-go engine";
        appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo instanceInfo{};
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceInfo.pApplicationInfo = &appInfo;

        if (dispatch_.vkCreateInstance(&instanceInfo, nullptr, &instance_) != VK_SUCCESS) {
            return false;
        }

        dispatch_.loadInstanceFunctions(instance_);
        return true;
    }

    void render(const RenderFrame& frame) override {
        if (frame.mainMenu != nullptr && !menuPrinted_) {
            spdlog::info("[MainMenu] {}", frame.mainMenu->title());
            menuPrinted_ = true;
        }
    }

    void shutdown() override {
        if (instance_ != VK_NULL_HANDLE && dispatch_.vkDestroyInstance != nullptr) {
            dispatch_.vkDestroyInstance(instance_, nullptr);
            instance_ = VK_NULL_HANDLE;
        }
        dispatch_.unloadLoader();
    }

private:
    VulkanDispatch dispatch_;
    VkInstance instance_ = VK_NULL_HANDLE;
    bool menuPrinted_ = false;
};

#endif

}  // namespace

std::unique_ptr<IRenderer> createRenderer() {
    return std::make_unique<VulkanRenderer>();
}

}  // namespace mycsg::renderer
