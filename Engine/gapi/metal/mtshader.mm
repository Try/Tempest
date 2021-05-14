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
    comp.set_msl_options   (optMSL );
    comp.set_common_options(optGLSL);

    msl = comp.compile();

    ShaderReflection::getVertexDecl(vdecl,comp);
    ShaderReflection::getBindings(lay,comp);
    for(auto& i:lay) {
      uint32_t idx = comp.get_automatic_msl_resource_binding(i.spvId);
      i.mslBinding = idx;
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

  Log::d(msl);

  auto     opt = [MTLCompileOptions new];
  NSError* err = nil;
  auto     str = [NSString stringWithCString:msl.c_str() encoding:[NSString defaultCStringEncoding]];

  library = [dev.impl newLibraryWithSource:str options:opt error:&err];
  [opt release];
  [str release];

  if(err!=nil) {
#if !defined(NDEBUG)
    const char* e = [[err domain] UTF8String];
    Log::d("cros-compile error: \"",e,"\"");
#endif
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
    }
  impl = [library newFunctionWithName: @"main0"];
  }

MtShader::~MtShader() {
  [impl    release];
  [library release];
  }
