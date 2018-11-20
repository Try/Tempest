#include "mainwindow.h"

#include <Tempest/Assets>
#include <Tempest/Pixmap>
#include <Tempest/Except>
#include <Tempest/Painter>
#include <Tempest/Rect>

#include "ui/projecttree.h"

using namespace Tempest;

MainWindow::MainWindow(Tempest::VulkanApi& api)
  :device(api,hwnd()),resources(device) {
  for(uint8_t i=0;i<device.maxFramesInFlight();++i){
    fLocal.emplace_back(device);
    commandBuffersSemaphores.emplace_back(device);
    }

  initSwapchain();
  setupUi();
  }

void MainWindow::setupUi() {
  setLayout(Horizontal);

  addWidget(new ProjectTree());
  addWidget(new Widget());//->resize(800,600);
  }

void MainWindow::paintEvent(PaintEvent& event) {
  Painter p(event);

  p.setBrush(Brush(background));
  p.drawRect(0,0,w(),h());

  p.setPen(Pen(Color(),1));
  p.drawLine(100,100,800,200);
  }

void MainWindow::resizeEvent(uint32_t, uint32_t) {
  device.reset();
  initSwapchain();
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

    dispatchPaintEvent(surface);

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
