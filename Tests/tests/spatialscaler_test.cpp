#include <Tempest/Device>
#include <Tempest/Encoder>
#include <Tempest/SpatialScaler>

#include <gtest/gtest.h>

#include <utility>
#include <vector>

using namespace Tempest;

namespace {

struct MockStats {
  int  scalerCreateAttempts  = 0;
  int  scalerCreated         = 0;
  int  scalerDestroyed       = 0;
  int  scalerEncoded         = 0;
  int  renderingBegun        = 0;
  int  renderingEnded        = 0;
  bool commandSupportsScaler = true;

  AbstractGraphicsApi::Texture* scalerInput  = nullptr;
  AbstractGraphicsApi::Texture* scalerOutput = nullptr;
  };

struct MockDevice final : AbstractGraphicsApi::Device {
  void waitIdle() override {}
  };

struct MockShader final : AbstractGraphicsApi::Shader {};

struct MockPipeline final : AbstractGraphicsApi::Pipeline {
  IVec3 workGroupSize() const override { return {1,1,1}; }
  size_t sizeofBuffer(size_t, size_t) const override { return 0; }
  };

struct MockCompPipeline final : AbstractGraphicsApi::CompPipeline {
  IVec3 workGroupSize() const override { return {1,1,1}; }
  size_t sizeofBuffer(size_t, size_t) const override { return 0; }
  };

struct MockBuffer final : AbstractGraphicsApi::Buffer {
  void update(const void*, size_t, size_t) override {}
  void read(void*, size_t, size_t) override {}
  };

struct MockTexture final : AbstractGraphicsApi::Texture {
  explicit MockTexture(NonUniqResId id):id(id) {}

  uint32_t mipCount() const override { return 1; }
  NonUniqResId syncId() const override { return id; }

  NonUniqResId id;
  };

struct MockSpatialScaler final : AbstractGraphicsApi::SpatialScaler {
  explicit MockSpatialScaler(MockStats& stats):stats(stats) {
    ++stats.scalerCreated;
    }

  ~MockSpatialScaler() override {
    ++stats.scalerDestroyed;
    }

  MockStats& stats;
  };

class MockCommandBuffer final : public AbstractGraphicsApi::CommandBuffer {
  public:
    explicit MockCommandBuffer(MockStats& stats):stats(stats) {}

    void beginRendering(const Detail::FrameBufferDesc&, size_t, uint32_t, uint32_t) override {
      ++stats.renderingBegun;
      }
    void endRendering() override {
      ++stats.renderingEnded;
      }

    void barrier(const AbstractGraphicsApi::SyncDesc&,
                 const AbstractGraphicsApi::BarrierDesc*, size_t) override {}

    void generateMipmap(AbstractGraphicsApi::Texture&, uint32_t, uint32_t, uint32_t) override {}
    void copy(AbstractGraphicsApi::Buffer&, size_t, AbstractGraphicsApi::Texture&,
              uint32_t, uint32_t, uint32_t) override {}

    bool isRecording() const override { return recording; }
    void begin() override { recording = true; }
    void end() override { recording = false; }
    void reset() override { recording = false; }

    void setPipeline(AbstractGraphicsApi::Pipeline&) override {}
    void setComputePipeline(AbstractGraphicsApi::CompPipeline&) override {}
    void setBinding(size_t, AbstractGraphicsApi::Texture*, uint32_t,
                    const ComponentMapping&, const Sampler&) override {}
    void setBinding(size_t, AbstractGraphicsApi::Buffer*, size_t) override {}
    void setBinding(size_t, AbstractGraphicsApi::DescArray*) override {}
    void setBinding(size_t, AbstractGraphicsApi::AccelerationStructure*) override {}
    void setBinding(size_t, const Sampler&) override {}

    void setViewport(const Rect&) override {}
    void setScissor(const Rect&) override {}

    void draw(const AbstractGraphicsApi::Buffer*, size_t, size_t, size_t,
              size_t, size_t) override {}
    void drawIndexed(const AbstractGraphicsApi::Buffer*, size_t, size_t,
                     const AbstractGraphicsApi::Buffer&, Detail::IndexClass,
                     size_t, size_t, size_t, size_t) override {}
    void drawIndirect(const AbstractGraphicsApi::Buffer&, size_t) override {}
    void dispatch(size_t, size_t, size_t) override {}
    void dispatchIndirect(const AbstractGraphicsApi::Buffer&, size_t) override {}

    bool spatialUpscale(AbstractGraphicsApi::SpatialScaler& scaler,
                        AbstractGraphicsApi::Texture& input,
                        AbstractGraphicsApi::Texture& output) override {
      if(!stats.commandSupportsScaler)
        return AbstractGraphicsApi::CommandBuffer::spatialUpscale(scaler,input,output);
      stats.scalerInput  = &input;
      stats.scalerOutput = &output;
      ++stats.scalerEncoded;
      return true;
      }

  private:
    MockStats& stats;
    bool       recording = false;
  };

class MockApi final : public AbstractGraphicsApi {
  public:
    MockApi(MockStats& stats, bool supportsScaler, bool supportsCommandScaler = true)
      :stats(stats),supportsScaler(supportsScaler) {
      stats.commandSupportsScaler = supportsCommandScaler;
      }

    std::vector<Props> devices() const override { return {Props()}; }

  protected:
    Device* createDevice(std::string_view) override { return new MockDevice(); }
    Swapchain* createSwapchain(SystemApi::Window*, AbstractGraphicsApi::Device*) override { return nullptr; }

    PPipeline createPipeline(Device*, const RenderState&, Topology,
                             const Shader* const*, size_t) override {
      return PPipeline(new MockPipeline());
      }

    PCompPipeline createComputePipeline(Device*, Shader*) override {
      return PCompPipeline(new MockCompPipeline());
      }

    PShader createShader(Device*, const void*, size_t) override {
      return PShader(new MockShader());
      }

    CommandBuffer* createCommandBuffer(Device*) override {
      return new MockCommandBuffer(stats);
      }

    DescArray* createDescriptors(Device*, AbstractGraphicsApi::Texture**, size_t, uint32_t) override {
      return new DescArray();
      }
    DescArray* createDescriptors(Device*, AbstractGraphicsApi::Texture**, size_t, uint32_t,
                                 const Sampler&) override {
      return new DescArray();
      }
    DescArray* createDescriptors(Device*, AbstractGraphicsApi::Buffer**, size_t) override {
      return new DescArray();
      }

    PBuffer createBuffer(Device*, const void*, size_t, MemUsage, BufferHeap) override {
      return PBuffer(new MockBuffer());
      }

    PTexture createTexture(Device*, const Pixmap&, TextureFormat, uint32_t) override {
      return newTexture();
      }
    PTexture createTexture(Device*, uint32_t, uint32_t, uint32_t, TextureFormat) override {
      return newTexture();
      }
    PTexture createStorage(Device*, uint32_t, uint32_t, uint32_t, TextureFormat) override {
      return newTexture();
      }
    PTexture createStorage(Device*, uint32_t, uint32_t, uint32_t, uint32_t,
                           TextureFormat) override {
      return newTexture();
      }

    SpatialScaler* createSpatialScaler(Device* device, const SpatialScalerDesc& desc) override {
      ++stats.scalerCreateAttempts;
      if(!supportsScaler)
        return AbstractGraphicsApi::createSpatialScaler(device,desc);
      return new MockSpatialScaler(stats);
      }

    void readPixels(Device*, Pixmap&, const PTexture, TextureFormat,
                    uint32_t, uint32_t, uint32_t, bool) override {}
    void readBytes(Device*, Buffer*, void*, size_t) override {}
    void present(Device*, Swapchain*) override {}
    std::shared_ptr<Fence> submit(Device*, CommandBuffer*) override { return {}; }

    void getCaps(Device*, Props& caps) override {
      const uint64_t rgba8 = uint64_t(1) << uint64_t(TextureFormat::RGBA8);
      caps.setSamplerFormats(rgba8);
      caps.setAttachFormats(rgba8);
      caps.setStorageFormats(rgba8);
      }

  private:
    PTexture newTexture() {
      const auto id = NonUniqResId(uint32_t(1) << nextTextureId++);
      return PTexture(new MockTexture(id));
      }

    MockStats& stats;
    bool       supportsScaler;
    uint32_t   nextTextureId = 0;
  };

SpatialScalerDesc scalerDesc() {
  SpatialScalerDesc desc;
  desc.inputFormat  = TextureFormat::RGBA8;
  desc.outputFormat = TextureFormat::RGBA8;
  desc.inputWidth   = 2;
  desc.inputHeight  = 2;
  desc.outputWidth  = 4;
  desc.outputHeight = 4;
  return desc;
  }

}

TEST(SpatialScaler, UnsupportedReturnsEmpty) {
  MockStats stats;
  MockApi   api(stats,false);
  Device    device(api);

  auto scaler = device.spatialScaler(scalerDesc());
  EXPECT_TRUE(scaler.isEmpty());
  EXPECT_FALSE(bool(scaler));
  EXPECT_EQ(stats.scalerCreateAttempts,1);
  EXPECT_EQ(stats.scalerCreated,0);
  EXPECT_EQ(stats.scalerDestroyed,0);

  auto input  = device.attachment(TextureFormat::RGBA8,2,2);
  auto output = device.image2d(TextureFormat::RGBA8,4,4);
  auto cmd    = device.commandBuffer();
  auto encoder = cmd.startEncoding(device);
  EXPECT_FALSE(encoder.spatialUpscale(scaler,input,output));
  EXPECT_EQ(stats.scalerEncoded,0);
  }

TEST(SpatialScaler, UnsupportedCommandReturnsFalse) {
  MockStats stats;
  MockApi   api(stats,true,false);
  Device    device(api);

  auto scaler = device.spatialScaler(scalerDesc());
  auto input  = device.attachment(TextureFormat::RGBA8,2,2);
  auto output = device.image2d(TextureFormat::RGBA8,4,4);
  auto cmd    = device.commandBuffer();
  auto encoder = cmd.startEncoding(device);

  EXPECT_FALSE(scaler.isEmpty());
  EXPECT_FALSE(encoder.spatialUpscale(scaler,input,output));
  EXPECT_EQ(stats.scalerEncoded,0);
  }

TEST(SpatialScaler, OwnsAndDestroysBackendObject) {
  MockStats stats;
  MockApi   api(stats,true);
  Device    device(api);

  {
    auto scaler = device.spatialScaler(scalerDesc());
    EXPECT_FALSE(scaler.isEmpty());
    EXPECT_EQ(stats.scalerCreated,1);
    EXPECT_EQ(stats.scalerDestroyed,0);
  }
  EXPECT_EQ(stats.scalerDestroyed,1);
  }

TEST(SpatialScaler, MoveLeavesSourceEmptyAndReleasesDestination) {
  MockStats stats;
  MockApi   api(stats,true);
  Device    device(api);

  {
    auto first = device.spatialScaler(scalerDesc());
    SpatialScaler second(std::move(first));
    EXPECT_TRUE(first.isEmpty());
    EXPECT_FALSE(second.isEmpty());

    auto third = device.spatialScaler(scalerDesc());
    EXPECT_EQ(stats.scalerCreated,2);
    third = std::move(second);
    EXPECT_TRUE(second.isEmpty());
    EXPECT_FALSE(third.isEmpty());
    EXPECT_EQ(stats.scalerDestroyed,1);

    third = std::move(third);
    EXPECT_FALSE(third.isEmpty());
  }
  EXPECT_EQ(stats.scalerDestroyed,2);
  }

TEST(SpatialScaler, EncoderUsesPublicResources) {
  MockStats stats;
  MockApi   api(stats,true);
  Device    device(api);

  auto scaler = device.spatialScaler(scalerDesc());
  auto input  = device.attachment(TextureFormat::RGBA8,2,2);
  auto output = device.image2d(TextureFormat::RGBA8,4,4);
  auto cmd    = device.commandBuffer();

  {
    auto encoder = cmd.startEncoding(device);
    encoder.setFramebuffer({{input,Vec4(),Tempest::Preserve}});
    EXPECT_TRUE(encoder.spatialUpscale(scaler,input,output));
  }

  EXPECT_EQ(stats.scalerEncoded,1);
  EXPECT_EQ(stats.renderingBegun,1);
  EXPECT_EQ(stats.renderingEnded,1);
  EXPECT_NE(stats.scalerInput,nullptr);
  EXPECT_NE(stats.scalerOutput,nullptr);
  EXPECT_NE(stats.scalerInput,stats.scalerOutput);
  }
