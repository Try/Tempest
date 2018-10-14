#include "game.h"

Game::Game(Tempest::VulkanApi& api)
  :device(api,hwnd()) {
  for(int i=0;i<MAX_FRAMES_IN_FLIGHT;++i){
    imageAvailableSemaphores.emplace_back(device);
    renderFinishedSemaphores.emplace_back(device);
    inFlightFences.emplace_back(device);
    }
  std::initializer_list<Point> source = {
    {-0.5f, +0.5f},
    {+0.5f, +0.5f},
    {+0.0f, -0.5f}
    };
  Ubo usrc={};

  vbo = device.loadVbo(source.begin(),source.size());
  ubo = device.loadUniforms(&usrc,sizeof(usrc));

  initSwapchain();
  }

void Game::initSwapchain(){
  auto vs  = device.loadShader("shader/vert.spv");
  auto fs  = device.loadShader("shader/frag.spv");

  pass     = device.pass();
  pipeline = device.pipeline(pass,width(),height(),vs,fs);

  commandBuffers.clear();
  fbo.clear();

  for(size_t i=0;i<3;++i) {
    Tempest::Frame frame=device.frame(i);
    fbo.emplace_back(device.frameBuffer(frame,pass));
    commandBuffers.emplace_back(device.commandBuffer());

    Tempest::CommandBuffer& buf=commandBuffers[i];
    buf.begin();
    //buf.clear(frame,0,1,1,0);

    buf.beginRenderPass(fbo[i],pass,width(),height());
    buf.setPipeline(pipeline);
    buf.setVbo(vbo);
    buf.draw(3);
    buf.endRenderPass();

    buf.end();
    }
  }

void Game::resizEvent(uint32_t, uint32_t) {
  initSwapchain();
  }

uint32_t Game::width() const {
  return uint32_t(this->w());
  }

uint32_t Game::height() const {
  return uint32_t(this->h());
  }

void Game::render(){
  Tempest::Fence&     frameReadyCpu=inFlightFences          [currentFrame];
  Tempest::Semaphore& frameReady   =imageAvailableSemaphores[currentFrame];
  Tempest::Semaphore& renderDone   =renderFinishedSemaphores[currentFrame];

  frameReadyCpu.wait();

  uint32_t imgId=device.nextImage(frameReady);
  device.draw(commandBuffers[imgId],frameReady,renderDone,frameReadyCpu);
  device.present(imgId,renderDone);

  currentFrame=(currentFrame+1)%MAX_FRAMES_IN_FLIGHT;
  }
