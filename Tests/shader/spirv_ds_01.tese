#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(triangles, equal_spacing, ccw) in;

// Per-vertex vector.
layout(location = 0)       in vec4 shInp0[];
// Per-vertex struct.
layout(location = 1)       in struct ShInp1
{
  vec4 val;
  vec2 b;
} shInp1[];
// Per-patch vector.
layout(location = 3) patch in vec4 shInp2;


layout(location = 0) out vec4 shOut;

vec4 interpolate(vec4 v0, vec4 v1, vec4 v2) {
  return gl_TessCoord.x*v0 + gl_TessCoord.y*v1 + gl_TessCoord.z*v2;
  }

void func() {
  shOut       = interpolate(shInp0[0],     shInp0[1],     shInp0[2]);
  shOut      += interpolate(shInp1[0].val, shInp1[1].val, shInp1[2].val);
  gl_Position = interpolate(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_in[2].gl_Position);
  }

void main(void) {
  func();
  }
