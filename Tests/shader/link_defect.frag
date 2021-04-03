#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 outColor;

layout(location = 0) in VsData {
  vec4 array[2];
  vec2 pos;
  } shInp;

void main() {
  vec2 v = shInp.pos.xy*vec2(0.5)+vec2(0.5);
  outColor = vec4(v.rg,0.f,1.f);
  }
