#version 450
#extension GL_ARB_separate_shader_objects : enable

out gl_PerVertex {
  vec4 gl_Position;
  };

layout(location = 1) out vec2 texcoord;

vec2 vbo[] = {
  {-1,-1},
  { 1,-1},
  { 1, 1},
  {-1,-1},
  { 1, 1},
  {-1, 1}
  };

vec2 texc[] = {
  { 0, 0},
  { 1, 0},
  { 1, 1},
  { 0, 0},
  { 1, 1},
  { 0, 1},
  };

void main() {
  vec2 pos    = vbo [gl_VertexIndex];
  texcoord    = texc[gl_VertexIndex];
  gl_Position = vec4(pos, 0.0, 1.0);
  }
