#include "mesh.h"

#include <Tempest/Device>
#include <Tempest/Log>

#include <unordered_map>
#include <stdexcept>

#define TINYOBJLOADER_IMPLEMENTATION
#include "lib/tiny_obj_loader.h"

using namespace Tempest;

bool operator == (const tinyobj::index_t a, const tinyobj::index_t b) {
  return a.normal_index  ==b.normal_index &&
         a.texcoord_index==b.texcoord_index &&
         a.vertex_index  ==b.vertex_index;
  }

template<>
struct std::hash<tinyobj::index_t> {
  size_t operator() (tinyobj::index_t a) const {
    std::hash<uint32_t> h;
    return h(uint32_t(a.vertex_index ^ a.texcoord_index));
    }
  };

template<>
struct std::equal_to<tinyobj::index_t> {
  size_t operator() (tinyobj::index_t a, tinyobj::index_t b) const {
    return a.normal_index  ==b.normal_index &&
           a.texcoord_index==b.texcoord_index &&
           a.vertex_index  ==b.vertex_index;
    }
  };

struct Mesh::Meshlet {
  tinyobj::index_t vbo[MaxVert] = {};
  Prim             ibo[MaxPrim] = {};

  uint8_t          vertSz = 0;
  uint8_t          primSz = 0;

  void clear() {
    vertSz = 0;
    primSz = 0;
    }

  bool flush(const tinyobj::attrib_t& data, std::vector<Vertex>& vr, std::vector<uint8_t>& id) {
    if(primSz==0)
      return false;

    size_t vertAt = vr.size();
    size_t indAt  = id.size();

    vr.resize(vertAt + MaxVert);
    id.resize(indAt  + MaxPrim*4);

    for(size_t i=0; i<vertSz; ++i) {
      vr[vertAt + i].pos.x = data.vertices[vbo[i].vertex_index*3 + 0];
      vr[vertAt + i].pos.y = data.vertices[vbo[i].vertex_index*3 + 1];
      vr[vertAt + i].pos.z = data.vertices[vbo[i].vertex_index*3 + 2];
      }

    for(size_t i=0; i<primSz; ++i) {
      id[indAt + i*4 + 0] = std::get<0>(ibo[i]);
      id[indAt + i*4 + 1] = std::get<1>(ibo[i]);
      id[indAt + i*4 + 2] = std::get<2>(ibo[i]);
      id[indAt + i*4 + 3] = 0;
      }
    for(size_t i=primSz; i<MaxPrim; ++i) {
      id[indAt + i*4 + 0] = 0;
      id[indAt + i*4 + 1] = 0;
      id[indAt + i*4 + 2] = 0;
      id[indAt + i*4 + 3] = 0;
      }
    id[indAt + 3] = primSz;

    // Log::i("vert = ", 100.f*vertSz/float(MaxVert), "% prim = ", 100.f*primSz/float(MaxPrim));
    // Log::i("vert = ", vertSz, " prim = ", primSz);
    clear();
    return true;
    }

  bool addTriangle(const tinyobj::mesh_t& m, uint32_t primitiveId) {
    return addTriangle(m.indices[primitiveId*3 + 0], m.indices[primitiveId*3 + 1], m.indices[primitiveId*3 + 2]);
    }

  bool addTriangle(tinyobj::index_t a, tinyobj::index_t b, tinyobj::index_t c) {
    if(primSz+1>MaxPrim)
      return false;

    uint8_t ea = MaxVert, eb = MaxVert, ec = MaxVert;
    for(uint8_t i=0; i<vertSz; ++i) {
      if(vbo[i]==a)
        ea = i;
      if(vbo[i]==b)
        eb = i;
      if(vbo[i]==c)
        ec = i;
      }

    uint8_t vSz = vertSz;
    if(ea==MaxVert) {
      ea = vSz;
      ++vSz;
      }
    if(eb==MaxVert) {
      eb = vSz;
      ++vSz;
      }
    if(ec==MaxVert) {
      ec = vSz;
      ++vSz;
      }

    if(vSz>MaxVert)
      return false;

    ibo[primSz] = std::make_tuple(ea,eb,ec);
    primSz++;

    vbo[ea] = a;
    vbo[eb] = b;
    vbo[ec] = c;
    vertSz  = vSz;

    return true;
    }
  };

Mesh::Mesh(Device& device, std::string_view name) {
  tinyobj::attrib_t                attrib;
  std::vector<tinyobj::shape_t>    shapes;
  std::vector<tinyobj::material_t> materials;

  std::string err;

  const auto inputfile = std::string(name);
  const bool ret       = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, inputfile.c_str(), nullptr, true);
  if(!ret)
    throw std::runtime_error(err);
  Log::i("[OK] obj parsing");

  // const std::vector<uint8_t> ibo8 = packMeshletsNaive(device, attrib, shapes);
  const std::vector<uint8_t> ibo8 = packMeshlets(device, attrib, shapes);
  this->ibo8 = device.ssbo(ibo8);
  Log::i("[OK] meshlet pass. Meshlets: ", meshletCount);

  const std::vector<uint32_t> ibo32 = flattenIbo(ibo8);
  this->ibo = device.ibo(ibo32);
  Log::i("[OK] ibo pass");

  computeBbox(attrib);
  }

std::vector<uint8_t> Mesh::packMeshlets(Device& device, const tinyobj::attrib_t& data, const std::vector<tinyobj::shape_t>& shapes) {
  std::vector<Vertex>  vbo;
  std::vector<uint8_t> ibo8;

  for(auto& shp:shapes) {
    packMeshlets(vbo, ibo8, data, shp);
    }

  this->vbo = device.vbo(vbo);
  return ibo8;
  }

std::vector<uint8_t> Mesh::packMeshletsNaive(Tempest::Device& device, const tinyobj::attrib_t& data, const std::vector<tinyobj::shape_t>& shapes) {
  std::vector<Vertex>  vbo;
  std::vector<uint8_t> ibo8;

  for(auto& shp:shapes) {
    packMeshletsNaive(vbo, ibo8, data, shp);
    }

  this->vbo = device.vbo(vbo);
  return ibo8;
  }

void Mesh::packMeshlets(std::vector<Vertex>& vbo, std::vector<uint8_t>& ibo8, const tinyobj::attrib_t& data, const tinyobj::shape_t& shp) {
  std::unordered_multimap<tinyobj::index_t,size_t> map;
  for(size_t i=0; i<shp.mesh.indices.size(); i++) {
    map.insert(std::make_pair(shp.mesh.indices[i], i/3));
    }

  std::vector<bool> used(shp.mesh.indices.size()/3);

  const bool tightPacking = false;
  Meshlet  active;
  uint8_t  firstVert   = 0;
  uint32_t firstUnused = 0;

  while(true) {
    uint32_t prim = uint32_t(-1);
    for(size_t r=firstVert; r<active.vertSz; ++r) {
      auto b = map.equal_range(active.vbo[firstVert]);
      for(auto i=b.first; i!=b.second; ++i) {
        if(used[i->second])
          continue;
        prim = uint32_t(i->second);
        if(!active.addTriangle(shp.mesh, prim))
          break;
        used[prim] = true;
        }
      firstVert = uint32_t(r+1);
      }

    if(prim==uint32_t(-1) && (active.primSz>OptPrim && !tightPacking)) {
      meshletCount += active.flush(data,vbo,ibo8);
      firstVert = 0;
      }

    if(prim==uint32_t(-1)) {
      for(size_t i=firstUnused; i<used.size(); ++i) {
        if(used[i])
          continue;
        firstUnused = uint32_t(i);
        prim        = uint32_t(i);
        break;
        }
      }

    if(prim!=uint32_t(-1) && active.addTriangle(shp.mesh, prim)) {
      used[prim] = true;
      continue;
      }

    if(prim==uint32_t(-1))
      break;

    meshletCount += active.flush(data,vbo,ibo8);
    firstVert = 0;
    }

  meshletCount += active.flush(data,vbo,ibo8);
  }

void Mesh::packMeshletsNaive(std::vector<Vertex>& vbo, std::vector<uint8_t>& ibo8,
                             const tinyobj::attrib_t& data, const tinyobj::shape_t& shp) {
  Meshlet tmp;
  for(size_t i=0; i<shp.mesh.indices.size(); ) {
    if(tmp.addTriangle(shp.mesh.indices[i+0], shp.mesh.indices[i+1], shp.mesh.indices[i+2])) {
      i += 3;
      continue;
      }
    meshletCount += tmp.flush(data,vbo,ibo8);
    }
  meshletCount += tmp.flush(data,vbo,ibo8);
  }

void Mesh::computeBbox(const tinyobj::attrib_t& data) {
  for(size_t i=0; i<data.vertices.size(); i+=3) {
    Vec3 at;
    at.x = data.vertices[i+0];
    at.y = data.vertices[i+1];
    at.z = data.vertices[i+2];
    if(i==0) {
      bbox[0] = at;
      bbox[1] = at;
      } else {
      bbox[0].x = std::min(bbox[0].x, at.x);
      bbox[0].y = std::min(bbox[0].y, at.y);
      bbox[0].z = std::min(bbox[0].z, at.z);

      bbox[1].x = std::max(bbox[1].x, at.x);
      bbox[1].y = std::max(bbox[1].y, at.y);
      bbox[1].z = std::max(bbox[1].z, at.z);
      }
    }
  }

std::vector<uint32_t> Mesh::flattenIbo(const std::vector<uint8_t>& ibo8) const {
  std::vector<uint32_t> ibo((ibo8.size()*3)/4);

  for(size_t i8=0, i32=0; i8<ibo8.size(); i8+=MaxPrim*4, i32+=MaxPrim*3) {
    uint32_t off = uint32_t(i8/(MaxPrim*4)) * (MaxVert);
    for(size_t r=0; r<MaxPrim; ++r) {
      ibo[i32 + r*3 + 0] = ibo8[i8 + r*4 + 0] + off;
      ibo[i32 + r*3 + 1] = ibo8[i8 + r*4 + 1] + off;
      ibo[i32 + r*3 + 2] = ibo8[i8 + r*4 + 2] + off;
      }
    }
  return ibo;
  }
