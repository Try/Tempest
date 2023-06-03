#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0, std430) buffer Input {
  uint ssbo[];
  };

out gl_PerVertex {
  vec4 gl_Position;
  };

vec2 vbo[] = {
  {-1,-1},
  { 1,-1},
  { 1, 1},
  {-1,-1},
  { 1, 1},
  {-1, 1}
  };

void main() {
  ssbo[gl_VertexIndex] = ssbo.length();

  vec2 pos    = vbo [gl_VertexIndex];
  gl_Position = vec4(pos, 0.0, 1.0);
  }
