#version 450
#extension GL_ARB_separate_shader_objects : enable

out gl_PerVertex {
  vec4 gl_Position;
  };

layout(location = 0) in  vec2 inPos;
layout(location = 1) out vec2 texcoord;

void main() {
  texcoord    = (inPos.xy+vec2(1.0))*0.5;
  gl_Position = vec4(inPos, 0.0, 1.0);
  }
