#include "shaderreflection.h"

#include <Tempest/Except>

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
  spirv_cross::ShaderResources resources = comp.get_shader_resources();
  for(auto &resource : resources.sampled_images) {
    unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);
    Binding b;
    b.cls    = UniformsLayout::Texture;
    b.layout = binding;
    lay.push_back(b);
    }
  for(auto &resource : resources.uniform_buffers) {
    auto&    t       = comp.get_type_from_variable(resource.id);
    unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);
    auto     sz      = comp.get_declared_struct_size(t);
    Binding b;
    b.cls    = UniformsLayout::Ubo;
    b.layout = binding;
    b.size   = sz;
    lay.push_back(b);
    }
  for(auto &resource : resources.storage_buffers) {
    auto&    t        = comp.get_type_from_variable(resource.id);
    unsigned binding  = comp.get_decoration(resource.id, spv::DecorationBinding);
    auto     readonly = comp.get_buffer_block_flags(resource.id);
    auto     sz       = comp.get_declared_struct_size(t);
    Binding b;
    b.cls    = (readonly.get(spv::DecorationNonWritable) ? UniformsLayout::SsboR : UniformsLayout::SsboRW);
    b.layout = binding;
    b.size   = sz;
    lay.push_back(b);
    }
  for(auto &resource : resources.storage_images) {
    unsigned binding  = comp.get_decoration(resource.id, spv::DecorationBinding);
    Binding b;
    b.cls    = UniformsLayout::ImgRW; // (readonly.get(spv::DecorationNonWritable) ? UniformsLayout::ImgR : UniformsLayout::ImgRW);
    b.layout = binding;
    lay.push_back(b);
    }
  for(auto &resource : resources.push_constant_buffers) {
    auto& t = comp.get_type_from_variable(resource.id);
    auto sz = comp.get_declared_struct_size(t);
    Binding b;
    b.cls    = UniformsLayout::Push;
    b.layout = uint32_t(-1);
    b.size   = sz;
    lay.push_back(b);
    }
  }

void ShaderReflection::merge(std::vector<ShaderReflection::Binding>& ret,
                             ShaderReflection::PushBlock& pb,
                             const std::vector<ShaderReflection::Binding>& comp) {
  ret.reserve(comp.size());
  size_t id=0;
  for(size_t i=0;i<comp.size();++i, ++id) {
    auto& u = comp[i];
    if(u.cls==UniformsLayout::Push) {
      pb.stage = UniformsLayout::Stage(pb.stage | UniformsLayout::Vertex);
      pb.size = u.size;
      continue;
      }
    ret.push_back(u);
    ret.back().stage = UniformsLayout::Compute;
    }

  finalize(ret);
  }

void ShaderReflection::merge(std::vector<ShaderReflection::Binding>& ret,
                             PushBlock& pb,
                             const std::vector<ShaderReflection::Binding>* sh[],
                             size_t count) {
  size_t expectedSz = 0;
  for(size_t i=0; i<count; ++i)
    if(sh[i]!=nullptr)
      expectedSz+=sh[i]->size();
  ret.reserve(expectedSz);

  static const UniformsLayout::Stage stages[] = {
    UniformsLayout::Vertex,
    UniformsLayout::Control,
    UniformsLayout::Evaluate,
    UniformsLayout::Geometry,
    UniformsLayout::Fragment
    };

  for(size_t shId=0; shId<count; ++shId) {
    if(sh[shId]==nullptr)
      continue;

    UniformsLayout::Stage stage = stages[shId];
    auto&                 vs    = *sh[shId];

    for(size_t i=0;i<vs.size();++i) {
      auto& u   = vs[i];
      if(u.cls==UniformsLayout::Push) {
        pb.stage = UniformsLayout::Stage(pb.stage | stage);
        pb.size  = u.size;
        continue;
        }
      if(shId>0) {
        bool  ins = false;
        for(auto& r:ret)
          if(r.layout==u.layout) {
            r.stage = UniformsLayout::Stage(r.stage | stage);
            ins = true;
            break;
            }
        if(ins)
          continue;
        }
      ret.push_back(u);
      ret.back().stage = stage;
      }
    }

  finalize(ret);
  }

void ShaderReflection::finalize(std::vector<ShaderReflection::Binding>& ret) {
  std::sort(ret.begin(),ret.end(),[](const Binding& a, const Binding& b){
    return a.layout<b.layout;
    });

  for(size_t i=0; i<ret.size(); ++i)
    if(ret[i].layout!=i) {
      ShaderReflection::Binding fill;
      fill.layout = uint32_t(i);
      fill.stage  = UniformsLayout::Stage(0);
      ret.insert(ret.begin()+int(i),fill);
      }
  }
