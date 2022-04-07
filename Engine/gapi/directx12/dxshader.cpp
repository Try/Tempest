#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxshader.h"
#include "dxdevice.h"

#include <Tempest/Log>
#include <d3dcompiler.h>

#include "gapi/shaderreflection.h"
#include "thirdparty/spirv_cross/spirv_hlsl.hpp"
#include "thirdparty/spirv_cross/spirv_msl.hpp"

using namespace Tempest;
using namespace Tempest::Detail;

static const char* target(spv::ExecutionModel exec, uint32_t sm) {
  switch(exec) {
    case spv::ExecutionModelGLCompute:
      return "cs_5_0";
    case spv::ExecutionModelVertex:
      return "vs_5_0";
    case spv::ExecutionModelTessellationControl:
      return "hs_5_0";
    case spv::ExecutionModelTessellationEvaluation:
      return "ds_5_0";
    case spv::ExecutionModelGeometry:
      return "gs_5_0";
    case spv::ExecutionModelFragment:
      if(sm==50)
        return "ps_5_0";
      if(sm==51)
        return "ps_5_1";
      if(sm==60)
        return "ps_6_0";
      return "ps_6_5";
    default: // unimplemented
      throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
    }
  }

DxShader::DxShader(const void *source, const size_t src_size) {
  if(src_size%4!=0)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);

  spirv_cross::CompilerHLSL::Options optHLSL;
  optHLSL.shader_model = 50;
  //optHLSL.shader_model = 65;

  spirv_cross::CompilerGLSL::Options optGLSL;
  optGLSL.vertex.flip_vert_y = false;

  std::string hlsl;
  spv::ExecutionModel exec = spv::ExecutionModelMax;

  try {
    spirv_cross::CompilerHLSL comp(reinterpret_cast<const uint32_t*>(source),src_size/4);
    exec = comp.get_execution_model();
    if(exec==spv::ExecutionModelVertex || exec==spv::ExecutionModelTessellationEvaluation)
      optGLSL.vertex.flip_vert_y = true;
    comp.set_hlsl_options  (optHLSL);
    comp.set_common_options(optGLSL);

    for(auto& cap:comp.get_declared_capabilities()) {
      switch(cap) {
        case spv::CapabilityRayQueryKHR:
          optHLSL.shader_model = std::max<uint32_t>(60,optHLSL.shader_model);
          break;
        default:
          break;
        }
      }
    // comp.remap_num_workgroups_builtin();
    hlsl = comp.compile();

    ShaderReflection::getVertexDecl(vdecl,comp);
    ShaderReflection::getBindings(lay,comp);

    if(exec==spv::ExecutionModelVertex) {
      tess.vertSrc.resize(src_size/4);
      std::memcpy(tess.vertSrc.data(),source,src_size);
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

  if(false) {
    spirv_cross::CompilerMSL comp(reinterpret_cast<const uint32_t*>(source),src_size/4);
    comp.set_common_options(optGLSL);
    auto glsl = comp.compile();
    Log::d(glsl);
    }

  ComPtr<ID3DBlob> err;
  // TODO: D3DCOMPILE_ALL_RESOURCES_BOUND
  UINT compileFlags = 0; //D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
  HRESULT hr = D3DCompile(hlsl.c_str(),hlsl.size(),nullptr,nullptr,nullptr,"main",
                          target(exec,optHLSL.shader_model),compileFlags,0,
                          reinterpret_cast<ID3DBlob**>(&shader),reinterpret_cast<ID3DBlob**>(&err));
  if(hr!=S_OK) {
#if !defined(NDEBUG)
    Log::d(hlsl.c_str());
    Log::d(reinterpret_cast<const char*>(err->GetBufferPointer()));
#endif
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
    }

  if(false && exec == spv::ExecutionModelTessellationControl) {
    Log::d(hlsl);
    disasm();
    }
  }

DxShader::~DxShader() {
  }

D3D12_SHADER_BYTECODE DxShader::bytecode(Flavor f) const {
  if(f==Flavor::Default)
    return D3D12_SHADER_BYTECODE{shader->GetBufferPointer(),shader->GetBufferSize()};

  if(f==Flavor::NoFlipY) {
    std::lock_guard<SpinLock> guard(tess.sync);
    if(tess.altShader.get()!=nullptr)
      return D3D12_SHADER_BYTECODE{tess.altShader->GetBufferPointer(),tess.altShader->GetBufferSize()};

    spv::ExecutionModel exec = spv::ExecutionModelVertex;

    spirv_cross::CompilerHLSL::Options optHLSL;
    optHLSL.shader_model = 50;
    spirv_cross::CompilerGLSL::Options optGLSL;
    optGLSL.vertex.flip_vert_y = false;

    spirv_cross::CompilerHLSL comp(tess.vertSrc.data(),tess.vertSrc.size());
    comp.set_hlsl_options  (optHLSL);
    comp.set_common_options(optGLSL);

    auto hlsl = comp.compile();
    UINT compileFlags = 0;
    HRESULT hr = D3DCompile(hlsl.c_str(),hlsl.size(),nullptr,nullptr,nullptr,"main",target(exec,optHLSL.shader_model),compileFlags,0,
                            reinterpret_cast<ID3DBlob**>(&tess.altShader),nullptr);
    if(hr!=S_OK)
      throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
    return D3D12_SHADER_BYTECODE{tess.altShader->GetBufferPointer(),tess.altShader->GetBufferSize()};
    }

  throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
  }

void DxShader::disasm() const {
  ID3D10Blob *asm_blob = nullptr;
  D3DDisassemble(shader->GetBufferPointer(),shader->GetBufferSize(),
                 D3D_DISASM_ENABLE_INSTRUCTION_NUMBERING,"",&asm_blob);
  if(!asm_blob)
    return;

  Log::d(reinterpret_cast<const char*>(asm_blob->GetBufferPointer()));
  asm_blob->Release();
  }

#endif
