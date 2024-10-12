#if defined(TEMPEST_BUILD_METAL)

#include "mtdevice.h"
#include "thirdparty/spirv_cross/spirv_msl.hpp"

#include <Tempest/Log>
#include <Foundation/NSProcessInfo.h>
// #include <Metal/MTLPixelFormat.h>

using namespace Tempest;
using namespace Tempest::Detail;

static NsPtr<MTL::Device> mkDevice(std::string_view name) {
  if(name.empty())
    return NsPtr<MTL::Device>(MTL::CreateSystemDefaultDevice());
#if defined(__OSX__)
  auto dev = NsPtr<NS::Array>(MTL::CopyAllDevices());
  for(size_t i=0; i<dev->count(); ++i) {
    NS::Object*  at = dev->object(i);
    MTL::Device* d  = reinterpret_cast<MTL::Device*>(at);
    if(name==d->name()->utf8String()) {
      return NsPtr<MTL::Device>(d);
      }
    }
#else
  return NsPtr<MTL::Device>(MTL::CreateSystemDefaultDevice());
#endif
  
  return NsPtr<MTL::Device>(nullptr);
  }

static MTL::LanguageVersion languageVersion() {
  auto opt = NsPtr<MTL::CompileOptions>::init();
  // clamp version to hight-most tested in engine
  return std::min<MTL::LanguageVersion>(MTL::LanguageVersion3_1, opt->languageVersion());
  }

MtDevice::MtDevice(std::string_view name, bool validation)
  : impl(mkDevice(name)), samplers(*impl), validation(validation) {
  if(impl.get()==nullptr)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);

  if(std::strcmp(impl->name()->utf8String(), "Apple Paravirtual device")==0)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice, "Apple Paravirtual device is not supported");

  queue = NsPtr<MTL::CommandQueue>(impl->newCommandQueue());
  if(queue.get()==nullptr)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);

  mslVersion = languageVersion();
  // mslVersion = MTL::LanguageVersion2_2; //testing
  ui32align  = impl->minimumLinearTextureAlignmentForPixelFormat(MTL::PixelFormatR32Uint);
  deductProps(prop,*impl);
  }

MtDevice::~MtDevice() {
  }

void MtDevice::onSubmit() {
  std::lock_guard<std::mutex> guard(devIdleSync);
  devCmdBuf.fetch_add(1u);
  }

void MtDevice::onFinish() {
  std::lock_guard<std::mutex> guard(devIdleSync);
  devCmdBuf.fetch_add(-1);
  devIdleCv.notify_all();
  }

void MtDevice::waitIdle() {
  std::unique_lock<std::mutex> guard(devIdleSync);
  devIdleCv.wait(guard,[this](){
    return 0==devCmdBuf.load();
    });
  }

void MtDevice::handleError(NS::Error *err) {
  if(err==nullptr)
    return;
#if !defined(NDEBUG)
  const char* e = err->localizedDescription()->utf8String();
  Log::d("NSError: \"",e,"\"");
#endif
  throw DeviceLostException();
  }

void MtDevice::deductProps(AbstractGraphicsApi::Props& prop, MTL::Device& dev) {
  int32_t majorVersion = 0, minorVersion = 0;
  if([[NSProcessInfo processInfo] respondsToSelector:@selector(operatingSystemVersion)]) {
    NSOperatingSystemVersion ver = [[NSProcessInfo processInfo] operatingSystemVersion];
    majorVersion = int32_t(ver.majorVersion);
    minorVersion = int32_t(ver.minorVersion);
    }

  std::strncpy(prop.name,dev.name()->utf8String(),sizeof(prop.name));
  if(dev.hasUnifiedMemory())
    prop.type = DeviceType::Integrated; else
    prop.type = DeviceType::Discrete;

  // https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
  static const TextureFormat smp[] = {TextureFormat::R8,   TextureFormat::RG8,   TextureFormat::RGBA8,
                                      TextureFormat::R16,  TextureFormat::RG16,  TextureFormat::RGBA16,
                                      TextureFormat::R32F, TextureFormat::RG32F, TextureFormat::RGBA32F,
                                      TextureFormat::R32U, TextureFormat::RG32U, TextureFormat::RGBA32U,
                                      TextureFormat::R11G11B10UF, TextureFormat::RGBA16F,
                                     };

  static const TextureFormat att[] = {TextureFormat::R8,   TextureFormat::RG8,   TextureFormat::RGBA8,
                                      TextureFormat::R16,  TextureFormat::RG16,  TextureFormat::RGBA16,
                                      TextureFormat::R32F, TextureFormat::RG32F, TextureFormat::RGBA32F,
                                      TextureFormat::R11G11B10UF, TextureFormat::RGBA16F,
                                     };

  static const TextureFormat sso[] = {TextureFormat::R8,   TextureFormat::RG8,   TextureFormat::RGBA8,
                                      TextureFormat::R16,  TextureFormat::RG16,  TextureFormat::RGBA16,
                                      TextureFormat::R32U, TextureFormat::RG32U, TextureFormat::RGBA32U,
                                      TextureFormat::R32F, TextureFormat::RGBA32F,
                                      TextureFormat::R11G11B10UF, TextureFormat::RGBA16F,
                                     };

  static const TextureFormat ds[]  = {TextureFormat::Depth16, TextureFormat::Depth32F};

  uint64_t smpBit = 0, attBit = 0, dsBit = 0, storBit = 0, atomBit = 0;
  for(auto& i:smp)
    smpBit |= uint64_t(1) << uint64_t(i);
  for(auto& i:att)
    attBit |= uint64_t(1) << uint64_t(i);
  for(auto& i:sso)
    storBit  |= uint64_t(1) << uint64_t(i);
  for(auto& i:ds)
    dsBit  |= uint64_t(1) << uint64_t(i);

  if(dev.supportsBCTextureCompression()) {
    static const TextureFormat bc[] = {TextureFormat::DXT1, TextureFormat::DXT3, TextureFormat::DXT5};
    for(auto& i:bc)
      smpBit |= uint64_t(1) << uint64_t(i);
    }
  
#ifdef __OSX__
  if(dev.depth24Stencil8PixelFormatSupported()) {
    static const TextureFormat ds[] = {TextureFormat::Depth24S8};
    for(auto& i:ds) {
      dsBit  |= uint64_t(1) << uint64_t(i);
      smpBit |= uint64_t(1) << uint64_t(i);
      }
    }
#endif

  {
  /* NOTE1: https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf
   * > You must declare textures with depth formats as one of the following texture data types
   * > depth2d
   *
   * NOTE2: https://github.com/KhronosGroup/SPIRV-Cross/issues/529
   * It seems MSL and Metal validation-layer doesn't really care, if depth is sampled as texture2d<>
   *
   * Testing shows, that texture2d works on MacOS
  */
#ifdef __OSX__
  if(majorVersion>=11 || (majorVersion==10 && minorVersion>=11)) {
    dsBit  |= uint64_t(1) << TextureFormat::Depth32F;
    smpBit |= uint64_t(1) << TextureFormat::Depth32F;
    }
#else
  // no iOS, for Depth32F
#endif

#ifdef __OSX__
  if(majorVersion>=11 || (majorVersion==10 && minorVersion>=12)) {
    dsBit  |= uint64_t(1) << TextureFormat::Depth16;
    smpBit |= uint64_t(1) << TextureFormat::Depth16;
    }
#else
  if(majorVersion>=13) {
    dsBit  |= uint64_t(1) << TextureFormat::Depth16;
    smpBit |= uint64_t(1) << TextureFormat::Depth16;
    }
#endif
  }

  // native or emulated; no extra formats yet
  atomBit  |= uint64_t(1) << TextureFormat::R32U;

  prop.setSamplerFormats(smpBit);
  prop.setAttachFormats (attBit);
  prop.setDepthFormats  (dsBit);
  prop.setStorageFormats(storBit);
  prop.setAtomicFormats (atomBit);

  prop.tex2d.maxSize = 8092;
  prop.tex3d.maxSize = 2048;
  if(dev.supportsFamily(MTL::GPUFamilyApple3)) {
    prop.tex2d.maxSize = 16384;
    }

  prop.render.maxColorAttachments = 4;
  if(dev.supportsFamily(MTL::GPUFamilyApple2))
    prop.render.maxColorAttachments = 8;
  props.render.maxViewportSize.w    = 8192;
  props.render.maxViewportSize.h    = 8192;
  props.render.maxClipCullDistances = 8;

  // https://developer.apple.com/documentation/metal/mtlrendercommandencoder/1515829-setvertexbuffer?language=objc
  prop.vbo.maxAttribs = 31;

#ifdef __IOS__
  prop.ubo .offsetAlign = 16;
  prop.ssbo.offsetAlign = 16;
#else
  prop.ubo .offsetAlign = 256; //assuming device-local memory
  prop.ssbo.offsetAlign = 16;
#endif

  // in fact there is no limit, just recomendation to submit less than 4kb of data
  prop.push.maxRange = 256;

  const auto maxGroupSize = dev.maxThreadsPerThreadgroup();
  prop.compute.maxGroupSize.x = maxGroupSize.width;
  prop.compute.maxGroupSize.y = maxGroupSize.height;
  prop.compute.maxGroupSize.z = maxGroupSize.depth;
  prop.compute.maxInvocations = std::max(std::max(
                                           prop.compute.maxGroupSize.x,
                                           prop.compute.maxGroupSize.y),
                                           prop.compute.maxGroupSize.z);
  prop.compute.maxSharedMemory = dev.maxThreadgroupMemoryLength();

  prop.anisotropy    = true;
  prop.maxAnisotropy = 16;

#ifdef __IOS__
  if(dev.supportsFeatureSet(MTL::FeatureSet_iOS_GPUFamily3_v2))
    prop.tesselationShader = false;//true;
#else
  if(dev.supportsFeatureSet(MTL::FeatureSet_macOS_GPUFamily1_v2))
    prop.tesselationShader = false;//true;
#endif

#ifdef __IOS__
  prop.storeAndAtomicVs = false;
  prop.storeAndAtomicFs = false;
#else

  const uint32_t mslVersion = languageVersion();
  if(mslVersion>=MTL::LanguageVersion2_2) {
    // 2.1 for buffers
    // 2.2 for images
    prop.storeAndAtomicVs = true;
    prop.storeAndAtomicFs = true;
    }
#endif

  if(dev.supportsFamily(MTL::GPUFamilyMetal3) && dev.argumentBuffersSupport()>=MTL::ArgumentBuffersTier2) {
    prop.descriptors.nonUniformIndexing = true;
    prop.descriptors.maxStorage         = 500000;
    prop.descriptors.maxTexture         = 500000;
    prop.descriptors.maxSamplers        = 2048;
    } else {
    // 30 total bindings + reserve 2 for internal use
    prop.descriptors.maxStorage         = 10;
    prop.descriptors.maxTexture         = 10;
    prop.descriptors.maxSamplers        = 8;
    }

#ifdef __OSX__
  if(majorVersion>=12)
    prop.raytracing.rayQuery = dev.supportsRaytracingFromRender() && dev.supportsRaytracing();
#endif

#ifdef __IOS__
  if(dev.supportsFamily(MTL::GPUFamilyMetal3)) {
    //prop.meshlets.taskShader = true;
    //prop.meshlets.meshShader = true;
    prop.meshlets.maxGroups = prop.compute.maxGroups;
    prop.meshlets.maxGroupSize = prop.compute.maxGroupSize;
    }
#else
  if(dev.supportsFamily(MTL::GPUFamilyMetal3) &&
     dev.supportsFamily(MTL::GPUFamilyApple7)) {
    // NOTE: need to disallow MTL::GPUFamilyMac2 - low limits make it usless
    prop.meshlets.taskShader   = true;
    prop.meshlets.meshShader   = true;
    prop.meshlets.maxGroups    = prop.compute.maxGroups;
    prop.meshlets.maxGroupSize = prop.compute.maxGroupSize;
    }
#endif
  }

bool MtDevice::useNativeImageAtomic() const {
  return mslVersion>=MTL::LanguageVersion3_1;
  }

uint32_t MtDevice::linearImageAlignment() const {
  return ui32align;
  }
#endif

