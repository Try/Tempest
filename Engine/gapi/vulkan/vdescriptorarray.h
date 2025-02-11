#pragma once

#include <Tempest/AbstractGraphicsApi>

#include "vulkan_sdk.h"

namespace Tempest {
namespace Detail {

class VDevice;

class VDescriptorArray : public AbstractGraphicsApi::DescArray {
  public:
    VDescriptorArray(VDevice& dev, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel, const Sampler& smp);
    VDescriptorArray(VDevice& dev, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel);
    VDescriptorArray(VDevice& dev, AbstractGraphicsApi::Buffer**  buf, size_t cnt);
    ~VDescriptorArray();

    size_t size() const;
    auto   set() const -> VkDescriptorSet { return dset; }

  private:
    void alloc(VkDescriptorSetLayout lay, VDevice& dev, size_t cnt);
    void populate(Tempest::Detail::VDevice &dev, AbstractGraphicsApi::Texture **tex, size_t cnt, uint32_t mipLevel, const Sampler *smp);
    void populate(Tempest::Detail::VDevice &dev, AbstractGraphicsApi::Buffer  **buf, size_t cnt);

    VDevice&         dev;
    size_t           cnt  = 0;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet  dset = VK_NULL_HANDLE;
  };

}}
