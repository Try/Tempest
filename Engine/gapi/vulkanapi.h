#pragma once

#include "abstractgraphicsapi.h"

#include <memory>
#include <cstdint>

namespace Tempest {

class VulkanApi : public AbstractGraphicsApi {
  public:
    VulkanApi();
    virtual ~VulkanApi();

    Device*    createDevice(SystemApi::Window* w) override;
    void       destroy(Device* d) override;

    Swapchain* createSwapchain(SystemApi::Window* w,AbstractGraphicsApi::Device *d) override;
    void       destroy(Swapchain* d) override;

    Pass*      createPass(AbstractGraphicsApi::Device *d,
                          AbstractGraphicsApi::Swapchain *s) override;
    void       destroy   (Pass* pass) override;

    Fbo*       createFbo(Device *d, Swapchain *s, Pass* pass, uint32_t imageId) override;
    void       destroy(Fbo* pass) override;

    Pipeline*  createPipeline(Device* d,Pass* pass,uint32_t width, uint32_t height,
                              const UniformsLayout& ulay,
                              const std::initializer_list<Shader*>& frag) override;
    void       destroy       (Pipeline* pass) override;

    Shader*    createShader(AbstractGraphicsApi::Device *d,const char* file) override;
    void       destroy     (Shader* shader) override;

    Fence*     createFence(Device *d) override;
    void       destroy    (Fence* fence) override;

    Semaphore* createSemaphore(Device *d) override;
    void       destroy        (Semaphore* semaphore) override;

    CmdPool*   createCommandPool(Device* d) override;
    void       destroy          (CmdPool* cmd) override;

    Buffer*    createBuffer(Device* d,const void *mem,size_t size,MemUsage usage) override;
    void       destroy(Buffer* cmd) override;

    Desc*      createDescriptors(Device* d, Pipeline* p, Buffer* b, size_t size, size_t count) override;
    void       destroy(Desc* d) override;

    CommandBuffer* createCommandBuffer(Device* d,CmdPool* pool) override;
    void           destroy            (CommandBuffer* cmd) override;

    uint32_t       nextImage(Device *d,Swapchain* sw,Semaphore* onReady) override;
    Image*         getImage (Device *d,Swapchain* sw,uint32_t id) override;
    void           present  (Device *d,Swapchain* sw,uint32_t imageId, const Semaphore *wait) override;

    void           draw     (Device *d,Swapchain *sw,CommandBuffer* cmd,Semaphore* wait,Semaphore* onReady,Fence* onReadyCpu) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    bool setupValidationLayer();
  };

}
