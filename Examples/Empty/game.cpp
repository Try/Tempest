#include "game.h"

#include <Tempest/Pixmap>
#include <Tempest/Except>
#include <Tempest/Painter>
#include <Tempest/Rect>

using namespace Tempest;

Game::FrameLocal::FrameLocal(Device &device)
  :imageAvailable(device.semaphore()),renderDone(device.semaphore()),gpuLock(device.fence()) {
  }

Game::Game(Device& device)
  :device(device), swapchain(device,hwnd()), texAtlass(device) {
  //VectorImage image;
  //image.load("img/test.svg");

  //Tempest::Pixmap pm("img/texture.png");
  Tempest::Pixmap pm("img/texture.hdr");
  texture = device.loadTexture(pm);

  for(uint8_t i=0;i<device.maxFramesInFlight();++i){
    fLocal.emplace_back(device);
    }

  pass = device.pass(Tempest::FboMode(Tempest::FboMode::PreserveOut,Color(0.f,0.f,1.f,1.f)));
  initSwapchain();
  }

Game::~Game() {
  device.waitIdle();
  }

void Game::initSwapchain(){
  const size_t imgC=swapchain.imageCount();

  fbo.clear();
  commands.resize(imgC);

  for(size_t i=0;i<imgC;++i) {
    Tempest::Attachment& frame=swapchain.frame(i);
    fbo.emplace_back(device.frameBuffer(frame));
    }
  }

void Game::paintEvent(PaintEvent& event) {
  Painter p(event);

  p.setBrush(Brush(texture));
  p.drawRect(100,50, 200,100);

  p.setBrush(Brush(texture,Color(1,0,0,1)));
  p.drawRect(300,150,texture.w(),texture.h());

  p.setPen(Pen(Color(),Painter::Alpha,4));
  p.drawLine(100,100,200,200);
  }

void Game::resizeEvent(SizeEvent&) {
  for(auto& i:fLocal)
    i.gpuLock.wait();
  swapchain.reset();
  initSwapchain();
  }

void Game::render(){
  try {
    auto&       context = fLocal  [swapchain.frameId()];
    auto&       cmd     = commands[swapchain.frameId()];

    context.gpuLock.wait();

    const uint32_t imgId=swapchain.nextImage(context.imageAvailable);

    dispatchPaintEvent(surface,texAtlass);

    {
    auto enc = cmd.startEncoding(device);
    enc.setFramebuffer(fbo[imgId],pass);
    surface.draw(device,swapchain,enc);
    }

    device.submit(cmd,context.imageAvailable,context.renderDone,context.gpuLock);
    device.present(swapchain,imgId,context.renderDone);
    }
  catch(const Tempest::SwapchainSuboptimal&) {
    swapchain.reset();
    initSwapchain();
    }
  }
