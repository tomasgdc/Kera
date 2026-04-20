#pragma once

#include <memory>
#include <vulkan/vulkan.h>

namespace kera {

enum class ImageUsage {
    Texture,
    DepthStencil,
    RenderTarget,
    Storage
};

enum class ImageFormat {
    RGBA8,
    RGB8,
    Depth24Stencil8,
    Depth32
};

class Device;

class Image {
public:
    Image();
    ~Image();

    // Delete copy operations
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    // Move operations
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;

    bool initialize(const Device& device, uint32_t width, uint32_t height, ImageFormat format, ImageUsage usage);
    void shutdown();

    VkImage getVulkanImage() const { return image_; }
    VkImageView getImageView() const { return image_view_; }
    VkSampler getSampler() const { return sampler_; }
    uint32_t getWidth() const { return width_; }
    uint32_t getHeight() const { return height_; }

    bool isValid() const { return image_ != VK_NULL_HANDLE; }

private:
    VkImage image_;
    VkDeviceMemory memory_;
    VkImageView image_view_;
    VkSampler sampler_;
    uint32_t width_;
    uint32_t height_;
    ImageFormat format_;
};

} // namespace kera