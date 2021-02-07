#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in  vec2 inPos;
layout(location = 0) out vec4 outColor;

void main() {
  vec2 v = inPos.xy*vec2(0.5)+vec2(0.5);
  outColor = vec4(v.rg,0.f,1.f);
  }
