#include "shaderreflection.h"

#include <Tempest/Except>
#include <Tempest/Log>
#include <algorithm>
#include <libspirv/libspirv.h>

//#include "thirdparty/spirv_cross/spirv_common.hpp"

using namespace Tempest;
using namespace Tempest::Detail;


static const spirv_cross::SPIRType& typeFromVariable(const spirv_cross::Compiler& comp, spirv_cross::VariableID i) {
  auto& ret = comp.get_type_from_variable(i);
  if(ret.pointer) {
    auto& base = comp.get_type(ret.parent_type);
    return base;
    }
  return ret;
  }

static bool isRuntimeSized(const spirv_cross::SPIRType& t) {
  if(t.array.empty())
    return false;
  if(t.op==spv::OpTypeRuntimeArray)
    return true;
  // NOTE: unused runtime array has size of 1
  return (!t.array.empty() && t.array[0]<=1);
  }

static uint32_t arraySize(const spirv_cross::SPIRType& t) {
  if(isRuntimeSized(t))
    return 0;
  uint32_t cnt = 1;
  for(auto& i:t.array)
    cnt *= i;
  return cnt;
  }

static bool is3dImage(const spirv_cross::SPIRType& t) {
  if(t.op!=spv::OpTypeImage)
    return false;
  return t.image.dim==spv::Dim3D;
  }

static uint32_t declaredSize(spirv_cross::Compiler& comp, const spirv_cross::SPIRType& t) {
  assert(t.basetype==spirv_cross::SPIRType::Struct);
  // Offsets can be declared out of order, so we need to deduce the actual size
  // based on last member instead.
  uint32_t member_index = 0;
  size_t highest_offset = 0;
  for(uint32_t i = 0; i < uint32_t(t.member_types.size()); i++) {
    size_t offset = comp.type_struct_member_offset(t, i);
    if(offset > highest_offset) {
      highest_offset = offset;
      member_index = i;
      }
    }

  size_t size = comp.get_declared_struct_member_size(t, member_index);
  if(size==0)
    return uint32_t(highest_offset); // runtime sized tail
  return uint32_t(highest_offset + size);
  }

static uint32_t declaredVarSize(spirv_cross::Compiler& comp, const spirv_cross::SPIRType& t) {
  for(uint32_t i = 0; i < uint32_t(t.member_types.size()); i++) {
    // GLSL: There can only be one array of variable size per SSBO.
    auto &type = comp.get_type(t.member_types[i]);
    if(type.op!=spv::OpTypeRuntimeArray)
      continue;
    const uint32_t sz = comp.type_struct_member_array_stride(t, i);
    return sz;
    }
  return 0;
  }


bool ShaderReflection::LayoutDesc::isUpdateAfterBind() const {
  return runtime!=0 || array!=0;
  }

size_t ShaderReflection::LayoutDesc::sizeofBuffer(size_t id, size_t arraylen) const {
  return bufferSz[id] + bufferEl[id]*arraylen;
  }


void ShaderReflection::getVertexDecl(std::vector<Decl::ComponentType>& data, spirv_cross::Compiler& comp) {
  if(comp.get_execution_model()!=spv::ExecutionModelVertex)
    return;

  spirv_cross::ShaderResources resources = comp.get_shader_resources();
  for(auto &resource : resources.stage_inputs) {
    auto&    t   = comp.get_type_from_variable(resource.id);
    unsigned loc = comp.get_decoration(resource.id, spv::DecorationLocation);
    data.resize(std::max<size_t>(loc+1,data.size()));

    switch(t.basetype) {
      case spirv_cross::SPIRType::Float: {
        data[loc] = Decl::ComponentType(Decl::float1+t.vecsize-1);
        break;
        }
      case spirv_cross::SPIRType::Int: {
        data[loc] = Decl::ComponentType(Decl::int1+t.vecsize-1);
        break;
        }
      case spirv_cross::SPIRType::UInt: {
        data[loc] = Decl::ComponentType(Decl::uint1+t.vecsize-1);
        break;
        }
      // TODO: add support for uint32_t packed color
      default:
        // not supported
        throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
      }
    }
  }

void ShaderReflection::getBindings(std::vector<Binding>& lay, spirv_cross::Compiler& comp, const uint32_t* source, const size_t size) {
  const Stage s = getExecutionModel(comp);

  spirv_cross::ShaderResources resources = comp.get_shader_resources();
  for(auto &resource : resources.sampled_images) {
    auto&    t       = typeFromVariable(comp, resource.id);
    unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);
    Binding b;
    b.layout       = binding;
    b.cls          = Texture;
    b.stage        = s;
    b.spvId        = resource.id;
    b.runtimeSized = isRuntimeSized(t);
    b.arraySize    = arraySize(t);
    b.is3DImage    = is3dImage(t);
    lay.push_back(b);
    }
  for(auto &resource : resources.separate_images) {
    auto&    t       = typeFromVariable(comp, resource.id);
    unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);
    Binding b;
    b.layout       = binding;
    b.cls          = Image;
    b.stage        = s;
    b.spvId        = resource.id;
    b.runtimeSized = isRuntimeSized(t);
    b.arraySize    = arraySize(t);
    b.is3DImage    = is3dImage(t);
    lay.push_back(b);
    }
  for(auto &resource : resources.separate_samplers) {
    auto&    t       = typeFromVariable(comp, resource.id);
    unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);
    Binding b;
    b.layout       = binding;
    b.cls          = Sampler;
    b.stage        = s;
    b.spvId        = resource.id;
    b.runtimeSized = isRuntimeSized(t);
    b.arraySize    = arraySize(t);
    lay.push_back(b);
    }
  for(auto &resource : resources.uniform_buffers) {
    auto&    t       = typeFromVariable(comp, resource.id);
    unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);
    auto     sz      = declaredSize(comp,t);
    Binding b;
    b.layout       = binding;
    b.cls          = Ubo;
    b.stage        = s;
    b.runtimeSized = isRuntimeSized(t);
    b.arraySize    = arraySize(t);
    b.byteSize     = sz;
    b.spvId        = resource.id;
    lay.push_back(b);
    }
  for(auto &resource : resources.storage_buffers) {
    auto&    t        = typeFromVariable(comp, resource.id);
    unsigned binding  = comp.get_decoration(resource.id, spv::DecorationBinding);
    auto     readonly = comp.get_buffer_block_flags(resource.id);
    auto     sz       = declaredSize(comp,t);
    Binding b;
    b.layout       = binding;
    b.cls          = (readonly.get(spv::DecorationNonWritable) ? SsboR : SsboRW);
    b.stage        = s;
    b.runtimeSized = isRuntimeSized(t);
    b.arraySize    = arraySize(t);
    b.byteSize     = sz;
    b.varByteSize  = declaredVarSize(comp, t);
    b.spvId        = resource.id;
    lay.push_back(b);
    }
  for(auto &resource : resources.storage_images) {
    auto&    t       = typeFromVariable(comp, resource.id);
    unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);
    Binding b;
    b.layout       = binding;
    b.cls          = ImgRW; // (readonly.get(spv::DecorationNonWritable) ? UniformsLayout::ImgR : UniformsLayout::ImgRW);
    b.stage        = s;
    b.runtimeSized = isRuntimeSized(t);
    b.arraySize    = arraySize(t);
    b.spvId        = resource.id;
    b.is3DImage    = is3dImage(t);
    lay.push_back(b);
    }
  for(auto &resource : resources.acceleration_structures) {
    auto&    t       = typeFromVariable(comp, resource.id);
    unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);
    Binding b;
    b.layout       = binding;
    b.cls          = Tlas;
    b.stage        = s;
    b.runtimeSized = isRuntimeSized(t);
    b.arraySize    = arraySize(t);
    b.spvId        = resource.id;
    lay.push_back(b);
    }
  for(auto &resource : resources.push_constant_buffers) {
    auto&    t       = typeFromVariable(comp, resource.id);
    unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);
    auto     sz      = declaredSize(comp,t);

    Binding b;
    b.layout   = binding;
    b.cls      = Push;
    b.stage    = s;
    b.byteSize = sz;
    b.spvId    = resource.id;
    lay.push_back(b);
    }

  if(lay.size()==0)
    return;

  const bool arrayWa = hasRuntimeArrays(lay) && !canHaveRuntimeArrays(comp, source, size);
  if(arrayWa) {
    // WA for GLSL bug: https://github.com/KhronosGroup/GLSL/issues/231
    for(auto& i:lay) {
      i.runtimeSized = false;
      i.arraySize    = std::max(1u, i.arraySize);
      }
    }
  }

bool ShaderReflection::canHaveRuntimeArrays(spirv_cross::Compiler& comp, const uint32_t* source, const size_t size) {
  auto& ext = comp.get_declared_extensions();
  for(auto& i:ext)
    if(i=="SPV_EXT_descriptor_indexing")
      return true;

  libspirv::Bytecode code(source, size);
  for(auto& i:code) {
    if(i.op()==spv::OpSourceExtension) {
      std::string_view str = reinterpret_cast<const char*>(&i[1]);
      if(str=="GL_EXT_nonuniform_qualifier") {
        return true;
        }
      }
    }
  return false;
  }

bool ShaderReflection::hasRuntimeArrays(const std::vector<Binding>& lay) {
  for(auto& i:lay) {
    if(i.runtimeSized || i.arraySize>1)
      return true;
    }
  return false;
  }

ShaderReflection::Stage ShaderReflection::getExecutionModel(spirv_cross::Compiler& comp) {
  return getExecutionModel(comp.get_execution_model());
  }

ShaderReflection::Stage ShaderReflection::getExecutionModel(libspirv::Bytecode& comp) {
  auto c = comp.findExecutionModel();
  return getExecutionModel(c);
  }

ShaderReflection::Stage ShaderReflection::getExecutionModel(spv::ExecutionModel m) {
  switch(m) {
    case spv::ExecutionModelGLCompute:
      return Stage::Compute;
    case spv::ExecutionModelVertex:
      return Stage::Vertex;
    case spv::ExecutionModelTessellationControl:
      return Stage::Control;
    case spv::ExecutionModelTessellationEvaluation:
      return Stage::Evaluate;
    case spv::ExecutionModelGeometry:
      return Stage::Geometry;
    case spv::ExecutionModelFragment:
      return Stage::Fragment;
    case spv::ExecutionModelTaskNV:
    case spv::ExecutionModelTaskEXT:
      return Stage::Task;
    case spv::ExecutionModelMeshNV:
    case spv::ExecutionModelMeshEXT:
      return Stage::Mesh;
    default: // unimplemented
      throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
    }
  }

void ShaderReflection::merge(std::vector<Binding>& ret,
                             PushBlock& pb,
                             const std::vector<Binding>* sh[],
                             size_t count) {
  size_t expectedSz = 0;
  for(size_t i=0; i<count; ++i)
    if(sh[i]!=nullptr)
      expectedSz+=sh[i]->size();
  ret.reserve(expectedSz);

  for(size_t shId=0; shId<count; ++shId) {
    if(sh[shId]==nullptr)
      continue;

    auto& vs = *sh[shId];
    for(size_t i=0; i<vs.size(); ++i) {
      auto& u   = vs[i];
      if(u.cls==Push) {
        pb.stage = Stage(pb.stage | u.stage);
        pb.size  = std::max(pb.size, u.byteSize);
        continue;
        }
      {
        bool  ins = false;
        for(auto& r:ret)
          if(r.layout==u.layout) {
            r.stage         = Stage(r.stage | u.stage);
            r.byteSize      = std::max(r.byteSize,  u.byteSize);
            // r.byteSize      = std::max(r.byteSize,  u.mslSize);
            r.arraySize     = std::max(r.arraySize, u.arraySize);
            r.runtimeSized |= u.runtimeSized;
            ins     = true;
            break;
            }
        if(ins)
          continue;
        }
      ret.push_back(u);
      }
    }

  finalize(ret);
  }

size_t ShaderReflection::sizeofBuffer(const Binding& bind, size_t arraylen) {
  if(bind.byteSize==uint32_t(-1))
    return bind.varByteSize*arraylen;
  return bind.byteSize + bind.varByteSize*arraylen;
  }

void ShaderReflection::finalize(std::vector<Binding>& ret) {
  std::sort(ret.begin(),ret.end(),[](const Binding& a, const Binding& b){
    return a.layout<b.layout;
    });

  for(size_t i=0; i<ret.size(); ++i)
    if(ret[i].layout!=i) {
      Binding fill;
      fill.layout = uint32_t(i);
      fill.stage  = Stage(0);
      ret.insert(ret.begin()+int(i),fill);
      }
  }

size_t ShaderReflection::mslSizeOf(const spirv_cross::ID& spvId,
                                   spirv_cross::Compiler& comp) {
  auto& t = comp.get_type_from_variable(spvId);
  return mslSizeOf(t,comp);
  }

size_t ShaderReflection::mslSizeOf(const spirv_cross::SPIRType& type,
                                   spirv_cross::Compiler& comp) {
  uint32_t member_index   = 0;
  size_t   highest_offset = 0;
  size_t   highest_sizeof = 0;
  for(uint32_t i = 0; i < uint32_t(type.member_types.size()); i++) {
    size_t offset = comp.type_struct_member_offset(type, i);
    if(offset > highest_offset) {
      highest_offset = offset;
      member_index = i;
      }
    size_t sz = comp.get_declared_struct_member_size(type, member_index);
    if(sz==3*4)
      sz = 4*4; // vec3
    if(highest_sizeof<sz)
      highest_sizeof = sz;
    }

  size_t size = highest_offset+comp.get_declared_struct_member_size(type, member_index);
  if(highest_sizeof>0)
    size = ((size+highest_sizeof-1)/highest_sizeof)*highest_sizeof;
  return size;
  }

void ShaderReflection::setupLayout(PushBlock& pb, LayoutDesc& lx, SyncDesc& sync,
                                   const std::vector<Binding>* sh[], size_t cnt) {
  std::fill(std::begin(lx.bindings), std::end(lx.bindings), ShaderReflection::Count);

  for(size_t i=0; i<cnt; ++i) {
    if(sh[i]==nullptr)
      continue;
    auto& bind = *sh[i];
    for(auto& e:bind) {
      if(e.cls==ShaderReflection::Push) {
        pb.stage = ShaderReflection::Stage(pb.stage | e.stage);
        pb.size  = std::max(pb.size, e.byteSize);
        pb.size  = std::max(pb.size, e.mslSize);
        continue;
        }
      const uint32_t id = (1u << e.layout);

      lx.bindings[e.layout] = e.cls;
      lx.count   [e.layout] = std::max(lx.count[e.layout] , e.arraySize);
      lx.stage   [e.layout] = ShaderReflection::Stage(e.stage | lx.stage[e.layout]);

      if(e.runtimeSized)
        lx.runtime |= id;
      if(e.runtimeSized || e.arraySize>1)
        lx.array   |= id;
      lx.active |= id;

      sync.read |= id;
      if(e.cls==ShaderReflection::ImgRW || e.cls==ShaderReflection::SsboRW)
        sync.write |= id;

      lx.bufferSz[e.layout] = std::max(lx.bufferSz[e.layout], e.byteSize);
      lx.bufferEl[e.layout] = std::max(lx.bufferEl[e.layout], e.varByteSize);
      }
    }
  }
