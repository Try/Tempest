#if defined(TEMPEST_BUILD_METAL)

#include "mtshader.h"

#include <Tempest/Log>
#include <Tempest/Except>

#include "mtdevice.h"
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

MtShader::MtShader(MtDevice& dev, const void* source, size_t srcSize) {
  if(srcSize%4!=0)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);

  auto pool = NsPtr<NS::AutoreleasePool>::init();
  spirv_cross::CompilerMSL::Options optMSL;
#if defined(__OSX__)
  optMSL.platform = spirv_cross::CompilerMSL::Options::macOS;
#else
  optMSL.platform = spirv_cross::CompilerMSL::Options::iOS;
#endif
  optMSL.buffer_size_buffer_index = MSL_BUFFER_LENGTH;

  spirv_cross::CompilerGLSL::Options optGLSL;
  optGLSL.vertex.flip_vert_y = true;

  std::string msl;
  try {
    fetchBindings(reinterpret_cast<const uint32_t*>(source),srcSize/4);

    spirv_cross::CompilerMSL comp(reinterpret_cast<const uint32_t*>(source),srcSize/4);
    optMSL.msl_version = spvVersion(dev.mslVersion);
    if(dev.prop.descriptors.nonUniformIndexing) {
      optMSL.argument_buffers_tier = spirv_cross::CompilerMSL::Options::ArgumentBuffersTier::Tier2;
      optMSL.runtime_array_rich_descriptor = true;
      }

    if(!dev.useNativeImageAtomic()) {
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

    if(comp.get_execution_model()==spv::ExecutionModelTessellationEvaluation) {
      auto& exec = comp.get_execution_mode_bitset();
      if(exec.get(spv::ExecutionModeTriangles)) {

        }

      if(exec.get(spv::ExecutionModeVertexOrderCw))
        tese.winding = MTL::WindingClockwise;
      else if(exec.get(spv::ExecutionModeVertexOrderCcw))
        tese.winding = MTL::WindingCounterClockwise;

      if(exec.get(spv::ExecutionModeSpacingEqual))
        tese.partition = MTL::TessellationPartitionModeInteger;
      else if(exec.get(spv::ExecutionModeSpacingFractionalEven))
        tese.partition = MTL::TessellationPartitionModeFractionalEven;
      else if(exec.get(spv::ExecutionModeSpacingFractionalOdd))
        tese.partition = MTL::TessellationPartitionModeFractionalOdd;
      }
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

void MtShader::fetchBindings(const uint32_t *source, size_t size) {
  // TODO: remove spirv-corss?
  spirv_cross::Compiler comp(source, size);
  ShaderReflection::getVertexDecl(vdecl,comp);
  ShaderReflection::getBindings(lay,comp);

  libspirv::Bytecode code(source, size);
  stage = ShaderReflection::getExecutionModel(code);
  if(stage==ShaderReflection::Compute || stage==ShaderReflection::Task || stage==ShaderReflection::Mesh) {
    for(auto& i:code) {
      if(i.op()!=spv::OpExecutionMode)
        continue;
      if(i[2]==spv::ExecutionModeLocalSize) {
        this->comp.localSize.width  = i[3];
        this->comp.localSize.height = i[4];
        this->comp.localSize.depth  = i[5];
        }
      }
    }
  for(auto& i:code) {
    if(i.op()!=spv::OpSource)
      continue;
    uint32_t src = i.length()>3 ? i[3] : 0;
    if(src==0)
      continue;

    for(auto& r:code) {
      if(r.op()==spv::OpString && r[1]==src) {
        dbg.source = reinterpret_cast<const char*>(&r[2]);
        break;
        }
      }
    break;
    }
  }

#endif
