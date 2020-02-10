#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxshader.h"
#include "dxdevice.h"

#include <Tempest/Log>
#include <d3dcompiler.h>

#include "thirdparty/spirv_cross/spirv_hlsl.hpp"

using namespace Tempest;
using namespace Tempest::Detail;

DxShader::DxShader(DxDevice& d, const void *source, size_t src_size)
  : device(d.device.get()) {
  if(src_size%4!=0)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);

  spirv_cross::CompilerHLSL::Options opt;
  opt.shader_model = 50;
  std::string hlsl;
  spv::ExecutionModel exec = spv::ExecutionModelMax;

  try {
    spirv_cross::CompilerHLSL comp(reinterpret_cast<const uint32_t*>(source),src_size/4);
    comp.set_hlsl_options(opt);
    hlsl = comp.compile();
    exec = comp.get_execution_model();
    }
  catch(const std::bad_alloc&) {
    throw;
    }
  catch(...) {
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
    }

  static const char* target = nullptr;
  switch(exec) {
    case spv::ExecutionModelVertex:
      target = "vs_5_0";
      break;
    case spv::ExecutionModelFragment:
      target = "ps_5_0";
      break;
    default: // unimplemented
      throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
    }
  // Log::d(hlsl);

  ComPtr<ID3DBlob> errs;
  UINT compileFlags = 0; //D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
  HRESULT hr = D3DCompile(hlsl.c_str(),hlsl.size(),nullptr,nullptr,nullptr,"main",target,compileFlags,0,
                      reinterpret_cast<ID3DBlob**>(&shader),reinterpret_cast<ID3DBlob**>(&errs));
  if(hr!=S_OK) {
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
    }
  }

DxShader::~DxShader() {
  }

#endif
