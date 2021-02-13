#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(triangles, equal_spacing, ccw) in;

vec4 interpolate(vec4 v0, vec4 v1, vec4 v2) {
  return gl_TessCoord.x*v0 + gl_TessCoord.y*v1 + gl_TessCoord.z*v2;
  }

void main(void) {
  gl_Position  = interpolate(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_in[2].gl_Position);
  }
