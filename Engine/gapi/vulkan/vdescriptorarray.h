#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "utility/smallarray.h"
#include "gapi/resourcestate.h"
#include "vpipelinelay.h"

#include "vulkan_sdk.h"

namespace Tempest {
namespace Detail {

class VPipelineLay;

class VDescriptorArray2 : public AbstractGraphicsApi::DescArray {
  public:
    VDescriptorArray2(VDevice& dev, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel, const Sampler& smp);
    VDescriptorArray2(VDevice& dev, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel);
    VDescriptorArray2(VDevice& dev, AbstractGraphicsApi::Buffer**  buf, size_t cnt);
    ~VDescriptorArray2();

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
