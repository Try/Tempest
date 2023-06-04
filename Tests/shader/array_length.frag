#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1, std430) buffer Input {
  uint ssbo[];
  };

layout(location = 0) out vec4 outColor;

void main() {
  ssbo[0] = 1234;
  outColor = vec4(1);
  }
