#include <Tempest/Device>
#include <Tempest/Encoder>
#include <Tempest/TemporalScaler>

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
  int  computePipelinesSet   = 0;
  bool commandSupportsScaler = true;

  TemporalScalerDesc scalerDesc;
  TemporalScalerArgs scalerArgs;

  AbstractGraphicsApi::Texture* scalerInput  = nullptr;
  AbstractGraphicsApi::Texture* scalerDepth  = nullptr;
  AbstractGraphicsApi::Texture* scalerMotion = nullptr;
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

struct MockTemporalScaler final : AbstractGraphicsApi::TemporalScaler {
  explicit MockTemporalScaler(MockStats& stats):stats(stats) {
    ++stats.scalerCreated;
    }

  ~MockTemporalScaler() override {
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
    void setComputePipeline(AbstractGraphicsApi::CompPipeline&) override {
      ++stats.computePipelinesSet;
      }
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

    bool temporalUpscale(AbstractGraphicsApi::TemporalScaler& scaler,
                         AbstractGraphicsApi::Texture& input,
                         AbstractGraphicsApi::Texture& depth,
                         AbstractGraphicsApi::Texture& motion,
                         AbstractGraphicsApi::Texture& output,
                         const TemporalScalerArgs& args) override {
      if(!stats.commandSupportsScaler)
        return AbstractGraphicsApi::CommandBuffer::temporalUpscale(scaler,input,depth,motion,output,args);
      stats.scalerInput  = &input;
      stats.scalerDepth  = &depth;
      stats.scalerMotion = &motion;
      stats.scalerOutput = &output;
      stats.scalerArgs   = args;
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

    TemporalScaler* createTemporalScaler(Device* device, const TemporalScalerDesc& desc) override {
      ++stats.scalerCreateAttempts;
      stats.scalerDesc = desc;
      if(!supportsScaler)
        return AbstractGraphicsApi::createTemporalScaler(device,desc);
      return new MockTemporalScaler(stats);
      }

    void readPixels(Device*, Pixmap&, const PTexture, TextureFormat,
                    uint32_t, uint32_t, uint32_t, bool) override {}
    void readBytes(Device*, Buffer*, void*, size_t) override {}
    void present(Device*, Swapchain*) override {}
    std::shared_ptr<Fence> submit(Device*, CommandBuffer*) override { return {}; }

    void getCaps(Device*, Props& caps) override {
      const uint64_t rgba8   = uint64_t(1) << uint64_t(TextureFormat::RGBA8);
      const uint64_t rgba16f = uint64_t(1) << uint64_t(TextureFormat::RGBA16F);
      const uint64_t rg32f   = uint64_t(1) << uint64_t(TextureFormat::RG32F);
      const uint64_t depth32 = uint64_t(1) << uint64_t(TextureFormat::Depth32F);
      caps.setSamplerFormats(rgba8|rgba16f|rg32f|depth32);
      caps.setAttachFormats(rgba8|rgba16f|rg32f);
      caps.setDepthFormats(depth32);
      caps.setStorageFormats(rgba8|rgba16f);
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

TemporalScalerDesc scalerDesc() {
  TemporalScalerDesc desc;
  desc.inputFormat  = TextureFormat::RGBA16F;
  desc.depthFormat  = TextureFormat::Depth32F;
  desc.motionFormat = TextureFormat::RG32F;
  desc.outputFormat = TextureFormat::RGBA8;
  desc.inputWidth   = 960;
  desc.inputHeight  = 540;
  desc.outputWidth  = 1920;
  desc.outputHeight = 1080;
  desc.autoExposure = false;
  return desc;
  }

}

TEST(TemporalScaler, UnsupportedReturnsEmpty) {
  MockStats stats;
  MockApi   api(stats,false);
  Device    device(api);

  auto scaler = device.temporalScaler(scalerDesc());
  EXPECT_TRUE(scaler.isEmpty());
  EXPECT_FALSE(bool(scaler));
  EXPECT_EQ(stats.scalerCreateAttempts,1);
  EXPECT_EQ(stats.scalerCreated,0);
  EXPECT_EQ(stats.scalerDestroyed,0);

  auto input  = device.attachment(TextureFormat::RGBA16F,2,2);
  auto depth  = device.zbuffer(TextureFormat::Depth32F,2,2);
  auto motion = device.attachment(TextureFormat::RG32F,2,2);
  auto output = device.image2d(TextureFormat::RGBA16F,4,4);
  auto cmd    = device.commandBuffer();
  auto encoder = cmd.startEncoding(device);
  EXPECT_FALSE(encoder.temporalUpscale(scaler,input,depth,motion,output,{}));
  EXPECT_EQ(stats.scalerEncoded,0);
  }

TEST(TemporalScaler, UnsupportedCommandReturnsFalse) {
  MockStats stats;
  MockApi   api(stats,true,false);
  Device    device(api);

  auto scaler = device.temporalScaler(scalerDesc());
  auto input  = device.attachment(TextureFormat::RGBA16F,2,2);
  auto depth  = device.zbuffer(TextureFormat::Depth32F,2,2);
  auto motion = device.attachment(TextureFormat::RG32F,2,2);
  auto output = device.image2d(TextureFormat::RGBA16F,4,4);
  auto cmd    = device.commandBuffer();
  auto encoder = cmd.startEncoding(device);

  EXPECT_FALSE(scaler.isEmpty());
  EXPECT_FALSE(encoder.temporalUpscale(scaler,input,depth,motion,output,{}));
  EXPECT_EQ(stats.scalerEncoded,0);
  }

TEST(TemporalScaler, EveryEmptyResourceReturnsFalse) {
  MockStats stats;
  MockApi   api(stats,true);
  Device    device(api);

  auto scaler = device.temporalScaler(scalerDesc());
  auto input  = device.attachment(TextureFormat::RGBA16F,2,2);
  auto depth  = device.zbuffer(TextureFormat::Depth32F,2,2);
  auto motion = device.attachment(TextureFormat::RG32F,2,2);
  auto output = device.image2d(TextureFormat::RGBA16F,4,4);
  auto cmd    = device.commandBuffer();
  auto encoder = cmd.startEncoding(device);

  TemporalScaler emptyScaler;
  Attachment     emptyInput;
  ZBuffer        emptyDepth;
  Attachment     emptyMotion;
  StorageImage   emptyOutput;

  EXPECT_FALSE(encoder.temporalUpscale(emptyScaler,input,depth,motion,output,{}));
  EXPECT_FALSE(encoder.temporalUpscale(scaler,emptyInput,depth,motion,output,{}));
  EXPECT_FALSE(encoder.temporalUpscale(scaler,input,emptyDepth,motion,output,{}));
  EXPECT_FALSE(encoder.temporalUpscale(scaler,input,depth,emptyMotion,output,{}));
  EXPECT_FALSE(encoder.temporalUpscale(scaler,input,depth,motion,emptyOutput,{}));
  EXPECT_EQ(stats.scalerEncoded,0);
  }

TEST(TemporalScaler, DeviceForwardsDescriptor) {
  MockStats stats;
  MockApi   api(stats,true);
  Device    device(api);

  const auto expected = scalerDesc();
  auto scaler = device.temporalScaler(expected);

  EXPECT_TRUE(bool(scaler));
  EXPECT_EQ(stats.scalerDesc.inputFormat,expected.inputFormat);
  EXPECT_EQ(stats.scalerDesc.depthFormat,expected.depthFormat);
  EXPECT_EQ(stats.scalerDesc.motionFormat,expected.motionFormat);
  EXPECT_EQ(stats.scalerDesc.outputFormat,expected.outputFormat);
  EXPECT_EQ(stats.scalerDesc.inputWidth,expected.inputWidth);
  EXPECT_EQ(stats.scalerDesc.inputHeight,expected.inputHeight);
  EXPECT_EQ(stats.scalerDesc.outputWidth,expected.outputWidth);
  EXPECT_EQ(stats.scalerDesc.outputHeight,expected.outputHeight);
  EXPECT_EQ(stats.scalerDesc.autoExposure,expected.autoExposure);
  }

TEST(TemporalScaler, OwnsAndDestroysBackendObject) {
  MockStats stats;
  MockApi   api(stats,true);
  Device    device(api);

  {
    auto scaler = device.temporalScaler(scalerDesc());
    EXPECT_FALSE(scaler.isEmpty());
    EXPECT_EQ(stats.scalerCreated,1);
    EXPECT_EQ(stats.scalerDestroyed,0);
  }
  EXPECT_EQ(stats.scalerDestroyed,1);
  }

TEST(TemporalScaler, MoveLeavesSourceEmptyAndReleasesDestination) {
  MockStats stats;
  MockApi   api(stats,true);
  Device    device(api);

  {
    auto first = device.temporalScaler(scalerDesc());
    TemporalScaler second(std::move(first));
    EXPECT_TRUE(first.isEmpty());
    EXPECT_FALSE(second.isEmpty());

    auto third = device.temporalScaler(scalerDesc());
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

TEST(TemporalScaler, EncoderForwardsResourcesAndArgsAndEndsRendering) {
  MockStats stats;
  MockApi   api(stats,true);
  Device    device(api);

  auto scaler = device.temporalScaler(scalerDesc());
  auto input  = device.attachment(TextureFormat::RGBA16F,2,2);
  auto depth  = device.zbuffer(TextureFormat::Depth32F,2,2);
  auto motion = device.attachment(TextureFormat::RG32F,2,2);
  auto output = device.image2d(TextureFormat::RGBA16F,4,4);
  auto cmd    = device.commandBuffer();

  TemporalScalerArgs args;
  args.jitterOffsetX      = 0.25f;
  args.jitterOffsetY      = -0.5f;
  args.motionVectorScaleX = 2.f;
  args.motionVectorScaleY = -3.f;
  args.resetHistory       = true;
  args.depthReversed      = true;

  {
    auto encoder = cmd.startEncoding(device);
    encoder.setFramebuffer({{input,Vec4(),Tempest::Preserve}});
    EXPECT_TRUE(encoder.temporalUpscale(scaler,input,depth,motion,output,args));
  }

  EXPECT_EQ(stats.scalerEncoded,1);
  EXPECT_EQ(stats.renderingBegun,1);
  EXPECT_EQ(stats.renderingEnded,1);
  auto* recordedInput  = dynamic_cast<MockTexture*>(stats.scalerInput);
  auto* recordedDepth  = dynamic_cast<MockTexture*>(stats.scalerDepth);
  auto* recordedMotion = dynamic_cast<MockTexture*>(stats.scalerMotion);
  auto* recordedOutput = dynamic_cast<MockTexture*>(stats.scalerOutput);
  ASSERT_NE(recordedInput,nullptr);
  ASSERT_NE(recordedDepth,nullptr);
  ASSERT_NE(recordedMotion,nullptr);
  ASSERT_NE(recordedOutput,nullptr);
  EXPECT_EQ(recordedInput->syncId(),NonUniqResId(1));
  EXPECT_EQ(recordedDepth->syncId(),NonUniqResId(2));
  EXPECT_EQ(recordedMotion->syncId(),NonUniqResId(4));
  EXPECT_EQ(recordedOutput->syncId(),NonUniqResId(8));
  EXPECT_FLOAT_EQ(stats.scalerArgs.jitterOffsetX,args.jitterOffsetX);
  EXPECT_FLOAT_EQ(stats.scalerArgs.jitterOffsetY,args.jitterOffsetY);
  EXPECT_FLOAT_EQ(stats.scalerArgs.motionVectorScaleX,args.motionVectorScaleX);
  EXPECT_FLOAT_EQ(stats.scalerArgs.motionVectorScaleY,args.motionVectorScaleY);
  EXPECT_EQ(stats.scalerArgs.resetHistory,args.resetHistory);
  EXPECT_EQ(stats.scalerArgs.depthReversed,args.depthReversed);
  }

TEST(TemporalScaler, RejectsThreeDimensionalOutput) {
  MockStats stats;
  MockApi   api(stats,true);
  Device    device(api);

  auto scaler = device.temporalScaler(scalerDesc());
  auto input  = device.attachment(TextureFormat::RGBA16F,2,2);
  auto depth  = device.zbuffer(TextureFormat::Depth32F,2,2);
  auto motion = device.attachment(TextureFormat::RG32F,2,2);
  auto output = device.image3d(TextureFormat::RGBA16F,4,4,2);
  auto cmd    = device.commandBuffer();
  auto encoder = cmd.startEncoding(device);

  EXPECT_THROW(encoder.temporalUpscale(scaler,input,depth,motion,output,{}),BadTextureCastException);
  EXPECT_EQ(stats.scalerEncoded,0);
  }

TEST(TemporalScaler, ClearsComputePipelineCache) {
  MockStats stats;
  MockApi   api(stats,true);
  Device    device(api);

  const uint32_t shaderCode = 0;
  auto shader   = device.shader(&shaderCode,sizeof(shaderCode));
  auto pipeline = device.pipeline(shader);
  auto scaler   = device.temporalScaler(scalerDesc());
  auto input    = device.attachment(TextureFormat::RGBA16F,2,2);
  auto depth    = device.zbuffer(TextureFormat::Depth32F,2,2);
  auto motion   = device.attachment(TextureFormat::RG32F,2,2);
  auto output   = device.image2d(TextureFormat::RGBA16F,4,4);
  auto cmd      = device.commandBuffer();

  {
    auto encoder = cmd.startEncoding(device);
    encoder.setPipeline(pipeline);
    EXPECT_EQ(stats.computePipelinesSet,1);
    EXPECT_TRUE(encoder.temporalUpscale(scaler,input,depth,motion,output,{}));
    encoder.setPipeline(pipeline);
    EXPECT_EQ(stats.computePipelinesSet,2);
  }

  EXPECT_EQ(stats.renderingEnded,0);
  EXPECT_EQ(stats.scalerEncoded,1);
  }
