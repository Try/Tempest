#pragma once

#include <Tempest/Window>
#include <Tempest/CommandBuffer>
#include <Tempest/Fence>
#include <Tempest/VulkanApi>
#include <Tempest/Device>
#include <Tempest/VertexBuffer>
#include <Tempest/DescriptorSet>
#include <Tempest/UniformBuffer>
#include <Tempest/VectorImage>
#include <Tempest/Event>

#include "mesh.h"

#include <vector>

class Game : public Tempest::Window {
  public:
  Game(Tempest::Device& device);
  ~Game();

  enum {
    MaxFramesInFlight = 2
    };

  private:
  void resizeEvent(Tempest::SizeEvent& event) override;
  void mouseWheelEvent(Tempest::MouseEvent& event) override;

  void mouseDownEvent(Tempest::MouseEvent& event) override;
  void mouseDragEvent(Tempest::MouseEvent& event) override;

  void setupUi();
  void render() override;
  void resetSwapchain();

  void onShaderType(size_t type);
  void onAsset(size_t type);

  Tempest::Matrix4x4 projMatrix() const;
  Tempest::Matrix4x4 viewMatrix() const;
  Tempest::Matrix4x4 objMatrix()  const;

  Tempest::Device&            device;
  Tempest::Swapchain          swapchain;
  Tempest::ZBuffer            zbuffer;
  Tempest::TextureAtlas       texAtlass;

  Tempest::VectorImage        surface;
  Tempest::VectorImage::Mesh  surfaceMesh[MaxFramesInFlight];

  Tempest::RenderPipeline     pso;
  Tempest::DescriptorSet      desc;
  Mesh                        mesh;
  bool                        useVertex = false;

  Tempest::CommandBuffer      commands[MaxFramesInFlight];
  std::vector<Tempest::Fence> fence;
  uint8_t                     cmdId = 0;

  Tempest::Point              mpos;
  Tempest::Vec2               rotation = {0, 100.f};
  float                       zoom = 0.5f;
  };
