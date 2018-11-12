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
    Game(Tempest::VulkanApi& api);

    struct Point {
      float x,y,z;
      float u,v;
      };

  private:
    void initSwapchain(const Tempest::UniformsLayout &ulay);
    void paintEvent(Tempest::PaintEvent& event);
    void resizeEvent(uint32_t /*w*/,uint32_t /*h*/);
    void render();

    uint32_t width() const;
    uint32_t height() const;

    struct Ubo {
      float r=1;
      float g=0;
      float b=0;
      };

    Tempest::Device                     device;
    Tempest::RenderPass                 pass;
    Tempest::RenderPipeline             pipeline;

    Tempest::VectorImage                surface;

    Tempest::VertexBuffer<Point>        vbo;
    Tempest::Texture2d                  texture;

    Tempest::UniformsLayout             ulay;
    Tempest::UniformBuffer              uboBuf;
    std::vector<Tempest::Uniforms>      ubo;

    struct FrameLocal {
      FrameLocal(Tempest::Device& dev);
      Tempest::Semaphore imageAvailable;
      Tempest::Fence     gpuLock;
      };

    std::vector<FrameLocal> fLocal;

    std::vector<Tempest::CommandBuffer> commandDynamic;
    std::vector<Tempest::CommandBuffer> commandStatic;
    std::vector<Tempest::Semaphore>     commandBuffersSemaphores;

    std::vector<Tempest::FrameBuffer>   fbo;
  };
