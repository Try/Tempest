#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
  float r;
  float g;
  float b;
  } ubo;
layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in  vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
  outColor = texture(texSampler, fragColor.xy);//vec4(fragColor+vec3(ubo.r,ubo.g,ubo.b), 1.0);
  }
