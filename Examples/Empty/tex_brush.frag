#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

layout(location = 0) in  vec2 inUV;
layout(location = 1) in  vec4 inColor;

void main() {
  outColor = texture(texSampler,inUV);//vec4(fragColor+vec3(ubo.r,ubo.g,ubo.b), 1.0);
  }
