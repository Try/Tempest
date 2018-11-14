#include "game.h"

#include <Tempest/Assets>
#include <Tempest/Pixmap>
#include <Tempest/Except>
#include <Tempest/Painter>
#include <Tempest/Rect>

namespace Tempest {
template<>
std::initializer_list<Decl::ComponentType> vertexBufferDecl<Game::Point>() {
  return {Decl::float3,Decl::float2};
  }
}

using namespace Tempest;

Game::FrameLocal::FrameLocal(Device &device)
  :imageAvailable(device),gpuLock(device) {
  }

Game::Game(Tempest::VulkanApi& api)
  :device(api,hwnd()) {
  Tempest::Assets data("shader");
  auto vs=data["vert.spv"];
  (void)vs;

  VectorImage image;
  image.load("img/test.svg");

  Tempest::Pixmap pm("img/texture.png");
  texture = device.loadTexture(pm);

  std::initializer_list<Point> source = {
    {-0.5f, +0.5f, 0, 0,0},
    {+0.5f, +0.5f, 0, 1,0},
    {+0.0f, -0.5f, 0, 0,1}
    };
  vbo = device.loadVbo(source.begin(),source.size(),Tempest::BufferFlags::Static);

  for(uint8_t i=0;i<device.maxFramesInFlight();++i){
    fLocal.emplace_back(device);
    commandBuffersSemaphores.emplace_back(device);
    }

  ulay.add(0,Tempest::UniformsLayout::Ubo,    Tempest::UniformsLayout::Fragment);
  ulay.add(1,Tempest::UniformsLayout::Texture,Tempest::UniformsLayout::Fragment);

  initSwapchain(ulay);
  }

void Game::initSwapchain(const Tempest::UniformsLayout &ulay){
  Ubo usrc={};

  auto vs  = device.loadShader("shader/vert.spv");
  auto fs  = device.loadShader("shader/frag.spv");

  pass     = device.pass(Tempest::FboMode::PreserveOut,Tempest::FboMode::Discard);
  pipeline = device.pipeline<Point>(pass,width(),height(),Triangles,ulay,vs,fs);
  uboBuf   = device.loadUbo(&usrc,sizeof(usrc));

  const size_t imgC=device.swapchainImageCount();
  if(ubo.size()==0) {
    for(size_t i=0;i<imgC;++i) {
      Tempest::Uniforms u=device.uniforms(ulay);
      u.set(0,uboBuf,0,sizeof(usrc));
      u.set(1,texture);
      ubo.emplace_back(std::move(u));
      }
    }

  commandStatic.clear();
  commandDynamic.clear();
  fbo.clear();

  for(size_t i=0;i<imgC;++i) {
    Tempest::Frame frame=device.frame(i);
    fbo.emplace_back(device.frameBuffer(frame,pass));
    commandStatic.emplace_back(device.commandBuffer());
    commandDynamic.emplace_back(device.commandBuffer());

    Tempest::CommandBuffer& buf=commandStatic[i];
    buf.begin();
    //buf.clear(frame,0,1,1,0);

    buf.beginRenderPass(fbo[i],pass,pipeline.w(),pipeline.h());
    buf.setUniforms(pipeline,ubo[i]);
    buf.draw(vbo,0,vbo.size());
    buf.endRenderPass();

    buf.end();
    }
  }

void Game::paintEvent(PaintEvent& event) {
  Painter p(event);

  p.setBrush(Brush(texture));
  p.drawRect(100,50, 200,100);

  p.setBrush(Brush(texture,Color(1,0,0,1)));
  p.drawRect(300,150,texture.w(),texture.h());

  p.setPen(Pen(Color(),4));
  p.drawLine(100,100,200,200);
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
    auto& context=fLocal[device.frameId()];
    Semaphore&     renderDone=commandBuffersSemaphores[device.frameId()];
    CommandBuffer& cmd       =commandDynamic[device.frameId()];

    context.gpuLock.wait();

    const  uint32_t imgId=device.nextImage(context.imageAvailable);

    PaintEvent p(surface,w(),h());
    paintEvent(p);

    cmd.begin();
    cmd.beginRenderPass(fbo[imgId],pass,width(),height());
    surface.draw(device,cmd,pass);
    cmd.endRenderPass();
    cmd.end();

    device.draw(cmd,context.imageAvailable,renderDone,context.gpuLock);
    //device.draw(commandStatic[imgId],context.imageAvailable,renderDone,context.gpuLock);

    device.present(imgId,renderDone);
    }
  catch(const Tempest::DeviceLostException&) {
    device.reset();
    initSwapchain(ulay);
    }
  }
