#version 450

layout(points) in;
layout(triangle_strip, max_vertices = 3) out;

in gl_PerVertex {
  vec4 gl_Position;
  };

layout(location = 0) out vec4 outColor;

const vec3 vboData[3] = {vec3(-1,-1,0), vec3(1,-1,0), vec3(1,1,0)};

void main() {
  for(int i=0; i<3; ++i) {
    outColor    = vec4(vboData[i].xy*vec2(0.5)+vec2(0.5),0,1);
    gl_Position = vec4(vboData[i], 1.0);
    EmitVertex();
    }
  EndPrimitive();
  }
