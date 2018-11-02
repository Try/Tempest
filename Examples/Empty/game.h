#pragma once

#include <Tempest/Window>
#include <Tempest/CommandBuffer>
#include <Tempest/Fence>
#include <Tempest/Semaphore>
#include <Tempest/VulkanApi>
#include <Tempest/Device>
#include <Tempest/VertexBuffer>
#include <Tempest/UniformsLayout>
#include <vector>

class Game : public Tempest::Window {
  public:
    enum { MAX_FRAMES_IN_FLIGHT=2 };

    Game(Tempest::VulkanApi& api);

  private:
    void initSwapchain(const Tempest::UniformsLayout &ulay);
    void resizeEvent(uint32_t /*w*/,uint32_t /*h*/);
    void render();

    uint32_t width() const;
    uint32_t height() const;

    struct Point {
      float x,y;
      };

    struct Ubo {
      float r=1;
      float g=0;
      float b=0;
      };

    Tempest::Device                     device;
    Tempest::RenderPass                 pass;
    Tempest::RenderPipeline             pipeline;

    Tempest::VertexBuffer<Point>        vbo;
    Tempest::Texture2d                  texture;

    Tempest::UniformsLayout             ulay;
    std::vector<Tempest::Uniforms>      ubo;

    std::vector<Tempest::Semaphore>     renderFinishedSemaphores;
    std::vector<Tempest::Semaphore>     imageAvailableSemaphores;
    std::vector<Tempest::Fence>         inFlightFences;
    std::vector<Tempest::CommandBuffer> commandBuffers;
    std::vector<Tempest::FrameBuffer>   fbo;
    uint32_t                            currentFrame=0;
  };
