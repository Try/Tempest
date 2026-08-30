#if defined(TEMPEST_BUILD_METAL)

#include "metalapi.h"

#if __has_feature(objc_arc)
#error "Objective C++ ARC is not supported"
#endif

#include <Tempest/Log>
#include <Tempest/Pixmap>

#include "gapi/metal/mtdevice.h"
#include "gapi/metal/mtbuffer.h"
#include "gapi/metal/mtshader.h"
#include "gapi/metal/mtpipeline.h"
#include "gapi/metal/mtcommandbuffer.h"
#include "gapi/metal/mttexture.h"
#include "gapi/metal/mtpipelinelay.h"
#include "gapi/metal/mtdescriptorarray.h"
#include "gapi/metal/mtsync.h"
#include "gapi/metal/mtswapchain.h"
#include "gapi/metal/mtaccelerationstructure.h"
#include "gapi/metal/mtmetalfx.h"
#include "gapi/metal/mtsha256.h"

#include <Metal/Metal.hpp>

#include <atomic>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

using namespace Tempest;
using namespace Tempest::Detail;

namespace {

void appendU8(MtSha256& hash, uint8_t value) {
  hash.update(&value,sizeof(value));
  }

void appendU32(MtSha256& hash, uint32_t value) {
  uint8_t data[4] = {uint8_t(value),uint8_t(value>>8u),uint8_t(value>>16u),uint8_t(value>>24u)};
  hash.update(data,sizeof(data));
  }

void appendU64(MtSha256& hash, uint64_t value) {
  uint8_t data[8] = {};
  for(size_t i=0; i<8; ++i)
    data[i] = uint8_t(value>>(i*8u));
  hash.update(data,sizeof(data));
  }

void appendString(MtSha256& hash, std::string_view value) {
  appendU64(hash,value.size());
  hash.update(value);
  }

struct MetalApiOptionsRegistry final {
  std::mutex mutex;
  std::unordered_map<const MetalApi*,std::shared_ptr<const MetalApi::Options>> entries;
  };

std::atomic<MetalApiOptionsRegistry*> publishedOptionsRegistry{nullptr};

MetalApiOptionsRegistry& ensureRegistry() {
  // Only the opt-in Options constructor reaches this allocation. The process-
  // lifetime registry keeps destruction of global MetalApi objects safe.
  static auto* instance = []() {
    auto* value = new MetalApiOptionsRegistry;
    publishedOptionsRegistry.store(value,std::memory_order_release);
    return value;
    }();
  return *instance;
  }

MetalApiOptionsRegistry* tryGetRegistry() noexcept {
  return publishedOptionsRegistry.load(std::memory_order_acquire);
  }

bool enableValidation(ApiFlags f) {
  if((f & ApiFlags::Validation)!=ApiFlags::Validation)
    return false;
  setenv("METAL_DEVICE_WRAPPER_TYPE","1",1);
  setenv("METAL_DEBUG_ERROR_MODE",   "5",0);
  setenv("METAL_ERROR_MODE",         "5",0);
  return true;
  }

void validateOptions(const MetalApi::Options& options) {
  if(options.swapchainBufferCount!=0 &&
     options.swapchainBufferCount!=2 && options.swapchainBufferCount!=3)
    throw std::invalid_argument("Metal swapchain buffer count must be 0, 2, or 3");
  if(options.swapchainRenderMode!=MetalApi::SwapchainRenderMode::Copy &&
     options.swapchainRenderMode!=MetalApi::SwapchainRenderMode::Direct)
    throw std::invalid_argument("Unknown Metal swapchain render mode");
  }

void registerOptions(const MetalApi* api, const MetalApi::Options& options) {
  auto value = std::make_shared<const MetalApi::Options>(options);
  auto& registry = ensureRegistry();
  std::lock_guard<std::mutex> guard(registry.mutex);
  registry.entries.insert_or_assign(api,std::move(value));
  }

void eraseRegistrationBestEffort(MetalApiOptionsRegistry& registry,
                                 const MetalApi* api) noexcept {
  try {
    std::lock_guard<std::mutex> guard(registry.mutex);
    registry.entries.erase(api);
    }
  catch(...) {
    }
  }

void copyOptionsRegistration(const MetalApi* destination,
                             const MetalApi* source) noexcept {
  if(destination==source)
    return;
  auto* registry = tryGetRegistry();
  if(registry==nullptr)
    return;
  try {
    std::lock_guard<std::mutex> guard(registry->mutex);
    const auto found = registry->entries.find(source);
    if(found==registry->entries.end()) {
      registry->entries.erase(destination);
      return;
      }
    auto value = found->second;
    registry->entries.insert_or_assign(destination,std::move(value));
    }
  catch(...) {
    eraseRegistrationBestEffort(*registry,destination);
    }
  }

std::shared_ptr<const MetalApi::Options> registeredOptions(const MetalApi* api) noexcept {
  auto* registry = tryGetRegistry();
  if(registry==nullptr)
    return {};
  try {
    std::lock_guard<std::mutex> guard(registry->mutex);
    const auto found = registry->entries.find(api);
    if(found==registry->entries.end())
      return {};
    return found->second;
    }
  catch(...) {
    return {};
    }
  }

void unregisterOptions(const MetalApi* api) noexcept {
  auto* registry = tryGetRegistry();
  if(registry==nullptr)
    return;
  eraseRegistrationBestEffort(*registry,api);
  }

}

struct Tempest::Detail::MetalApiAbiProbe final {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#endif
  static constexpr size_t validationOffset = __builtin_offsetof(MetalApi,validation);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
  };

#if defined(__arm64__) || defined(__aarch64__)
static_assert(sizeof(MetalApi)==16,"MetalApi arm64 ABI size changed");
static_assert(alignof(MetalApi)==8,"MetalApi arm64 ABI alignment changed");
static_assert(Detail::MetalApiAbiProbe::validationOffset==8,
              "MetalApi validation bool moved from its legacy arm64 offset");
#endif

MetalApi::MetalApi(ApiFlags f)
  :validation(enableValidation(f)) {
  }

MetalApi::MetalApi(ApiFlags f, const Options& options) {
  validateOptions(options);
  validation = enableValidation(f);
  registerOptions(this,options);
  }

MetalApi::MetalApi(const MetalApi& other) noexcept
  :validation(other.validation) {
  copyOptionsRegistration(this,&other);
  }

MetalApi& MetalApi::operator=(const MetalApi& other) noexcept {
  if(this==&other)
    return *this;
  copyOptionsRegistration(this,&other);
  validation = other.validation;
  return *this;
  }

MetalApi::~MetalApi() {
  unregisterOptions(this);
  }

MetalApi::PrecompiledShaderKey MetalApi::precompiledShaderKey(std::string_view canonicalMsl,
                                                              const PrecompiledShaderProfile& profile) {
  static constexpr std::string_view domain = "Tempest.Metal.PrecompiledShader";
  MtSha256 hash;
  hash.update(domain);
  appendU8(hash,0);
  appendU32(hash,profile.schemaVersion);
  appendU64(hash,canonicalMsl.size());
  hash.update(canonicalMsl);
  appendU32(hash,profile.mslGeneratorVersion);
  appendU8(hash,uint8_t(profile.platform));
  appendU8(hash,uint8_t(profile.stage));
  appendString(hash,profile.entryPoint);
  appendU32(hash,profile.mslVersion);
  appendU8(hash,profile.flipVertY ? 1 : 0);
  appendU32(hash,profile.bufferSizeBufferIndex);
  appendU8(hash,profile.argumentBuffersTier);
  appendU8(hash,profile.runtimeArrayRichDescriptor ? 1 : 0);
  appendU8(hash,profile.readWriteTextureFences ? 1 : 0);
  appendU8(hash,profile.nativeImageAtomics ? 1 : 0);
  appendU32(hash,profile.r32uiLinearTextureAlignment);
  appendU32(hash,profile.r32uiAlignmentConstantId);
  return hash.finalize();
  }

MetalApi::PrecompiledLibraryHash MetalApi::precompiledLibraryHash(const void* data,
                                                                  size_t size) {
  if(data==nullptr && size!=0)
    return {};
  return MtSha256::hash(data,size);
  }

#if !defined(TEMPEST_BUILD_METALFX)
AbstractGraphicsApi::SpatialScaler*
  MetalApi::createSpatialScaler(AbstractGraphicsApi::Device*, const SpatialScalerDesc&) {
  return nullptr;
  }
#endif

#if !defined(TEMPEST_BUILD_METALFX_TEMPORAL)
AbstractGraphicsApi::TemporalScaler*
  MetalApi::createTemporalScaler(AbstractGraphicsApi::Device*, const TemporalScalerDesc&) {
  return nullptr;
  }
#endif

std::vector<AbstractGraphicsApi::Props> MetalApi::devices() const {
#if defined(__OSX__)
  auto dev = MTL::CopyAllDevices();
  try {
    std::vector<AbstractGraphicsApi::Props> p(dev->count());
    for(size_t i=0; i<p.size(); ++i) {
      MtDevice::deductProps(p[i],*reinterpret_cast<MTL::Device*>(dev->object(i)));
      }
    dev->release();
    return p;
    }
  catch(...) {
    dev->release();
    throw;
    }
#else
  std::vector<AbstractGraphicsApi::Props> p(1);
  auto     dev    = NsPtr<MTL::Device>(MTL::CreateSystemDefaultDevice());
  uint32_t mslVer = 0;
  MtDevice::deductProps(p[0],*dev);
  return p;
#endif
  }

AbstractGraphicsApi::Device* MetalApi::createDevice(std::string_view gpuName) {
  auto options = registeredOptions(this);
  return new MtDevice(gpuName,validation,std::move(options));
  }

AbstractGraphicsApi::Swapchain *MetalApi::createSwapchain(SystemApi::Window *w,
                                                          AbstractGraphicsApi::Device* d) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  auto options = registeredOptions(this);
  const uint32_t bufferCount = options!=nullptr ? options->swapchainBufferCount : 0;
  const auto renderMode = options!=nullptr ? options->swapchainRenderMode : SwapchainRenderMode::Copy;
  return new MtSwapchain(dev,w,bufferCount,renderMode==SwapchainRenderMode::Direct);
  }

AbstractGraphicsApi::PPipeline MetalApi::createPipeline(AbstractGraphicsApi::Device *d,
                                                        const RenderState &st,
                                                        Topology tp,
                                                        const AbstractGraphicsApi::Shader*const* sh,
                                                        size_t cnt) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  const Detail::MtShader* shader[5] = {};
  for(size_t i=0; i<cnt; ++i)
    shader[i] = reinterpret_cast<const Detail::MtShader*>(sh[i]);
  return PPipeline(new MtPipeline(dx,tp,st,shader,cnt));
  }

AbstractGraphicsApi::PCompPipeline MetalApi::createComputePipeline(AbstractGraphicsApi::Device *d,
                                                                   AbstractGraphicsApi::Shader *cs) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  auto& cx = *reinterpret_cast<const MtShader*>(cs);
  return PCompPipeline(new MtCompPipeline(dx,cx));
  }

AbstractGraphicsApi::PShader MetalApi::createShader(AbstractGraphicsApi::Device *d, const void *source, size_t src_size) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  return dx.shaderModules.getOrCreate(source,src_size,[&dx,source,src_size]() {
    return PShader(new MtShader(dx,source,src_size));
    });
  }

AbstractGraphicsApi::PBuffer MetalApi::createBuffer(AbstractGraphicsApi::Device *d, const void *mem, size_t size,
                                                    MemUsage usage, BufferHeap flg) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);

  MTL::ResourceOptions opt = 0;
  // https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/MTLBestPracticesGuide/ResourceOptions.html#//apple_ref/doc/uid/TP40016642-CH17-SW1
  // https://developer.apple.com/documentation/metal/choosing-a-resource-storage-mode-for-intel-and-amd-gpus?language=objc
  switch(flg) {
    case BufferHeap::Device:
      opt |= MTL::ResourceStorageModePrivate;
      break;
    case BufferHeap::Upload: {
      opt |= hostVisibleResourceOptions(*dx.impl);
      opt |= MTL::ResourceCPUCacheModeWriteCombined;
      break;
      }
    case BufferHeap::Readback:
      opt |= hostVisibleResourceOptions(*dx.impl);
      opt |= MTL::ResourceCPUCacheModeDefaultCache;
      break;
    }

  return PBuffer(new MtBuffer(dx,mem,size,opt));
  }

AbstractGraphicsApi::PTexture MetalApi::createTexture(AbstractGraphicsApi::Device *d,
                                                      const Pixmap &p, TextureFormat frm, uint32_t mips) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return PTexture(new MtTexture(dev,p,mips,frm));
  }

AbstractGraphicsApi::PTexture MetalApi::createTexture(AbstractGraphicsApi::Device *d,
                                                      const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return PTexture(new MtTexture(dev,w,h,0,mips,frm,false));
  }

AbstractGraphicsApi::PTexture MetalApi::createStorage(AbstractGraphicsApi::Device *d,
                                                      const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return PTexture(new MtTexture(dev,w,h,0,mips,frm,true));
  }

AbstractGraphicsApi::PTexture MetalApi::createStorage(Device* d,
                                                      const uint32_t w, const uint32_t h, const uint32_t depth, uint32_t mips,
                                                      TextureFormat frm) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return PTexture(new MtTexture(dev,w,h,depth,mips,frm,true));
  }

AbstractGraphicsApi::AccelerationStructure* MetalApi::createBottomAccelerationStruct(Device* d, const RtGeometry* geom, size_t size) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  // auto& ix  = *reinterpret_cast<MtBuffer*>(ibo);
  // auto& vx  = *reinterpret_cast<MtBuffer*>(vbo);
  return new MtAccelerationStructure(dev, geom, size);
  }

AbstractGraphicsApi::AccelerationStructure* MetalApi::createTopAccelerationStruct(Device* d, const RtInstance* inst, AccelerationStructure*const* as, size_t size) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return new MtTopAccelerationStructure(dev,inst,as,size);
  }

AbstractGraphicsApi::DescArray* MetalApi::createDescriptors(Device* d, Texture** tex, size_t cnt, uint32_t mipLevel) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return new MtDescriptorArray(dev,tex,cnt,mipLevel);
  }

AbstractGraphicsApi::DescArray* MetalApi::createDescriptors(Device* d, Texture** tex, size_t cnt, uint32_t mipLevel,
                                                            const Sampler& smp) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return new MtDescriptorArray(dev,tex,cnt,mipLevel,smp);
  }

AbstractGraphicsApi::DescArray* MetalApi::createDescriptors(Device* d, Buffer** buf, size_t cnt) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return new MtDescriptorArray(dev,buf,cnt);
  }

void MetalApi::readPixels(AbstractGraphicsApi::Device*,
                          Pixmap& out, const AbstractGraphicsApi::PTexture t,
                          TextureFormat frm, const uint32_t w, const uint32_t h, uint32_t mip, bool storageImg) {
  auto& tx = *reinterpret_cast<MtTexture*>(t.handler);
  tx.readPixels(out,frm,w,h,mip);
  }

void MetalApi::readBytes(AbstractGraphicsApi::Device*, AbstractGraphicsApi::Buffer *buf,
                         void *out, size_t size) {
  buf->read(out,0,size);
  }

AbstractGraphicsApi::CommandBuffer *MetalApi::createCommandBuffer(AbstractGraphicsApi::Device *d) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  return new MtCommandBuffer(dx);
  }

void MetalApi::present(AbstractGraphicsApi::Device*, AbstractGraphicsApi::Swapchain *sw) {
  auto& s   = *reinterpret_cast<MtSwapchain*>(sw);
  s.present();
  }

std::shared_ptr<AbstractGraphicsApi::Fence> MetalApi::submit(Device* d, CommandBuffer* c) {
  auto* dx = reinterpret_cast<MtDevice*>(d);
  auto& cx = *reinterpret_cast<MtCommandBuffer*>(c);

  auto pfence = dx->aquireFence();
  if(pfence==nullptr)
    throw DeviceLostException();

  MTL::CommandBuffer& cmd = *cx.impl;
  auto swapchainFrames = std::make_shared<std::vector<MtSwapchain::Frame>>();
  swapchainFrames->swap(cx.swapchainFrames);
  dx->onSubmit();
  cmd.addCompletedHandler(^(MTL::CommandBuffer* c){
    (void)swapchainFrames;
    const MTL::CommandBufferStatus s = c->status();
    dx->signalFence(*pfence, s, MTL::CommandBufferError(c->error()->code()), c->error());
    if(s==MTL::CommandBufferStatusCompleted || s==MTL::CommandBufferStatusError)
      dx->onFinish();
    });
  cmd.commit();
  return pfence;
  }

void MetalApi::getCaps(AbstractGraphicsApi::Device *d, AbstractGraphicsApi::Props &caps) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  caps = dx.prop;
  }

#endif
