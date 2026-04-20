#include "kera/renderer/image.h"
#include "kera/renderer/device.h"
#include <vulkan/vulkan.h>
#include <iostream>

namespace kera {

namespace {

VkFormat imageFormatToVkFormat(ImageFormat format) {
    switch (format) {
        case ImageFormat::RGBA8: return VK_FORMAT_R8G8B8A8_SRGB;
        case ImageFormat::RGB8: return VK_FORMAT_R8G8B8_SRGB;
        case ImageFormat::Depth24Stencil8: return VK_FORMAT_D24_UNORM_S8_UINT;
        case ImageFormat::Depth32: return VK_FORMAT_D32_SFLOAT;
        default: return VK_FORMAT_R8G8B8A8_SRGB;
    }
}

} // anonymous namespace

Image::Image()
    : image_(VK_NULL_HANDLE)
    , memory_(VK_NULL_HANDLE)
    , image_view_(VK_NULL_HANDLE)
    , sampler_(VK_NULL_HANDLE)
    , width_(0)
    , height_(0)
    , format_(ImageFormat::RGBA8) {
}

Image::~Image() {
    shutdown();
}

Image::Image(Image&& other) noexcept
    : image_(other.image_)
    , memory_(other.memory_)
    , image_view_(other.image_view_)
    , sampler_(other.sampler_)
    , width_(other.width_)
    , height_(other.height_)
    , format_(other.format_) {
    other.image_ = VK_NULL_HANDLE;
    other.memory_ = VK_NULL_HANDLE;
    other.image_view_ = VK_NULL_HANDLE;
    other.sampler_ = VK_NULL_HANDLE;
    other.width_ = 0;
    other.height_ = 0;
}

Image& Image::operator=(Image&& other) noexcept {
    if (this != &other) {
        shutdown();
        image_ = other.image_;
        memory_ = other.memory_;
        image_view_ = other.image_view_;
        sampler_ = other.sampler_;
        width_ = other.width_;
        height_ = other.height_;
        format_ = other.format_;

        other.image_ = VK_NULL_HANDLE;
        other.memory_ = VK_NULL_HANDLE;
        other.image_view_ = VK_NULL_HANDLE;
        other.sampler_ = VK_NULL_HANDLE;
        other.width_ = 0;
        other.height_ = 0;
    }
    return *this;
}

bool Image::initialize(const Device& device, uint32_t width, uint32_t height, ImageFormat format, ImageUsage usage) {
    if (image_) {
        shutdown();
    }

    VkDevice vkDevice = device.getVulkanDevice();
    width_ = width;
    height_ = height;
    format_ = format;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = imageFormatToVkFormat(format);
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

    switch (usage) {
        case ImageUsage::Texture:
            imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            break;
        case ImageUsage::DepthStencil:
            imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            break;
        case ImageUsage::RenderTarget:
            imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            break;
        case ImageUsage::Storage:
            imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT;
            break;
    }

    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult result = vkCreateImage(vkDevice, &imageInfo, nullptr, &image_);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create image: " << result << std::endl;
        return false;
    }

    // TODO: Allocate and bind memory
    // TODO: Create image view
    // TODO: Create sampler if needed

    std::cout << "Image created successfully (" << width << "x" << height << ")" << std::endl;
    return true;
}

void Image::shutdown() {
    if (sampler_) {
        // TODO: Need device reference
        // vkDestroySampler(device, sampler_, nullptr);
        sampler_ = VK_NULL_HANDLE;
    }

    if (image_view_) {
        // TODO: Need device reference
        // vkDestroyImageView(device, image_view_, nullptr);
        image_view_ = VK_NULL_HANDLE;
    }

    if (image_) {
        // TODO: Need device reference
        // vkDestroyImage(device, image_, nullptr);
        image_ = VK_NULL_HANDLE;
    }

    if (memory_) {
        // TODO: Need device reference
        // vkFreeMemory(device, memory_, nullptr);
        memory_ = VK_NULL_HANDLE;
    }

    width_ = 0;
    height_ = 0;
}

} // namespace kera