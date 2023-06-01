#pragma once

#include <cstdint>
#include <string_view>

#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/StorageBuffer>

#include "lib/tiny_obj_loader.h"

class Mesh {
  public:
    Mesh() = default;
    Mesh(Tempest::Device& device, std::string_view name);

    enum {
      MaxVert = 64,
      OptPrim = 48,
      MaxPrim = 64,
      };

    struct Vertex {
      Tempest::Vec3 pos;
      };

    Tempest::VertexBuffer<Vertex>  vbo;
    Tempest::IndexBuffer<uint32_t> ibo;
    Tempest::StorageBuffer         ibo8;
    uint32_t                       meshletCount = 0;

    Tempest::Vec3                  bbox[2];

  private:
    using Vert = std::tuple<uint32_t,uint32_t,uint32_t>;
    using Prim = std::tuple<uint8_t, uint8_t, uint8_t>;

    std::vector<uint8_t>  packMeshlets(Tempest::Device& device, const tinyobj::attrib_t& data, const std::vector<tinyobj::shape_t>& shapes);
    std::vector<uint8_t>  packMeshletsNaive(Tempest::Device& device, const tinyobj::attrib_t& data, const std::vector<tinyobj::shape_t>& shapes);

    void packMeshlets(std::vector<Vertex>& vbo, std::vector<uint8_t>& ibo8, const tinyobj::attrib_t& data, const tinyobj::shape_t& shape);
    void packMeshletsNaive(std::vector<Vertex>& vbo, std::vector<uint8_t>& ibo8, const tinyobj::attrib_t& data, const tinyobj::shape_t& shape);

    std::vector<uint32_t> flattenIbo(const std::vector<uint8_t>& ibo8) const;

    void computeBbox(const tinyobj::attrib_t& data);

    struct Meshlet;
  };

