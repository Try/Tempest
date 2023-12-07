#version 450
#extension GL_ARB_separate_shader_objects : enable

const uint MaxVert = 64;
const uint MaxPrim = 64;

const uint MAX_DEBUG_COLORS = 10;

const vec3 debugColors[MAX_DEBUG_COLORS] = {
  vec3(1,1,1),
  vec3(1,0,0),
  vec3(0,1,0),
  vec3(0,0,1),
  vec3(1,1,0),
  vec3(1,0,1),
  vec3(0,1,1),
  vec3(1,0.5,0),
  vec3(0.5,1,0),
  vec3(0,0.5,1),
  };

layout(location = 0) in vec3 vertices;

out gl_PerVertex {
  vec4 gl_Position;
  };

layout(push_constant, std430) uniform PushConstant {
  mat4 mvp;
  uint meshletCount;
  } push;
// layout(std430, binding = 0) readonly buffer Vbo { vec3 vertices[]; };
// layout(std430, binding = 1) readonly buffer Ibo { uint indices []; };

layout(location = 0) out vec4 outColor;

void main() {
  gl_Position = push.mvp * vec4(vertices, 1);
  // Vertices color
  outColor    = vec4(debugColors[(gl_VertexIndex/(3*MaxPrim))%MAX_DEBUG_COLORS], 1.0);
  }
