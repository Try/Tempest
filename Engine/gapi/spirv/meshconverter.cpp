#include "meshconverter.h"
#include "libspirv/GLSL.std.450.h"

#include <cassert>
#include <iostream>

MeshConverter::MeshConverter(libspirv::MutableBytecode& code)
  : code(code) {
  }

void MeshConverter::exec() {
  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpDecorate && i[2]==spv::DecorationBuiltIn) {
      if(i[3]==spv::BuiltInPrimitiveCountNV) {
        idPrimitiveCountNV = i[1];
        it.setToNop();
        }
      if(i[3]==spv::BuiltInPrimitiveIndicesNV) {
        idPrimitiveIndicesNV = i[1];
        it.setToNop();
        }

      if(i[3]==spv::BuiltInLocalInvocationId) {
        idLocalInvocationId = i[1];
        }
      if(i[3]==spv::BuiltInGlobalInvocationId) {
        idGlobalInvocationId = i[1];
        }
      if(i[3]==spv::BuiltInWorkgroupId) {
        idWorkGroupID = i[1];
        }
      }
    if(i.op()==spv::OpExtInstImport) {
      std::string_view name = reinterpret_cast<const char*>(&i[2]);
      if(name=="GLSL.std.450")
        std450Import = i[1];
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

  // TODO: import std450Import automatically
  assert(std450Import!=0);

  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if((i.op()==spv::OpVariable && i[3]==spv::StorageClassOutput)) {
      if(i[2]!=idPrimitiveCountNV &&
         i[2]!=idPrimitiveIndicesNV) {
        VarItm it;
        it.type = i[1];
        outVar.insert({i[2],it});
        }
      it.set(3, spv::StorageClassWorkgroup);
      }
    if((i.op()==spv::OpTypePointer && i[2]==spv::StorageClassOutput)) {
      it.set(2, spv::StorageClassWorkgroup);
      }
    }

  // gl_MeshPerVertexNV block
  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpMemberDecorate && i[3]==spv::DecorationPerViewNV) {
      idMeshPerVertexNV = i[1];
      break;
      }
    }

  removeMultiview(code);
  removeCullClip(code);
  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpDecorate && i[2]==spv::DecorationBlock) {
      it.setToNop();
      continue;
      }
    if(i.op()==spv::OpDecorate && i[2]==spv::DecorationLocation) {
      outVar[i[1]].location = i[3];
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
    if(i.op()==spv::OpName) {
      auto str = std::string_view(reinterpret_cast<const char*>(&i[2]));
      if(str=="gl_MeshPerVertexNV")
        idGlPerVertex = i[1];
      continue;
      }
    }

  avoidReservedFixup();

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
        if(cp!=idLocalInvocationId &&
           cp!=idGlobalInvocationId &&
           cp!=idWorkGroupID) {
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

  if(idLocalInvocationId==0) {
    auto fn = code.findSectionEnd(libspirv::Bytecode::S_Types);
    const uint32_t tUInt             = code.OpTypeInt(fn,32,false);
    const uint32_t v3uint            = code.OpTypeVector(fn,tUInt,3);
    const uint32_t _ptr_Input_v3uint = code.OpTypePointer(fn,spv::StorageClassInput,v3uint);

    idLocalInvocationId = code.fetchAddBound();
    fn.insert(spv::OpVariable, {_ptr_Input_v3uint, idLocalInvocationId, spv::StorageClassInput});

    fn = code.findSectionEnd(libspirv::Bytecode::S_Debug);
    fn.insert(spv::OpName, idLocalInvocationId, "gl_LocalInvocationID");

    fn = code.findSectionEnd(libspirv::Bytecode::S_Annotations);
    fn.insert(spv::OpDecorate, {idLocalInvocationId, spv::DecorationBuiltIn, spv::BuiltInLocalInvocationId});

    auto ep = code.findOpEntryPoint(spv::ExecutionModelGLCompute,"main");
    if(ep!=code.end()) {
      ep.append(idLocalInvocationId);
      }
    }

  injectCountingPass(idMain);
  // nops are not valid to have outsize of block
  code.removeNops();

  generateVs();
  }

void MeshConverter::generateVs() {
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
  for(auto& i:outVar) {
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
  uint32_t varCount = 0;
  {
  for(auto& i:outVar) {
    // preallocate indexes
    code.traverseType(i.second.type,[&](const libspirv::MutableBytecode::AccessChain* ids, uint32_t len) {
      if(!code.isBasicTypeDecl(ids[len-1].type->op()))
        return;
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
  for(auto& i:outVar) {
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
  for(auto& i:outVar) {
    uint32_t tId = typeRemaps[i.second.type];
    uint32_t id  = varRemaps [i.first];
    fn.insert(spv::OpVariable,       {tId, id, spv::StorageClassOutput});
    }

  fn.insert(spv::OpFunction,         {void_t, main, spv::FunctionControlMaskNone, func_t});
  fn.insert(spv::OpLabel,            {lblMain});
  {
  const uint32_t rAt = vert.fetchAddBound();
  fn.insert(spv::OpLoad, {int_t, rAt, gl_VertexIndex});

  uint32_t seq  = 0;
  for(auto& i:outVar) {
    uint32_t varId = varRemaps[i.first];
    uint32_t type  = i.second.type;
    code.traverseType(type,[&](const libspirv::MutableBytecode::AccessChain* ids, uint32_t len) {
      if(!code.isBasicTypeDecl(ids[len-1].type->op()))
        return;
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

  fn = vert.findSectionEnd(libspirv::Bytecode::S_Debug);
  fn.insert(spv::OpName, gl_VertexIndex, "gl_VertexIndex");

  fn.insert(spv::OpName,       typeRemaps[idGlPerVertex], "gl_PerVertex");
  fn.insert(spv::OpMemberName, typeRemaps[idGlPerVertex], 0, "gl_Position");
  fn.insert(spv::OpMemberName, typeRemaps[idGlPerVertex], 1, "gl_PointSize");
  //fn.insert(spv::OpMemberName, typeRemaps[idGlPerVertex], 2, "gl_ClipDistance");
  //fn.insert(spv::OpMemberName, typeRemaps[idGlPerVertex], 3, "gl_CullDistance");

  fn = vert.findSectionEnd(libspirv::Bytecode::S_Annotations);
  for(auto& i:outVar) {
    uint32_t varId = varRemaps[i.first];
    uint32_t type  = i.second.type;
    for(auto& v:outVar) {
      if(v.second.type!=type)
        continue;
      uint32_t loc = v.second.location;
      if(loc==uint32_t(-1))
        continue;
      fn.insert(spv::OpDecorate, {varId, spv::DecorationLocation, loc});
      }
    }
  fn.insert(spv::OpDecorate,       {gl_VertexIndex, spv::DecorationBuiltIn, spv::BuiltInVertexIndex});
  fn.insert(spv::OpDecorate,       {typeRemaps[idGlPerVertex], spv::DecorationBlock});
  fn.insert(spv::OpMemberDecorate, {typeRemaps[idGlPerVertex], 0, spv::DecorationBuiltIn, spv::BuiltInPosition});
  fn.insert(spv::OpMemberDecorate, {typeRemaps[idGlPerVertex], 1, spv::DecorationBuiltIn, spv::BuiltInPointSize});
  //fn.insert(spv::OpMemberDecorate, {typeRemaps[idGlPerVertex], 2, spv::DecorationBuiltIn, spv::BuiltInClipDistance});
  //fn.insert(spv::OpMemberDecorate, {typeRemaps[idGlPerVertex], 3, spv::DecorationBuiltIn, spv::BuiltInCullDistance});
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
        uint32_t uint_t = vert.OpTypeInt(fn, 32, false);
        uint32_t elt    = typeRemaps[op[2]];
        uint32_t sz     = vert.OpConstant(fn, uint_t, 1);
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
  const uint32_t IndirectCommand      = code.OpTypeStruct (fn, {uint_t, uint_t, uint_t, int_t, uint_t, uint_t, uint_t, uint_t});
  const uint32_t _runtimearr_cmd      = code.OpTypeRuntimeArray(fn, IndirectCommand);

  const uint32_t EngineInternal0      = code.OpTypeStruct (fn, {_runtimearr_cmd});
  const uint32_t EngineInternal1      = code.OpTypeStruct (fn, {uint_t, uint_t, uint_t, _runtimearr_uint});
  const uint32_t EngineInternal2      = code.OpTypeStruct (fn, {uint_t, _runtimearr_uint});

  const uint32_t _ptr_Uniform_EngineInternal0 = code.OpTypePointer(fn,spv::StorageClassUniform, EngineInternal0);
  const uint32_t _ptr_Uniform_EngineInternal1 = code.OpTypePointer(fn,spv::StorageClassUniform, EngineInternal1);
  const uint32_t _ptr_Uniform_EngineInternal2 = code.OpTypePointer(fn,spv::StorageClassUniform, EngineInternal2);

  const uint32_t const0   = code.OpConstant(fn,uint_t,0);
  const uint32_t const1   = code.OpConstant(fn,uint_t,1);
  const uint32_t const2   = code.OpConstant(fn,uint_t,2);
  const uint32_t const3   = code.OpConstant(fn,uint_t,3);
  const uint32_t const10  = code.OpConstant(fn,uint_t,10);
  const uint32_t const18  = code.OpConstant(fn,uint_t,18);
  const uint32_t const128 = code.OpConstant(fn,uint_t,128);
  const uint32_t const264 = code.OpConstant(fn,uint_t,264);

  std::unordered_map<uint32_t,uint32_t> constants;
  uint32_t varCount = 0;
  {
  for(auto& i:outVar) {
    // preallocate indexes
    code.traverseType(i.second.type,[&](const libspirv::MutableBytecode::AccessChain* ids, uint32_t len) {
      if(!code.isBasicTypeDecl(ids[len-1].type->op()))
        return;
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
  fn.insert(spv::OpDecorate, {_runtimearr_cmd,  spv::DecorationArrayStride, 32});

  fn.insert(spv::OpDecorate, {EngineInternal0, spv::DecorationBufferBlock});
  fn.insert(spv::OpDecorate, {vEngine0, spv::DecorationDescriptorSet, 1});
  fn.insert(spv::OpDecorate, {vEngine0, spv::DecorationBinding, 0});

  fn.insert(spv::OpMemberDecorate, {EngineInternal0, 0, spv::DecorationOffset, 0});
  for(uint32_t i=0; i<8; ++i)
    fn.insert(spv::OpMemberDecorate, {IndirectCommand, i, spv::DecorationOffset, i*4});

  fn.insert(spv::OpDecorate, {EngineInternal1, spv::DecorationBufferBlock});
  fn.insert(spv::OpDecorate, {vEngine1, spv::DecorationDescriptorSet, 1});
  fn.insert(spv::OpDecorate, {vEngine1, spv::DecorationBinding, 1});
  for(uint32_t i=0; i<4; ++i)
    fn.insert(spv::OpMemberDecorate, {EngineInternal1, i, spv::DecorationOffset, i*4});

  fn.insert(spv::OpDecorate, {EngineInternal2, spv::DecorationBufferBlock});
  fn.insert(spv::OpDecorate, {vEngine2, spv::DecorationDescriptorSet, 1});
  fn.insert(spv::OpDecorate, {vEngine2, spv::DecorationBinding, 2});
  for(uint32_t i=0; i<2; ++i)
    fn.insert(spv::OpMemberDecorate, {EngineInternal2, i, spv::DecorationOffset, i*4});

  fn = code.findSectionEnd(libspirv::Bytecode::S_Debug);
  fn.insert(spv::OpName,       IndirectCommand,    "VkDrawIndexedIndirectCommand");
  fn.insert(spv::OpMemberName, IndirectCommand, 0, "indexCount");
  fn.insert(spv::OpMemberName, IndirectCommand, 1, "instanceCount");
  fn.insert(spv::OpMemberName, IndirectCommand, 2, "firstIndex");
  fn.insert(spv::OpMemberName, IndirectCommand, 3, "vertexOffset");
  fn.insert(spv::OpMemberName, IndirectCommand, 4, "firstInstance");
  fn.insert(spv::OpMemberName, IndirectCommand, 5, "self");
  fn.insert(spv::OpMemberName, IndirectCommand, 6, "vboOffset");
  fn.insert(spv::OpMemberName, IndirectCommand, 7, "padd1");
  fn.insert(spv::OpName,       EngineInternal0,    "EngineInternal0");

  fn.insert(spv::OpName,       EngineInternal1,    "EngineInternal1");
  fn.insert(spv::OpMemberName, EngineInternal1, 0, "grow");
  fn.insert(spv::OpMemberName, EngineInternal1, 1, "dispatchY");
  fn.insert(spv::OpMemberName, EngineInternal1, 2, "dispatchZ");
  fn.insert(spv::OpMemberName, EngineInternal1, 3, "desc");

  fn.insert(spv::OpName,       EngineInternal2,    "EngineInternal2");
  fn.insert(spv::OpMemberName, EngineInternal2, 0, "grow");
  fn.insert(spv::OpMemberName, EngineInternal2, 1, "heap");

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
  fn.insert(spv::OpLoad, {uint_t, primCount, idPrimitiveCountNV});

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
    fn.insert(spv::OpAccessChain, {_ptr_Input_uint, ptrInvocationIdX, idLocalInvocationId, const0});
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
    fn.insert(spv::OpAccessChain, {_ptr_Workgroup_uint, ptrIndicesNV, idPrimitiveIndicesNV, rI});

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
  const uint32_t ptrCmdDest = code.fetchAddBound();
  fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrCmdDest, vEngine0, const0, const0, const0});
  const uint32_t cmdDest  = code.fetchAddBound();
  fn.insert(spv::OpAtomicIAdd, {uint_t, cmdDest, ptrCmdDest, const1/*scope*/, const0/*semantices*/, indSize});

  const uint32_t ptrCmdDest2 = code.fetchAddBound();
  fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrCmdDest2, vEngine0, const0, const0, const1});
  const uint32_t cmdDest2  = code.fetchAddBound();
  fn.insert(spv::OpAtomicIAdd, {uint_t, cmdDest2, ptrCmdDest2, const1/*scope*/, const0/*semantices*/, maxVar});
  }

  const uint32_t heapAllocSz = code.fetchAddBound();
  fn.insert(spv::OpIAdd, {uint_t, heapAllocSz, maxVar, indSize});

  const uint32_t ptrHeapDest = code.fetchAddBound();
  fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrHeapDest, vEngine2, const0});
  const uint32_t heapDest  = code.fetchAddBound();
  fn.insert(spv::OpAtomicIAdd, {uint_t, heapDest, ptrHeapDest, const1/*scope*/, const0/*semantices*/, heapAllocSz});
  // uint varDest = indDest + indSize;
  const uint32_t varDest  = code.fetchAddBound();
  fn.insert(spv::OpIAdd, {uint_t, varDest, heapDest, indSize});

  // uint meshDest = atomicAdd(mesh.grow, 1)*3;
  const uint32_t ptrMeshDest = code.fetchAddBound();
  fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrMeshDest, vEngine1, const0});
  const uint32_t meshDestRaw  = code.fetchAddBound();
  fn.insert(spv::OpAtomicIAdd, {uint_t, meshDestRaw, ptrMeshDest, const1/*scope*/, const0/*semantices*/, const1});
  const uint32_t meshDest = code.fetchAddBound();
  fn.insert(spv::OpIMul, {uint_t, meshDest, meshDestRaw, const3});

  // Writeout indexes
  injectLoop(fn, varI, const0, indSize, const1, [&](libspirv::MutableBytecode::Iterator& fn) {
    const uint32_t rI = code.fetchAddBound();
    fn.insert(spv::OpLoad, {uint_t, rI, varI});
    const uint32_t rDst = code.fetchAddBound();
    fn.insert(spv::OpIAdd, {uint_t, rDst, rI, heapDest});
    const uint32_t ptrHeap = code.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrHeap, vEngine2, const1, rDst});

    const uint32_t ptrIndicesNV = code.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Workgroup_uint, ptrIndicesNV, idPrimitiveIndicesNV, rI});

    const uint32_t rInd = code.fetchAddBound();
    fn.insert(spv::OpLoad, {uint_t, rInd, ptrIndicesNV});

    fn.insert(spv::OpStore, {ptrHeap, rInd});
    });

  // Writeout varying
  {
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
    for(auto& i:outVar) {
      uint32_t type = i.second.type;
      code.traverseType(type,[&](const libspirv::MutableBytecode::AccessChain* ids, uint32_t len) {
        if(!code.isBasicTypeDecl(ids[len-1].type->op()))
          return;
        // at += memberId
        const uint32_t rDst = code.fetchAddBound();
        block.insert(spv::OpIAdd, {uint_t, rDst, rAt, constants[seq]});
        ++seq;
        const uint32_t ptrHeap = code.fetchAddBound();
        block.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrHeap, vEngine2, const1, rDst});

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
  {
    const uint32_t ptrHeap0 = code.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrHeap0, vEngine1, const3, meshDest});
    fn.insert(spv::OpStore, {ptrHeap0, const0}); // TODO: writeout self-id

    const uint32_t dest1 = code.fetchAddBound();
    fn.insert(spv::OpIAdd, {uint_t, dest1, meshDest, const1});
    const uint32_t ptrHeap1 = code.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrHeap1, vEngine1, const3, dest1});
    fn.insert(spv::OpStore, {ptrHeap1, heapDest});

    const uint32_t dest2 = code.fetchAddBound();
    fn.insert(spv::OpIAdd, {uint_t, dest2, meshDest, const2});
    const uint32_t ptrHeap2 = code.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrHeap2, vEngine1, const3, dest2});

    const uint32_t tmp0 = code.fetchAddBound();
    fn.insert(spv::OpShiftLeftLogical, {uint_t, tmp0, maxVertex, const10});

    const uint32_t tmp1 = code.fetchAddBound();
    fn.insert(spv::OpShiftLeftLogical, {uint_t, tmp1, varSize, const18});

    const uint32_t tmp2 = code.fetchAddBound();
    fn.insert(spv::OpBitwiseOr, {uint_t, tmp2, tmp1, tmp0});

    const uint32_t tmp3 = code.fetchAddBound();
    fn.insert(spv::OpBitwiseOr, {uint_t, tmp3, tmp2, indSize});

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
  assert(ep!=code.end());
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
  if(idMeshPerVertexNV==0)
    return;

  std::unordered_set<uint32_t> perView;
  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpMemberDecorate && i[3]==spv::DecorationPerViewNV) {
      perView.insert(i[2]);
      continue;
      }
    }

  removeFromPerVertex(code,perView);
  }

void MeshConverter::removeCullClip(libspirv::MutableBytecode& code) {
  if(idMeshPerVertexNV==0)
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
  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if((i.op()!=spv::OpMemberDecorate && i.op()!=spv::OpMemberName) || i[1]!=idMeshPerVertexNV)
      continue;
    if(fields.find(i[2])==fields.end())
      continue;
    it.setToNop();
    }

  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()!=spv::OpTypeStruct)
      continue;
    if(idMeshPerVertexNV!=i[1])
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
