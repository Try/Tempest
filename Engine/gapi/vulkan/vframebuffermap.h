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
      AccessOp      load  = AccessOp::Discard;
      AccessOp      store = AccessOp::Discard;
      VkFormat      frm   = VK_FORMAT_UNDEFINED;
      };

    struct RenderPass {
      VkRenderPass   pass = VK_NULL_HANDLE;

      Desc           desc[MaxFramebufferAttachments];
      uint8_t        descSize = 0;
      bool           isCompatible(const RenderPass& other) const;
      bool           isSame(const Desc* d, size_t cnt) const;
      };

    struct Fbo {
      VkFramebuffer  fbo  = VK_NULL_HANDLE;
      std::shared_ptr<RenderPass> pass;

      VkImageView    view[MaxFramebufferAttachments] = {};
      uint8_t        descSize = 0;
      bool           isSame(const Desc* d, const VkImageView* view, size_t cnt) const;
      bool           hasImg(VkImageView v) const;
      };

    std::shared_ptr<Fbo> find(const AttachmentDesc* desc, size_t descSize,
                              const TextureFormat* frm,
                              AbstractGraphicsApi::Texture** cl,
                              AbstractGraphicsApi::Swapchain** sw, const uint32_t* imageId,
                              uint32_t w, uint32_t h);
    void                 notifyDestroy(VkImageView img);

  private:
    std::shared_ptr<RenderPass> findRenderpass(const Desc* desc, size_t cnt);

    Fbo                mkFbo(const Desc* desc, const VkImageView* view, size_t attCount, uint32_t w, uint32_t h);
    VkRenderPass       mkRenderPass (const Desc* desc, size_t cnt);
    VkFramebuffer      mkFramebuffer(const VkImageView* view, size_t cnt, uint32_t w, uint32_t h, VkRenderPass rp);

    VDevice&           dev;

    std::mutex         syncFbo;
    std::vector<std::shared_ptr<Fbo>>        val;

    std::mutex         syncRp;
    std::vector<std::shared_ptr<RenderPass>> rp;
  };

}
}
