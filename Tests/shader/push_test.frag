#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in  vec4 inColor;
layout(location = 0) out vec4 outColor;

layout(binding = 0, std140) uniform SSBO0 {
  vec4 clr;
  };

void main() {
  outColor = vec4(clr.xy,inColor.z,1);
  }
