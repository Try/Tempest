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
  Stage s = Stage::Compute;
  switch(comp.get_execution_model()) {
    case spv::ExecutionModelGLCompute:
      s = Stage::Compute;
      break;
    case spv::ExecutionModelVertex:
      s = Stage::Vertex;
      break;
    case spv::ExecutionModelTessellationControl:
      s = Stage::Control;
      break;
    case spv::ExecutionModelTessellationEvaluation:
      s = Stage::Evaluate;
      break;
    case spv::ExecutionModelGeometry:
      s = Stage::Geometry;
      break;
    case spv::ExecutionModelFragment:
      s = Stage::Fragment;
      break;
    default: // unimplemented
      throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
    }

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

void ShaderReflection::merge(std::vector<ShaderReflection::Binding>& ret,
                             ShaderReflection::PushBlock& pb,
                             const std::vector<ShaderReflection::Binding>& comp) {
  ret.reserve(comp.size());
  size_t id=0;
  for(size_t i=0; i<comp.size(); ++i, ++id) {
    auto& u = comp[i];
    if(u.cls==ShaderReflection::Push) {
      pb.stage = ShaderReflection::Compute;
      pb.size = u.size;
      continue;
      }
    ret.push_back(u);
    ret.back().stage = ShaderReflection::Compute;
    }

  finalize(ret);
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
        continue;
        }
      if(shId>0) {
        bool  ins = false;
        for(auto& r:ret)
          if(r.layout==u.layout) {
            r.stage = Stage(r.stage | u.stage);
            r.size  = std::max(r.size, u.size);
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
