#pragma once

#include <Tempest/Window>
#include <Tempest/CommandBuffer>
#include <Tempest/Fence>
#include <Tempest/Semaphore>
#include <Tempest/VulkanApi>
#include <Tempest/Device>
#include <Tempest/VertexBuffer>
#include <Tempest/UniformsLayout>
#include <Tempest/UniformBuffer>
#include <Tempest/VectorImage>
#include <Tempest/Event>

#include <vector>

class Game : public Tempest::Window {
  public:
    Game(Tempest::Device& device);
    ~Game();

    struct Point {
      float x,y,z;
      float u,v;
      };

  private:
    void initSwapchain();
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

    Tempest::RenderPass                 pass;

    Tempest::VectorImage                surface;

    Tempest::VertexBuffer<Point>        vbo;
    Tempest::Texture2d                  texture;

    struct FrameLocal {
      FrameLocal(Tempest::Device& dev);
      Tempest::Semaphore imageAvailable;
      Tempest::Semaphore renderDone;
      Tempest::Fence     gpuLock;
      };

    std::vector<FrameLocal> fLocal;

    std::vector<Tempest::CommandBuffer> commands;
    std::vector<Tempest::FrameBuffer>   fbo;
  };
