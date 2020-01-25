#pragma once

#include "abstractgraphicsapi.h"

#include <memory>
#include <cstdint>

namespace Tempest {

class DirectX12Api : public AbstractGraphicsApi {
  public:
    explicit DirectX12Api(ApiFlags f=ApiFlags::NoFlags);
    virtual ~DirectX12Api();

  Device*        createDevice() override;
  void           destroy(Device* d) override;

  Swapchain*     createSwapchain(SystemApi::Window* w,AbstractGraphicsApi::Device *d) override;

  PPass          createPass(Device *d, const FboMode** att, size_t acount) override;

  PFbo           createFbo(Device *d, FboLayout* lay, Swapchain *s, uint32_t imageId) override;
  PFbo           createFbo(Device *d, FboLayout* lay, Swapchain *s, uint32_t imageId, Texture* zbuf) override;
  PFbo           createFbo(Device *d, FboLayout* lay, uint32_t w, uint32_t h, Texture* cl, Texture* zbuf) override;
  PFbo           createFbo(Device *d, FboLayout *lay, uint32_t w, uint32_t h, Texture* cl) override;

  PFboLayout     createFboLayout(Device *d, uint32_t w, uint32_t h, Swapchain *s,
                                 TextureFormat *att, size_t attCount) override;

  PPipeline      createPipeline(Device* d,
                                const RenderState &st,
                                const Tempest::Decl::ComponentType *decl, size_t declSize,
                                size_t stride,
                                Topology tp,
                                const UniformsLayout& ulay,
                                std::shared_ptr<UniformsLay> &ulayImpl,
                                const std::initializer_list<Shader*>& shaders) override;

  PShader        createShader(AbstractGraphicsApi::Device *d, const char* source, size_t src_size) override;

  Fence*         createFence(Device *d) override;

  Semaphore*     createSemaphore(Device *d) override;

  PBuffer        createBuffer(Device* d, const void *mem, size_t count,size_t sz,size_t alignedSz, MemUsage usage, BufferFlags flg) override;

  Desc*          createDescriptors(Device* d, const UniformsLayout &lay, std::shared_ptr<UniformsLay> &layP) override;

  std::shared_ptr<AbstractGraphicsApi::UniformsLay>
                 createUboLayout(Device *d,const UniformsLayout&) override;

  PTexture       createTexture(Device* d,const Pixmap& p,TextureFormat frm,uint32_t mips) override;
  PTexture       createTexture(Device* d,const uint32_t w,const uint32_t h,uint32_t mips, TextureFormat frm) override;

  void           readPixels(AbstractGraphicsApi::Device *d, Pixmap &out, const PTexture t,
                            TextureLayout lay, TextureFormat frm,
                            const uint32_t w, const uint32_t h, uint32_t mip) override;

  CmdPool*       createCommandPool(Device* d) override;
  CommandBuffer* createCommandBuffer(Device* d, CmdPool* pool, FboLayout* fbo, CmdType type) override;

  void           present  (Device *d,Swapchain* sw,uint32_t imageId, const Semaphore *wait) override;

  void           submit   (Device *d,CommandBuffer* cmd,Semaphore* wait,Semaphore* onReady,Fence* onReadyCpu) override;
  void           submit   (Device *d,
                           CommandBuffer** cmd, size_t count,
                           Semaphore** wait, size_t waitCnt,
                           Semaphore** done, size_t doneCnt,
                           Fence *doneCpu) override;

  void           getCaps  (Device *d,Caps& caps) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
  };

}

