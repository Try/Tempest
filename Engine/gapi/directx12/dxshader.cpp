#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxshader.h"

#include <Tempest/Log>
#include <Tempest/Except>

#include <libspirv/libspirv.h>

#include <dxcapi.h>

#include "gapi/shaderreflection.h"
#include "thirdparty/spirv_cross/spirv_hlsl.hpp"
#include "thirdparty/spirv_cross/spirv_msl.hpp"

#include "dxdevice.h"
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

DxShader::DxShader(const void *source, const size_t src_size)
  : Shader(source, src_size){
  spirv_cross::CompilerHLSL::Options optHLSL;

  spirv_cross::CompilerGLSL::Options optGLSL;
  optGLSL.vertex.flip_vert_y = false;

  std::string hlsl;
  spv::ExecutionModel exec = spv::ExecutionModelMax;

  try {
    libspirv::Bytecode code(reinterpret_cast<const uint32_t*>(source),src_size/4);
    exec = code.findExecutionModel();

    spirv_cross::CompilerHLSL comp(reinterpret_cast<const uint32_t*>(source),src_size/4);
    if(stage==ShaderReflection::Stage::Vertex || stage==ShaderReflection::Stage::Mesh)
      optGLSL.vertex.flip_vert_y = true;
    optHLSL.shader_model = calcShaderModel(comp);
    optHLSL.support_nonzero_base_vertex_base_instance = vert.has_baseVertex_baseInstance;
    optHLSL.point_size_compat = true;

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

    for(auto& i:lay)
      if(i.runtimeSized) {
        spirv_cross::HLSLResourceBinding bindless = {};
        bindless.stage    = exec;
        bindless.desc_set = 0;
        bindless.binding  = i.layout;

        // bindless.*.register_binding = 0
        bindless.cbv    .register_space = i.layout+1;
        bindless.uav    .register_space = i.layout+1;
        bindless.srv    .register_space = i.layout+1;
        bindless.sampler.register_space = i.layout+1;

        comp.add_hlsl_resource_binding(bindless);
        }

    // comp.remap_num_workgroups_builtin();
    hlsl = comp.compile();
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

  HRESULT hr = compile(shader,hlsl.c_str(),hlsl.size(),exec,optHLSL.shader_model);
  if(hr!=S_OK) {
#if !defined(NDEBUG)
    Log::d(hlsl);
#endif
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule, "\"" + dbg.source + "\"");
    }

  if(stage==ShaderReflection::Stage::Compute) {
    //Log::d(hlsl);
    //disasm();
    }
  }

DxShader::~DxShader() {
  }

D3D12_SHADER_BYTECODE DxShader::bytecode() const {
  return D3D12_SHADER_BYTECODE{shader->GetBufferPointer(),shader->GetBufferSize()};
  }

HRESULT DxShader::compile(ComPtr<ID3DBlob>& shader, const char* hlsl, size_t len, spv::ExecutionModel exec, uint32_t sm) const {
  char target[32] = {};
  ::target(exec,sm,target);
  ComPtr<IDxcCompiler3> compiler;

  HRESULT hr = S_OK;
  //hr = dx.dllApi.DxcCreateInstance(CLSID_DxcCompiler, uuid<IDxcCompiler3>(), reinterpret_cast<void**>(&compiler.get()));
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
    L"-res-may-alias",
    L"-Wno-ambig-lit-shift", // bitextract in spirv-cross
#if !defined(NDEBUG)
    DXC_ARG_DEBUG,
#else
    L"-Qstrip_reflect",
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
