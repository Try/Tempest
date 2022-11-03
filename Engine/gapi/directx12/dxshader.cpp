#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxshader.h"
#include "dxdevice.h"

#include <Tempest/Log>
#include <d3dcompiler.h>
#include <libspirv/libspirv.h>

#include <dxcapi.h>

#include "gapi/shaderreflection.h"
#include "thirdparty/spirv_cross/spirv_hlsl.hpp"
#include "thirdparty/spirv_cross/spirv_msl.hpp"

#include "guid.h"

using namespace Tempest;
using namespace Tempest::Detail;

static const char* target(spv::ExecutionModel exec, uint32_t sm, char* buf) {
  switch(exec) {
    case spv::ExecutionModelGLCompute:
      std::snprintf(buf,32,"cs_%d_%d", sm/10, sm%10);
      return buf;
    case spv::ExecutionModelVertex:
      std::snprintf(buf,32,"vs_%d_%d", sm/10, sm%10);
      return buf;
    case spv::ExecutionModelTessellationControl:
      std::snprintf(buf,32,"hs_%d_%d", sm/10, sm%10);
      return buf;
    case spv::ExecutionModelTessellationEvaluation:
      std::snprintf(buf,32,"ds_%d_%d", sm/10, sm%10);
      return buf;
    case spv::ExecutionModelGeometry:
      std::snprintf(buf,32,"gs_%d_%d", sm/10, sm%10);
      return buf;
    case spv::ExecutionModelFragment:
      std::snprintf(buf,32,"ps_%d_%d", sm/10, sm%10);
      return buf;
    case spv::ExecutionModelTaskEXT:
      std::snprintf(buf,32,"as_%d_%d", sm/10, sm%10);
      return buf;
    case spv::ExecutionModelMeshEXT:
      std::snprintf(buf,32,"ms_%d_%d", sm/10, sm%10);
      return buf;
    default: // unimplemented
      throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
    }
  }

static int calcShaderModel(const spirv_cross::CompilerHLSL& comp) {
  //uint32_t shader_model = 50;
  uint32_t shader_model = 65;
  for(auto& cap:comp.get_declared_capabilities()) {
    switch(cap) {
      case spv::CapabilityRayQueryKHR:
        shader_model = std::max<uint32_t>(65,shader_model);
        break;
      default:
        break;
      }
    }
  return shader_model;
  }

DxShader::DxShader(const void *source, const size_t src_size) {
  if(src_size%4!=0)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);

  spirv_cross::CompilerHLSL::Options optHLSL;

  spirv_cross::CompilerGLSL::Options optGLSL;
  optGLSL.vertex.flip_vert_y = false;

  std::string hlsl;
  spv::ExecutionModel exec = spv::ExecutionModelMax;

  try {
    libspirv::Bytecode code(reinterpret_cast<const uint32_t*>(source),src_size/4);
    exec = code.findExecutionModel();
    stage = ShaderReflection::getExecutionModel(exec);
    for(auto& i:code) {
      if(i.op()==spv::OpDecorate && i[2]==spv::DecorationBuiltIn && i[3]==spv::BuiltInInstanceIndex) {
        has_baseVertex_baseInstance = true;
        }
      if(i.op()==spv::OpDecorate && i[2]==spv::DecorationBuiltIn && i[3]==spv::BuiltInBaseVertex) {
        has_baseVertex_baseInstance = true;
        }
      if(stage==ShaderReflection::Compute||
         stage==ShaderReflection::Task || stage==ShaderReflection::Mesh) {
        if(i.op()==spv::OpExecutionMode && i[2]==spv::ExecutionModeLocalSize) {
          this->comp.wgSize.x = i[3];
          this->comp.wgSize.y = i[4];
          this->comp.wgSize.z = i[5];
          }
        }
      }

    spirv_cross::CompilerHLSL comp(reinterpret_cast<const uint32_t*>(source),src_size/4);
    if(stage==ShaderReflection::Stage::Vertex || stage==ShaderReflection::Stage::Evaluate)
      optGLSL.vertex.flip_vert_y = true;
    optHLSL.shader_model = calcShaderModel(comp);
    optHLSL.support_nonzero_base_vertex_base_instance = has_baseVertex_baseInstance;
    comp.set_hlsl_options  (optHLSL);
    comp.set_common_options(optGLSL);
    comp.set_hlsl_aux_buffer_binding(spirv_cross::HLSL_AUX_BINDING_BASE_VERTEX_INSTANCE, HLSL_BASE_VERTEX_INSTANCE, 0);

    spirv_cross::HLSLResourceBinding push_binding;
    push_binding.stage = exec;
    push_binding.desc_set = spirv_cross::ResourceBindingPushConstantDescriptorSet;
    push_binding.binding = spirv_cross::ResourceBindingPushConstantBinding;
    push_binding.cbv.register_binding = HLSL_PUSH;
    push_binding.cbv.register_space = 0;
    comp.add_hlsl_resource_binding(push_binding);

    // comp.remap_num_workgroups_builtin();
    hlsl = comp.compile();

    ShaderReflection::getVertexDecl(vdecl,comp);
    ShaderReflection::getBindings(lay,comp);

    if(stage==ShaderReflection::Stage::Vertex) {
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

  HRESULT hr = compile(shader,hlsl.c_str(),hlsl.size(),exec,optHLSL.shader_model);
  if(hr!=S_OK) {
#if !defined(NDEBUG)
    Log::d(hlsl);
#endif
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
    }

  if(stage==ShaderReflection::Stage::Mesh) {
    //Log::d(hlsl);
    //disasm();
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
    spirv_cross::CompilerGLSL::Options optGLSL;
    optGLSL.vertex.flip_vert_y = false;

    spirv_cross::CompilerHLSL comp(tess.vertSrc.data(),tess.vertSrc.size());
    optHLSL.shader_model = calcShaderModel(comp);
    comp.set_hlsl_options  (optHLSL);
    comp.set_common_options(optGLSL);
    comp.set_hlsl_aux_buffer_binding(spirv_cross::HLSL_AUX_BINDING_BASE_VERTEX_INSTANCE, HLSL_BASE_VERTEX_INSTANCE, 0);

    spirv_cross::HLSLResourceBinding push_binding;
    push_binding.stage = exec;
    push_binding.desc_set = spirv_cross::ResourceBindingPushConstantDescriptorSet;
    push_binding.binding = spirv_cross::ResourceBindingPushConstantBinding;
    push_binding.cbv.register_binding = HLSL_PUSH;
    push_binding.cbv.register_space = 0;
    comp.add_hlsl_resource_binding(push_binding);

    auto    hlsl = comp.compile();
    HRESULT hr   = compile(tess.altShader,hlsl.c_str(),hlsl.size(),exec,optHLSL.shader_model);

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

HRESULT DxShader::compile(ComPtr<ID3DBlob>& shader, const char* hlsl, size_t len, spv::ExecutionModel exec, uint32_t sm) const {
  char tg[32] = {};
  if(sm>50)
    return compileDXC(shader,hlsl,len,target(exec,sm,tg));
  return compileOld(shader,hlsl,len,target(exec,sm,tg));
  }

HRESULT DxShader::compileOld(ComPtr<ID3DBlob>& shader, const char* hlsl, size_t len, const char* target) const {
  ComPtr<ID3DBlob> err;
  // TODO: D3DCOMPILE_ALL_RESOURCES_BOUND
  UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
  HRESULT hr = D3DCompile(hlsl,len,nullptr,nullptr,nullptr,"main",
                          target,compileFlags,0,
                          reinterpret_cast<ID3DBlob**>(&shader.get()),reinterpret_cast<ID3DBlob**>(&err.get()));
  if(hr!=S_OK) {
#if !defined(NDEBUG)
    Log::d(reinterpret_cast<const char*>(err->GetBufferPointer()));
#endif
    }
  return hr;
  }

HRESULT DxShader::compileDXC(ComPtr<ID3DBlob>& shader, const char* hlsl, size_t len, const char* target) const {
  ComPtr<IDxcCompiler3> compiler;

  HRESULT hr = S_OK;
  hr = DxcCreateInstance(CLSID_DxcCompiler, uuid<IDxcCompiler3>(), reinterpret_cast<void**>(&compiler.get()));
  if(FAILED(hr))
    return hr;

  wchar_t targetW[32] = {};
  for(size_t i=0; i<32 && target[i]; ++i)
    targetW[i] = target[i];

  LPCWSTR  argv[] = {
    L"-E",
    L"main",
    L"-T",
    targetW,
    L"-Qstrip_reflect",
#if !defined(NDEBUG)
    // DXC_ARG_DEBUG,
#endif
    };
  uint32_t argc = _countof(argv);

  DxcBuffer srcBuffer;
  srcBuffer.Ptr      = hlsl;
  srcBuffer.Size     = len;
  srcBuffer.Encoding = DXC_CP_ACP;

  ComPtr<IDxcResult> result;
  hr = compiler->Compile(&srcBuffer,argv,argc,nullptr,uuid<IDxcResult>(),reinterpret_cast<void**>(&result.get()));
  if(FAILED(hr))
    return hr;

#if !defined(NDEBUG)
  {
  ComPtr<IDxcBlobUtf8> pErrors;
  result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors.get()), nullptr);
  if(pErrors.get()!=nullptr && pErrors->GetStringLength()>0)
    Log::d(reinterpret_cast<const char*>(pErrors->GetStringPointer()));
  }
#endif

  HRESULT hrStatus = 0;
  result->GetStatus(&hrStatus);
  if(FAILED(hrStatus)) {
    return hrStatus;
    }

  hr = result->GetResult(reinterpret_cast<IDxcBlob**>(&shader.get()));
  return hr;
  }

#endif
