#include "meshconverter.h"
#include "libspirv/GLSL.std.450.h"

#include <cassert>
#include <iostream>

static bool isVertexFriendly(spv::StorageClass cls) {
  if(cls==spv::StorageClassWorkgroup ||
     cls==spv::StorageClassInput ||
     cls==spv::StorageClassOutput) {
    return true;
    }
  return false;
  }

MeshConverter::MeshConverter(libspirv::MutableBytecode& code)
  : code(code), an(code) {
  }

void MeshConverter::exec() {
  an.analyze();
  an.analyzeThreadMap();

  // gl_MeshPerVertexNV block
  if(an.idMeshPerVertexNV!=0)
    gl_MeshPerVertexNV.gl_Position = true;
  if(an.idPointSize!=0)
    gl_MeshPerVertexNV.gl_PointSize = true;

  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpDecorate && i[2]==spv::DecorationBuiltIn) {
      if(i[3]==spv::BuiltInPrimitiveCountNV ||
         i[3]==spv::BuiltInPrimitiveIndicesNV) {
        it.setToNop();
        }
      }
    if(i.op()==spv::OpExtInstImport) {
      std::string_view name = reinterpret_cast<const char*>(&i[2]);
      if(name=="GLSL.std.450")
        std450Import = i[1];
      }
    if(i.op()==spv::OpExecutionMode) {
      if(i[2]==spv::ExecutionModeOutputVertices || i[2]==spv::ExecutionModeOutputLinesNV ||
         i[2]==spv::ExecutionModeOutputPrimitivesNV || i[2]==spv::ExecutionModeOutputTrianglesNV) {
        it.setToNop();
        }
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

  // TODO: import std450Import automatically
  assert(std450Import!=0);

  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if((i.op()==spv::OpVariable && i[3]==spv::StorageClassOutput)) {
      it.set(3, spv::StorageClassWorkgroup);
      }
    if((i.op()==spv::OpTypePointer && i[2]==spv::StorageClassOutput)) {
      it.set(2, spv::StorageClassWorkgroup);
      }
    }

  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpDecorate && i[2]==spv::DecorationBlock && i[1]==an.idMeshPerVertexNV) {
      it.setToNop();
      continue;
      }
    if(i.op()==spv::OpDecorate && i[2]==spv::DecorationLocation) {
      it.setToNop();
      continue;
      }
    if(i.op()==spv::OpMemberDecorate && i[3]==spv::DecorationBuiltIn) {
      if(i[4]==spv::BuiltInPosition || i[4]==spv::BuiltInPointSize)
        it.setToNop();
      continue;
      }
    if(i.op()==spv::OpName) {
      auto str = std::string_view(reinterpret_cast<const char*>(&i[2]));
      if(str=="gl_MeshPerVertexNV")
        idGlPerVertex = i[1];
      continue;
      }
    if(i.op()==spv::OpSourceExtension) {
      auto str = std::string_view(reinterpret_cast<const char*>(&i[2]));
      if(str=="GL_NV_mesh_shader")
        it.setToNop();
      continue;
      }
    }

  avoidReservedFixup();

  if(an.canGenerateVs()) {
    generateVsSplit();
    removeMultiview(code);
    removeCullClip(code);
    } else {
    removeMultiview(code);
    removeCullClip(code);
    generateVsDefault();
    }

  // meslet entry-point
  uint32_t idMain = -1;
  {
  auto ep = code.findOpEntryPoint(spv::ExecutionModelMeshNV,"main");
  if(ep!=code.end()) {
    idMain = (*ep)[2];
    ep.set(1,spv::ExecutionModelGLCompute);
    // remove in/out variables
    if(code.spirvVersion()<0x00010400) {
      /* Starting with version 1.4, the interface’s storage classes are all
       * storage classes used in declaring all global variables
       * referenced by the entry point’s call tree.
       * So on newer spir-v - there is no point of patching entry-point
       */

      auto& i  = *ep;
      auto  ix = i;
      std::string_view name = reinterpret_cast<const char*>(&i[3]);
      ix.len = 3+(name.size()+1+3)/4;

      for(uint16_t r=ix.length(); r<i.length(); ++r) {
        auto cp = (*ep)[r];
        if(cp!=an.idLocalInvocationID &&
           cp!=an.idGlobalInvocationID &&
           cp!=an.idWorkGroupID) {
          continue;
          }
        ep.set(ix.len,cp);
        ix.len++;
        }
      for(uint16_t r=ix.length(); r<i.length(); ++r) {
        ep.set(r,libspirv::Bytecode::OpCode{0,1});
        }
      ep.set(0,ix);
      }
    }
  }

  if(an.idLocalInvocationID==0) {
    auto fn = code.findSectionEnd(libspirv::Bytecode::S_Types);
    const uint32_t tUInt             = code.OpTypeInt(fn,32,false);
    const uint32_t v3uint            = code.OpTypeVector(fn,tUInt,3);
    const uint32_t _ptr_Input_v3uint = code.OpTypePointer(fn,spv::StorageClassInput,v3uint);

    an.idLocalInvocationID = code.fetchAddBound();
    fn.insert(spv::OpVariable, {_ptr_Input_v3uint, an.idLocalInvocationID, spv::StorageClassInput});

    fn = code.findSectionEnd(libspirv::Bytecode::S_Debug);
    fn.insert(spv::OpName, an.idLocalInvocationID, "gl_LocalInvocationID");

    fn = code.findSectionEnd(libspirv::Bytecode::S_Annotations);
    fn.insert(spv::OpDecorate, {an.idLocalInvocationID, spv::DecorationBuiltIn, spv::BuiltInLocalInvocationId});

    auto ep = code.findOpEntryPoint(spv::ExecutionModelGLCompute,"main");
    if(ep!=code.end()) {
      ep.append(an.idLocalInvocationID);
      }
    }

  if(an.idWorkGroupID==0) {
    auto fn = code.findSectionEnd(libspirv::Bytecode::S_Types);
    const uint32_t tUInt             = code.OpTypeInt(fn,32,false);
    const uint32_t v3uint            = code.OpTypeVector(fn,tUInt,3);
    const uint32_t _ptr_Input_v3uint = code.OpTypePointer(fn,spv::StorageClassInput,v3uint);

    an.idWorkGroupID = code.fetchAddBound();
    fn.insert(spv::OpVariable, {_ptr_Input_v3uint, an.idWorkGroupID, spv::StorageClassInput});

    fn = code.findSectionEnd(libspirv::Bytecode::S_Debug);
    fn.insert(spv::OpName, an.idWorkGroupID, "gl_WorkGroupID");

    fn = code.findSectionEnd(libspirv::Bytecode::S_Annotations);
    fn.insert(spv::OpDecorate, {an.idWorkGroupID, spv::DecorationBuiltIn, spv::BuiltInWorkgroupId});

    auto ep = code.findOpEntryPoint(spv::ExecutionModelGLCompute,"main");
    if(ep!=code.end()) {
      ep.append(an.idWorkGroupID);
      }
    }

  injectCountingPass(idMain);
  // nops are not valid to have outsize of block
  code.removeNops();
  }

void MeshConverter::vsTypeRemaps(libspirv::MutableBytecode::Iterator& fn,
                                 std::unordered_map<uint32_t,uint32_t>& typeRemaps,
                                 const libspirv::MutableBytecode::AccessChain* ids, uint32_t len) {
  const auto&    op = (*ids[len-1].type);
  const uint32_t id = op[1];
  switch(op.op()) {
    case spv::OpTypeVoid:{
      if(typeRemaps.find(id)==typeRemaps.end()) {
        uint32_t ix = vert.OpTypeVoid(fn);
        typeRemaps[id] = ix;
        }
      break;
      }
    case spv::OpTypeBool:{
      if(typeRemaps.find(id)==typeRemaps.end()) {
        uint32_t ix = vert.OpTypeBool(fn);
        typeRemaps[id] = ix;
        }
      break;
      }
    case spv::OpTypeInt:{
      if(typeRemaps.find(id)==typeRemaps.end()) {
        uint32_t ix = vert.OpTypeInt(fn, op[2], op[3]);
        typeRemaps[id] = ix;
        }
      break;
      }
    case spv::OpTypeFloat:{
      if(typeRemaps.find(id)==typeRemaps.end()) {
        uint32_t ix = vert.OpTypeFloat(fn, op[2]);
        typeRemaps[id] = ix;
        }
      break;
      }
    case spv::OpTypeVector:{
      if(typeRemaps.find(id)==typeRemaps.end()) {
        uint32_t elt = typeRemaps[op[2]];
        uint32_t ix  = vert.OpTypeVector(fn, elt, op[3]);
        typeRemaps[id] = ix;
        }
      break;
      }
    case spv::OpTypeMatrix:{
      if(typeRemaps.find(id)==typeRemaps.end()) {
        uint32_t elt = typeRemaps[op[2]];
        uint32_t ix  = vert.OpTypeMatrix(fn, elt, op[3]);
        typeRemaps[id] = ix;
        }
      break;
      }
    case spv::OpTypeArray:{
      if(typeRemaps.find(id)==typeRemaps.end()) {
        uint32_t csize = 1;
        for(auto& i:code)
          if(i.op()==spv::OpConstant && i[2]==op[3]) {
            csize = i[3];
            break;
            }
        uint32_t uint_t = vert.OpTypeInt(fn, 32, false);
        uint32_t elt    = typeRemaps[op[2]];
        uint32_t sz     = vert.OpConstant(fn, uint_t, csize);
        uint32_t ix     = vert.OpTypeArray(fn, elt, sz);
        typeRemaps[id] = ix;
        }
      break;
      }
    case spv::OpTypeRuntimeArray:{
      if(typeRemaps.find(id)==typeRemaps.end()) {
        uint32_t elt = typeRemaps[op[2]];
        uint32_t ix  = vert.OpTypeRuntimeArray(fn, elt);
        typeRemaps[id] = ix;
        }
      break;
      }
    case spv::OpTypeStruct:{
      if(typeRemaps.find(id)==typeRemaps.end()) {
        std::vector<uint32_t> idx(op.length()-2);
        for(size_t i=0; i<idx.size(); ++i)
          idx[i] = typeRemaps[op[2+i]];
        uint32_t ix = vert.OpTypeStruct(fn, idx.data(), idx.size());
        typeRemaps[id] = ix;
        }
      break;
      }
    case spv::OpTypePointer:{
      if(typeRemaps.find(id)==typeRemaps.end()) {
        uint32_t elt = typeRemaps[op[3]];
        for(auto& i:vert)
          if(i.op()==spv::OpTypeArray && i[1]==elt) {
            // remove arrayness
            elt = i[2];
            break;
            }
        assert(spv::StorageClass(op[2])==spv::StorageClassWorkgroup);
        uint32_t ix  = vert.OpTypePointer(fn, spv::StorageClassOutput, elt);
        typeRemaps[id] = ix;
        }
      break;
      }
    default:
      assert(false);
    }
  }

void MeshConverter::avoidReservedFixup() {
  // avoid _RESERVED_IDENTIFIER_FIXUP_ spit of spirv-cross
  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()!=spv::OpName && i.op()!=spv::OpMemberName)
      continue;
    size_t at = (i.op()==spv::OpName ? 2 : 3);
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

void MeshConverter::injectLoop(libspirv::MutableBytecode::Iterator& fn,
                               uint32_t varI, uint32_t begin, uint32_t end, uint32_t inc,
                               std::function<void(libspirv::MutableBytecode::Iterator& fn)> body) {
  auto  fnT  = code.findSectionEnd(libspirv::Bytecode::S_Types);
  auto  off0 = fnT.toOffset();

  const uint32_t tBool  = code.OpTypeBool(fnT);
  const uint32_t tUInt  = code.OpTypeInt (fnT,32,false);

  const auto     diff   = fnT.toOffset()-off0;
  fn = code.fromOffset(fn.toOffset()+diff);// TODO: test it properly

  fn.insert(spv::OpStore, {varI, begin});
  const uint32_t loopBegin = code.fetchAddBound();
  const uint32_t loopBody  = code.fetchAddBound();
  const uint32_t loopEnd   = code.fetchAddBound();
  const uint32_t loopStep  = code.fetchAddBound();
  const uint32_t loopCond  = code.fetchAddBound();
  const uint32_t rgCond    = code.fetchAddBound();
  const uint32_t rgI0      = code.fetchAddBound();
  const uint32_t rgI1      = code.fetchAddBound();
  const uint32_t rgI2      = code.fetchAddBound();

  fn.insert(spv::OpBranch,            {loopBegin});
  fn.insert(spv::OpLabel,             {loopBegin});

  fn.insert(spv::OpLoopMerge,         {loopEnd, loopStep, spv::SelectionControlMaskNone});
  fn.insert(spv::OpBranch,            {loopCond});

  fn.insert(spv::OpLabel,             {loopCond});
  fn.insert(spv::OpLoad,              {tUInt, rgI0, varI});
  fn.insert(spv::OpULessThan,         {tBool, rgCond, rgI0, end});
  fn.insert(spv::OpBranchConditional, {rgCond, loopBody, loopEnd});

  fn.insert(spv::OpLabel,             {loopBody});
  body(fn);
  fn.insert(spv::OpBranch,            {loopStep});

  fn.insert(spv::OpLabel,             {loopStep});
  fn.insert(spv::OpLoad,              {tUInt, rgI1, varI});
  fn.insert(spv::OpIAdd,              {tUInt, rgI2, rgI1, inc});
  fn.insert(spv::OpStore,             {varI,  rgI2});
  fn.insert(spv::OpBranch,            {loopBegin});

  fn.insert(spv::OpLabel,             {loopEnd});
  }

void MeshConverter::injectCountingPass(const uint32_t idMainFunc) {
  const bool canGenerateVs = an.canGenerateVs();

  auto fn = code.findSectionEnd(libspirv::Bytecode::S_Types);

  const uint32_t void_t               = code.OpTypeVoid(fn);
  const uint32_t bool_t               = code.OpTypeBool(fn);
  const uint32_t uint_t               = code.OpTypeInt(fn,32,false);
  const uint32_t int_t                = code.OpTypeInt(fn,32,true);
  const uint32_t float_t              = code.OpTypeFloat(fn,32);
  const uint32_t func_void            = code.OpTypeFunction(fn,void_t);
  const uint32_t _ptr_Input_uint      = code.OpTypePointer(fn, spv::StorageClassInput,     uint_t);
  const uint32_t _ptr_Uniform_uint    = code.OpTypePointer(fn, spv::StorageClassUniform,   uint_t);
  const uint32_t _ptr_Workgroup_uint  = code.OpTypePointer(fn, spv::StorageClassWorkgroup, uint_t);
  const uint32_t _ptr_Function_uint   = code.OpTypePointer(fn, spv::StorageClassFunction,  uint_t);
  const uint32_t _ptr_Workgroup_float = code.OpTypePointer(fn, spv::StorageClassWorkgroup, float_t);

  const uint32_t _runtimearr_uint     = code.OpTypeRuntimeArray(fn, uint_t);
  const uint32_t IndirectCommand      = code.OpTypeStruct      (fn, {uint_t, uint_t});
  const uint32_t _runtimearr_cmd      = code.OpTypeRuntimeArray(fn, IndirectCommand);

  const uint32_t EngineInternal0      = code.OpTypeStruct (fn, {_runtimearr_cmd});
  const uint32_t EngineInternal1      = code.OpTypeStruct (fn, {uint_t, uint_t, uint_t, uint_t, uint_t, uint_t, _runtimearr_uint});
  const uint32_t EngineInternal2      = code.OpTypeStruct (fn, {_runtimearr_uint});

  const uint32_t _ptr_Uniform_EngineInternal0 = code.OpTypePointer(fn,spv::StorageClassUniform, EngineInternal0);
  const uint32_t _ptr_Uniform_EngineInternal1 = code.OpTypePointer(fn,spv::StorageClassUniform, EngineInternal1);
  const uint32_t _ptr_Uniform_EngineInternal2 = code.OpTypePointer(fn,spv::StorageClassUniform, EngineInternal2);

  const uint32_t const0   = code.OpConstant(fn,uint_t,0);
  const uint32_t const1   = code.OpConstant(fn,uint_t,1);
  const uint32_t const2   = code.OpConstant(fn,uint_t,2);
  const uint32_t const3   = code.OpConstant(fn,uint_t,3);
  const uint32_t const6   = code.OpConstant(fn,uint_t,6);
  const uint32_t const8   = code.OpConstant(fn,uint_t,8);
  const uint32_t const10  = code.OpConstant(fn,uint_t,10);
  const uint32_t const18  = code.OpConstant(fn,uint_t,18);
  const uint32_t const128 = code.OpConstant(fn,uint_t,128);
  const uint32_t const264 = code.OpConstant(fn,uint_t,264);

  std::unordered_map<uint32_t,uint32_t> constants;
  uint32_t varCount = 0;
  if(canGenerateVs) {
    varCount = 0;
    } else {
    for(auto& i:an.varying) {
      // preallocate indexes
      code.traverseType(i.second.type,[&](const libspirv::MutableBytecode::AccessChain* ids, uint32_t len) {
        if(!code.isBasicTypeDecl(ids[len-1].type->op()))
          return;
        if(len<2 || ids[1].index!=0)
          return; // [max_vertex] arrayness
        for(uint32_t i=0; i<len; ++i) {
          uint32_t x = ids[i].index;
          if(constants.find(x)==constants.end())
            constants[x] = code.OpConstant(fn,uint_t, x);
          }
        if(constants.find(varCount)==constants.end())
          constants[varCount] = code.OpConstant(fn,uint_t, varCount);
        ++varCount;
        });
      }
    }

  const uint32_t topology = code.OpConstant(fn,uint_t,3);
  const uint32_t varSize  = code.OpConstant(fn,uint_t,varCount);

  const uint32_t vEngine0 = code.OpVariable(fn, _ptr_Uniform_EngineInternal0, spv::StorageClassUniform);
  const uint32_t vEngine1 = code.OpVariable(fn, _ptr_Uniform_EngineInternal1, spv::StorageClassUniform);
  const uint32_t vEngine2 = code.OpVariable(fn, _ptr_Uniform_EngineInternal2, spv::StorageClassUniform);

  fn = code.findSectionEnd(libspirv::Bytecode::S_Annotations);
  fn.insert(spv::OpDecorate, {_runtimearr_uint, spv::DecorationArrayStride, 4});
  fn.insert(spv::OpDecorate, {_runtimearr_cmd,  spv::DecorationArrayStride, 4*2});

  fn.insert(spv::OpDecorate, {EngineInternal0, spv::DecorationBufferBlock});
  fn.insert(spv::OpDecorate, {vEngine0, spv::DecorationDescriptorSet, 1});
  fn.insert(spv::OpDecorate, {vEngine0, spv::DecorationBinding, 0});

  fn.insert(spv::OpMemberDecorate, {EngineInternal0, 0, spv::DecorationOffset, 0});
  for(uint32_t i=0; i<2; ++i)
    fn.insert(spv::OpMemberDecorate, {IndirectCommand, i, spv::DecorationOffset, i*4});

  fn.insert(spv::OpDecorate, {EngineInternal1, spv::DecorationBufferBlock});
  fn.insert(spv::OpDecorate, {vEngine1, spv::DecorationDescriptorSet, 1});
  fn.insert(spv::OpDecorate, {vEngine1, spv::DecorationBinding, 1});
  for(uint32_t i=0; i<7; ++i)
    fn.insert(spv::OpMemberDecorate, {EngineInternal1, i, spv::DecorationOffset, i*4});

  fn.insert(spv::OpDecorate, {EngineInternal2, spv::DecorationBufferBlock});
  fn.insert(spv::OpDecorate, {vEngine2, spv::DecorationDescriptorSet, 1});
  fn.insert(spv::OpDecorate, {vEngine2, spv::DecorationBinding, 2});
  for(uint32_t i=0; i<1; ++i)
    fn.insert(spv::OpMemberDecorate, {EngineInternal2, i, spv::DecorationOffset, i*4});

  fn = code.findSectionEnd(libspirv::Bytecode::S_Debug);
  fn.insert(spv::OpName,       IndirectCommand,    "IndirectCommand");
  fn.insert(spv::OpMemberName, IndirectCommand, 0, "indexCount");
  fn.insert(spv::OpMemberName, IndirectCommand, 1, "varyingCount");
  fn.insert(spv::OpName,       EngineInternal0,    "EngineInternal0");

  fn.insert(spv::OpName,       EngineInternal1,    "EngineInternal1");
  fn.insert(spv::OpMemberName, EngineInternal1, 0, "varGrow");
  fn.insert(spv::OpMemberName, EngineInternal1, 1, "grow");
  fn.insert(spv::OpMemberName, EngineInternal1, 2, "meshletCount");
  fn.insert(spv::OpMemberName, EngineInternal1, 3, "dispatchX");
  fn.insert(spv::OpMemberName, EngineInternal1, 4, "dispatchY");
  fn.insert(spv::OpMemberName, EngineInternal1, 5, "dispatchZ");
  fn.insert(spv::OpMemberName, EngineInternal1, 6, "desc");

  fn.insert(spv::OpName,       EngineInternal2,    "EngineInternal2");
  fn.insert(spv::OpMemberName, EngineInternal2, 0, "heap");

  // engine-level main
  fn = code.end();
  const uint32_t engineMain = code.fetchAddBound();
  const uint32_t lblMain    = code.fetchAddBound();
  fn.insert(spv::OpFunction, {void_t, engineMain, spv::FunctionControlMaskNone, func_void});
  fn.insert(spv::OpLabel, {lblMain});

  const uint32_t varI   = code.OpVariable(fn, _ptr_Function_uint, spv::StorageClassFunction);
  const uint32_t maxInd = code.OpVariable(fn, _ptr_Function_uint, spv::StorageClassFunction);

  const uint32_t tmp0 = code.fetchAddBound();
  fn.insert(spv::OpFunctionCall, {void_t, tmp0, idMainFunc});

  fn.insert(spv::OpMemoryBarrier,  {const1, const264});         // memoryBarrierShared() // needed?
  fn.insert(spv::OpControlBarrier, {const2, const2, const264}); // barrier()

  const uint32_t primCount = code.fetchAddBound();
  fn.insert(spv::OpLoad, {uint_t, primCount, an.idPrimitiveCountNV});

  // gl_PrimitiveCountNV <= 0
  {
    const uint32_t cond0 = code.fetchAddBound();
    fn.insert(spv::OpULessThanEqual, {bool_t, cond0, primCount, const0});

    const uint32_t condBlockBegin = code.fetchAddBound();
    const uint32_t condBlockEnd   = code.fetchAddBound();
    fn.insert(spv::OpSelectionMerge,    {condBlockEnd, spv::SelectionControlMaskNone});
    fn.insert(spv::OpBranchConditional, {cond0, condBlockBegin, condBlockEnd});
    fn.insert(spv::OpLabel,             {condBlockBegin});
    fn.insert(spv::OpReturn,            {});
    fn.insert(spv::OpLabel,             {condBlockEnd});
  }

  // gl_LocalInvocationID.x != 0
  {
    const uint32_t ptrInvocationIdX = code.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Input_uint, ptrInvocationIdX, an.idLocalInvocationID, const0});
    const uint32_t invocationId = code.fetchAddBound();
    fn.insert(spv::OpLoad, {uint_t, invocationId, ptrInvocationIdX});
    const uint32_t cond1 = code.fetchAddBound();
    fn.insert(spv::OpINotEqual, {bool_t, cond1, invocationId, const0});

    const uint32_t condBlockBegin = code.fetchAddBound();
    const uint32_t condBlockEnd   = code.fetchAddBound();
    fn.insert(spv::OpSelectionMerge,    {condBlockEnd, spv::SelectionControlMaskNone});
    fn.insert(spv::OpBranchConditional, {cond1, condBlockBegin, condBlockEnd});
    fn.insert(spv::OpLabel,             {condBlockBegin});
    fn.insert(spv::OpReturn,            {});
    fn.insert(spv::OpLabel,             {condBlockEnd});
  }

  // indSize = gl_PrimitiveCountNV*3
  const uint32_t indSize = code.fetchAddBound();
  fn.insert(spv::OpIMul, {uint_t, indSize, primCount, topology});

  // Max Vertex. Assume empty output
  fn.insert(spv::OpStore, {maxInd, const0});
  injectLoop(fn, varI, const0, indSize, const1, [&](libspirv::MutableBytecode::Iterator& fn) {
    const uint32_t rI = code.fetchAddBound();
    fn.insert(spv::OpLoad, {uint_t, rI, varI});

    const uint32_t ptrIndicesNV = code.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Workgroup_uint, ptrIndicesNV, an.idPrimitiveIndicesNV, rI});

    const uint32_t rInd = code.fetchAddBound();
    fn.insert(spv::OpLoad, {uint_t, rInd, ptrIndicesNV});

    uint32_t a = code.fetchAddBound();
    fn.insert(spv::OpLoad, {uint_t, a, maxInd});

    uint32_t b = code.fetchAddBound();
    fn.insert(spv::OpExtInst, {uint_t, b, std450Import, GLSLstd450UMax, a, rInd});

    fn.insert(spv::OpStore, {maxInd, b});
    });

  // Fair counting of indices
  const uint32_t rMaxInd = code.fetchAddBound();
  fn.insert(spv::OpLoad, {uint_t, rMaxInd, maxInd});
  const uint32_t maxVertex = code.fetchAddBound();
  fn.insert(spv::OpIAdd, {uint_t, maxVertex, rMaxInd, const1});

  const uint32_t maxVar = code.fetchAddBound();
  fn.insert(spv::OpIMul, {uint_t, maxVar, maxVertex, varSize});
  {
  const uint32_t ptrWorkGroupID = code.fetchAddBound();
  fn.insert(spv::OpAccessChain, {_ptr_Input_uint, ptrWorkGroupID, an.idWorkGroupID, const1});
  const uint32_t workIdX = code.fetchAddBound();
  fn.insert(spv::OpLoad, {uint_t, workIdX, ptrWorkGroupID});

  const uint32_t ptrCmdDest = code.fetchAddBound();
  fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrCmdDest, vEngine0, const0, workIdX, const0});
  const uint32_t cmdDest  = code.fetchAddBound();
  fn.insert(spv::OpAtomicIAdd, {uint_t, cmdDest, ptrCmdDest, const1/*scope*/, const0/*semantices*/, indSize});

  if(!canGenerateVs) {
    const uint32_t ptrCmdDest2 = code.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrCmdDest2, vEngine0, const0, workIdX, const1});
    const uint32_t cmdDest2  = code.fetchAddBound();
    fn.insert(spv::OpAtomicIAdd, {uint_t, cmdDest2, ptrCmdDest2, const1/*scope*/, const0/*semantices*/, maxVar});
    }
  }

  const uint32_t heapAllocSz = code.fetchAddBound();
  if(canGenerateVs) {
    fn.insert(spv::OpIAdd, {uint_t, heapAllocSz, const0, indSize});
    } else {
    fn.insert(spv::OpIAdd, {uint_t, heapAllocSz, maxVar, indSize});
    }

  // uint heapDest  = atomicAdd(mesh.varGrow, indSize + maxVar);
  const uint32_t ptrHeapDest = code.fetchAddBound();
  fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrHeapDest, vEngine1, const0});
  const uint32_t heapDest  = code.fetchAddBound();
  fn.insert(spv::OpAtomicIAdd, {uint_t, heapDest, ptrHeapDest, const1/*scope*/, const0/*semantices*/, heapAllocSz});

  // uint meshDest = atomicAdd(mesh.grow, 1)*3;
  const uint32_t ptrMeshDest = code.fetchAddBound();
  fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrMeshDest, vEngine1, const1});
  const uint32_t meshDestRaw  = code.fetchAddBound();
  fn.insert(spv::OpAtomicIAdd, {uint_t, meshDestRaw, ptrMeshDest, const1/*scope*/, const0/*semantices*/, const1});

  // uint varDest = indDest + indSize;
  const uint32_t varDest  = code.fetchAddBound();
  fn.insert(spv::OpIAdd, {uint_t, varDest, heapDest, indSize});

  // meshDest = grow*3;
  const uint32_t meshDest = code.fetchAddBound();
  fn.insert(spv::OpIMul, {uint_t, meshDest, meshDestRaw, const3});

  // Writeout indexes
  {
  uint32_t wgMasked = 0;
  if(canGenerateVs) {
    const uint32_t ptrWorkGroupIdX = code.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Input_uint, ptrWorkGroupIdX, an.idWorkGroupID, const0});
    const uint32_t workGroupId = code.fetchAddBound();
    fn.insert(spv::OpLoad, {uint_t, workGroupId, ptrWorkGroupIdX});
    wgMasked = code.fetchAddBound();
    fn.insert(spv::OpShiftLeftLogical, {uint_t, wgMasked, workGroupId, const8});
    }

  injectLoop(fn, varI, const0, indSize, const1, [&](libspirv::MutableBytecode::Iterator& fn) {
    const uint32_t rI = code.fetchAddBound();
    fn.insert(spv::OpLoad, {uint_t, rI, varI});
    const uint32_t rDst = code.fetchAddBound();
    fn.insert(spv::OpIAdd, {uint_t, rDst, rI, heapDest});
    const uint32_t ptrHeap = code.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrHeap, vEngine2, const0, rDst});

    const uint32_t ptrIndicesNV = code.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Workgroup_uint, ptrIndicesNV, an.idPrimitiveIndicesNV, rI});

    const uint32_t rInd = code.fetchAddBound();
    fn.insert(spv::OpLoad, {uint_t, rInd, ptrIndicesNV});

    if(canGenerateVs) {
      const uint32_t invId = code.fetchAddBound();
      fn.insert(spv::OpBitwiseOr, {uint_t, invId, wgMasked, rInd});
      fn.insert(spv::OpStore, {ptrHeap, invId});
      } else {
      fn.insert(spv::OpStore, {ptrHeap, rInd});
      }
    });
  }

  // Writeout varying
  if(!canGenerateVs) {
    injectLoop(fn, varI, const0, maxVertex, const1, [&](libspirv::MutableBytecode::Iterator& fn) {
      // uint at = varDest + i*varSize;
      const uint32_t rI = code.fetchAddBound();
      fn.insert(spv::OpLoad, {uint_t, rI, varI});
      const uint32_t rTmp0 = code.fetchAddBound();
      fn.insert(spv::OpIMul, {uint_t, rTmp0, rI, varSize});
      const uint32_t rAt = code.fetchAddBound();
      fn.insert(spv::OpIAdd, {uint_t, rAt, rTmp0, varDest});

      libspirv::MutableBytecode b;
      auto block = b.end();
      uint32_t seq  = 0;
      for(auto& i:an.varying) {
        uint32_t type = i.second.type;
        code.traverseType(type,[&](const libspirv::MutableBytecode::AccessChain* ids, uint32_t len) {
          if(!code.isBasicTypeDecl(ids[len-1].type->op()))
            return;
          if(len<2 || ids[1].index!=0)
            return; // [max_vertex] arrayness
          // at += memberId
          const uint32_t rDst = code.fetchAddBound();
          block.insert(spv::OpIAdd, {uint_t, rDst, rAt, constants[seq]});
          ++seq;
          const uint32_t ptrHeap = code.fetchAddBound();
          block.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrHeap, vEngine2, const0, rDst});

          // NOTE: ids is pointer to array of X, we need only X
          const uint32_t varPtr = code.fetchAddBound();
          uint32_t chain[255] = {_ptr_Workgroup_float, varPtr, i.first, rI};
          uint32_t chainSz    = 4;
          for(uint32_t i=2; i+1<len; ++i) {
            chain[chainSz] = constants[ids[i].index];
            ++chainSz;
            }

          block.insert(spv::OpAccessChain, chain, chainSz);
          const uint32_t valR = code.fetchAddBound();
          if(ids[len-1].type->op()==spv::OpTypeFloat) {
            const uint32_t rCast = code.fetchAddBound();
            chain[0] = _ptr_Workgroup_float;
            block.insert(spv::OpLoad,    {float_t, valR,  varPtr});
            block.insert(spv::OpBitcast, {uint_t,  rCast, valR});
            block.insert(spv::OpStore,   {ptrHeap, rCast});
            } else {
            chain[0] = _ptr_Workgroup_uint;
            block.insert(spv::OpLoad,  {uint_t,  valR, varPtr});
            block.insert(spv::OpStore, {ptrHeap, valR});
            }
          });
        }
      fn.insert(b);
      });
    }

  // Writeout meshlet descriptor
  if(canGenerateVs) {
    const uint32_t ptrWorkGroupID = code.fetchAddBound();
    const uint32_t workIdY        = code.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Input_uint, ptrWorkGroupID, an.idWorkGroupID, const1});
    fn.insert(spv::OpLoad,        {uint_t, workIdY, ptrWorkGroupID});

    const uint32_t ptrHeap0 = code.fetchAddBound();
    const uint32_t dest1    = code.fetchAddBound();
    const uint32_t ptrHeap1 = code.fetchAddBound();
    const uint32_t dest2    = code.fetchAddBound();
    const uint32_t ptrHeap2 = code.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrHeap0, vEngine1, const6, meshDest});
    fn.insert(spv::OpIAdd, {uint_t, dest1, meshDest, const1});
    fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrHeap1, vEngine1, const6, dest1});
    fn.insert(spv::OpIAdd, {uint_t, dest2, meshDest, const2});
    fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrHeap2, vEngine1, const6, dest2});

    fn.insert(spv::OpStore, {ptrHeap0, workIdY});
    fn.insert(spv::OpStore, {ptrHeap1, heapDest});
    fn.insert(spv::OpStore, {ptrHeap2, indSize});
    } else {
    const uint32_t ptrWorkGroupID = code.fetchAddBound();
    const uint32_t workIdY        = code.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Input_uint, ptrWorkGroupID, an.idWorkGroupID, const1});
    fn.insert(spv::OpLoad,        {uint_t, workIdY, ptrWorkGroupID});

    const uint32_t ptrHeap0 = code.fetchAddBound();
    const uint32_t dest1    = code.fetchAddBound();
    const uint32_t ptrHeap1 = code.fetchAddBound();
    const uint32_t dest2    = code.fetchAddBound();
    const uint32_t ptrHeap2 = code.fetchAddBound();

    fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrHeap0, vEngine1, const6, meshDest});
    fn.insert(spv::OpIAdd,        {uint_t, dest1, meshDest, const1});
    fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrHeap1, vEngine1, const6, dest1});
    fn.insert(spv::OpIAdd,        {uint_t, dest2, meshDest, const2});
    fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrHeap2, vEngine1, const6, dest2});

    const uint32_t tmp0 = code.fetchAddBound();
    fn.insert(spv::OpShiftLeftLogical, {uint_t, tmp0, maxVertex, const10});
    const uint32_t tmp1 = code.fetchAddBound();
    fn.insert(spv::OpShiftLeftLogical, {uint_t, tmp1, varSize, const18});
    const uint32_t tmp2 = code.fetchAddBound();
    fn.insert(spv::OpBitwiseOr, {uint_t, tmp2, tmp1, tmp0});
    const uint32_t tmp3 = code.fetchAddBound();
    fn.insert(spv::OpBitwiseOr, {uint_t, tmp3, tmp2, indSize});

    fn.insert(spv::OpStore, {ptrHeap0, workIdY});
    fn.insert(spv::OpStore, {ptrHeap1, heapDest});
    fn.insert(spv::OpStore, {ptrHeap2, tmp3});
  }

  fn.insert(spv::OpReturn, {});
  fn.insert(spv::OpFunctionEnd, {});

  // entry-point
  replaceEntryPoint(idMainFunc,engineMain);

  // debug info
  fn = code.findSectionEnd(libspirv::Bytecode::S_Debug);
  fn.insert(spv::OpName, engineMain, "main");
  fn.insert(spv::OpName, varI,       "i");
  }

void MeshConverter::replaceEntryPoint(const uint32_t idMainFunc, const uint32_t idEngineMain) {
  auto ep = code.findOpEntryPoint(spv::ExecutionModelGLCompute,"main");
  ep.set(2,idEngineMain);

  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpEntryPoint && i[2]==idMainFunc) {
      it.setToNop();
      }
    if(i.op()==spv::OpName && i[1]==idMainFunc) {
      // to not confuse spirv-cross
      it.setToNop();
      }
    if(i.op()==spv::OpExecutionMode && i[1]==idMainFunc) {
      it.set(1,idEngineMain);
      }
    }
  }

void MeshConverter::removeMultiview(libspirv::MutableBytecode& code) {
  if(an.idMeshPerVertexNV==0)
    return;

  std::unordered_set<uint32_t> fields;
  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpMemberDecorate && i[3]==spv::DecorationPerViewNV) {
      fields.insert(i[2]);
      continue;
      }
    }

  removeFromPerVertex(code,fields);
  }

void MeshConverter::removeCullClip(libspirv::MutableBytecode& code) {
  if(an.idMeshPerVertexNV==0)
    return;

  std::unordered_set<uint32_t> perView;
  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()!=spv::OpMemberDecorate || i[3]!=spv::DecorationBuiltIn)
      continue;

    if(i[4]==spv::BuiltInClipDistance) {
      perView.insert(i[2]);
      continue;
      }
    if(i[4]==spv::BuiltInCullDistance) {
      perView.insert(i[2]);
      continue;
      }
    }

  removeFromPerVertex(code,perView);
  }

void MeshConverter::removeFromPerVertex(libspirv::MutableBytecode& code,
                                        const std::unordered_set<uint32_t>& fields) {
  if(fields.size()==0)
    return;

  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if((i.op()!=spv::OpMemberDecorate && i.op()!=spv::OpMemberName) || i[1]!=an.idMeshPerVertexNV)
      continue;
    if(fields.find(i[2])==fields.end())
      continue;
    it.setToNop();
    }

  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()!=spv::OpTypeStruct)
      continue;
    if(an.idMeshPerVertexNV!=i[1])
      continue;

    uint16_t argc = 1;
    uint32_t args[16] = {};
    args[0] = i[1];
    for(size_t r=2; r<i.length(); ++r) {
      if(fields.find(r-2)!=fields.end())
        continue;
      args[argc] = i[r];
      ++argc;
      }
    it.setToNop();
    it.insert(spv::OpTypeStruct,args,argc);
    break;
    }
  }

uint32_t MeshConverter::mappingTable(libspirv::MutableBytecode::Iterator& fn, uint32_t arrType, uint32_t eltType) {
  const uint32_t vRet = vert.fetchAddBound();

  uint32_t table[ShaderAnalyzer::MaxIndex+2] = {};
  table[0] = arrType;
  table[1] = vRet;
  for(size_t i=0; i<an.outputIndexes; ++i)
    table[i+2] = vert.OpConstant(fn,eltType, an.threadMapping[i]);
  fn.insert(spv::OpConstantComposite, table, an.outputIndexes+2);
  return vRet;
  }

uint32_t MeshConverter::declareGlPerVertex(libspirv::MutableBytecode::Iterator& fn) {
  const uint32_t float_t   = vert.OpTypeFloat (fn, 32);
  const uint32_t v4float_t = vert.OpTypeVector(fn, float_t, 4);

  size_t   sz   = 1;
  uint32_t m[5] = {};
  m[0] = v4float_t;
  if(gl_MeshPerVertexNV.gl_PointSize) {
    m[sz] = float_t; ++sz;
    }

  const uint32_t perVertex_t = vert.OpTypeStruct(fn, m, sz);
  return perVertex_t;
  }

void MeshConverter::generateVsDefault() {
  const uint32_t glslStd                      = vert.fetchAddBound();
  const uint32_t main                         = vert.fetchAddBound();
  const uint32_t lblMain                      = vert.fetchAddBound();
  const uint32_t gl_VertexIndex               = vert.fetchAddBound();

  const uint32_t void_t                       = vert.fetchAddBound();
  const uint32_t bool_t                       = vert.fetchAddBound();
  const uint32_t int_t                        = vert.fetchAddBound();
  const uint32_t uint_t                       = vert.fetchAddBound();
  const uint32_t float_t                      = vert.fetchAddBound();
  const uint32_t _runtimearr_uint             = vert.fetchAddBound();
  const uint32_t func_t                       = vert.fetchAddBound();

  const uint32_t EngineInternal3              = vert.fetchAddBound();
  const uint32_t _ptr_Input_int               = vert.fetchAddBound();
  const uint32_t _ptr_Uniform_uint            = vert.fetchAddBound();
  const uint32_t _ptr_Uniform_EngineInternal3 = vert.fetchAddBound();
  const uint32_t _ptr_Output_uint             = vert.fetchAddBound();
  const uint32_t _ptr_Output_float            = vert.fetchAddBound();

  const uint32_t vEngine3                     = vert.fetchAddBound();

  std::unordered_map<uint32_t,uint32_t> varRemaps;
  for(auto& i:an.varying) {
    varRemaps[i.first] = vert.fetchAddBound();
    }

  auto fn = vert.end();
  fn.insert(spv::OpCapability,       {spv::CapabilityShader});
  fn.insert(spv::OpExtInstImport,    glslStd, "GLSL.std.450");
  fn.insert(spv::OpMemoryModel,      {spv::AddressingModelLogical, spv::MemoryModelGLSL450});
  {
  const uint32_t ep = fn.toOffset();
  fn.insert(spv::OpEntryPoint,       {spv::ExecutionModelVertex, main, 0x6E69616D, 0x0, gl_VertexIndex});
  auto e = vert.fromOffset(ep);
  for(auto& i:varRemaps)
    e.append(i.second);
  fn = vert.end();
  }
  fn.insert(spv::OpSource,           {spv::SourceLanguageGLSL, 450});

  fn.insert(spv::OpName,             EngineInternal3,    "EngineInternal3");
  fn.insert(spv::OpMemberName,       EngineInternal3, 0, "heap");

  fn.insert(spv::OpDecorate,         {_runtimearr_uint, spv::DecorationArrayStride, 4});
  fn.insert(spv::OpDecorate,         {EngineInternal3,  spv::DecorationBufferBlock});
  fn.insert(spv::OpMemberDecorate,   {EngineInternal3, 0, spv::DecorationOffset, 0});
  fn.insert(spv::OpDecorate,         {vEngine3, spv::DecorationDescriptorSet, 1});
  fn.insert(spv::OpDecorate,         {vEngine3, spv::DecorationBinding, 0});

  fn.insert(spv::OpTypeVoid,         {void_t});
  fn.insert(spv::OpTypeBool,         {bool_t});
  fn.insert(spv::OpTypeInt,          {uint_t,  32, 0});
  fn.insert(spv::OpTypeInt,          {int_t,   32, 1});
  fn.insert(spv::OpTypeFloat,        {float_t, 32});
  fn.insert(spv::OpTypeFunction,     {func_t, void_t});
  fn.insert(spv::OpTypeRuntimeArray, {_runtimearr_uint, uint_t});

  std::unordered_map<uint32_t,uint32_t> constants;
  {
  uint32_t varCount = 0;
  for(auto& i:an.varying) {
    // preallocate indexes
    code.traverseType(i.second.type,[&](const libspirv::MutableBytecode::AccessChain* ids, uint32_t len) {
      if(!code.isBasicTypeDecl(ids[len-1].type->op()))
        return;
      if(len<2 || ids[1].index!=0)
        return; // [max_vertex] arrayness
      for(uint32_t i=0; i<len; ++i) {
        uint32_t x = ids[i].index;
        if(constants.find(x)==constants.end())
          constants[x] = vert.OpConstant(fn,uint_t, x);
        }
      if(constants.find(varCount)==constants.end())
        constants[varCount] = vert.OpConstant(fn,uint_t, varCount);
      ++varCount;
      });
    }
  }

  std::unordered_map<uint32_t,uint32_t> typeRemaps;
  for(auto& i:an.varying) {
    code.traverseType(i.second.type,[&](const libspirv::MutableBytecode::AccessChain* ids, uint32_t len) {
      vsTypeRemaps(fn,typeRemaps,ids,len);
      },libspirv::Bytecode::T_PostOrder);
    }

  // uniform types
  fn.insert(spv::OpTypeStruct,       {EngineInternal3,              _runtimearr_uint});
  fn.insert(spv::OpTypePointer,      {_ptr_Uniform_EngineInternal3, spv::StorageClassUniform, EngineInternal3});
  fn.insert(spv::OpTypePointer,      {_ptr_Uniform_uint,            spv::StorageClassUniform, uint_t});
  fn.insert(spv::OpTypePointer,      {_ptr_Input_int,               spv::StorageClassInput,   int_t});

  fn.insert(spv::OpTypePointer,      {_ptr_Output_uint,             spv::StorageClassOutput,  uint_t});
  fn.insert(spv::OpTypePointer,      {_ptr_Output_float,            spv::StorageClassOutput,  float_t});

  // uniform variables
  fn.insert(spv::OpVariable,         {_ptr_Uniform_EngineInternal3, vEngine3, spv::StorageClassUniform});

  // input
  fn.insert(spv::OpVariable,         {_ptr_Input_int, gl_VertexIndex, spv::StorageClassInput});

  // varyings variables
  for(auto& i:an.varying) {
    uint32_t tId = typeRemaps[i.second.type];
    uint32_t id  = varRemaps [i.first];
    fn.insert(spv::OpVariable,       {tId, id, spv::StorageClassOutput});
    }

  fn.insert(spv::OpFunction,         {void_t, main, spv::FunctionControlMaskNone, func_t});
  fn.insert(spv::OpLabel,            {lblMain});
  {
  const uint32_t rAt = vert.fetchAddBound();
  fn.insert(spv::OpLoad, {int_t, rAt, gl_VertexIndex});

  uint32_t seq = 0;
  for(auto& i:an.varying) {
    uint32_t varId = varRemaps[i.first];
    uint32_t type  = i.second.type;
    code.traverseType(type,[&](const libspirv::MutableBytecode::AccessChain* ids, uint32_t len) {
      if(!code.isBasicTypeDecl(ids[len-1].type->op()))
        return;
      if(len<2 || ids[1].index!=0)
        return; // [max_vertex] arrayness
      const uint32_t ptrR = vert.fetchAddBound();
      const uint32_t valR = vert.fetchAddBound();
      const uint32_t rDst = vert.fetchAddBound();
      fn.insert(spv::OpIAdd,        {uint_t, rDst, rAt, constants[seq]});
      fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrR, vEngine3, constants[0], rDst});
      fn.insert(spv::OpLoad,        {uint_t, valR, ptrR});

      const uint32_t ptrL  = vert.fetchAddBound();
      const uint32_t rCast = vert.fetchAddBound();
      // NOTE: ids is pointer to array of X, we need only X
      const uint32_t ptrVar = code.fetchAddBound();
      uint32_t chain[255] = {_ptr_Output_float, ptrL, varId};
      uint32_t chainSz    = 3;
      for(uint32_t i=2; i+1<len; ++i) {
        chain[chainSz] = constants[ids[i].index];
        ++chainSz;
        }

      if(ids[len-1].type->op()==spv::OpTypeFloat) {
        chain[0] = _ptr_Output_float;
        fn.insert(spv::OpAccessChain, chain, chainSz);
        fn.insert(spv::OpBitcast,     {float_t,  rCast, valR});
        fn.insert(spv::OpStore,       {ptrL, rCast});
        } else {
        chain[0] = _ptr_Output_uint;
        fn.insert(spv::OpLoad,  {uint_t, valR, ptrVar});
        fn.insert(spv::OpStore, {ptrL,   valR});
        }

      ++seq;
      });
    }
  }
  fn.insert(spv::OpReturn,           {});
  fn.insert(spv::OpFunctionEnd,      {});

  {
  auto fn = vert.findSectionEnd(libspirv::Bytecode::S_Annotations);
  for(auto& i:an.varying) {
    uint32_t varId = varRemaps[i.first];
    uint32_t type  = i.second.type;
    for(auto& v:an.varying) {
      if(v.second.type!=type)
        continue;
      uint32_t loc = v.second.location;
      if(loc==uint32_t(-1))
        continue;
      fn.insert(spv::OpDecorate, {varId, spv::DecorationLocation, loc});
      }
    }
  annotateVertexBuiltins(vert,gl_VertexIndex,typeRemaps[idGlPerVertex]);
  }
  }

void MeshConverter::generateVsSplit() {
  vert.fetchAddBound(code.bound()-vert.bound());
  auto gen = vert.end();

  const uint32_t idVertexIndex = vert.fetchAddBound();
  std::unordered_map<uint32_t,uint32_t> varRemaps;
  for(auto& i:an.varying) {
    varRemaps[i.first] = vert.fetchAddBound();
    }

  std::unordered_set<uint32_t> var;
  // copy most of mesh code
  for(auto it = code.begin(); it!=code.end(); ++it) {
    auto& i = *it;
    if(i.op()==spv::OpCapability) {
      if(i[1]==spv::CapabilityMeshShadingNV) {
        gen.insert(spv::OpCapability,{spv::CapabilityShader});
        continue;
        }
      }
    if(i.op()==spv::OpExecutionMode) {
      continue;
      }
    if(i.op()==spv::OpEntryPoint) {
      // OpEntryPoint Vertex i[2] "main"
      gen.insert(spv::OpEntryPoint, {spv::ExecutionModelVertex, i[2], 0x6E69616D, 0x0});
      continue;
      }
    if(i.op()==spv::OpDecorate) {
      if(i[2]==spv::DecorationBuiltIn) {
        if(i[3]==spv::BuiltInPrimitiveCountNV ||
           i[3]==spv::BuiltInPrimitiveIndicesNV ||
           i[3]==spv::BuiltInLocalInvocationId ||
           i[3]==spv::BuiltInGlobalInvocationId ||
           i[3]==spv::BuiltInWorkgroupSize ||
           i[3]==spv::BuiltInWorkgroupId) {
          continue;
          }
        }
      if(i[2]==spv::DecorationBlock) {
        if(i[1]==an.idMeshPerVertexNV)
          continue;
        }
      }
    if(i.op()==spv::OpMemberDecorate && i[3]==spv::DecorationBuiltIn) {
      continue;
      }

    if(i.op()==spv::OpTypePointer) {
      uint32_t cls = i[2];
      if(isVertexFriendly(spv::StorageClass(cls))) {
        cls = spv::StorageClassPrivate;
        }
      gen.insert(i.op(),{i[1],cls,i[3]});
      continue;
      }
    if(i.op()==spv::OpVariable) {
      uint32_t cls = i[3];
      if(isVertexFriendly(spv::StorageClass(cls))) {
        cls = spv::StorageClassPrivate;
        }
      var.insert(i[2]);
      gen.insert(i.op(),{i[1],i[2],cls});
      continue;
      }

    if(i.op()==spv::OpTypeStruct) {
      var.insert(i[1]);
      gen.insert(i.op(),&i[1],i.length()-1);
      continue;
      }

    if(i.op()==spv::OpFunction) {
      auto& fn = an.function(i[2]);
      var.insert(i[2]);
      if(!(fn.isPureUniform() || i[2]==an.idMain)) {
        gen.insert(i.op(),&i[1],i.length()-1);
        while((*it).op()!=spv::OpFunctionEnd) {
          auto& i = *it;
          if(i.op()==spv::OpFunctionParameter)
            gen.insert(i.op(),&i[1],i.length()-1);
          ++it;
          }
        uint32_t id = vert.fetchAddBound();
        gen.insert(spv::OpLabel,{id});

        if(fn.returnType==an.idVoid) {
          gen.insert(spv::OpReturn,{});
          } else {
          uint32_t id = vert.fetchAddBound();
          gen.insert(spv::OpUndef,{fn.returnType, id});
          gen.insert(spv::OpReturnValue,{id});
          }
        gen.insert(spv::OpFunctionEnd,{});
        continue;
        }
      }

    if(i.op()==spv::OpFunctionCall) {
      //
      }

    if(i.op()==spv::OpSelectionMerge) {
      const bool skipFromVs = an.canSkipFromVs(code.toOffset(i));
      if(skipFromVs) {
        uint32_t merge = i[1];
        for(;;++it) {
          auto& i = *it;
          if(i.op()==spv::OpLabel && i[1]==merge)
            break;
          }
        continue;
        }
      }

    gen.insert(i.op(),&i[1],i.length()-1);
    }

  for(auto it = vert.begin(); it!=vert.end(); ++it) {
    auto& i = *it;
    if(i.op()==spv::OpName) {
      if(var.find(i[1])==var.end() || i[1]==an.idMain)
        it.setToNop();
      std::string_view name = reinterpret_cast<const char*>(&i[2]);
      if(name=="gl_LocalInvocationID")
        ;//it.setToNop();
      if(name=="gl_MeshPerVertexNV")
        it.setToNop();
      }
    if(i.op()==spv::OpMemberName) {
      if(var.find(i[1])==var.end() || i[1]==idGlPerVertex)
        it.setToNop();
      std::string_view name = reinterpret_cast<const char*>(&i[3]);
      it.setToNop();
      }
    }

  // inject engine-level main
  auto fn = vert.findSectionEnd(libspirv::Bytecode::S_Types);
  const uint32_t void_t             = vert.OpTypeVoid(fn);
  const uint32_t func_void          = vert.OpTypeFunction(fn, void_t);
  const uint32_t uint_t             = vert.OpTypeInt(fn, 32, false);
  const uint32_t int_t              = vert.OpTypeInt(fn, 32, true);
  const uint32_t _ptr_Input_int     = vert.OpTypePointer(fn,spv::StorageClassInput,    int_t);
  const uint32_t _ptr_Private_uint  = vert.OpTypePointer(fn,spv::StorageClassPrivate,  uint_t);
  const uint32_t _ptr_Function_uint = vert.OpTypePointer(fn,spv::StorageClassFunction, uint_t);

  const uint32_t mapSize            = vert.OpConstant(fn,uint_t, an.outputIndexes);
  const uint32_t mapping_arr_t      = vert.OpTypeArray(fn, uint_t, mapSize);
  const uint32_t mapping_arr_p      = vert.OpTypePointer(fn,spv::StorageClassFunction, mapping_arr_t);

  const uint32_t const0             = vert.OpConstant(fn,int_t,  0);
  const uint32_t const1             = vert.OpConstant(fn,int_t,  1);
  const uint32_t const2             = vert.OpConstant(fn,int_t,  2);
  const uint32_t const1u            = vert.OpConstant(fn,uint_t, 1);
  const uint32_t const8u            = vert.OpConstant(fn,uint_t, 8);
  const uint32_t const255u          = vert.OpConstant(fn,uint_t, 255);
  const uint32_t mappings           = mappingTable(fn,mapping_arr_t,uint_t);

  const uint32_t float_t            = vert.OpTypeFloat(fn, 32);
  const uint32_t v4float_t          = vert.OpTypeVector(fn, float_t, 4);
  const uint32_t perVertex_t        = declareGlPerVertex(fn);
  const uint32_t v4float_p          = vert.OpTypePointer(fn, spv::StorageClassPrivate, v4float_t);
  const uint32_t v4float_o          = vert.OpTypePointer(fn, spv::StorageClassOutput,  v4float_t);
  const uint32_t perVertex_p        = vert.OpTypePointer(fn, spv::StorageClassOutput,  perVertex_t);

  std::unordered_map<uint32_t,uint32_t> typeRemaps;
  std::unordered_map<uint32_t,uint32_t> typeRemapsOutput;
  std::unordered_map<uint32_t,uint32_t> typeRemapsPrivate;
  for(auto& i:an.varying) {
    code.traverseType(i.second.type,[&](const libspirv::MutableBytecode::AccessChain* ids, uint32_t len) {
      // vsTypeRemaps(fn,typeRemaps,ids,len);
      const auto&    op = (*ids[len-1].type);
      const uint32_t id = op[1];
      if(op.op()==spv::OpTypePointer && typeRemapsOutput.find(id)==typeRemapsOutput.end()) {
        uint32_t elt = 0;
        for(auto& i:vert)
          if(i.op()==spv::OpTypeArray && i[1]==op[3]) {
            // remove arrayness
            elt = i[2];
            break;
            }
        assert(spv::StorageClass(op[2])==spv::StorageClassWorkgroup);

        typeRemaps[id] = elt;
        uint32_t ix  = vert.OpTypePointer(fn, spv::StorageClassOutput, elt);
        typeRemapsOutput[id] = ix;
        ix  = vert.OpTypePointer(fn, spv::StorageClassPrivate, elt);
        typeRemapsPrivate[id] = ix;
        }
      },libspirv::Bytecode::T_PostOrder);
    }

  // input
  const uint32_t vMapping = vert.fetchAddBound();
  fn.insert(spv::OpVariable, {_ptr_Input_int, idVertexIndex, spv::StorageClassInput});

  // varyings variables
  for(auto& i:an.varying) {
    if(i.second.location==uint32_t(-1))
      continue;
    uint32_t tId = typeRemapsOutput[i.second.type];
    uint32_t id  = varRemaps [i.first];
    fn.insert(spv::OpVariable, {tId,  id, spv::StorageClassOutput});
    }
  const uint32_t vPerVertex = vert.fetchAddBound();
  fn.insert(spv::OpVariable, {perVertex_p, vPerVertex, spv::StorageClassOutput});

  fn = vert.end();
  const uint32_t engineMain = vert.fetchAddBound();
  const uint32_t lblMain    = vert.fetchAddBound();
  fn.insert(spv::OpFunction, {void_t, engineMain, spv::FunctionControlMaskNone, func_void});
  fn.insert(spv::OpLabel,    {lblMain});
  fn.insert(spv::OpVariable, {mapping_arr_p, vMapping, spv::StorageClassFunction});
  fn.insert(spv::OpStore,    {vMapping, mappings});

  const uint32_t rAt = vert.fetchAddBound();
  fn.insert(spv::OpLoad, {int_t, rAt, idVertexIndex});
  const uint32_t rIndex = vert.fetchAddBound();
  fn.insert(spv::OpBitcast, {uint_t, rIndex, rAt});

  if(an.idLocalInvocationID!=0) {
    // gl_PrimitiveIndex = gl_VertexIndex & 0xFF;
    // gl_LocalInvocationID.x = tbl[gl_PrimitiveIndex];
    const uint32_t ind    = vert.fetchAddBound();
    const uint32_t ptrTbl = vert.fetchAddBound();
    const uint32_t tbl    = vert.fetchAddBound();

    fn.insert(spv::OpBitwiseAnd,  {uint_t, ind, rIndex, const255u});
    fn.insert(spv::OpAccessChain, {_ptr_Function_uint, ptrTbl, vMapping, ind});
    fn.insert(spv::OpLoad,        {uint_t, tbl, ptrTbl});

    const uint32_t ptrIdX = vert.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Private_uint, ptrIdX, an.idLocalInvocationID, const0});
    fn.insert(spv::OpStore,       {ptrIdX, tbl});

    const uint32_t ptrIdY = vert.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Private_uint, ptrIdY, an.idLocalInvocationID, const1});
    fn.insert(spv::OpStore, {ptrIdY, const1u});

    const uint32_t ptrIdZ = vert.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Private_uint, ptrIdZ, an.idLocalInvocationID, const2});
    fn.insert(spv::OpStore, {ptrIdZ, const1u});
    }

  if(an.idWorkGroupID!=0) {
    // gl_WorkGroupID.x = gl_VertexIndex >> 8;
    const uint32_t ptrIdX = vert.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Private_uint, ptrIdX, an.idWorkGroupID, const0});
    const uint32_t mod = vert.fetchAddBound();
    fn.insert(spv::OpShiftRightLogical, {uint_t, mod, rIndex, const8u});
    fn.insert(spv::OpStore,             {ptrIdX, mod});

    const uint32_t ptrIdY = vert.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Private_uint, ptrIdY, an.idWorkGroupID, const1});
    fn.insert(spv::OpStore, {ptrIdY, const1u});

    const uint32_t ptrIdZ = vert.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Private_uint, ptrIdZ, an.idWorkGroupID, const2});
    fn.insert(spv::OpStore, {ptrIdZ, const1u});
    }

  const uint32_t tmp0 = vert.fetchAddBound();
  fn.insert(spv::OpFunctionCall, {void_t, tmp0, an.idMain});

  // copy varyings variables
  {
  const uint32_t ind = vert.fetchAddBound();
  fn.insert(spv::OpBitwiseAnd,  {uint_t, ind, rIndex, const255u});

  for(auto& i:an.varying) {
    if(i.second.location==uint32_t(-1)) {
      const uint32_t ptrD = vert.fetchAddBound();
      const uint32_t ptrS = vert.fetchAddBound();
      const uint32_t reg  = vert.fetchAddBound();

      fn.insert(spv::OpAccessChain, {v4float_p, ptrS, i.first, ind, const0});
      fn.insert(spv::OpAccessChain, {v4float_o, ptrD, vPerVertex,   const0});
      fn.insert(spv::OpLoad,        {v4float_t, reg, ptrS});
      fn.insert(spv::OpStore,       {ptrD, reg});
      if(gl_MeshPerVertexNV.gl_PointSize) {
        // TODO
        }
      } else {
      const uint32_t tId  = typeRemaps[i.second.type];
      const uint32_t tIdP = typeRemapsPrivate[i.second.type];
      const uint32_t id   = varRemaps [i.first];

      const uint32_t ptrV = vert.fetchAddBound();
      const uint32_t rR   = vert.fetchAddBound();

      fn.insert(spv::OpAccessChain, {tIdP, ptrV, i.first, ind});
      fn.insert(spv::OpLoad,  {tId, rR, ptrV});
      fn.insert(spv::OpStore, {id,  rR});
      }
    }
  }

  fn.insert(spv::OpReturn,      {});
  fn.insert(spv::OpFunctionEnd, {});

  // entry-point
  fn = vert.findOpEntryPoint(spv::ExecutionModelVertex,"main");
  fn.set(2, engineMain);
  for(auto& v:an.varying) {
    if(v.second.location==uint32_t(-1))
      continue;
    fn.append(varRemaps[v.first]);
    }
  fn.append(vPerVertex);
  fn.append(idVertexIndex);
  fn = vert.end();

  {
  auto fn = vert.findSectionEnd(libspirv::Bytecode::S_Annotations);
  for(auto& i:an.varying) {
    uint32_t varId = varRemaps[i.first];
    uint32_t type  = i.second.type;
    for(auto& v:an.varying) {
      if(v.second.type!=type)
        continue;
      uint32_t loc = v.second.location;
      if(loc==uint32_t(-1))
        continue;
      fn.insert(spv::OpDecorate, {varId, spv::DecorationLocation, loc});
      }
    }
  annotateVertexBuiltins(vert,idVertexIndex,perVertex_t);
  }

  vert.removeNops();
  }

void MeshConverter::annotateVertexBuiltins(libspirv::MutableBytecode& vert, uint32_t idVertexIndex, uint32_t glPerVertexT) {
  auto fn = vert.findSectionEnd(libspirv::Bytecode::S_Annotations);
  fn.insert(spv::OpDecorate,       {idVertexIndex, spv::DecorationBuiltIn, spv::BuiltInVertexIndex});
  fn.insert(spv::OpDecorate,       {glPerVertexT,  spv::DecorationBlock});
  fn.insert(spv::OpMemberDecorate, {glPerVertexT, 0, spv::DecorationBuiltIn, spv::BuiltInPosition});
  if(gl_MeshPerVertexNV.gl_PointSize) {
    fn.insert(spv::OpMemberDecorate, {glPerVertexT, 1, spv::DecorationBuiltIn, spv::BuiltInPointSize});
    }

  fn = vert.findSectionEnd(libspirv::Bytecode::S_Debug);
  fn.insert(spv::OpName, idVertexIndex, "gl_VertexIndex");

  fn.insert(spv::OpName,       glPerVertexT, "gl_PerVertex");
  fn.insert(spv::OpMemberName, glPerVertexT, 0, "gl_Position");
  if(gl_MeshPerVertexNV.gl_PointSize) {
    fn.insert(spv::OpMemberName, glPerVertexT, 1, "gl_PointSize");
    }
  //fn.insert(spv::OpMemberDecorate, {typeRemaps[idGlPerVertex], 2, spv::DecorationBuiltIn, spv::BuiltInClipDistance});
  //fn.insert(spv::OpMemberDecorate, {typeRemaps[idGlPerVertex], 3, spv::DecorationBuiltIn, spv::BuiltInCullDistance});
  }
