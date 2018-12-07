#include "mainwindow.h"

#include <Tempest/Except>
#include <Tempest/Painter>

#include <Tempest/Brush>
#include <Tempest/Pen>

#include "ui/projecttree.h"

using namespace Tempest;

MainWindow::MainWindow(Tempest::VulkanApi& api)
  :device(api,hwnd()),resources(device),atlas(device) {
  for(uint8_t i=0;i<device.maxFramesInFlight();++i){
    fLocal.emplace_back(device);
    commandBuffersSemaphores.emplace_back(device);
    }

  initSwapchain();
  setupUi();
  }

void MainWindow::setupUi() {
  font = Font("data/font/Roboto.ttf");
  auto lt = font.letter(u'a',atlas);

  spr = atlas.load("img/tst.png");

  setLayout(Horizontal);

  addWidget(new ProjectTree());
  addWidget(new Widget());
  }

void MainWindow::paintEvent(PaintEvent& event) {
  Painter p(event);

  p.setBrush(Brush(background));
  p.drawRect(0,0,w(),h());

  p.setBrush(Brush(spr));
  p.drawRect(300,100,spr.w(),spr.h());

  p.setPen(Pen(Color(0,0,0,1),1));
  p.drawLine(100,100,mpos.x,mpos.y);

  p.setFont(font);
  p.drawText(300,200,u"Hello world!");
  }

void MainWindow::resizeEvent(SizeEvent&) {
  device.reset();
  initSwapchain();
  }

void MainWindow::mouseMoveEvent(MouseEvent &event) {
  mpos=event.pos();
  }

void MainWindow::initSwapchain(){
  const size_t imgC=device.swapchainImageCount();
  commandDynamic.clear();
  fbo.clear();

  mainPass=device.pass(FboMode::PreserveOut,FboMode::Discard);

  for(size_t i=0;i<imgC;++i) {
    Tempest::Frame frame=device.frame(i);
    fbo.emplace_back(device.frameBuffer(frame,mainPass));
    commandDynamic.emplace_back(device.commandBuffer());
    }
  }

void MainWindow::render(){
  try {
    auto& context=fLocal[device.frameId()];
    Semaphore&     renderDone=commandBuffersSemaphores[device.frameId()];
    CommandBuffer& cmd       =commandDynamic[device.frameId()];

    context.gpuLock.wait();

    const  uint32_t imgId=device.nextImage(context.imageAvailable);

    dispatchPaintEvent(surface,atlas);

    cmd.begin();
    cmd.beginRenderPass(fbo[imgId],mainPass,w(),h());
    surface.draw(device,cmd,mainPass);
    cmd.endRenderPass();
    cmd.end();

    device.draw(cmd,context.imageAvailable,renderDone,context.gpuLock);

    device.present(imgId,renderDone);
    }
  catch(const Tempest::DeviceLostException&) {
    device.reset();
    initSwapchain();
    }
  }
