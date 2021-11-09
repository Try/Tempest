#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in  vec4 inColor;
layout(location = 0) out vec4 outColor;

layout(binding = 0, std140) readonly buffer Input {
  vec4 val[];
  } ssbo;

void main() {
  outColor = vec4(0.25,0.5,0.75,1.0);//ssbo.val[int(gl_FragCoord.x)];
  }
