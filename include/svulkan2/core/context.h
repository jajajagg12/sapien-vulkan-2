#pragma once
#include "allocator.h"
#include "svulkan2/common/vk.h"
#include <future>

namespace svulkan2 {
namespace core {

class Context {
  uint32_t mApiVersion;
  bool mPresent;

  vk::UniqueInstance mInstance;

  vk::PhysicalDevice mPhysicalDevice;
  uint32_t mQueueFamilyIndex;

  vk::UniqueDevice mDevice;
  std::unique_ptr<class Allocator> mAllocator;

  vk::UniqueCommandPool mCommandPool;
  vk::UniqueDescriptorPool mDescriptorPool;

  uint32_t mMaxNumObjects;
  uint32_t mMaxNumTextures;

public:
  Context(uint32_t apiVersion = VK_API_VERSION_1_1, bool present = true,
          uint32_t maxNumObjects = 5000, uint32_t maxNumTextures = 1000);
  ~Context();

  vk::Queue getQueue() const;
  inline class Allocator &getAllocator() { return *mAllocator; }

  vk::UniqueCommandBuffer createCommandBuffer(
      vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary) const;
  void submitCommandBufferAndWait(vk::CommandBuffer commandBuffer) const;
  vk::UniqueFence
  submitCommandBufferForFence(vk::CommandBuffer commandBuffer) const;
  std::future<void> submitCommandBuffer(vk::CommandBuffer commandBuffer) const;

  inline uint32_t getGraphicsQueueFamilyIndex() const {
    return mQueueFamilyIndex;
  }
  inline vk::Instance getInstance() const { return mInstance.get(); }
  inline vk::Device getDevice() const { return mDevice.get(); }
  inline vk::PhysicalDevice getPhysicalDevice() const {
    return mPhysicalDevice;
  }
  inline vk::DescriptorPool getDescriptorPool() const {
    return mDescriptorPool.get();
  }

private:
  void createInstance();
  void pickSuitableGpuAndQueueFamilyIndex();
  void createDevice();
  void createMemoryAllocator();

  void createCommandPool();
  void createDescriptorPool();
};

} // namespace core
} // namespace svulkan2