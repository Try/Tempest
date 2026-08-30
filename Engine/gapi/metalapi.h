#pragma once

#include <Tempest/AbstractGraphicsApi>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Tempest {

namespace Detail {
struct MetalApiAbiProbe;
}

class MetalApi : public AbstractGraphicsApi {
  public:
    enum class SwapchainRenderMode:uint8_t {
      Copy,
      Direct,
    };

    static constexpr uint32_t PrecompiledShaderSchemaVersion = 1;
    static constexpr uint32_t MslGeneratorVersion            = 1;

    enum class PrecompiledPlatform : uint8_t {
      MacOS,
      IOSDevice,
      IOSSimulator,
      };

    enum class PrecompiledShaderStage : uint8_t {
      Vertex,
      Control,
      Evaluate,
      Geometry,
      Fragment,
      Compute,
      Task,
      Mesh,
      };

    using PrecompiledShaderKey   = std::array<uint8_t,32>;
    using PrecompiledLibraryHash = std::array<uint8_t,32>;

    /**
     * Exact input profile used when SPIR-V was converted to canonical MSL.
     * MslGeneratorVersion pins every SPIRV-Cross default not listed here; it
     * must be incremented whenever those defaults or the serialization change.
     */
    struct PrecompiledShaderProfile {
      uint32_t               schemaVersion               = PrecompiledShaderSchemaVersion;
      uint32_t               mslGeneratorVersion         = MslGeneratorVersion;
      PrecompiledPlatform    platform                    = PrecompiledPlatform::MacOS;
      PrecompiledShaderStage stage                       = PrecompiledShaderStage::Compute;
      std::string            entryPoint                  = "main0";
      uint32_t               mslVersion                  = 0;
      bool                   flipVertY                   = true;
      uint32_t               bufferSizeBufferIndex       = 29;
      uint8_t                argumentBuffersTier         = 0;
      bool                   runtimeArrayRichDescriptor  = false;
      bool                   readWriteTextureFences      = true;
      bool                   nativeImageAtomics          = true;
      uint32_t               r32uiLinearTextureAlignment = 0;
      uint32_t               r32uiAlignmentConstantId    = 0;
      };

    struct PrecompiledShader {
      PrecompiledShaderProfile profile;
      PrecompiledShaderKey     key = {};
      };

    struct PrecompiledLibrary {
      // One target-specific metallib. MetalApi copies these bytes at construction.
      std::vector<uint8_t>           data;
      // Full SHA-256 of data. It is verified before Metal sees the library.
      PrecompiledLibraryHash         dataHash = {};
      std::vector<PrecompiledShader> shaders;
      };

    struct Options {
      uint32_t            swapchainBufferCount = 0;
      // Maximum number of compiled Metal shader modules kept per device.
      // Zero disables caching.
      size_t              shaderModuleCacheSize = 0;
      SwapchainRenderMode swapchainRenderMode = SwapchainRenderMode::Copy;
      // Empty by default: runtime MSL compilation remains the only code path.
      std::vector<PrecompiledLibrary> precompiledLibraries;
      };

    explicit MetalApi(ApiFlags f=ApiFlags::NoFlags);
    MetalApi(ApiFlags f, const Options& options);
    MetalApi(const MetalApi& other) noexcept;
    MetalApi& operator=(const MetalApi& other) noexcept;
    ~MetalApi();

    /**
     * Computes the canonical, full SHA-256 identity of an MSL source/profile
     * pair. The serialized profile includes the schema, target, entry point,
     * shader stage and every Metal code-generation option above. An artifact
     * is used only when this key and the complete runtime profile agree.
     */
    static PrecompiledShaderKey precompiledShaderKey(std::string_view canonicalMsl,
                                                     const PrecompiledShaderProfile& profile);

    /** Computes the full SHA-256 identity of a target-specific metallib. */
    static PrecompiledLibraryHash precompiledLibraryHash(const void* data, size_t size);

    std::vector<Props> devices() const override;

  protected:
    Device*        createDevice(std::string_view gpuName) override;
    Swapchain*     createSwapchain(SystemApi::Window* w, Device *d) override;

    PPipeline      createPipeline(Device* d, const RenderState &st, Topology tp,
                                  const Shader*const* sh, size_t cnt) override;
    PCompPipeline  createComputePipeline(Device* d, Shader* sh) override;
    PShader        createShader(Device *d, const void* source, size_t src_size) override;

    PBuffer        createBuffer (Device* d, const void *mem, size_t size, MemUsage usage, BufferHeap flg) override;
    PTexture       createTexture(Device* d, const Pixmap& p, TextureFormat frm, uint32_t mips) override;
    PTexture       createTexture(Device* d, const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm) override;
    PTexture       createStorage(Device* d, const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm) override;
    PTexture       createStorage(Device* d, const uint32_t w, const uint32_t h, const uint32_t depth, uint32_t mips, TextureFormat frm) override;
    SpatialScaler*  createSpatialScaler(Device* d, const SpatialScalerDesc& desc) override;
    TemporalScaler* createTemporalScaler(Device* d, const TemporalScalerDesc& desc) override;

    AccelerationStructure* createBottomAccelerationStruct(Device* d, const RtGeometry* geom, size_t size) override;
    AccelerationStructure* createTopAccelerationStruct(Device* d, const RtInstance* inst, AccelerationStructure*const* as, size_t size) override;

    DescArray*     createDescriptors(Device* d, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel) override;
    DescArray*     createDescriptors(Device* d, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel, const Sampler& smp) override;
    DescArray*     createDescriptors(Device* d, AbstractGraphicsApi::Buffer**  buf, size_t cnt) override;

    void           readPixels(Device *d, Pixmap &out, const PTexture t,
                              TextureFormat frm, const uint32_t w, const uint32_t h, uint32_t mip, bool storageImg) override;
    void           readBytes(Device* d, Buffer* buf, void* out, size_t size) override;

    CommandBuffer* createCommandBuffer(Device* d) override;

    void           present(Device *d, Swapchain* sw) override;
    auto           submit (Device *d, CommandBuffer* cmd) -> std::shared_ptr<AbstractGraphicsApi::Fence> override;

    void           getCaps(Device *d, Props& caps) override;

  private:
    bool validation = false;

  friend struct Detail::MetalApiAbiProbe;
  };

}
