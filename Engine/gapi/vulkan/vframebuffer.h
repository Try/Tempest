#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vector>
#include "vulkan_sdk.h"

namespace Tempest {
namespace Detail {

class VDevice;
class VRenderPass;
class VSwapchain;
class VTexture;
class VFramebufferLayout;

class VFramebuffer : public AbstractGraphicsApi::Fbo {
  public:
    struct Attach final : AbstractGraphicsApi::Attach {
      Attach()=default;
      Attach(VTexture* tex, size_t id):tex(tex),id(id){}
      Attach(VSwapchain* sw,size_t id):sw(sw),  id(id){}

      TextureLayout defaultLayout() override;
      TextureLayout renderLayout()  override;
      void*         nativeHandle()  override;

      VTexture*   tex=nullptr;
      VSwapchain* sw =nullptr;
      size_t      id =0;
      };

    VFramebuffer(VDevice &device, VFramebufferLayout& lay,
                 uint32_t w, uint32_t h, uint32_t outCnt, VSwapchain** swapchain, VTexture** color, const uint32_t* imgId, VTexture* zbuf);
    ~VFramebuffer();

    VkFramebuffer                           impl=VK_NULL_HANDLE;
    Detail::DSharedPtr<VFramebufferLayout*> rp;
    std::vector<Attach>                     attach;

  private:
    VkDevice                                device=nullptr;

    VkFramebuffer allocFbo(uint32_t w, uint32_t h,const VkImageView* attach,size_t cnt);
  };

}}
