#version 450
#extension GL_ARB_separate_shader_objects : enable

#if defined(TEXTURE)
layout(binding  = 0) uniform sampler2D texSampler;
layout(location = 1) in      vec2      inUV;
#endif

#if defined(EXT)
layout(push_constant, std140) uniform UboPush {
  float radius;
  } push;
#endif

layout(location = 0) out vec4 outColor;
layout(location = 0) in  vec4 inColor;

void main() {
#if defined(TEXTURE)
  outColor = inColor*texture(texSampler,inUV);
#else
  outColor = inColor;
#endif
  }
