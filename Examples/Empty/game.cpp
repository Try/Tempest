#include "game.h"

#include <Tempest/Pixmap>
#include <Tempest/Except>
#include <Tempest/Painter>
#include <Tempest/Rect>

using namespace Tempest;

Game::Game(Device& device)
  :device(device), swapchain(device,hwnd()), texAtlass(device) {
  //VectorImage image;
  //image.load("img/test.svg");

  Tempest::Pixmap pm("img/texture.png");
  //Tempest::Pixmap pm("img/texture.hdr");
  texture = device.loadTexture(pm);

  for(uint8_t i=0;i<MaxFramesInFlight;++i)
    fence.emplace_back(device.fence());

  pass = device.pass(Tempest::FboMode(Tempest::FboMode::PreserveOut,Color(0.f,0.f,1.f,1.f)));
  initSwapchain();
  }

Game::~Game() {
  device.waitIdle();
  }

void Game::initSwapchain(){
  const size_t imgC=swapchain.imageCount();

  fbo.clear();
  for(size_t i=0;i<imgC;++i) {
    Tempest::Attachment& frame=swapchain.image(i);
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
  for(auto& i:fence)
    i.wait();
  swapchain.reset();
  initSwapchain();
  }

void Game::render(){
  try {
    auto&       sync = fence   [cmdId];
    auto&       cmd  = commands[cmdId];

    sync.wait();

    dispatchPaintEvent(surface,texAtlass);

    {
    auto enc = cmd.startEncoding(device);
    enc.setFramebuffer(fbo[swapchain.currentImage()],pass);
    surfaceMesh[cmdId].update(device,surface);
    surfaceMesh[cmdId].draw(enc);
    }

    device.submit(cmd,sync);
    device.present(swapchain);
    }
  catch(const Tempest::SwapchainSuboptimal&) {
    swapchain.reset();
    initSwapchain();
    }
  }
