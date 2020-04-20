#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Color>
#include <vector>
#include "vulkan_sdk.h"

#include "utility/spinlock.h"

namespace Tempest {

class Attachment;

namespace Detail {

class VDevice;
class VSwapchain;
class VFramebufferLayout;

class VRenderPass : public AbstractGraphicsApi::Pass {
  public:
    struct Att {
      VkFormat frm=VK_FORMAT_UNDEFINED;
      };

    VRenderPass()=default;
    VRenderPass(VDevice& device, const FboMode** attach, uint8_t attCount);
    VRenderPass(VRenderPass&& other);
    ~VRenderPass();

    void operator=(VRenderPass&& other);

    struct Impl {
      Impl(VFramebufferLayout &lay,VkRenderPass impl,std::unique_ptr<VkClearValue[]>&& clr)
        :lay(&lay),impl(impl),clear(std::move(clr)){}
      DSharedPtr<VFramebufferLayout*> lay;
      VkRenderPass                    impl=VK_NULL_HANDLE;
      std::unique_ptr<VkClearValue[]> clear;

      bool                            isCompatible(VFramebufferLayout &lay) const;
      };

    Impl&                           instance(VFramebufferLayout &lay);

    static VkRenderPass             createLayoutInstance(VkDevice &device, VSwapchain& sw, const VkFormat    *attach, uint8_t attCount);

    uint8_t                         attCount=0;

  private:
    VkDevice                        device=nullptr;
    std::vector<Impl>               impl;
    std::unique_ptr<FboMode[]>      input;
    SpinLock                        sync;

    static VkRenderPass             createInstance(VkDevice &device, VSwapchain* sw,
                                                   const FboMode* attach, const VkFormat *frm,
                                                   uint8_t attCount);

    static void                     setupAttach(VkAttachmentDescription& desc,
                                                VkAttachmentReference &r,
                                                const FboMode& x,
                                                VkSubpassDescription &subpass);
  };

}}
