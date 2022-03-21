#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(vertices = 3) out;

layout(location = 0) in  vec2 inPos[];
layout(location = 0) out vec2 outPos[];

void main(void) {
  if(gl_InvocationID==0) {
    gl_TessLevelInner[0] = 1;

    gl_TessLevelOuter[0] = 2;
    gl_TessLevelOuter[1] = 2;
    gl_TessLevelOuter[2] = 2;
    }
  outPos[gl_InvocationID]             = inPos[gl_InvocationID];
  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
  }
