#if defined(TEMPEST_BUILD_METAL)

#include "mtshader.h"

#include <Tempest/Log>
#include <Tempest/Except>

#include "mtdevice.h"
#include "mtprecompiledlibrary.h"
#include "gapi/shaderreflection.h"
#include "thirdparty/spirv_cross/spirv_msl.hpp"

#include "libspirv/libspirv.h"

using namespace Tempest;
using namespace Tempest::Detail;

static uint32_t spvVersion(MTL::LanguageVersion v) {
  const uint32_t major = v >> 16u;
  const uint32_t minor = v & 0xFFFF;
  return spirv_cross::CompilerMSL::Options::make_msl_version(major,minor,0);
  }

static MetalApi::PrecompiledShaderStage shaderStage(ShaderReflection::Stage stage) {
  switch(stage) {
    case ShaderReflection::Stage::Vertex:   return MetalApi::PrecompiledShaderStage::Vertex;
    case ShaderReflection::Stage::Control:  return MetalApi::PrecompiledShaderStage::Control;
    case ShaderReflection::Stage::Evaluate: return MetalApi::PrecompiledShaderStage::Evaluate;
    case ShaderReflection::Stage::Geometry: return MetalApi::PrecompiledShaderStage::Geometry;
    case ShaderReflection::Stage::Fragment: return MetalApi::PrecompiledShaderStage::Fragment;
    case ShaderReflection::Stage::Compute:  return MetalApi::PrecompiledShaderStage::Compute;
    case ShaderReflection::Stage::Task:     return MetalApi::PrecompiledShaderStage::Task;
    case ShaderReflection::Stage::Mesh:     return MetalApi::PrecompiledShaderStage::Mesh;
    case ShaderReflection::Stage::None:     break;
    }
  return MetalApi::PrecompiledShaderStage::Compute;
  }

MtShader::MtShader(MtDevice& dev, const void* source, size_t srcSize)
  : Shader(source, srcSize) {
  auto pool = NsPtr<NS::AutoreleasePool>::init();
  spirv_cross::CompilerMSL::Options optMSL;
  MetalApi::PrecompiledShaderProfile precompiledProfile;
#if defined(__OSX__)
  optMSL.platform = spirv_cross::CompilerMSL::Options::macOS;
  precompiledProfile.platform = MetalApi::PrecompiledPlatform::MacOS;
#else
  optMSL.platform = spirv_cross::CompilerMSL::Options::iOS;
#if defined(TARGET_OS_SIMULATOR) && TARGET_OS_SIMULATOR
  precompiledProfile.platform = MetalApi::PrecompiledPlatform::IOSSimulator;
#else
  precompiledProfile.platform = MetalApi::PrecompiledPlatform::IOSDevice;
#endif
#endif
  optMSL.buffer_size_buffer_index = MSL_BUFFER_LENGTH;
  precompiledProfile.stage                 = shaderStage(stage);
  precompiledProfile.bufferSizeBufferIndex = MSL_BUFFER_LENGTH;

  spirv_cross::CompilerGLSL::Options optGLSL;
  optGLSL.vertex.flip_vert_y = true;

  std::string msl;
  try {
    spirv_cross::CompilerMSL comp(reinterpret_cast<const uint32_t*>(source),srcSize/4);
    optMSL.msl_version = spvVersion(dev.prop.mslVersion);
    if(dev.prop.descriptors.nonUniformIndexing) {
      optMSL.argument_buffers_tier = spirv_cross::CompilerMSL::Options::ArgumentBuffersTier::Tier2;
      optMSL.runtime_array_rich_descriptor = true;
      }

    if(dev.prop.mslVersion>=MTL::LanguageVersion2_0) {
      // can relay on threadgroup_barrier(mem_flags::mem_texture) instead
      optMSL.readwrite_texture_fences = false;
      }

    const bool nativeImageAtomics = dev.useNativeImageAtomic();
    if(!nativeImageAtomics) {
      const uint32_t align = dev.linearImageAlignment();
      optMSL.r32ui_linear_texture_alignment = align;
      optMSL.r32ui_alignment_constant_id    = 0;
      }

    for(auto& cap:comp.get_declared_capabilities()) {
      switch(cap) {
        case spv::CapabilityRayQueryKHR: {
          auto ver = spirv_cross::CompilerMSL::Options::make_msl_version(2,3);
          optMSL.msl_version = std::max(optMSL.msl_version, ver);
          break;
          }
        case spv::CapabilityMeshShadingEXT: {
          auto ver = spirv_cross::CompilerMSL::Options::make_msl_version(3,0);
          optMSL.msl_version = std::max(optMSL.msl_version, ver);
          break;
          }
        default:
          break;
        }
      }
    precompiledProfile.mslVersion                 = optMSL.msl_version;
    precompiledProfile.flipVertY                  = optGLSL.vertex.flip_vert_y;
    precompiledProfile.argumentBuffersTier        = uint8_t(optMSL.argument_buffers_tier);
    precompiledProfile.runtimeArrayRichDescriptor = optMSL.runtime_array_rich_descriptor;
    precompiledProfile.readWriteTextureFences     = optMSL.readwrite_texture_fences;
    precompiledProfile.nativeImageAtomics         = nativeImageAtomics;
    precompiledProfile.r32uiLinearTextureAlignment= optMSL.r32ui_linear_texture_alignment;
    precompiledProfile.r32uiAlignmentConstantId   = optMSL.r32ui_alignment_constant_id;
    comp.set_msl_options   (optMSL );
    comp.set_common_options(optGLSL);

    msl = comp.compile();

    bufferSizeBuffer = comp.needs_buffer_size_buffer();

    for(auto& i:lay) {
      i.mslBinding = comp.get_automatic_msl_resource_binding(i.spvId);
      if(i.cls==ShaderReflection::Texture)
        i.mslBinding2 = comp.get_automatic_msl_resource_binding_secondary(i.spvId);
      if(i.cls==ShaderReflection::ImgR || i.cls==ShaderReflection::ImgRW)
        i.mslBinding2 = comp.get_automatic_msl_resource_binding_secondary(i.spvId);
      if(i.cls==ShaderReflection::Push)
        i.mslSize = ShaderReflection::mslSizeOf(i.spvId,comp);
      }

    size_t nsize = 0;
    for(size_t i=0; i<lay.size(); ++i) {
      bool uniq = true;
      if(lay[i].cls!=ShaderReflection::Push) {
        for(size_t r=0; r<i; ++r) {
          if(lay[r].cls==ShaderReflection::Push)
            continue;
          if(lay[i].layout!=lay[r].layout)
            continue;
          uniq = false;
          lay[r].mslBinding  = std::min(lay[r].mslBinding,  lay[i].mslBinding);
          lay[r].mslBinding2 = std::min(lay[r].mslBinding2, lay[i].mslBinding2);
          }
        }
      if(uniq) {
        lay[nsize] = lay[i];
        nsize++;
        }
      }
    lay.resize(nsize);
    }
  catch(const std::bad_alloc&) {
    throw;
    }
  catch(const spirv_cross::CompilerError& err) {
#if !defined(NDEBUG)
    Log::d("cross-compile error: \"",err.what(),"\"");
#else
    (void)err;
#endif
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
    }
  catch(...) {
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
    }

  //Log::d(msl);

  if(dev.precompiledLibraries!=nullptr) {
    impl = dev.precompiledLibraries->find(msl,precompiledProfile);
    if(impl!=nullptr)
      return;
    }

  auto       opt = NsPtr<MTL::CompileOptions>::init();
  NS::Error* err = nullptr;
  auto       str = NsPtr<NS::String>(NS::String::string(msl.c_str(),NS::UTF8StringEncoding));
  str->retain();
  library = NsPtr<MTL::Library>(dev.impl->newLibrary(str.get(), opt.get(), &err));

  if(err!=nullptr) {
    const char* e = err->localizedDescription()->utf8String();
#if !defined(NDEBUG)
    Log::d("cros-compile error: \"",e,"\"\n");
    Log::d(msl);
#endif
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule, e);
    }

  if(stage==ShaderReflection::Stage::Mesh) {
    Log::d(msl);
    }

  auto main = NsPtr<NS::String>(NS::String::string("main0",NS::UTF8StringEncoding));
  main->retain();

  auto cvar = NsPtr<MTL::FunctionConstantValues>::init();
  //cvar->setConstantValues();
  impl = NsPtr<MTL::Function>(library->newFunction(main.get(), cvar.get(), &err));
  }

MtShader::~MtShader() {
  }

#endif
