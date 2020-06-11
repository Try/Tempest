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
  }

void ShaderReflection::merge(std::vector<ShaderReflection::Binding>& ret,
                             const std::vector<ShaderReflection::Binding>& vs,
                             const std::vector<ShaderReflection::Binding>& fs) {
  ret.resize(vs.size()+fs.size());
  size_t id=0;
  for(size_t i=0;i<vs.size();++i, ++id) {
    ret[id]      = vs[i];
    ret[id].stage = UniformsLayout::Vertex;
    }
  for(size_t i=0;i<fs.size();++i, ++id) {
    ret[id]      = fs[i];
    ret[id].stage = UniformsLayout::Fragment;
    }
  }
