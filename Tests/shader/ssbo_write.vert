#version 450
#extension GL_ARB_separate_shader_objects : enable

out gl_PerVertex {
  vec4 gl_Position;
  };

layout(binding = 0, std140) writeonly buffer Output {
  vec4 val[];
  } sideEffect;

layout(location = 0) in  vec2 inPos;
layout(location = 0) out vec4 outColor;

void main() {
  // Note: this write requires an extra barrier
  sideEffect.val[gl_VertexIndex] = vec4(inPos,gl_VertexIndex,0);

  outColor    = vec4(inPos,    0.0, 0.0);
  gl_Position = vec4(inPos.xy, 1.0, 1.0);
  }
