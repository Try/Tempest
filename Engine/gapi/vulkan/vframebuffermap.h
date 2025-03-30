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

    struct RenderPassDesc {
      RenderPassDesc() = default;
      RenderPassDesc(const VkRenderingInfo& info, const VkPipelineRenderingCreateInfoKHR& fbo);

      bool operator == (const RenderPassDesc& other) const;

      VkFormat            attachmentFormats[MaxFramebufferAttachments] = {};
      VkAttachmentLoadOp  loadOp[MaxFramebufferAttachments] = {};
      VkAttachmentStoreOp storeOp[MaxFramebufferAttachments] = {};
      uint32_t            numAttachments = 0;
      };

    struct RenderPass {
      VkRenderPass    pass = VK_NULL_HANDLE;
      RenderPassDesc  desc;

      bool            isCompatible(const RenderPass& other) const;
      bool            isSame(const RenderPassDesc& other) const;
      };

    struct Fbo {
      VkFramebuffer  fbo  = VK_NULL_HANDLE;
      std::shared_ptr<RenderPass> pass;

      VkImageView    view[MaxFramebufferAttachments] = {};
      uint8_t        descSize = 0;

      bool           isSame(const VkImageView* attach, size_t attCount, const RenderPassDesc& desc) const;
      bool           hasImg(VkImageView v) const;
      };

    std::shared_ptr<Fbo> find(const VkRenderingInfo* info, const VkPipelineRenderingCreateInfoKHR* fbo);
    void                 notifyDestroy(VkImageView img);

  private:
    std::shared_ptr<RenderPass> findRenderpass(const VkRenderingInfo* info, const VkPipelineRenderingCreateInfoKHR* fbo);
    Fbo                mkFbo(const VkRenderingInfo* info, const VkPipelineRenderingCreateInfoKHR* fbo, const VkImageView* attach, size_t attCount);
    VkRenderPass       mkRenderPass(const VkRenderingInfo* info, const VkPipelineRenderingCreateInfoKHR* fbo);
    VkFramebuffer      mkFramebuffer(const VkImageView* view, size_t cnt, VkExtent2D size, VkRenderPass rp);

    VDevice&           dev;

    std::mutex         syncFbo;
    std::vector<std::shared_ptr<Fbo>>        val;

    std::mutex         syncRp;
    std::vector<std::shared_ptr<RenderPass>> rp;
  };

}
}
