#include "game.h"

#include "mesh.h"
#include "shader.h"

#include <Tempest/Application>

using namespace Tempest;

Game::Game(Device& device)
  : Window(Maximized), device(device), swapchain(device,hwnd()), texAtlass(device) {
  for(uint8_t i=0;i<MaxFramesInFlight;++i)
    fence.emplace_back(device.fence());
  resetSwapchain();

  mesh = Mesh(device, "assets/dragon.obj");
  //mesh = Mesh(device, "assets/box.obj");
  //mesh = Mesh(device, "assets/cylinder.obj");

  RenderState rs;
  rs.setCullFaceMode(RenderState::CullMode::NoCull);
  rs.setZTestMode(RenderState::ZTestMode::Less);

  if(useVertex) {
    auto sh   = AppShader::get("shader.vert.sprv");
    auto vert = device.shader(sh.data, sh.len);
    sh        = AppShader::get("shader.frag.sprv");
    auto frag = device.shader(sh.data, sh.len);

    pso = device.pipeline(Topology::Triangles, rs, vert, frag);
    } else {
    auto sh   = AppShader::get("shader.mesh.sprv");
    auto mesh = device.shader(sh.data, sh.len);
    sh        = AppShader::get("shader.frag.sprv");
    auto frag = device.shader(sh.data, sh.len);
    pso = device.pipeline(Topology::Triangles, rs, mesh, frag);
    }

  desc = device.descriptors(pso);
  if(!useVertex) {
    desc.set(0, mesh.vbo);
    desc.set(1, mesh.ibo8);
    }
  }

Game::~Game() {
  device.waitIdle();
  }

void Game::resizeEvent(SizeEvent&) {
  for(auto& i:fence)
    i.wait();
  resetSwapchain();
  update();
  }

void Game::mouseWheelEvent(Tempest::MouseEvent& event) {
  if(event.delta>0)
    zoom *= 1.1f; else
    zoom /= 1.1f;
  }

void Game::mouseDownEvent(Tempest::MouseEvent& event) {
  mpos = event.pos();
  }

void Game::mouseDragEvent(Tempest::MouseEvent& event) {
  auto dp = event.pos() - mpos;
  rotation += Vec2(dp.x,dp.y);
  mpos = event.pos();
  update();
  }

Matrix4x4 Game::projMatrix() const {
  Matrix4x4 proj;
  proj.perspective(45.0f, float(w())/float(h()), 0.1f, 100.0f);
  return proj;
  }

Matrix4x4 Game::viewMatrix() const {
  Matrix4x4 view = Matrix4x4::mkIdentity();
  view.translate(0,0,4);
  view.rotate(rotation.y, 1, 0, 0);
  view.rotate(rotation.x, 0, 0, 1);
  view.scale(zoom);
  //view.rotateOX(90); // Y-up
  //view.translate(-(mesh.bbox[0] + mesh.bbox[1])*0.5);
  return view;
  }

Matrix4x4 Game::objMatrix() const {
  const float size = (mesh.bbox[1] - mesh.bbox[0]).length();

  Matrix4x4 obj = Matrix4x4::mkIdentity();
  obj.scale(8.f/size);
  obj.rotateOX(90); // Y-up
  obj.translate(-(mesh.bbox[0] + mesh.bbox[1])*0.5);
  return obj;
  }

void Game::render(){
  try {
    static uint64_t time = Application::tickCount();

    auto&       sync = fence   [cmdId];
    auto&       cmd  = commands[cmdId];
    sync.wait();

    Matrix4x4 ret = projMatrix();
    ret.mul(viewMatrix());
    ret.mul(objMatrix());

    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer({{swapchain[swapchain.currentImage()],Vec4(0),Tempest::Preserve}}, {zbuffer, 1.f, Tempest::Preserve});
      enc.setUniforms(pso,desc,&ret,sizeof(ret));

      if(useVertex) {
        enc.draw(mesh.vbo, mesh.ibo);
        } else {
        enc.dispatchMesh(mesh.meshletCount);
        }
    }

    device.submit(cmd,sync);
    device.present(swapchain);

    auto t = Application::tickCount();
    setWindowTitle(("FPS = " + std::to_string(1000.f/std::max<float>(1, t-time))).c_str());
    time = t;
    }
  catch(const Tempest::SwapchainSuboptimal&) {
    resetSwapchain();
    update();
    }
  }

void Game::resetSwapchain() {
  swapchain.reset();
  zbuffer = device.zbuffer(TextureFormat::Depth32F, w(), h());
  }

