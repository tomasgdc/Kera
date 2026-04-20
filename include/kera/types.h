#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <optional>

namespace kera {

// Basic types
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using f32 = float;
using f64 = double;

// Vulkan forward declarations
struct VkInstance_T;
struct VkPhysicalDevice_T;
struct VkDevice_T;
struct VkSurfaceKHR_T;
struct VkSwapchainKHR_T;
struct VkImage_T;
struct VkImageView_T;
struct VkBuffer_T;
struct VkDeviceMemory_T;
struct VkShaderModule_T;
struct VkPipeline_T;
struct VkPipelineLayout_T;
struct VkRenderPass_T;
struct VkFramebuffer_T;
struct VkCommandPool_T;
struct VkCommandBuffer_T;
struct VkSemaphore_T;
struct VkFence_T;

using VkInstance = VkInstance_T*;
using VkPhysicalDevice = VkPhysicalDevice_T*;
using VkDevice = VkDevice_T*;
using VkSurfaceKHR = VkSurfaceKHR_T*;
using VkSwapchainKHR = VkSwapchainKHR_T*;
using VkImage = VkImage_T*;
using VkImageView = VkImageView_T*;
using VkBuffer = VkBuffer_T*;
using VkDeviceMemory = VkDeviceMemory_T*;
using VkShaderModule = VkShaderModule_T*;
using VkPipeline = VkPipeline_T*;
using VkPipelineLayout = VkPipelineLayout_T*;
using VkRenderPass = VkRenderPass_T*;
using VkFramebuffer = VkFramebuffer_T*;
using VkCommandPool = VkCommandPool_T*;
using VkCommandBuffer = VkCommandBuffer_T*;
using VkSemaphore = VkSemaphore_T*;
using VkFence = VkFence_T*;

// SDL forward declarations
struct SDL_Window;

// Common data structures
struct Extent2D {
    u32 width = 0;
    u32 height = 0;

    bool operator==(const Extent2D& other) const {
        return width == other.width && height == other.height;
    }

    bool operator!=(const Extent2D& other) const {
        return !(*this == other);
    }
};

struct Offset2D {
    i32 x = 0;
    i32 y = 0;

    bool operator==(const Offset2D& other) const {
        return x == other.x && y == other.y;
    }

    bool operator!=(const Offset2D& other) const {
        return !(*this == other);
    }
};

struct Rect2D {
    Offset2D offset;
    Extent2D extent;

    bool operator==(const Rect2D& other) const {
        return offset == other.offset && extent == other.extent;
    }

    bool operator!=(const Rect2D& other) const {
        return !(*this == other);
    }
};

// Version structure
struct Version {
    u32 major = 0;
    u32 minor = 0;
    u32 patch = 0;

    bool operator==(const Version& other) const {
        return major == other.major && minor == other.minor && patch == other.patch;
    }

    bool operator!=(const Version& other) const {
        return !(*this == other);
    }

    bool operator<(const Version& other) const {
        if (major != other.major) return major < other.major;
        if (minor != other.minor) return minor < other.minor;
        return patch < other.patch;
    }

    std::string toString() const {
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }
};

} // namespace kera