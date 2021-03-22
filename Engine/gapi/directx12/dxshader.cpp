#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxshader.h"
#include "dxdevice.h"

#include <Tempest/Log>
#include <d3dcompiler.h>

#include "gapi/shaderreflection.h"
#include "thirdparty/spirv_cross/spirv_hlsl.hpp"

using namespace Tempest;
using namespace Tempest::Detail;

DxShader::DxShader(const void *source, size_t src_size) {
  if(src_size%4!=0)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);

  spirv_cross::CompilerHLSL::Options optHLSL;
  optHLSL.shader_model = 50;

  spirv_cross::CompilerGLSL::Options optGLSL;
  optGLSL.vertex.flip_vert_y = true;

  std::string hlsl;
  spv::ExecutionModel exec = spv::ExecutionModelMax;

  try {
    spirv_cross::CompilerHLSL comp(reinterpret_cast<const uint32_t*>(source),src_size/4);
    comp.set_hlsl_options  (optHLSL);
    comp.set_common_options(optGLSL);
    // comp.remap_num_workgroups_builtin();
    hlsl = comp.compile();
    exec = comp.get_execution_model();

    ShaderReflection::getVertexDecl(vdecl,comp);
    ShaderReflection::getBindings(lay,comp);
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

  static const char* target = nullptr;
  switch(exec) {
    case spv::ExecutionModelGLCompute:
      target = "cs_5_0";
      break;
    case spv::ExecutionModelVertex:
      target = "vs_5_0";
      break;
    case spv::ExecutionModelTessellationControl:
      target = "hs_5_0";
      break;
    case spv::ExecutionModelTessellationEvaluation:
      target = "ds_5_0";
      break;
    case spv::ExecutionModelGeometry:
      target = "gs_5_0";
      break;
    case spv::ExecutionModelFragment:
      target = "ps_5_0";
      break;
    default: // unimplemented
      throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
    }
  //Log::d(hlsl);

  ComPtr<ID3DBlob> err;
  UINT compileFlags = 0; //D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
  HRESULT hr = D3DCompile(hlsl.c_str(),hlsl.size(),nullptr,nullptr,nullptr,"main",target,compileFlags,0,
                          reinterpret_cast<ID3DBlob**>(&shader),reinterpret_cast<ID3DBlob**>(&err));
  if(hr!=S_OK) {
#if !defined(NDEBUG)
    Log::d(hlsl.c_str());
    Log::d(reinterpret_cast<const char*>(err->GetBufferPointer()));
#endif
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
    }
  }

DxShader::~DxShader() {
  }

D3D12_SHADER_BYTECODE DxShader::bytecode() const {
  return D3D12_SHADER_BYTECODE{shader->GetBufferPointer(),shader->GetBufferSize()};
  }

#endif
