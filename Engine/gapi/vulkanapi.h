#pragma once

#include "abstractgraphicsapi.h"

#include <memory>
#include <cstdint>

namespace Tempest {

class VulkanApi : public AbstractGraphicsApi {
  public:
    explicit VulkanApi(ApiFlags f=ApiFlags::NoFlags);
    virtual ~VulkanApi();

    Device*      createDevice(SystemApi::Window* w) override;
    void         destroy(Device* d) override;
    void         waitIdle(Device* d) override;

    Swapchain*   createSwapchain(SystemApi::Window* w,AbstractGraphicsApi::Device *d) override;
    void         destroy(Swapchain* d) override;

    Pass*        createPass(Device *d,
                            Swapchain* sw,
                            TextureFormat zformat,
                            FboMode fcolor, const Color *clear,
                            FboMode fzbuf, const float *zclear) override;

    Pass*        createPass(Device *d,
                            TextureFormat clFormat,
                            TextureFormat zformat,
                            FboMode fcolor, const Color *clear,
                            FboMode fzbuf, const float *zclear) override;
    void         destroy   (Pass* pass) override;

    Fbo*         createFbo(Device *d, Swapchain *s, Pass* pass, uint32_t imageId) override;
    Fbo*         createFbo(Device *d, Swapchain *s, Pass* pass, uint32_t imageId, Texture* zbuf) override;
    Fbo*         createFbo(Device *d, uint32_t w, uint32_t h, Pass* pass, Texture* cl, Texture* zbuf) override;
    void         destroy(Fbo* pass) override;

    Pipeline*    createPipeline(Device* d, Pass* pass,
                                uint32_t width, uint32_t height,
                                const RenderState &st,
                                const Tempest::Decl::ComponentType *decl, size_t declSize,
                                size_t stride,
                                Topology tp,
                                const UniformsLayout& ulay,
                                std::shared_ptr<UniformsLay> &ulayImpl,
                                const std::initializer_list<Shader*>& frag) override;

    Shader*       createShader(AbstractGraphicsApi::Device *d, const char* source, size_t src_size) override;
    void          destroy     (Shader* shader) override;

    Fence*        createFence(Device *d) override;
    void          destroy    (Fence* fence) override;

    Semaphore*    createSemaphore(Device *d) override;
    void          destroy        (Semaphore* semaphore) override;

    CmdPool*      createCommandPool(Device* d) override;
    void          destroy          (CmdPool* cmd) override;

    PBuffer       createBuffer(Device* d, const void *mem, size_t size, MemUsage usage, BufferFlags flg) override;

    Desc*         createDescriptors(Device* d, const UniformsLayout &lay, AbstractGraphicsApi::UniformsLay* layP) override;
    void          destroy(Desc* d) override;

    std::shared_ptr<AbstractGraphicsApi::UniformsLay> createUboLayout(Device *d,const UniformsLayout&) override;

    Texture*      createTexture(Device* d,const Pixmap& p,TextureFormat frm,uint32_t mips) override;
    Texture*      createTexture(Device* d,const uint32_t w,const uint32_t h,uint32_t mips, TextureFormat frm) override;
    //void       destroy(Texture* t) override;

    CommandBuffer* createCommandBuffer(Device* d, CmdPool* pool, CmdType secondary) override;
    void           destroy            (CommandBuffer* cmd) override;

    uint32_t       nextImage(Device *d,Swapchain* sw,Semaphore* onReady) override;
    Image*         getImage (Device *d,Swapchain* sw,uint32_t id) override;
    void           present  (Device *d,Swapchain* sw,uint32_t imageId, const Semaphore *wait) override;

    void           draw     (Device *d,Swapchain *sw,CommandBuffer* cmd,Semaphore* wait,Semaphore* onReady,Fence* onReadyCpu) override;
    void           draw     (Device *d,Swapchain *sw,CommandBuffer** cmd,size_t count,Semaphore* wait,Semaphore* onReady,Fence* onReadyCpu) override;

    void           getCaps  (Device *d,Caps& caps) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    bool           setupValidationLayer();
  };

}
