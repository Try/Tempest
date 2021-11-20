#version 450
#extension GL_ARB_separate_shader_objects : enable

out gl_PerVertex {
  vec4 gl_Position;
  };

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec4 outColor;
#if defined(TEXTURE)
layout(location = 1) out vec2 outUV;
#endif

void main() {
  gl_Position = vec4(inPos, 1.0);
  outColor    = inColor;
#if defined(TEXTURE)
  outUV       = inUV;
#endif
  }
