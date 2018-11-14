#pragma once

#include "resources.h"

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
#include <Tempest/Pixmap>

#include <vector>

#include "resources.h"

class MainWindow : public Tempest::Window {
  public:
    MainWindow(Tempest::VulkanApi& api);

  private:
    void paintEvent(Tempest::PaintEvent& event);
    void resizeEvent(uint32_t /*w*/,uint32_t /*h*/);

    void setupUi();

    void render();
    void initSwapchain();

    Tempest::Device      device;
    Resources            resources;

    Tempest::RenderPass  mainPass;
    Tempest::VectorImage surface;

    const Tempest::Texture2d& background = Resources::get<Tempest::Texture2d>("back.png");

    struct FrameLocal {
      FrameLocal(Tempest::Device& dev):imageAvailable(dev),gpuLock(dev){}
      Tempest::Semaphore imageAvailable;
      Tempest::Fence     gpuLock;
      };

    std::vector<FrameLocal>             fLocal;

    std::vector<Tempest::CommandBuffer> commandDynamic;
    std::vector<Tempest::Semaphore>     commandBuffersSemaphores;

    std::vector<Tempest::FrameBuffer>   fbo;
  };
