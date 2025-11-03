#pragma once

#include "abstractgraphicsapi.h"

#include <memory>
#include <cstdint>
#include <vector>

namespace Tempest {

class VulkanApi : public AbstractGraphicsApi {
  public:
    explicit VulkanApi(ApiFlags f=ApiFlags::NoFlags);
    virtual ~VulkanApi();

    std::vector<Props> devices() const override;

  protected:
    Device*        createDevice(std::string_view gpuName) override;

    Swapchain*     createSwapchain(SystemApi::Window* w, Device *d) override;

    PPipeline      createPipeline(Device* d, const RenderState &st, Topology tp,
                                  const Shader*const* sh, size_t cnt) override;
    PCompPipeline  createComputePipeline(Device* d, Shader* sh) override;

    PShader        createShader(AbstractGraphicsApi::Device *d, const void* source, size_t src_size) override;

    DescArray*     createDescriptors(Device* d, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel, const Sampler& smp) override;
    DescArray*     createDescriptors(Device* d, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel) override;
    DescArray*     createDescriptors(Device* d, AbstractGraphicsApi::Buffer**  buf, size_t cnt) override;

    PBuffer        createBuffer (Device* d, const void *mem, size_t size, MemUsage usage, BufferHeap flg) override;
    PTexture       createTexture(Device* d, const Pixmap& p, TextureFormat frm, uint32_t mips) override;
    PTexture       createTexture(Device* d, const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm) override;
    PTexture       createStorage(Device* d, const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm) override;
    PTexture       createStorage(Device* d, const uint32_t w, const uint32_t h, const uint32_t depth, uint32_t mips, TextureFormat frm) override;

    AccelerationStructure* createBottomAccelerationStruct(Device* d, const RtGeometry* geom, size_t size) override;
    AccelerationStructure* createTopAccelerationStruct(Device* d, const RtInstance* inst, AccelerationStructure*const* as, size_t size) override;

    void           readPixels(Device *d, Pixmap &out, const PTexture t, TextureFormat frm,
                              const uint32_t w, const uint32_t h, uint32_t mip, bool storageImg) override;
    void           readBytes(Device* d, Buffer* buf, void* out, size_t size) override;

    CommandBuffer* createCommandBuffer(Device* d) override;

    void           present(Device *d, Swapchain* sw) override;
    auto           submit(Device *d, CommandBuffer* cmd) -> std::shared_ptr<AbstractGraphicsApi::Fence> override;

    void           getCaps(Device *d, Props& props) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
  };

}
