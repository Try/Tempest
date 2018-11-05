#include "game.h"

#include <Tempest/Assets>
#include <Tempest/Pixmap>
#include <Tempest/Except>

Game::Game(Tempest::VulkanApi& api)
  :device(api,hwnd()) {
  Tempest::Assets data("shader");
  auto vs=data["vert.spv"];
  (void)vs;

  Tempest::Pixmap pm("img/1.jpg");
  texture = device.loadTexture(pm);

  std::initializer_list<Point> source = {
    {-0.5f, +0.5f},
    {+0.5f, +0.5f},
    {+0.0f, -0.5f}
    };
  vbo     = device.loadVbo(source.begin(),source.size(),Tempest::BufferFlags::Static);

  for(int i=0;i<MAX_FRAMES_IN_FLIGHT;++i){
    imageAvailableSemaphores.emplace_back(device);
    renderFinishedSemaphores.emplace_back(device);
    inFlightFences.emplace_back(device);
    }

  ulay.add(0,Tempest::UniformsLayout::Ubo,    Tempest::UniformsLayout::Fragment);
  ulay.add(1,Tempest::UniformsLayout::Texture,Tempest::UniformsLayout::Fragment);

  initSwapchain(ulay);
  }

void Game::initSwapchain(const Tempest::UniformsLayout &ulay){
  Ubo usrc={};

  auto vs  = device.loadShader("shader/vert.spv");
  auto fs  = device.loadShader("shader/frag.spv");

  pass     = device.pass();
  pipeline = device.pipeline(pass,width(),height(),ulay,vs,fs);
  uboBuf   = device.loadUbo(&usrc,sizeof(usrc));

  const size_t imgC=device.swapchainImageCount();
  if(ubo.size()==0) {
    for(size_t i=0;i<imgC;++i) {
      Tempest::Uniforms u=device.loadUniforms(ulay);
      u.set(0,uboBuf,0,sizeof(usrc));
      u.set(1,texture);
      ubo.emplace_back(std::move(u));
      }
    }

  commandBuffers.clear();
  fbo.clear();

  for(size_t i=0;i<imgC;++i) {
    Tempest::Frame frame=device.frame(i);
    fbo.emplace_back(device.frameBuffer(frame,pass));
    commandBuffers.emplace_back(device.commandBuffer());

    Tempest::CommandBuffer& buf=commandBuffers[i];
    buf.begin();
    //buf.clear(frame,0,1,1,0);

    buf.beginRenderPass(fbo[i],pass,width(),height());
    buf.setPipeline(pipeline);
    buf.setUniforms(pipeline,ubo[i]);
    buf.setVbo(vbo);
    buf.draw(3);
    buf.endRenderPass();

    buf.end();
    }
  }

void Game::resizeEvent(uint32_t, uint32_t) {
  device.reset();
  initSwapchain(ulay);
  }

uint32_t Game::width() const {
  return uint32_t(this->w());
  }

uint32_t Game::height() const {
  return uint32_t(this->h());
  }

void Game::render(){
  try {
    Tempest::Fence&     frameReadyCpu=inFlightFences          [currentFrame];
    Tempest::Semaphore& frameReady   =imageAvailableSemaphores[currentFrame];
    Tempest::Semaphore& renderDone   =renderFinishedSemaphores[currentFrame];

    frameReadyCpu.wait();

    uint32_t imgId=device.nextImage(frameReady);
    device.draw(commandBuffers[imgId],frameReady,renderDone,frameReadyCpu);
    device.present(imgId,renderDone);

    currentFrame=(currentFrame+1)%MAX_FRAMES_IN_FLIGHT;
    }
  catch(const Tempest::DeviceLostException&) {
    device.reset();
    initSwapchain(ulay);
    }
  }
