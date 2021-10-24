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

#include <vector>

class Game : public Tempest::Window {
  public:
    Game(Tempest::Device& device);
    ~Game();

    enum {
      MaxFramesInFlight = 2
      };

    struct Point {
      float x,y,z;
      float u,v;
      };

  private:
    void paintEvent(Tempest::PaintEvent& event);
    void resizeEvent(Tempest::SizeEvent& event);
    void render();

    struct Ubo {
      float r=1;
      float g=0;
      float b=0;
      };

    Tempest::Device&                    device;
    Tempest::Swapchain                  swapchain;
    Tempest::TextureAtlas               texAtlass;

    Tempest::VectorImage                surface;
    Tempest::VectorImage::Mesh          surfaceMesh[MaxFramesInFlight];

    Tempest::VertexBuffer<Point>        vbo;
    Tempest::Texture2d                  texture;

    Tempest::CommandBuffer              commands[MaxFramesInFlight];
    std::vector<Tempest::Fence>         fence;
    uint8_t                             cmdId = 0;
  };
