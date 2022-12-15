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

MtShader::MtShader(MtDevice& dev, const void* source, size_t srcSize) {
  if(srcSize%4!=0)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);

  spirv_cross::CompilerMSL::Options optMSL;
#if defined(__OSX__)
  optMSL.platform = spirv_cross::CompilerMSL::Options::macOS;
#else
  optMSL.platform = spirv_cross::CompilerMSL::Options::iOS;
#endif

  spirv_cross::CompilerGLSL::Options optGLSL;
  optGLSL.vertex.flip_vert_y = true;

  std::string msl;
  try {
    spirv_cross::CompilerMSL comp(reinterpret_cast<const uint32_t*>(source),srcSize/4);
    optMSL.msl_version = spirv_cross::CompilerMSL::Options::make_msl_version(2,0);
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

    fetchBindings(reinterpret_cast<const uint32_t*>(source),srcSize/4);

    for(auto& i:lay) {
      i.mslBinding = comp.get_automatic_msl_resource_binding(i.spvId);
      if(i.cls==ShaderReflection::Texture)
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
  library = NsPtr<MTL::Library>(dev.impl->newLibrary(str.get(), opt.get(), &err));

  if(err!=nullptr) {
#if !defined(NDEBUG)
    const char* e = err->localizedDescription()->utf8String();
    Log::d("cros-compile error: \"",e,"\"");
    Log::d(msl);
#endif
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
    }

  if(stage==ShaderReflection::Stage::Mesh) {
    // Log::d(msl);
    }

  auto main = NsPtr<NS::String>(NS::String::string("main0",NS::UTF8StringEncoding));
  impl = NsPtr<MTL::Function>(library->newFunction(main.get()));
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
  }

#endif
