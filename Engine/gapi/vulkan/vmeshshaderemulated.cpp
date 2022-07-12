#if defined(TEMPEST_BUILD_VULKAN)

#include "vmeshshaderemulated.h"

#include <Tempest/File>
#include <libspirv/libspirv.h>

#include "vdevice.h"
#include "gapi/shaderreflection.h"

using namespace Tempest::Detail;

static void debugLog(const char* file, const uint32_t* spv, size_t len) {
  Tempest::WFile f(file);
  f.write(reinterpret_cast<const char*>(spv),len*4);
  }

static void avoidReservedFixup(libspirv::MutableBytecode& code) {
  // avoid _RESERVED_IDENTIFIER_FIXUP_ spit of spirv-cross
  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()!=spv::OpName && i.op()!=spv::OpMemberName)
      continue;
    uint16_t         at   = (i.op()==spv::OpName ? 2 : 3);
    std::string_view name = reinterpret_cast<const char*>(&i[at]);
    if(name=="gl_MeshVerticesNV" ||
       name=="gl_PrimitiveCountNV" ||
       name=="gl_PrimitiveIndicesNV" ||

       name=="gl_MeshPerVertexNV" ||
       name=="gl_Position" ||
       name=="gl_PointSize" ||
       name=="gl_ClipDistance" ||
       name=="gl_CullDistance" ||
       name=="gl_PositionPerViewNV"  ||
       name=="gl_PositionPerViewNV" ) {
      uint32_t w0 = i[at];
      auto* b = reinterpret_cast<char*>(&w0);
      b[0] = 'g';
      b[1] = '1';
      it.set(at,w0);
      }
    }
  }

static void injectCountingPass(libspirv::MutableBytecode& code, const uint32_t idPrimitiveCountNV) {
  auto fn = code.findSectionEnd(libspirv::Bytecode::S_Types);

  const uint32_t tVoid     = code.OpTypeVoid(fn);
  const uint32_t tBool     = code.OpTypeBool(fn);
  const uint32_t tUInt     = code.OpTypeInt(fn,32,false);
  const uint32_t tSInt     = code.OpTypeInt(fn,32,true);
  const uint32_t tVoidFn   = code.OpTypeFunction(fn,tVoid);

  const uint32_t constZero = code.OpConstant(fn,tUInt,0);
  const uint32_t constOne  = code.OpConstant(fn,tUInt,1);

  const uint32_t EngineInternal = code.fetchAddBound();
  fn.insert(spv::OpTypeStruct, {EngineInternal, tUInt, tUInt, tUInt, tSInt, tUInt});

  const uint32_t _ptr_Uniform_uint = code.fetchAddBound();
  fn.insert(spv::OpTypePointer, {_ptr_Uniform_uint, spv::StorageClassUniform, tUInt});

  const uint32_t _ptr_Uniform_EngineInternal = code.fetchAddBound();
  fn.insert(spv::OpTypePointer, {_ptr_Uniform_EngineInternal, spv::StorageClassUniform, EngineInternal});

  const uint32_t vEngine = code.fetchAddBound();
  fn.insert(spv::OpVariable, { _ptr_Uniform_EngineInternal, vEngine, spv::StorageClassUniform});

  fn = code.findSectionEnd(libspirv::Bytecode::S_Annotations);
  fn.insert(spv::OpDecorate, {EngineInternal, spv::DecorationBufferBlock});
  fn.insert(spv::OpDecorate, {vEngine, spv::DecorationDescriptorSet, 1});
  fn.insert(spv::OpDecorate, {vEngine, spv::DecorationBinding, 0});
  fn.insert(spv::OpMemberDecorate, {EngineInternal, 0, spv::DecorationOffset, 0});
  fn.insert(spv::OpMemberDecorate, {EngineInternal, 1, spv::DecorationOffset, 4});
  fn.insert(spv::OpMemberDecorate, {EngineInternal, 2, spv::DecorationOffset, 8});
  fn.insert(spv::OpMemberDecorate, {EngineInternal, 3, spv::DecorationOffset, 12});
  fn.insert(spv::OpMemberDecorate, {EngineInternal, 4, spv::DecorationOffset, 16});
  //fn.insert(spv::OpDecorate, {tUIntArr, spv::DecorationArrayStride, 4}); // TODO: only of redeclarated

  fn = code.findSectionEnd(libspirv::Bytecode::S_Debug);
  fn.insert(spv::OpName,       EngineInternal,    "VkDrawIndexedIndirectCommand");
  fn.insert(spv::OpMemberName, EngineInternal, 0, "indexCount");
  fn.insert(spv::OpMemberName, EngineInternal, 1, "instanceCount");
  fn.insert(spv::OpMemberName, EngineInternal, 2, "firstIndex");
  fn.insert(spv::OpMemberName, EngineInternal, 3, "vertexOffset");
  fn.insert(spv::OpMemberName, EngineInternal, 4, "firstInstance");

  auto ep = code.findOpEntryPoint(spv::ExecutionModelGLCompute,"main");
  auto mainId = (*ep)[2];

  fn = code.end();
  const uint32_t countMain = code.fetchAddBound();
  const uint32_t lblMain   = code.fetchAddBound();
  fn.insert(spv::OpFunction, {tVoid, countMain, spv::FunctionControlMaskNone, tVoidFn});
  fn.insert(spv::OpLabel, {lblMain});
  {
  const uint32_t tmp0 = code.fetchAddBound();
  fn.insert(spv::OpFunctionCall, {tVoid, tmp0, mainId});

  const uint32_t primCount = code.fetchAddBound();
  fn.insert(spv::OpLoad, {tUInt, primCount, idPrimitiveCountNV});

  const uint32_t cond = code.fetchAddBound();
  fn.insert(spv::OpUGreaterThan, {tBool, cond, primCount, constZero});

  const uint32_t condBlockBegin = code.fetchAddBound();
  const uint32_t condBlockEnd   = code.fetchAddBound();
  fn.insert(spv::OpSelectionMerge,    {condBlockEnd, spv::SelectionControlMaskNone});
  fn.insert(spv::OpBranchConditional, {cond, condBlockBegin, condBlockEnd});
  fn.insert(spv::OpLabel,             {condBlockBegin});
  {
    const uint32_t p_indexCount = code.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, p_indexCount, vEngine, constZero});

    const uint32_t p_add = code.fetchAddBound();
    fn.insert(spv::OpAtomicIAdd, {tUInt, p_add, p_indexCount, constOne/*device*/, constZero/*none*/, primCount});
  }
  fn.insert(spv::OpBranch,            {condBlockEnd});
  fn.insert(spv::OpLabel,             {condBlockEnd});
  }
  fn.insert(spv::OpReturn, {});
  fn.insert(spv::OpFunctionEnd, {});

  ep = code.findSectionEnd(libspirv::Bytecode::S_EntryPoint);
  ep.insert(spv::OpEntryPoint, spv::ExecutionModelGLCompute, countMain, "tempest_count_pass");

  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpEntryPoint && i[2]==mainId) {
      it.setToNop();
      }
    if(i.op()==spv::OpName && i[1]==mainId) {
      // to not confuse spirv-cross
      it.setToNop();
      }
    if(i.op()==spv::OpExecutionMode && i[1]==mainId) {
      //it.setToNop();
      it.set(1,countMain);
      }
    }

  // debug info
  fn = code.findSectionEnd(libspirv::Bytecode::S_Debug);
  fn.insert(spv::OpName, countMain, "tempest_count_pass");
  }

VMeshShaderEmulated::VMeshShaderEmulated(VDevice& device, const void *source, size_t src_size)
  :device(device.device.impl) {
  if(src_size%4!=0)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
  {
  // TODO: remove spirv-corss?
  spirv_cross::Compiler comp{reinterpret_cast<const uint32_t*>(source),src_size/4};
  ShaderReflection::getBindings(lay,comp);
  }

  {
  libspirv::MutableBytecode code{reinterpret_cast<const uint32_t*>(source),src_size/4};
  assert(code.findExecutionModel()==spv::ExecutionModelMeshNV);
  avoidReservedFixup(code);

  // meslet entry-point
  auto ep = code.findOpEntryPoint(spv::ExecutionModelMeshNV,"main");
  if(ep!=code.end()) {
    ep.set(1,spv::ExecutionModelGLCompute);
    // remove in/out variables
    if(code.spirvVersion()<0x00010400) {
      /* Starting with version 1.4, the interface’s storage classes are all
       * storage classes used in declaring all global variables
       * referenced by the entry point’s call tree.
       * So on newer spir-v - there is no point of patching entry-point
       */
      auto i  = *ep;
      auto ix = i;
      std::string_view name = reinterpret_cast<const char*>(&i[3]);
      ix.len = uint16_t(3+(name.size()+1+3)/4);

      for(uint16_t r=ix.length(); r<i.length(); ++r) {
        libspirv::Bytecode::OpCode nop = {spv::OpNop, 1};
        ep.set(r,nop);
        }
      ep.set(0,ix);
      }
    }
  // meslet builtins
  uint32_t idPrimitiveCountNV = -1;
  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpDecorate && i[2]==spv::DecorationBuiltIn) {
      if(i[3]==spv::BuiltInPrimitiveCountNV) {
        idPrimitiveCountNV = i[1];
        it.setToNop();
        }
      if(i[3]==spv::BuiltInPrimitiveIndicesNV) {
        it.setToNop();
        }
      }
    if(i.op()==spv::OpExecutionMode &&
       (i[2]==spv::ExecutionModeOutputVertices || i[2]==spv::ExecutionModeOutputLinesNV || i[2]==spv::ExecutionModeOutputPrimitivesNV ||
        i[2]==spv::ExecutionModeOutputTrianglesNV)) {
      it.setToNop();
      }
    if(i.op()==spv::OpExtension) {
      std::string_view name = reinterpret_cast<const char*>(&i[1]);
      if(name=="SPV_NV_mesh_shader")
        it.setToNop();
      }
    if(i.op()==spv::OpCapability && i[1]==spv::CapabilityMeshShadingNV) {
      it.set(1,spv::CapabilityShader);
      }
    }
  // gl_MeshPerVertexNV block
  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpDecorate && i[2]==spv::DecorationBlock) {
      it.setToNop();
      continue;
      }
    if(i.op()==spv::OpMemberDecorate && i[3]==spv::DecorationBuiltIn) {
      it.setToNop();
      continue;
      }
    if(i.op()==spv::OpMemberDecorate && i[3]==spv::DecorationPerViewNV) {
      it.setToNop();
      continue;
      }
    }
  // move in/out to shared memory
  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if((i.op()==spv::OpVariable && i[3]==spv::StorageClassOutput)) {
      it.set(3, spv::StorageClassWorkgroup);
      }
    if((i.op()==spv::OpTypePointer && i[2]==spv::StorageClassOutput)) {
      it.set(2, spv::StorageClassWorkgroup);
      }
    }

  // inject counting buffer
  injectCountingPass(code,idPrimitiveCountNV);

  // nops are not valid to have outsize of block
  code.removeNops();
  debugLog("mesh_count.comp.spv", code.opcodes(), code.size());

  VkShaderModuleCreateInfo createInfo = {};
  createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = src_size;
  createInfo.pCode    = reinterpret_cast<const uint32_t*>(source);
  if(vkCreateShaderModule(device.device.impl,&createInfo,nullptr,&countPass)!=VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
  }


  // TODO
  throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);

  /*
  VkShaderModuleCreateInfo createInfo = {};
  createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = src_size;
  createInfo.pCode    = reinterpret_cast<const uint32_t*>(source);

  {
  spirv_cross::Compiler comp(createInfo.pCode,uint32_t(src_size/4));
  ShaderReflection::getBindings(lay,comp);

  libspirv::Bytecode code(reinterpret_cast<const uint32_t*>(source),src_size/4);
  stage = ShaderReflection::getExecutionModel(code);
  if(stage==ShaderReflection::Compute) {
    for(auto& i:code) {
      if(i.op()!=spv::OpExecutionMode)
        continue;
      if(i[2]==spv::ExecutionModeLocalSize) {
        this->comp.wgSize.x = i[3];
        this->comp.wgSize.y = i[4];
        this->comp.wgSize.z = i[5];
        }
      }
    }
  }

  if(vkCreateShaderModule(device.device.impl,&createInfo,nullptr,&impl)!=VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
  */
  }

VMeshShaderEmulated::~VMeshShaderEmulated() {
  //vkDestroyShaderModule(device,geomPass,nullptr);
  vkDestroyShaderModule(device,countPass,nullptr);
  }

#endif
