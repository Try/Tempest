#include "mtshader.h"

#include "mtdevice.h"

#include <Tempest/Log>
#include <Tempest/Except>

#include "gapi/shaderreflection.h"
#include "thirdparty/spirv_cross/spirv_msl.hpp"

#include <Metal/MTLLibrary.h>

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
    for(auto& cap:comp.get_declared_capabilities()) {
      switch(cap) {
        case spv::CapabilityRayQueryKHR:
          optMSL.msl_version = spirv_cross::CompilerMSL::Options::make_msl_version(2,3);
          break;
        default:
          break;
        }
      }

    comp.set_msl_options   (optMSL );
    comp.set_common_options(optGLSL);

    msl = comp.compile();

    ShaderReflection::getVertexDecl(vdecl,comp);
    ShaderReflection::getBindings(lay,comp);
    for(auto& i:lay) {
      i.mslBinding = comp.get_automatic_msl_resource_binding(i.spvId);
      if(i.cls==ShaderReflection::Texture)
        i.mslBinding2 = comp.get_automatic_msl_resource_binding_secondary(i.spvId);
      if(i.cls==ShaderReflection::Push)
        i.mslSize = ShaderReflection::mslSizeOf(i.spvId,comp);
      }

    if(comp.get_execution_model()==spv::ExecutionModelTessellationEvaluation) {
      auto& exec = comp.get_execution_mode_bitset();
      if(exec.get(spv::ExecutionModeTriangles)) {

        }

      if(exec.get(spv::ExecutionModeVertexOrderCw))
        tese.winding = MTLWindingClockwise;
      else if(exec.get(spv::ExecutionModeVertexOrderCcw))
        tese.winding = MTLWindingCounterClockwise;

      if(exec.get(spv::ExecutionModeSpacingEqual))
        tese.partition = MTLTessellationPartitionModeInteger;
      else if(exec.get(spv::ExecutionModeSpacingFractionalEven))
        tese.partition = MTLTessellationPartitionModeFractionalEven;
      else if(exec.get(spv::ExecutionModeSpacingFractionalOdd))
        tese.partition = MTLTessellationPartitionModeFractionalOdd;
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

  auto     opt = [MTLCompileOptions new];
  NSError* err = nil;
  auto     str = [NSString stringWithCString:msl.c_str() encoding:[NSString defaultCStringEncoding]];

  library = [dev.impl newLibraryWithSource:str options:opt error:&err];
  [opt release];
  [str release];

  if(err!=nil) {
#if !defined(NDEBUG)
    const char* e = [[err localizedDescription] UTF8String];
    Log::d("cros-compile error: \"",e,"\"");
    Log::d(msl);
#endif
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
    }
  impl = [library newFunctionWithName: @"main0"];
  }

MtShader::~MtShader() {
  [impl    release];
  [library release];
  }
