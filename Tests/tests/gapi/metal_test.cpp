#include <Tempest/MetalApi>
#include <Tempest/Except>
#include <Tempest/Device>
#include <Tempest/Fence>
#include <Tempest/Pixmap>
#include <Tempest/Log>
#include <Tempest/Application>
#include <Tempest/Window>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <thread>

#if defined(__OSX__)
#include "../../../Engine/gapi/metal/mtmetalfx.h"
#if defined(TEMPEST_METALFX_TEMPORAL_SDK_AVAILABLE)
#include <dlfcn.h>
#endif
#endif

#include "gapi_test_common.h"

#if defined(__OSX__) && defined(TEMPEST_TEST_METAL_PRECOMPILED)
#include "gapi/metal/mtdevice.h"
#include "gapi/metal/mtprecompiledlibrary.h"
#include "gapi/metal/mtsha256.h"

#include <Metal/Metal.hpp>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <thread>
#include <type_traits>
#endif

using namespace testing;
using namespace Tempest;

namespace {

float unpackUnsignedFloat(uint32_t value, uint32_t mantissaBits) {
  const uint32_t mantissaMask = (1u << mantissaBits)-1u;
  const uint32_t mantissa     = value & mantissaMask;
  const uint32_t exponent     = (value >> mantissaBits) & 0x1Fu;
  if(exponent==0)
    return std::ldexp(float(mantissa),-14-int(mantissaBits));
  return std::ldexp(1.f+float(mantissa)/float(1u << mantissaBits),int(exponent)-15);
  }

float unpackHalf(uint16_t value) {
  const uint32_t sign     = value >> 15;
  const uint32_t exponent = (value >> 10) & 0x1Fu;
  const uint32_t mantissa = value & 0x3FFu;
  float ret = 0.f;
  if(exponent==0)
    ret = std::ldexp(float(mantissa),-24); else
  if(exponent==0x1Fu)
    ret = mantissa==0 ? std::numeric_limits<float>::infinity() :
                        std::numeric_limits<float>::quiet_NaN(); else
    ret = std::ldexp(float(0x400u+mantissa),int(exponent)-25);
  return sign!=0 ? -ret : ret;
  }

std::array<double,4> averageRgba16F(const Pixmap& image) {
  std::array<double,4> average = {};
  const auto* pixels = reinterpret_cast<const uint16_t*>(image.data());
  const size_t count = size_t(image.w())*image.h();
  for(size_t i=0; i<count; ++i)
    for(size_t component=0; component<average.size(); ++component)
      average[component] += unpackHalf(pixels[i*4+component]);
  for(auto& component:average)
    component /= double(count);
  return average;
  }

#if defined(__OSX__)
bool metalFxTemporalSupported() {
#if defined(TEMPEST_METALFX_TEMPORAL_SDK_AVAILABLE)
  using CreateDevice = void*       (*)();
  using LookupClass  = void*       (*)(const char*);
  using RegisterSel  = const void* (*)(const char*);
  using SendBool     = bool        (*)(void*,const void*,void*);
  using SendVoid     = void        (*)(void*,const void*);
  auto createDevice = reinterpret_cast<CreateDevice>(dlsym(RTLD_DEFAULT,"MTLCreateSystemDefaultDevice"));
  auto lookupClass  = reinterpret_cast<LookupClass> (dlsym(RTLD_DEFAULT,"objc_lookUpClass"));
  auto registerSel  = reinterpret_cast<RegisterSel> (dlsym(RTLD_DEFAULT,"sel_registerName"));
  auto sendBool     = reinterpret_cast<SendBool>    (dlsym(RTLD_DEFAULT,"objc_msgSend"));
  auto sendVoid     = reinterpret_cast<SendVoid>    (dlsym(RTLD_DEFAULT,"objc_msgSend"));
  if(createDevice==nullptr || lookupClass==nullptr || registerSel==nullptr ||
     sendBool==nullptr || sendVoid==nullptr)
    return false;
  auto scalerClass = lookupClass("MTLFXTemporalScalerDescriptor");
  if(scalerClass==nullptr)
    return false;

  auto device = createDevice();
  if(device==nullptr)
    return false;

  const bool supported = sendBool(scalerClass,registerSel("supportsDevice:"),device);
  sendVoid(device,registerSel("release"));
  return supported;
#else
  return false;
#endif
  }

class MetalSwapchainWindow final : public Window {
  public:
    SystemApi::Window* nativeWindow() const { return hwnd(); }
  };

void swapchainSmoke(MetalApi::SwapchainRenderMode mode) {
  MetalApi::Options options;
  options.swapchainRenderMode = mode;
  MetalApi api(ApiFlags::Validation,options);
  Device device(api);
  MetalSwapchainWindow window;
  window.resize(64,64);
  Application::processEvents();

  Swapchain swapchain(device,window.nativeWindow());
  std::vector<Fence> fences;
  auto renderFrame = [&](const Vec4& clear) {
    const uint32_t before = swapchain.currentImage();
    auto command = device.commandBuffer();
    {
      auto encoder = command.startEncoding(device);
      encoder.setFramebuffer({{swapchain[before],clear,Tempest::Preserve}});
      }
    fences.emplace_back(device.submit(command));
    device.present(swapchain);
    EXPECT_EQ(swapchain.currentImage(),(before+1)%swapchain.imageCount());
    };

  renderFrame(Vec4(1.f,0.f,0.f,1.f));
  renderFrame(Vec4(0.f,1.f,0.f,1.f));
  // Reset while both render and presentation command buffers can still be in
  // flight. The operation gate and device idle tracking must close this race.
  swapchain.reset();
  for(auto& fence:fences)
    fence.wait();
  fences.clear();

  window.resize(96,80);
  Application::processEvents();
  swapchain.reset();
  EXPECT_GT(swapchain.w(),0u);
  EXPECT_GT(swapchain.h(),0u);

  renderFrame(Vec4(0.f,0.f,1.f,1.f));
  renderFrame(Vec4(0.25f,0.5f,0.75f,1.f));
  if(mode==MetalApi::SwapchainRenderMode::Direct)
    EXPECT_THROW(device.present(swapchain),SwapchainSuboptimal);
  // Leave the final submissions in flight. MtSwapchain destruction must wait
  // for both CPU presentation setup and GPU completion without using `this`
  // from a completion handler.
  }

#endif

}
#if defined(__OSX__) && defined(TEMPEST_TEST_METAL_PRECOMPILED)
namespace {

std::string hexDigest(const Detail::MtSha256::Digest& digest) {
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for(uint8_t value:digest)
    out << std::setw(2) << uint32_t(value);
  return out.str();
  }

MetalApi::PrecompiledShaderProfile testProfile() {
  MetalApi::PrecompiledShaderProfile ret;
  ret.platform   = MetalApi::PrecompiledPlatform::MacOS;
  ret.stage      = MetalApi::PrecompiledShaderStage::Compute;
  ret.entryPoint = "main0";
  ret.mslVersion = 20400;
  return ret;
  }

struct TemporaryMetallib final {
  explicit TemporaryMetallib(uint32_t value=7, bool alternate=false) {
    char path[] = "/tmp/tempest-metal-precompiled-XXXXXX";
    const char* created = mkdtemp(path);
    if(created==nullptr)
      return;
    directory = created;

    canonicalMsl =
      "#include <metal_stdlib>\n"
      "using namespace metal;\n"
      "kernel void main0(device uint* output [[buffer(0)]], "
      "uint id [[thread_position_in_grid]]) { output[id] = "+
      std::to_string(value)+"; }\n";
    if(alternate) {
      canonicalMsl +=
        "kernel void alternate(device uint* output [[buffer(0)]], "
        "uint id [[thread_position_in_grid]]) { output[id] = "+
        std::to_string(value+1)+"; }\n";
      }

    const auto source = directory/"fixture.metal";
    const auto air    = directory/"fixture.air";
    const auto library= directory/"fixture.metallib";
    {
      std::ofstream out(source);
      out << canonicalMsl;
      }

    const std::string compile = "xcrun -sdk macosx metal -std=macos-metal2.4 -c \""+
                                source.string()+"\" -o \""+air.string()+"\"";
    const std::string link    = "xcrun -sdk macosx metallib \""+air.string()+
                                "\" -o \""+library.string()+"\"";
    if(std::system(compile.c_str())!=0 || std::system(link.c_str())!=0)
      return;

    std::ifstream in(library,std::ios::binary);
    data.assign(std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>());
    }

  ~TemporaryMetallib() {
    if(!directory.empty())
      std::filesystem::remove_all(directory);
    }

  bool valid() const {
    return !data.empty();
    }

  std::filesystem::path directory;
  std::string           canonicalMsl;
  std::vector<uint8_t>  data;
  };

MetalApi::Options optionsFor(const TemporaryMetallib& fixture,
                             MetalApi::PrecompiledShaderProfile profile = testProfile()) {
  MetalApi::PrecompiledShader shader;
  shader.profile      = std::move(profile);
  shader.key          = MetalApi::precompiledShaderKey(fixture.canonicalMsl,shader.profile);

  MetalApi::PrecompiledLibrary library;
  library.data     = fixture.data;
  library.dataHash = MetalApi::precompiledLibraryHash(library.data.data(),library.data.size());
  library.shaders.push_back(std::move(shader));

  MetalApi::Options ret;
  ret.precompiledLibraries.push_back(std::move(library));
  return ret;
  }

Detail::NsPtr<MTL::Device> testMetalDevice() {
  setenv("METAL_DEVICE_WRAPPER_TYPE","1",1);
  setenv("METAL_DEBUG_ERROR_MODE",   "5",0);
  setenv("METAL_ERROR_MODE",         "5",0);
  return Detail::NsPtr<MTL::Device>(MTL::CreateSystemDefaultDevice());
  }

std::vector<uint8_t> readBinary(const char* filename) {
  std::ifstream in(filename,std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(in),
                              std::istreambuf_iterator<char>());
  }

class InspectableMetalApi final : public MetalApi {
  public:
    using MetalApi::MetalApi;

    InspectableMetalApi(const InspectableMetalApi&) noexcept = default;
    InspectableMetalApi& operator=(const InspectableMetalApi&) noexcept = default;

    std::unique_ptr<Detail::MtDevice> createTestDevice() {
      return std::unique_ptr<Detail::MtDevice>(
          static_cast<Detail::MtDevice*>(createDevice(std::string_view{})));
      }
  };

}
#endif

TEST(MetalApi,PrecompiledSha256Vectors) {
#if defined(__OSX__) && defined(TEMPEST_TEST_METAL_PRECOMPILED)
  EXPECT_EQ(hexDigest(Detail::MtSha256::hash("")),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  EXPECT_EQ(hexDigest(Detail::MtSha256::hash("abc")),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  EXPECT_EQ(hexDigest(Detail::MtSha256::hash(
              "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")),
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

  Detail::MtSha256 incremental;
  const std::string block(1000,'a');
  for(size_t i=0; i<1000; ++i)
    incremental.update(block);
  EXPECT_EQ(hexDigest(incremental.finalize()),
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
  EXPECT_EQ(hexDigest(MetalApi::precompiledLibraryHash("abc",3)),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  EXPECT_EQ(sizeof(MetalApi),2*sizeof(void*));
#endif
  }

TEST(MetalApi,PrecompiledKeyCoversProfile) {
#if defined(__OSX__) && defined(TEMPEST_TEST_METAL_PRECOMPILED)
  const std::string msl = "kernel void main0() {}";
  const auto base = testProfile();
  const auto key  = MetalApi::precompiledShaderKey(msl,base);
  auto differs = [&](auto mutate) {
    auto profile = base;
    mutate(profile);
    EXPECT_NE(MetalApi::precompiledShaderKey(msl,profile),key);
    };

  EXPECT_NE(MetalApi::precompiledShaderKey(msl+"\n",base),key);
  differs([](auto& p){ ++p.schemaVersion; });
  differs([](auto& p){ ++p.mslGeneratorVersion; });
  differs([](auto& p){ p.platform=MetalApi::PrecompiledPlatform::IOSDevice; });
  differs([](auto& p){ p.stage=MetalApi::PrecompiledShaderStage::Fragment; });
  differs([](auto& p){ p.entryPoint="alternate"; });
  differs([](auto& p){ ++p.mslVersion; });
  differs([](auto& p){ p.flipVertY=!p.flipVertY; });
  differs([](auto& p){ ++p.bufferSizeBufferIndex; });
  differs([](auto& p){ ++p.argumentBuffersTier; });
  differs([](auto& p){ p.runtimeArrayRichDescriptor=!p.runtimeArrayRichDescriptor; });
  differs([](auto& p){ p.readWriteTextureFences=!p.readWriteTextureFences; });
  differs([](auto& p){ p.nativeImageAtomics=!p.nativeImageAtomics; });
  differs([](auto& p){ ++p.r32uiLinearTextureAlignment; });
  differs([](auto& p){ ++p.r32uiAlignmentConstantId; });
#endif
  }

TEST(MetalApi,PrecompiledLibraryValidHitAndLifetime) {
#if defined(__OSX__) && defined(TEMPEST_TEST_METAL_PRECOMPILED)
  TemporaryMetallib fixture;
  ASSERT_TRUE(fixture.valid());
  auto device = testMetalDevice();
  ASSERT_NE(device,nullptr);

  std::unique_ptr<Detail::MtPrecompiledLibraries> loader;
  {
    auto temporaryOptions = optionsFor(fixture);
    loader = std::make_unique<Detail::MtPrecompiledLibraries>(*device,temporaryOptions);
    temporaryOptions.precompiledLibraries.clear();
  }

  auto function = loader->find(fixture.canonicalMsl,testProfile());
  ASSERT_NE(function,nullptr);
  EXPECT_EQ(function->functionType(),MTL::FunctionTypeKernel);
  NS::Error* error = nullptr;
  auto pipeline = Detail::NsPtr<MTL::ComputePipelineState>(
      device->newComputePipelineState(function.get(),&error));
  EXPECT_EQ(error,nullptr);
  EXPECT_NE(pipeline,nullptr);
#endif
  }

TEST(MetalApi,PrecompiledOptionsCopyAndLegacyFallback) {
#if defined(__OSX__) && defined(TEMPEST_TEST_METAL_PRECOMPILED)
  static_assert(std::is_copy_constructible_v<MetalApi>,"MetalApi lost legacy copying");
  static_assert(std::is_copy_assignable_v<MetalApi>,"MetalApi lost legacy assignment");
  static_assert(std::is_nothrow_copy_constructible_v<MetalApi>,"MetalApi copy must remain noexcept");
  static_assert(std::is_nothrow_copy_assignable_v<MetalApi>,"MetalApi assignment must remain noexcept");
  static_assert(std::is_nothrow_move_constructible_v<MetalApi>,"MetalApi move-via-copy must remain noexcept");
#if defined(__arm64__) || defined(__aarch64__)
  static_assert(sizeof(MetalApi)==16,"MetalApi arm64 ABI size changed");
  static_assert(alignof(MetalApi)==8,"MetalApi arm64 ABI alignment changed");
#endif

  TemporaryMetallib fixture;
  ASSERT_TRUE(fixture.valid());

  auto sourceOptions = optionsFor(fixture);
  sourceOptions.swapchainBufferCount = 3;
  sourceOptions.shaderModuleCacheSize = 2;
  sourceOptions.swapchainRenderMode = MetalApi::SwapchainRenderMode::Direct;
  InspectableMetalApi source(ApiFlags::Validation,sourceOptions);
  InspectableMetalApi copied(source);
  InspectableMetalApi assigned;
  assigned = source;

  auto expectRegisteredHit = [&](InspectableMetalApi& api) {
    auto device = api.createTestDevice();
    ASSERT_NE(device,nullptr);
    ASSERT_NE(device->precompiledOptions,nullptr);
    ASSERT_NE(device->precompiledLibraries,nullptr);
    EXPECT_TRUE(device->validation);
    EXPECT_EQ(device->precompiledOptions->swapchainBufferCount,3u);
    EXPECT_EQ(device->precompiledOptions->shaderModuleCacheSize,2u);
    EXPECT_EQ(device->precompiledOptions->swapchainRenderMode,
              MetalApi::SwapchainRenderMode::Direct);
    EXPECT_NE(device->precompiledLibraries->find(fixture.canonicalMsl,testProfile()),nullptr);
    };
  expectRegisteredHit(copied);
  expectRegisteredHit(assigned);

  std::unique_ptr<Detail::MtDevice> survivingDevice;
  {
    InspectableMetalApi scoped(ApiFlags::NoFlags,optionsFor(fixture));
    survivingDevice = scoped.createTestDevice();
  }
  ASSERT_NE(survivingDevice,nullptr);
  ASSERT_NE(survivingDevice->precompiledOptions,nullptr);
  ASSERT_NE(survivingDevice->precompiledLibraries,nullptr);
  EXPECT_NE(survivingDevice->precompiledLibraries->find(fixture.canonicalMsl,testProfile()),nullptr);

  InspectableMetalApi legacy;
  InspectableMetalApi legacyCopy(legacy);
  InspectableMetalApi overwritten(ApiFlags::NoFlags,optionsFor(fixture));
  overwritten = legacy;
  auto expectLegacyFallback = [](InspectableMetalApi& api) {
    auto device = api.createTestDevice();
    ASSERT_NE(device,nullptr);
    EXPECT_EQ(device->precompiledOptions,nullptr);
    EXPECT_EQ(device->precompiledLibraries,nullptr);
    };
  expectLegacyFallback(legacyCopy);
  expectLegacyFallback(overwritten);
#endif
  }

TEST(MetalApi,PrecompiledLibraryMismatchAndDuplicatesFailClosed) {
#if defined(__OSX__) && defined(TEMPEST_TEST_METAL_PRECOMPILED)
  TemporaryMetallib fixture;
  ASSERT_TRUE(fixture.valid());
  auto device = testMetalDevice();
  ASSERT_NE(device,nullptr);

  auto options = optionsFor(fixture);
  Detail::MtPrecompiledLibraries loader(*device,options);
  auto runtime = testProfile();

  auto expectMiss = [&](auto mutate) {
    auto mismatch = runtime;
    mutate(mismatch);
    EXPECT_EQ(loader.find(fixture.canonicalMsl,mismatch),nullptr);
    };
  EXPECT_EQ(loader.find(fixture.canonicalMsl+" ",runtime),nullptr);
  expectMiss([](auto& p){ ++p.schemaVersion; });
  expectMiss([](auto& p){ ++p.mslGeneratorVersion; });
  expectMiss([](auto& p){ p.platform=MetalApi::PrecompiledPlatform::IOSDevice; });
  expectMiss([](auto& p){ p.stage=MetalApi::PrecompiledShaderStage::Fragment; });
  expectMiss([](auto& p){ p.entryPoint="alternate"; });
  expectMiss([](auto& p){ ++p.mslVersion; });
  expectMiss([](auto& p){ p.flipVertY=!p.flipVertY; });
  expectMiss([](auto& p){ ++p.bufferSizeBufferIndex; });
  expectMiss([](auto& p){ ++p.argumentBuffersTier; });
  expectMiss([](auto& p){ p.runtimeArrayRichDescriptor=!p.runtimeArrayRichDescriptor; });
  expectMiss([](auto& p){ p.readWriteTextureFences=!p.readWriteTextureFences; });
  expectMiss([](auto& p){ p.nativeImageAtomics=!p.nativeImageAtomics; });
  expectMiss([](auto& p){ ++p.r32uiLinearTextureAlignment; });
  expectMiss([](auto& p){ ++p.r32uiAlignmentConstantId; });

  auto badKey = optionsFor(fixture);
  ++badKey.precompiledLibraries[0].shaders[0].key[0];
  Detail::MtPrecompiledLibraries badKeyLoader(*device,badKey);
  EXPECT_EQ(badKeyLoader.find(fixture.canonicalMsl,runtime),nullptr);

  auto duplicate = optionsFor(fixture);
  duplicate.precompiledLibraries[0].shaders.push_back(
      duplicate.precompiledLibraries[0].shaders[0]);
  Detail::MtPrecompiledLibraries duplicateLoader(*device,duplicate);
  EXPECT_EQ(duplicateLoader.find(fixture.canonicalMsl,runtime),nullptr);

  auto wrongStage = optionsFor(fixture);
  wrongStage.precompiledLibraries[0].shaders[0].profile.stage =
      MetalApi::PrecompiledShaderStage::Fragment;
  auto& stageEntry = wrongStage.precompiledLibraries[0].shaders[0];
  stageEntry.key = MetalApi::precompiledShaderKey(fixture.canonicalMsl,stageEntry.profile);
  Detail::MtPrecompiledLibraries wrongStageLoader(*device,wrongStage);
  EXPECT_EQ(wrongStageLoader.find(fixture.canonicalMsl,stageEntry.profile),nullptr);

  auto missingEntry = optionsFor(fixture);
  missingEntry.precompiledLibraries[0].shaders[0].profile.entryPoint = "missing_entry";
  auto& missing = missingEntry.precompiledLibraries[0].shaders[0];
  missing.key = MetalApi::precompiledShaderKey(fixture.canonicalMsl,missing.profile);
  Detail::MtPrecompiledLibraries missingEntryLoader(*device,missingEntry);
  EXPECT_EQ(missingEntryLoader.find(fixture.canonicalMsl,missing.profile),nullptr);

  TemporaryMetallib alternateFixture(7,true);
  ASSERT_TRUE(alternateFixture.valid());
  auto alternateProfile = testProfile();
  alternateProfile.entryPoint = "alternate";
  auto alternateOptions = optionsFor(alternateFixture,alternateProfile);
  Detail::MtPrecompiledLibraries alternateLoader(*device,alternateOptions);
  EXPECT_NE(alternateLoader.find(alternateFixture.canonicalMsl,alternateProfile),nullptr);
  EXPECT_EQ(alternateLoader.find(alternateFixture.canonicalMsl,testProfile()),nullptr);
#endif
  }

TEST(MetalApi,PrecompiledLibraryBytesHashRejectsValidSubstitution) {
#if defined(__OSX__) && defined(TEMPEST_TEST_METAL_PRECOMPILED)
  TemporaryMetallib expected(7);
  TemporaryMetallib replacement(9);
  ASSERT_TRUE(expected.valid());
  ASSERT_TRUE(replacement.valid());
  auto device = testMetalDevice();
  ASSERT_NE(device,nullptr);

  auto replacementOptions = optionsFor(replacement);
  Detail::MtPrecompiledLibraries replacementLoader(*device,replacementOptions);
  ASSERT_NE(replacementLoader.find(replacement.canonicalMsl,testProfile()),nullptr);

  auto substituted = optionsFor(expected);
  substituted.precompiledLibraries[0].data = replacement.data;
  Detail::MtPrecompiledLibraries substitutedLoader(*device,substituted);
  EXPECT_EQ(substitutedLoader.find(expected.canonicalMsl,testProfile()),nullptr);
#endif
  }

TEST(MetalApi,PrecompiledLibraryCorruptionFallsBack) {
#if defined(__OSX__) && defined(TEMPEST_TEST_METAL_PRECOMPILED)
  TemporaryMetallib fixture;
  ASSERT_TRUE(fixture.valid());
  auto options = optionsFor(fixture);
  options.precompiledLibraries[0].data.assign({0x01,0x02,0x03,0x04});
  options.precompiledLibraries[0].dataHash = MetalApi::precompiledLibraryHash(
      options.precompiledLibraries[0].data.data(),
      options.precompiledLibraries[0].data.size());

  MetalApi api(ApiFlags::Validation,std::move(options));
  Device device(api);
  EXPECT_NO_THROW({
    auto shader = device.shader("shader/simple_test.comp.sprv");
    auto pipeline = device.pipeline(shader);
    });
#endif
  }

TEST(MetalApi,PrecompiledLibraryConcurrentLookup) {
#if defined(__OSX__) && defined(TEMPEST_TEST_METAL_PRECOMPILED)
  TemporaryMetallib fixture;
  ASSERT_TRUE(fixture.valid());
  auto device = testMetalDevice();
  ASSERT_NE(device,nullptr);
  auto options = optionsFor(fixture);
  Detail::MtPrecompiledLibraries loader(*device,options);

  std::atomic_uint32_t hits = 0;
  std::vector<std::thread> threads;
  for(size_t i=0; i<8; ++i) {
    threads.emplace_back([&]() {
      for(size_t r=0; r<16; ++r) {
        auto function = loader.find(fixture.canonicalMsl,testProfile());
        if(function!=nullptr)
          hits.fetch_add(1,std::memory_order_relaxed);
        }
      });
    }
  for(auto& thread:threads)
    thread.join();
  EXPECT_EQ(hits.load(),8u*16u);

  const auto spirv = readBinary("shader/simple_test.comp.sprv");
  ASSERT_FALSE(spirv.empty());
  MetalApi api(ApiFlags::Validation,optionsFor(fixture));
  Device tempestDevice(api);
  std::atomic_uint32_t shaders = 0;
  threads.clear();
  for(size_t i=0; i<4; ++i) {
    threads.emplace_back([&]() {
      for(size_t r=0; r<4; ++r) {
        auto shader = tempestDevice.shader(spirv.data(),spirv.size());
        shaders.fetch_add(1,std::memory_order_relaxed);
        }
      });
    }
  for(auto& thread:threads)
    thread.join();
  EXPECT_EQ(shaders.load(),4u*4u);
#endif
  }
TEST(MetalApi,MetalApi) {
#if defined(__OSX__)
  GapiTestCommon::init<MetalApi>();
#endif
  }

TEST(MetalApi,SwapchainBufferCountOptions) {
#if defined(__OSX__)
  for(uint32_t count : {0u,2u,3u}) {
    MetalApi::Options options;
    options.swapchainBufferCount = count;
    EXPECT_NO_THROW(MetalApi(ApiFlags::NoFlags,options));
    }

  for(uint32_t count : {1u,4u}) {
    MetalApi::Options options;
    options.swapchainBufferCount = count;
    EXPECT_THROW(MetalApi(ApiFlags::NoFlags,options),std::invalid_argument);
    }
#endif
  }

TEST(MetalApi,ShaderModuleCacheOptions) {
#if defined(__OSX__)
  try {
    MetalApi::Options options;
    options.shaderModuleCacheSize = 1;
    MetalApi api(ApiFlags::Validation,options);
    Device   device(api);

    auto firstVert  = device.shader("shader/simple_test.vert.sprv");
    auto secondVert = device.shader("shader/simple_test.vert.sprv");
    auto frag       = device.shader("shader/simple_test.frag.sprv");
    auto firstPso   = device.pipeline(Topology::Triangles,RenderState(),firstVert,frag);

    // Loading the vertex shader after eviction also verifies that client-held
    // shader modules remain valid when the cache drops its own reference.
    auto thirdVert = device.shader("shader/simple_test.vert.sprv");
    auto secondPso = device.pipeline(Topology::Triangles,RenderState(),thirdVert,frag);
    (void)secondVert;
    (void)firstPso;
    (void)secondPso;
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
#endif
  }

TEST(MetalApi,SpatialScaler) {
#if defined(__OSX__)
  try {
    MetalApi api{ApiFlags::Validation};
    Device   device(api);

    SpatialScalerDesc desc;
    desc.inputFormat  = TextureFormat::R11G11B10UF;
    desc.outputFormat = TextureFormat::R11G11B10UF;
    desc.inputWidth   = 32;
    desc.inputHeight  = 32;
    desc.outputWidth  = 64;
    desc.outputHeight = 64;
    desc.colorMode    = SpatialScalerColorMode::HDR;

    auto scaler = device.spatialScaler(desc);
    if(scaler.isEmpty()) {
      Log::d("Skipping MetalFX spatial scaler testcase: unsupported device or system");
      return;
      }

    auto input  = device.attachment(desc.inputFormat,desc.inputWidth,desc.inputHeight);
    auto output = device.image2d(desc.outputFormat,desc.outputWidth,desc.outputHeight);

    Device otherDevice(api);
    auto otherScaler = otherDevice.spatialScaler(desc);
    auto otherInput  = otherDevice.attachment(desc.inputFormat,desc.inputWidth,desc.inputHeight);
    auto otherOutput = otherDevice.image2d(desc.outputFormat,desc.outputWidth,desc.outputHeight);
    ASSERT_FALSE(otherScaler.isEmpty());
    {
      auto otherCmd = otherDevice.commandBuffer();
      auto enc      = otherCmd.startEncoding(otherDevice);
      EXPECT_FALSE(enc.spatialUpscale(scaler,otherInput,otherOutput));
    }
    {
      auto localCmd = device.commandBuffer();
      auto enc      = localCmd.startEncoding(device);
      EXPECT_FALSE(enc.spatialUpscale(scaler,otherInput,otherOutput));
      EXPECT_FALSE(enc.spatialUpscale(otherScaler,input,output));
    }

    auto cmd    = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer({{input,Vec4(0.25f,0.5f,0.75f,1.f),Tempest::Preserve}});
      EXPECT_TRUE(enc.spatialUpscale(scaler,input,output));
    }

    auto sync = device.submit(cmd);
    sync.wait();
    auto result = device.readPixels(output);
    EXPECT_EQ(result.w(),desc.outputWidth);
    EXPECT_EQ(result.h(),desc.outputHeight);
    ASSERT_EQ(result.format(),TextureFormat::R11G11B10UF);
    ASSERT_EQ(result.dataSize(),size_t(desc.outputWidth)*desc.outputHeight*sizeof(uint32_t));

    const auto* pixels = reinterpret_cast<const uint32_t*>(result.data());
    double average[3] = {};
    for(size_t i=0; i<size_t(result.w())*result.h(); ++i) {
      average[0] += unpackUnsignedFloat(pixels[i]       & 0x7FFu,6);
      average[1] += unpackUnsignedFloat(pixels[i] >> 11 & 0x7FFu,6);
      average[2] += unpackUnsignedFloat(pixels[i] >> 22 & 0x3FFu,5);
      }
    const double pixelCount = double(result.w())*result.h();
    EXPECT_NEAR(average[0]/pixelCount,0.25,0.03);
    EXPECT_NEAR(average[1]/pixelCount,0.50,0.03);
    EXPECT_NEAR(average[2]/pixelCount,0.75,0.03);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping MetalFX spatial scaler testcase: ", e.what()); else
      throw;
    }
#endif
  }

TEST(MetalApi,TemporalScalerDescriptorValidation) {
#if defined(__OSX__)
  try {
    MetalApi api;
    Device   device(api);

    TemporalScalerDesc desc;
    desc.inputFormat  = TextureFormat::RGBA16F;
    desc.depthFormat  = TextureFormat::Depth32F;
    desc.motionFormat = TextureFormat::RG32F;
    desc.outputFormat = TextureFormat::RGBA16F;
    desc.inputWidth   = 64;
    desc.inputHeight  = 64;
    desc.outputWidth  = 128;
    desc.outputHeight = 128;

    auto invalid = desc;
    invalid.inputFormat = TextureFormat::Undefined;
    EXPECT_TRUE(device.temporalScaler(invalid).isEmpty());
    invalid = desc;
    invalid.depthFormat = TextureFormat::Undefined;
    EXPECT_TRUE(device.temporalScaler(invalid).isEmpty());
    invalid = desc;
    invalid.motionFormat = TextureFormat::Undefined;
    EXPECT_TRUE(device.temporalScaler(invalid).isEmpty());
    invalid = desc;
    invalid.outputFormat = TextureFormat::Undefined;
    EXPECT_TRUE(device.temporalScaler(invalid).isEmpty());

    invalid = desc;
    invalid.inputWidth = 0;
    EXPECT_TRUE(device.temporalScaler(invalid).isEmpty());
    invalid = desc;
    invalid.inputHeight = 0;
    EXPECT_TRUE(device.temporalScaler(invalid).isEmpty());
    invalid = desc;
    invalid.outputWidth = 0;
    EXPECT_TRUE(device.temporalScaler(invalid).isEmpty());
    invalid = desc;
    invalid.outputHeight = 0;
    EXPECT_TRUE(device.temporalScaler(invalid).isEmpty());

    invalid = desc;
    invalid.inputWidth = device.properties().tex2d.maxSize+1;
    EXPECT_TRUE(device.temporalScaler(invalid).isEmpty());
    invalid = desc;
    invalid.outputHeight = device.properties().tex2d.maxSize+1;
    EXPECT_TRUE(device.temporalScaler(invalid).isEmpty());

    if(metalFxTemporalSupported()) {
      auto manualExposure = desc;
      manualExposure.autoExposure = false;
      EXPECT_FALSE(device.temporalScaler(manualExposure).isEmpty());
      }
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping MetalFX temporal descriptor testcase: ", e.what()); else
      throw;
    }
#endif
  }

TEST(MetalApi,TemporalScaler) {
#if defined(__OSX__)
  try {
    MetalApi api{ApiFlags::Validation};
    Device   device(api);

    TemporalScalerDesc desc;
    desc.inputFormat  = TextureFormat::RGBA16F;
    desc.depthFormat  = TextureFormat::Depth32F;
    desc.motionFormat = TextureFormat::RG32F;
    desc.outputFormat = TextureFormat::RGBA16F;
    desc.inputWidth   = 64;
    desc.inputHeight  = 64;
    desc.outputWidth  = 128;
    desc.outputHeight = 128;
    desc.autoExposure = true;

    if(!metalFxTemporalSupported()) {
      Log::d("Skipping MetalFX temporal scaler testcase: unsupported device or system");
      return;
      }
    auto scaler = device.temporalScaler(desc);
    ASSERT_FALSE(scaler.isEmpty());

    auto input       = device.attachment(desc.inputFormat,desc.inputWidth,desc.inputHeight);
    auto depth       = device.zbuffer(desc.depthFormat,desc.inputWidth,desc.inputHeight);
    auto motion      = device.attachment(desc.motionFormat,desc.inputWidth,desc.inputHeight);
    auto output      = device.image2d(desc.outputFormat,desc.outputWidth,desc.outputHeight);
    auto wrongInput  = device.attachment(desc.inputFormat,desc.inputWidth/2,desc.inputHeight);
    auto wrongDepth  = device.zbuffer(desc.depthFormat,desc.inputWidth,desc.inputHeight/2);
    auto wrongMotion = device.attachment(desc.motionFormat,desc.inputWidth/2,desc.inputHeight);
    auto wrongOutput = device.image2d(desc.outputFormat,desc.outputWidth/2,desc.outputHeight);
    auto wrongInputFormat  = device.attachment(TextureFormat::R11G11B10UF,desc.inputWidth,desc.inputHeight);
    auto wrongDepthFormat  = device.zbuffer(TextureFormat::Depth16,desc.inputWidth,desc.inputHeight);
    auto wrongMotionFormat = device.attachment(TextureFormat::RGBA16F,desc.inputWidth,desc.inputHeight);
    auto wrongOutputFormat = device.image2d(TextureFormat::R11G11B10UF,desc.outputWidth,desc.outputHeight);

    Device otherDevice(api);
    auto otherScaler = otherDevice.temporalScaler(desc);
    auto otherInput  = otherDevice.attachment(desc.inputFormat,desc.inputWidth,desc.inputHeight);
    auto otherDepth  = otherDevice.zbuffer(desc.depthFormat,desc.inputWidth,desc.inputHeight);
    auto otherMotion = otherDevice.attachment(desc.motionFormat,desc.inputWidth,desc.inputHeight);
    auto otherOutput = otherDevice.image2d(desc.outputFormat,desc.outputWidth,desc.outputHeight);
    ASSERT_FALSE(otherScaler.isEmpty());
    {
      auto otherCmd = otherDevice.commandBuffer();
      auto enc      = otherCmd.startEncoding(otherDevice);
      EXPECT_FALSE(enc.temporalUpscale(scaler,otherInput,otherDepth,otherMotion,otherOutput,{}));
    }

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      EXPECT_FALSE(enc.temporalUpscale(scaler,wrongInput,depth,motion,output,{}));
      EXPECT_FALSE(enc.temporalUpscale(scaler,input,wrongDepth,motion,output,{}));
      EXPECT_FALSE(enc.temporalUpscale(scaler,input,depth,wrongMotion,output,{}));
      EXPECT_FALSE(enc.temporalUpscale(scaler,input,depth,motion,wrongOutput,{}));
      EXPECT_FALSE(enc.temporalUpscale(scaler,wrongInputFormat,depth,motion,output,{}));
      EXPECT_FALSE(enc.temporalUpscale(scaler,input,wrongDepthFormat,motion,output,{}));
      EXPECT_FALSE(enc.temporalUpscale(scaler,input,depth,wrongMotionFormat,output,{}));
      EXPECT_FALSE(enc.temporalUpscale(scaler,input,depth,motion,wrongOutputFormat,{}));
      EXPECT_FALSE(enc.temporalUpscale(scaler,otherInput,otherDepth,otherMotion,otherOutput,{}));
      EXPECT_FALSE(enc.temporalUpscale(otherScaler,input,depth,motion,output,{}));

      enc.setFramebuffer({{input,Vec4(0.25f,0.5f,0.75f,1.f),Tempest::Preserve},
                          {motion,Vec4(1.f/float(desc.inputWidth),
                                      -1.f/float(desc.inputHeight),0.f,0.f),Tempest::Preserve}},
                         {depth,0.75f,Tempest::Preserve});

      TemporalScalerArgs args;
      args.jitterOffsetX      = 0.25f;
      args.jitterOffsetY      = -0.25f;
      args.motionVectorScaleX = float(desc.inputWidth);
      args.motionVectorScaleY = float(desc.inputHeight);
      args.resetHistory       = true;
      args.depthReversed      = false;
      EXPECT_TRUE(enc.temporalUpscale(scaler,input,depth,motion,output,args));
    }

    auto sync = device.submit(cmd);
    sync.wait();
    auto result = device.readPixels(output);
    EXPECT_EQ(result.w(),desc.outputWidth);
    EXPECT_EQ(result.h(),desc.outputHeight);
    ASSERT_EQ(result.format(),desc.outputFormat);
    ASSERT_EQ(result.dataSize(),size_t(desc.outputWidth)*desc.outputHeight*4*sizeof(uint16_t));
    const auto firstAverage = averageRgba16F(result);
    for(auto component:firstAverage)
      EXPECT_TRUE(std::isfinite(component));
    EXPECT_NEAR(firstAverage[0],0.25,0.12);
    EXPECT_NEAR(firstAverage[1],0.50,0.12);
    EXPECT_NEAR(firstAverage[2],0.75,0.12);
    EXPECT_NEAR(firstAverage[3],1.00,0.08);

    TemporalScalerArgs secondArgs;
    secondArgs.jitterOffsetX      = -0.125f;
    secondArgs.jitterOffsetY      = 0.375f;
    secondArgs.motionVectorScaleX = float(desc.inputWidth);
    secondArgs.motionVectorScaleY = float(desc.inputHeight);
    secondArgs.resetHistory       = false;
    secondArgs.depthReversed      = true;
    auto secondCmd = device.commandBuffer();
    {
      auto enc = secondCmd.startEncoding(device);
      enc.setFramebuffer({{input,Vec4(0.75f,0.25f,0.5f,1.f),Tempest::Preserve},
                          {motion,Vec4(-1.f/float(desc.inputWidth),
                                      1.f/float(desc.inputHeight),0.f,0.f),Tempest::Preserve}},
                         {depth,0.6f,Tempest::Preserve});
      EXPECT_TRUE(enc.temporalUpscale(scaler,input,depth,motion,output,secondArgs));
    }
    auto secondSync = device.submit(secondCmd);
    secondSync.wait();
    auto secondResult = device.readPixels(output);
    ASSERT_EQ(secondResult.format(),desc.outputFormat);
    const auto secondAverage = averageRgba16F(secondResult);
    for(auto component:secondAverage)
      EXPECT_TRUE(std::isfinite(component));
    EXPECT_NEAR(secondAverage[0],0.75,0.15);
    EXPECT_NEAR(secondAverage[1],0.25,0.15);
    EXPECT_NEAR(secondAverage[2],0.50,0.15);
    EXPECT_NEAR(secondAverage[3],1.00,0.08);

    std::array<CommandBuffer,2> parallelCmd = {
      device.commandBuffer(),device.commandBuffer()
      };
    std::array<StorageImage,2> parallelOutput = {
      device.image2d(desc.outputFormat,desc.outputWidth,desc.outputHeight),
      device.image2d(desc.outputFormat,desc.outputWidth,desc.outputHeight)
      };
    std::array<bool,2> encoded = {};
    std::array<std::thread,2> workers;
    for(size_t i=0; i<workers.size(); ++i) {
      workers[i] = std::thread([&,i]() {
        TemporalScalerArgs args = secondArgs;
        args.jitterOffsetX = i==0 ? 0.125f : -0.375f;
        args.jitterOffsetY = i==0 ? -0.25f : 0.25f;
        args.resetHistory  = true;
        auto enc = parallelCmd[i].startEncoding(device);
        encoded[i] = enc.temporalUpscale(scaler,input,depth,motion,parallelOutput[i],args);
        });
      }
    for(auto& worker:workers)
      worker.join();
    for(bool value:encoded)
      ASSERT_TRUE(value);
    for(auto& parallel:parallelCmd) {
      auto parallelSync = device.submit(parallel);
      parallelSync.wait();
      }
    for(auto& parallel:parallelOutput) {
      const auto parallelResult  = device.readPixels(parallel);
      const auto parallelAverage = averageRgba16F(parallelResult);
      for(auto component:parallelAverage)
        EXPECT_TRUE(std::isfinite(component));
      EXPECT_GT(parallelAverage[0]+parallelAverage[1]+parallelAverage[2],0.5);
      EXPECT_NEAR(parallelAverage[3],1.0,0.08);
      }
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping MetalFX temporal scaler testcase: ", e.what()); else
      throw;
    }
#endif
  }

TEST(MetalApi,SwapchainRenderModeOptions) {
  MetalApi::Options defaults;
  EXPECT_EQ(defaults.swapchainRenderMode,MetalApi::SwapchainRenderMode::Copy);

  MetalApi::Options direct;
  direct.swapchainRenderMode = MetalApi::SwapchainRenderMode::Direct;
#if defined(__OSX__)
  EXPECT_NO_THROW(MetalApi(ApiFlags::NoFlags,direct));

  MetalApi::Options invalid;
  invalid.swapchainRenderMode = static_cast<MetalApi::SwapchainRenderMode>(255);
  EXPECT_THROW(MetalApi(ApiFlags::NoFlags,invalid),std::invalid_argument);
#endif
  }

TEST(MetalApi,SwapchainCopyAndDirectSmoke) {
#if defined(__OSX__)
  Application app;
  swapchainSmoke(MetalApi::SwapchainRenderMode::Copy);
  swapchainSmoke(MetalApi::SwapchainRenderMode::Direct);
#endif
  }

TEST(MetalApi,Vbo) {
#if defined(__OSX__)
  GapiTestCommon::Vbo<MetalApi>();
#endif
  }

TEST(MetalApi,VboInit) {
#if defined(__OSX__)
  GapiTestCommon::VboInit<MetalApi>();
#endif
  }

TEST(MetalApi,VboDyn) {
#if defined(__OSX__)
  GapiTestCommon::VboDyn<MetalApi>();
#endif
  }

TEST(MetalApi,SsboCopy) {
#if defined(__OSX__)
  GapiTestCommon::SsboCopy<MetalApi,TextureFormat::RGBA8,uint8_t>();
#endif
  }

TEST(MetalApi,SsboEmpty) {
#if defined(__OSX__)
  GapiTestCommon::SsboEmpty<MetalApi>();
#endif
  }

TEST(MetalApi,ArrayLength) {
#if defined(__OSX__)
  GapiTestCommon::ArrayLength<MetalApi>();
#endif
  }

TEST(MetalApi,NonSampledTexture) {
#if defined(__OSX__)
  GapiTestCommon::NonSampledTexture<MetalApi>("MetalApi_NonSampledTexture.png");
#endif
  }

TEST(MetalApi,Shader) {
#if defined(__OSX__)
  GapiTestCommon::Shader<MetalApi>();
#endif
  }

TEST(MetalApi,Pso) {
#if defined(__OSX__)
  GapiTestCommon::Pso<MetalApi>();
#endif
  }

TEST(MetalApi,PsoInconsistentVaryings) {
#if defined(__OSX__)
  GapiTestCommon::PsoInconsistentVaryings<MetalApi>();
#endif
  }

TEST(MetalApi,Fbo) {
#if defined(__OSX__)
  GapiTestCommon::Fbo<MetalApi>("MetalApi_Fbo.png");
#endif
  }

TEST(MetalApi,Draw) {
#if defined(__OSX__)
  GapiTestCommon::Draw<MetalApi,TextureFormat::RGBA8>  ("MetalApi_Draw_RGBA8.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RGB8>   ("MetalApi_Draw_RGB8.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RG8>    ("MetalApi_Draw_RG8.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::R8>     ("MetalApi_Draw_R8.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RGBA16> ("MetalApi_Draw_RGBA16.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RGB16>  ("MetalApi_Draw_RGB16.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RG16>   ("MetalApi_Draw_RG16.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::R16>    ("MetalApi_Draw_R16.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RGBA32F>("MetalApi_Draw_RGBA32F.hdr");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RGB32F> ("MetalApi_Draw_RGB32F.hdr");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RG32F>  ("MetalApi_Draw_RG32F.hdr");
  GapiTestCommon::Draw<MetalApi,TextureFormat::R32F>   ("MetalApi_Draw_R32F.hdr");
#endif
  }

TEST(MetalApi,RG16F) {
#if defined(__OSX__)
  GapiTestCommon::FormatRG16F<MetalApi>();
#endif
  }

TEST(MetalApi,DepthWrite) {
#if defined(__OSX__)
  GapiTestCommon::DepthWrite<MetalApi>("MetalApi_DepthWrite.png");
#endif
  }

TEST(MetalApi,InstanceIndex) {
#if defined(__OSX__)
  GapiTestCommon::InstanceIndex<MetalApi>("MetalApi_InstanceIndex.png");
#endif
  }

TEST(MetalApi,Viewport) {
#if defined(__OSX__)
  GapiTestCommon::Viewport<MetalApi>("MetalApi_Viewport.png");
#endif
  }

TEST(MetalApi,Uniforms) {
#if defined(__OSX__)
  GapiTestCommon::Uniforms<MetalApi>("MetalApi_Uniforms_UBO.png", true);
  GapiTestCommon::Uniforms<MetalApi>("MetalApi_Uniforms_SSBO.png",false);
#endif
  }

TEST(MetalApi,SsboOverlap) {
#if defined(__OSX__)
  GapiTestCommon::SsboOverlap<MetalApi>();
#endif
  }

TEST(MetalApi,Compute) {
#if defined(__OSX__)
  GapiTestCommon::Compute<MetalApi>();
#endif
  }

TEST(MetalApi,ComputeImage) {
#if defined(__OSX__)
  GapiTestCommon::ComputeImage<MetalApi>("MetalApi_ComputeImage.png");
#endif
  }

TEST(MetalApi,AtomicImage) {
#if defined(__OSX__)
  GapiTestCommon::AtomicImage<MetalApi>("MetalApi_AtomicImage.png");
#endif
  }

TEST(MetalApi,AtomicImage3D) {
#if defined(__OSX__)
  GapiTestCommon::AtomicImage3D<MetalApi>("MetalApi_AtomicImage3D.png");
#endif
  }

TEST(MetalApi,MipMaps) {
#if defined(__OSX__)
  GapiTestCommon::MipMaps<MetalApi,TextureFormat::RGBA8>  ("MetalApi_MipMaps_RGBA8.png");
  GapiTestCommon::MipMaps<MetalApi,TextureFormat::RGBA16> ("MetalApi_MipMaps_RGBA16.png");
  GapiTestCommon::MipMaps<MetalApi,TextureFormat::RGBA32F>("MetalApi_MipMaps_RGBA32.png");
#endif
  }

TEST(MetalApi,S3TC) {
#if defined(__OSX__)
  try {
    MetalApi api{ApiFlags::Validation};
    Device       device(api);

    auto tex = device.texture("assets/pixmap_io/dxt5.dds");
    EXPECT_EQ(tex.format(),TextureFormat::DXT5);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping directx testcase: ", e.what()); else
      throw;
    }
#endif
  }

TEST(MetalApi,PsoTess) {
#if defined(__OSX__)
  GapiTestCommon::PsoTess<MetalApi>();
#endif
  }

TEST(MetalApi,DISABLED_TesselationBasic) {
#if defined(__OSX__)
  GapiTestCommon::TesselationBasic<MetalApi>("MetalApi_TesselationBasic.png");
#endif
  }

TEST(MetalApi,SsboWrite) {
#if defined(__OSX__)
  GapiTestCommon::SsboWrite<MetalApi>();
#endif
  }

TEST(MetalApi,UnboundSsbo) {
#if defined(__OSX__)
  GapiTestCommon::UnboundSsbo<MetalApi>();
#endif
  }

TEST(MetalApi,ComponentSwizzle) {
#if defined(__OSX__)
  GapiTestCommon::ComponentSwizzle<MetalApi>();
#endif
  }

TEST(MetalApi,PushRemapping) {
#if defined(__OSX__)
  GapiTestCommon::PushRemapping<MetalApi>();
#endif
  }

TEST(MetalApi,Bindless) {
#if defined(__OSX__)
  GapiTestCommon::Bindless<MetalApi>("MetalApi_Bindless.png");
#endif
  }

TEST(MetalApi,Bindless2) {
#if defined(__OSX__)
  GapiTestCommon::Bindless2<MetalApi>("MetalApi_Bindless2.png");
#endif
  }

TEST(MetalApi,UnusedDescriptor) {
#if defined(__OSX__)
  GapiTestCommon::UnusedDescriptor<MetalApi>("MetalApi_UnusedDescriptor.png");
#endif
  }

TEST(MetalApi,Blas) {
#if defined(__OSX__)
  GapiTestCommon::Blas<MetalApi>();
#endif
  }

TEST(MetalApi,RayQuery) {
#if defined(__OSX__)
  GapiTestCommon::RayQuery<MetalApi>("MetalApi_RayQuery.png");
#endif
  }

TEST(MetalApi,DISABLED_RayQueryFace) {
#if defined(__OSX__)
  GapiTestCommon::RayQueryFace<MetalApi>("MetalApi_RayQueryFace.png");
#endif
  }

TEST(MetalApi,DISABLED_MeshShader) {
#if defined(__OSX__)
  GapiTestCommon::MeshShader<MetalApi>("MetalApi_MeshShader.png");
#endif
  }

TEST(MetalApi,DispathIndirect) {
#if defined(__OSX__)
  GapiTestCommon::DispathIndirect<MetalApi>();
#endif
  }
