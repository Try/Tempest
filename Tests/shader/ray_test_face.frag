#version 460

#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_ray_query : enable
#extension GL_EXT_ray_flags_primitive_culling : enable

layout(location = 0) in  vec4 inPos;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform accelerationStructureEXT topLevelAS;


bool testIteration(uint flags) {
  vec3  rayOrigin    = vec3(inPos.xy*4.0-vec2(2.0),1.0);
  vec3  rayDirection = vec3(0,0,-1);
  float rayDistance  = 2.0;

  flags |= gl_RayFlagsTerminateOnFirstHitEXT;

  rayQueryEXT rayQuery;
  rayQueryInitializeEXT(rayQuery, topLevelAS, flags, 0xFF,
                        rayOrigin, 0.001, rayDirection, rayDistance);

  while(rayQueryProceedEXT(rayQuery))
    ;

  if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionNoneEXT)
    return false;
  return true;
  }

void main() {
  bool  front = testIteration(gl_RayFlagsCullBackFacingTrianglesEXT);
  bool  back  = testIteration(gl_RayFlagsCullFrontFacingTrianglesEXT);

  if(!front && !back)
    discard;

  outColor = vec4(0,0,0,1);
  if(front)
    outColor.x = 1;
  if(back)
    outColor.y = 1;
  }
