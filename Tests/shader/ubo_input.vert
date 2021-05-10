#version 450
#extension GL_ARB_separate_shader_objects : enable

out gl_PerVertex {
  vec4 gl_Position;
  };

layout(location = 0) in  vec2 inPos;
layout(location = 0) out vec4 outColor;

layout(binding = 2, std140) uniform Ubo {
  vec4 v[3];
  } color;

void main() {
  outColor    = color.v[gl_VertexIndex];
  gl_Position = vec4(inPos.xy, 1.0, 1.0);
  }
