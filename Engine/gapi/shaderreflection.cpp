#include "shaderreflection.h"

#include "thirdparty/spirv_cross/spirv_common.hpp"

using namespace Tempest;
using namespace Tempest::Detail;

void ShaderReflection::getBindings(std::vector<ShaderReflection::Binding>& b, const uint32_t* sprv, uint32_t size) {
  spirv_cross::Compiler comp(sprv,size);
  getBindings(b,comp);
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
    unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);
    Binding b;
    b.cls    = UniformsLayout::Ubo;
    b.layout = binding;
    lay.push_back(b);
    }
  for(auto &resource : resources.storage_buffers) {
    unsigned binding  = comp.get_decoration(resource.id, spv::DecorationBinding);
    auto     readonly = comp.get_buffer_block_flags(resource.id);
    Binding b;
    b.cls    = (readonly.get(spv::DecorationNonWritable) ? UniformsLayout::SsboR : UniformsLayout::SsboRW);
    b.layout = binding;
    lay.push_back(b);
    }
  for(auto &resource : resources.storage_images) {
    unsigned binding  = comp.get_decoration(resource.id, spv::DecorationBinding);
    auto     readonly = comp.get_decoration_bitset(resource.id);
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

  std::sort(ret.begin(),ret.end(),[](const Binding& a, const Binding& b){
    return a.layout<b.layout;
    });
  }

void ShaderReflection::merge(std::vector<ShaderReflection::Binding>& ret,
                             PushBlock& pb,
                             const std::vector<ShaderReflection::Binding>& vs,
                             const std::vector<ShaderReflection::Binding>& fs) {
  ret.reserve(vs.size()+fs.size());
  size_t id=0;
  for(size_t i=0;i<vs.size();++i, ++id) {
    auto& u = vs[i];
    if(u.cls==UniformsLayout::Push) {
      pb.stage = UniformsLayout::Stage(pb.stage | UniformsLayout::Vertex);
      pb.size = u.size;
      continue;
      }
    ret.push_back(u);
    ret.back().stage = UniformsLayout::Vertex;
    }
  for(size_t i=0;i<fs.size();++i) {
    auto& u   = fs[i];
    if(u.cls==UniformsLayout::Push) {
      pb.stage = UniformsLayout::Stage(pb.stage | UniformsLayout::Fragment);
      pb.size = u.size;
      continue;
      }
    bool  ins = false;
    for(auto& r:ret)
      if(r.layout==u.layout) {
        r.stage = UniformsLayout::Stage(r.stage | UniformsLayout::Fragment);
        ins = true;
        break;
        }
    if(ins)
      continue;
    ret.push_back(u);
    ret.back().stage = UniformsLayout::Fragment;
    }
  std::sort(ret.begin(),ret.end(),[](const Binding& a, const Binding& b){
    return a.layout<b.layout;
    });
  }
