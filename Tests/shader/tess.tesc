#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(vertices = 3) out;

layout(location = 0) in  vec4 dummyIn[];
layout(location = 0) out vec4 dummyOut[];
layout(location = 1) patch out vec4 dummyOut2;

void main(void) {
  if(gl_InvocationID==0) {
    dummyOut2 = vec4(7);
    gl_TessLevelInner[0] = 9;
    gl_TessLevelInner[1] = 8;

    gl_TessLevelOuter[0] = 0;
    gl_TessLevelOuter[1] = 1;
    gl_TessLevelOuter[2] = 2;
    gl_TessLevelOuter[3] = 3;
    }

  dummyOut[gl_InvocationID] = dummyIn[gl_InvocationID];
  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
  }
