#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(triangles, equal_spacing, cw) in;

layout(location = 0) in  vec2 inPos[];
layout(location = 0) out vec4 outClr;

vec2 interpolate(vec2 v0, vec2 v1, vec2 v2) {
  return gl_TessCoord.x*v0 + gl_TessCoord.y*v1 + gl_TessCoord.z*v2;
  }

vec4 interpolate(vec4 v0, vec4 v1, vec4 v2) {
  return gl_TessCoord.x*v0 + gl_TessCoord.y*v1 + gl_TessCoord.z*v2;
  }

void main(void) {
  vec4 clr[3] = vec4[](
      vec4(1, 0, 0, 1),
      vec4(0, 1, 0, 1),
      vec4(0, 0, 1, 1)
  );

  outClr      = interpolate(clr[0], clr[1], clr[2]);
  gl_Position = interpolate(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_in[2].gl_Position);
  }
