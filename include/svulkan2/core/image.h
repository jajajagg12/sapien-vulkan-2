/**
 * @file image.h
 * @brief GPU resources for a Vulkan image
 * @author Fanbo Xiang
 * Contact: fxiang@eng.ucsd.edu
 */

#pragma once
#include "svulkan2/common/log.h"
#include "svulkan2/common/vk.h"

namespace svulkan2 {
namespace core {

inline size_t findSizeFromFormat(vk::Format format) {
  if (format == vk::Format::eR8Unorm) {
    return 1;
  }
  if (format == vk::Format::eR8G8B8A8Unorm) {
    return 4;
  }
  if (format == vk::Format::eR32G32B32A32Uint) {
    return 16;
  }
  if (format == vk::Format::eR32G32B32A32Sfloat) {
    return 16;
  }
  if (format == vk::Format::eD32Sfloat) {
    return 4;
  }
  if (format == vk::Format::eD24UnormS8Uint) {
    return 4;
  }
  throw std::runtime_error("unknown image format");
}

class Image {
private:
  class Context *mContext;
  vk::Extent3D mExtent;
  vk::Format mFormat;
  vk::ImageUsageFlags mUsageFlags;
  vk::SampleCountFlagBits mSampleCount;
  uint32_t mMipLevels;
  uint32_t mArrayLayers;
  vk::ImageTiling mTiling;
  bool mHostVisible;
  bool mHostCoherent;

  vk::Image mImage;
  VmaAllocation mAllocation;

  bool mMapped{};
  void *mMappedData;

  void generateMipmaps(vk::CommandBuffer cb);
public:
  Image(Context &context, vk::Extent3D extent, vk::Format format,
        vk::ImageUsageFlags usageFlags, VmaMemoryUsage memoryUsage,
        vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1,
        uint32_t mipLevels = 1, uint32_t arrayLayers = 1,
        vk::ImageTiling tiling = vk::ImageTiling::eOptimal,
        vk::ImageCreateFlags flags = {});

  Image(const Image &) = delete;
  Image(const Image &&) = delete;
  Image &operator=(const Image &) = delete;
  Image &operator=(Image &&) = delete;

  ~Image();

  vk::Image getVulkanImage() const { return mImage; }

  void *map();
  void unmap();

  void upload(void const *data, size_t size);
  template <typename DataType> void upload(std::vector<DataType> const &data) {
    upload(data.data(), data.size() * sizeof(DataType));
  }

  void download(void *data, size_t size, vk::Offset3D offset,
                vk::Extent3D extent);
  void download(void *data, size_t size);
  void downloadPixel(void *data, size_t pixelSize, vk::Offset3D offset);

  template <typename DataType>
  std::vector<DataType> download(vk::Offset3D offset, vk::Extent3D extent) {
    static_assert(sizeof(DataType) == 1 ||
                  sizeof(DataType) == 4); // only support char, int or float
    size_t size = findSizeFromFormat(mFormat) * extent.width * extent.height *
                  extent.depth;
    std::vector<DataType> data(size / sizeof(DataType));
    download(data.data(), size, offset, extent);
    return data;
  }
  template <typename DataType> std::vector<DataType> download() {
    return download<DataType>({0, 0, 0}, mExtent);
  }
  template <typename DataType>
  std::vector<DataType> downloadPixel(vk::Offset3D offset) {
    return download<DataType>(offset, {1, 1, 1});
  }

  void transitionLayout(vk::CommandBuffer commandBuffer,
                        vk::ImageLayout oldImageLayout,
                        vk::ImageLayout newImageLayout,
                        vk::AccessFlags sourceAccessMask,
                        vk::AccessFlags destAccessMask,
                        vk::PipelineStageFlags sourceStage,
                        vk::PipelineStageFlags destStage);

  inline vk::Extent3D getExtent() const { return mExtent; }
  inline vk::Format const &getFormat() const { return mFormat; }
  inline uint32_t getMipLevels() const { return mMipLevels; }
  inline uint32_t getArrayLayers() const { return mArrayLayers; }
  inline vk::ImageUsageFlags getUsage() const { return mUsageFlags; }
  inline vk::SampleCountFlagBits getSampleCount() const { return mSampleCount; }
  inline vk::ImageTiling getTiling() const { return mTiling; }
};

} // namespace core
} // namespace svulkan2