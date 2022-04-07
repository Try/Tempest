#version 460

#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_ray_query : enable

layout(location = 0) in  vec4 inPos;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform accelerationStructureEXT topLevelAS;

void main() {
  vec3  rayOrigin    = vec3(inPos.xy*4.0-vec2(2.0),1.0);
  vec3  rayDirection = vec3(0,0,-1);
  float rayDistance  = 2.0;

  rayQueryEXT rayQuery;
  rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF,
                        rayOrigin, 0.001, rayDirection, rayDistance);

  while(rayQueryProceedEXT(rayQuery))
    ;

  if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionNoneEXT)
    discard;

  outColor = inPos;
  }
