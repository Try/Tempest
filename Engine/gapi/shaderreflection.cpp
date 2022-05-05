#include "shaderreflection.h"

#include <Tempest/Except>
#include <Tempest/Log>
#include <algorithm>

#include "thirdparty/spirv_cross/spirv_common.hpp"

using namespace Tempest;
using namespace Tempest::Detail;

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

void ShaderReflection::getBindings(std::vector<Binding>&  lay,
                                   spirv_cross::Compiler& comp) {
  const Stage s = getExecutionModel(comp);

  spirv_cross::ShaderResources resources = comp.get_shader_resources();
  for(auto &resource : resources.sampled_images) {
    unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);
    Binding b;
    b.layout = binding;
    b.cls    = Texture;
    b.stage  = s;
    b.spvId  = resource.id;
    lay.push_back(b);
    }
  for(auto &resource : resources.uniform_buffers) {
    auto&    t       = comp.get_type_from_variable(resource.id);
    unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);
    auto     sz      = comp.get_declared_struct_size(t);
    Binding b;
    b.layout = binding;
    b.cls    = Ubo;
    b.stage  = s;
    b.size   = sz;
    b.spvId  = resource.id;
    lay.push_back(b);
    }
  for(auto &resource : resources.storage_buffers) {
    auto&    t        = comp.get_type_from_variable(resource.id);
    unsigned binding  = comp.get_decoration(resource.id, spv::DecorationBinding);
    auto     readonly = comp.get_buffer_block_flags(resource.id);
    auto     sz       = comp.get_declared_struct_size(t);
    Binding b;
    b.layout = binding;
    b.cls    = (readonly.get(spv::DecorationNonWritable) ? SsboR : SsboRW);
    b.stage  = s;
    b.size   = sz;
    b.spvId  = resource.id;
    lay.push_back(b);
    }
  for(auto &resource : resources.storage_images) {
    unsigned binding  = comp.get_decoration(resource.id, spv::DecorationBinding);
    Binding b;
    b.layout = binding;
    b.cls    = ImgRW; // (readonly.get(spv::DecorationNonWritable) ? UniformsLayout::ImgR : UniformsLayout::ImgRW);
    b.stage  = s;
    b.spvId  = resource.id;
    lay.push_back(b);
    }
  for(auto &resource : resources.acceleration_structures) {
    unsigned binding  = comp.get_decoration(resource.id, spv::DecorationBinding);
    Binding b;
    b.layout = binding;
    b.cls    = Tlas;
    b.stage  = s;
    b.spvId  = resource.id;
    lay.push_back(b);
    }
  for(auto &resource : resources.push_constant_buffers) {
    unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);
    auto&    t       = comp.get_type_from_variable(resource.id);
    auto     sz      = comp.get_declared_struct_size(t);

    Binding b;
    b.layout = binding;
    b.cls    = Push;
    b.stage  = s;
    b.size   = sz;
    b.spvId  = resource.id;
    lay.push_back(b);
    }
  }

ShaderReflection::Stage ShaderReflection::getExecutionModel(spirv_cross::Compiler& comp) {
  switch(comp.get_execution_model()) {
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
      return Stage::Task;
    case spv::ExecutionModelMeshNV:
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
        pb.size  = std::max(pb.size, u.size);
        pb.size  = std::max(pb.size, u.mslSize);
        continue;
        }
      if(shId>0) {
        bool  ins = false;
        for(auto& r:ret)
          if(r.layout==u.layout) {
            r.stage = Stage(r.stage | u.stage);
            r.size  = std::max(r.size, u.size);
            r.size  = std::max(r.size, u.mslSize);
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

size_t Tempest::Detail::ShaderReflection::mslSizeOf(const spirv_cross::ID& spvId,
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
    if(highest_sizeof<sz)
      highest_sizeof = sz;
    }

  size_t size = highest_offset+comp.get_declared_struct_member_size(type, member_index);
  if(highest_sizeof>0)
    size = ((size+highest_sizeof-1)/highest_sizeof)*highest_sizeof;
  return size;
  }
