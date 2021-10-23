#pragma once

#include "gapi/abstractgraphicsapi.h"
#include "vulkan_sdk.h"

#include <mutex>

namespace Tempest {

class AttachmentDesc;

namespace Detail {

class VDevice;

class VFramebufferMap {
  public:
    VFramebufferMap(VDevice& dev);
    ~VFramebufferMap();

    struct Desc {
      Tempest::Vec4 clear = {};
      VkImageView   view  = VK_NULL_HANDLE;
      AccessOp      load  = AccessOp::Discard;
      AccessOp      store = AccessOp::Discard;
      VkFormat      frm   = VK_FORMAT_UNDEFINED;
      };

    struct Fbo {
      VkFramebuffer  fbo  = VK_NULL_HANDLE;
      VkRenderPass   pass = VK_NULL_HANDLE;
      VkClearValue   clr[MaxFramebufferAttachments] = {};

      Desc           desc[MaxFramebufferAttachments];
      uint8_t        descSize = 0;
      bool           is(const Desc* d, size_t cnt) const;
      bool           isCompatible(const Fbo& other) const;
      };

    std::shared_ptr<Fbo> find(const AttachmentDesc* desc, size_t descSize,
                              const TextureFormat* frm,
                              AbstractGraphicsApi::Texture** cl,
                              AbstractGraphicsApi::Swapchain** sw, const uint32_t* imageId,
                              uint32_t w, uint32_t h);

  private:
    Fbo                mkFbo(const Desc* desc, size_t attCount, uint32_t w, uint32_t h);
    VkRenderPass       mkRenderPass(const Desc* desc, size_t cnt);
    VkFramebuffer      mkFramebuffer(const Desc* desc, size_t cnt, uint32_t w, uint32_t h, VkRenderPass rp);

    std::mutex         sync;
    VDevice&           dev;
    std::vector<std::shared_ptr<Fbo>>   val;
  };

}
}
