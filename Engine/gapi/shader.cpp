#include "shader.h"

#include <Tempest/Except>
#include <libspirv/libspirv.h>

using namespace Tempest;
using namespace Tempest::Detail;

Shader::Shader(const void *src, size_t src_size) {
  if(src_size%4!=0)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
  fetchBindings(reinterpret_cast<const uint32_t*>(src), src_size/4);
  }

void Shader::fetchBindings(const uint32_t* source, const size_t size){
  // TODO: remove spirv-corss?
  spirv_cross::Compiler compiler(source, size);
  ShaderReflection::getVertexDecl(vert.decl,compiler);
  ShaderReflection::getBindings(lay,compiler,source,size);

  libspirv::Bytecode code(source, size);
  stage = ShaderReflection::getExecutionModel(code);

  for(auto& i:code) {
    if(i.op()==spv::OpExecutionMode) {
      if(i[2]==spv::ExecutionModeLocalSize) {
        comp.wgSize.x = i[3];
        comp.wgSize.y = i[4];
        comp.wgSize.z = i[5];
        }
      }
    if(i.op()==spv::OpDecorate && i[2]==spv::DecorationBuiltIn && i[3]==spv::BuiltInInstanceIndex) {
      vert.has_baseVertex_baseInstance = true;
      }
    if(i.op()==spv::OpDecorate && i[2]==spv::DecorationBuiltIn && i[3]==spv::BuiltInBaseVertex) {
      vert.has_baseVertex_baseInstance = true;
      }
    if(i.op()==spv::OpDecorate && i[2]==spv::DecorationBuiltIn && i[3]==spv::BuiltInNumWorkgroups) {
      comp.has_NumworkGroups = true;
      }
    }

  for(auto& i:code) {
    if(i.op()!=spv::OpSource)
      continue;
    uint32_t src = i.length()>3 ? i[3] : 0;
    if(src==0)
      continue;

    for(auto& r:code) {
      if(r.op()==spv::OpString && r[1]==src) {
        this->dbg.source = reinterpret_cast<const char*>(&r[2]);
        break;
        }
      }
    break;
    }
  }
