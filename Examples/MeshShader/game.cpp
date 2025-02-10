#include "game.h"

#include <Tempest/Application>
#include <Tempest/ComboBox>
#include <Tempest/Button>
#include <Tempest/Painter>
#include <Tempest/Panel>

#include "mesh.h"
#include "shader.h"

using namespace Tempest;

Game::Game(Device& device)
  : Window(Maximized), device(device), swapchain(device,hwnd()), texAtlass(device) {
  for(uint8_t i=0;i<MaxFramesInFlight;++i)
    fence.emplace_back(device.fence());
  resetSwapchain();
  setupUi();

  onAsset(0);
  onShaderType(useVertex ? 1 : 0);
  }

Game::~Game() {
  device.waitIdle();
  }

void Game::setupUi() {
  auto& p = addWidget(new Panel());
  p.setDragable(true);

  auto& cbShader = p.addWidget(new ComboBox());
  cbShader.setItems({"Mesh shader", "Vertex shader"});
  cbShader.setCurrentIndex(useVertex ? 1 : 0);
  cbShader.onItemSelected.bind(this, &Game::onShaderType);

  auto& cbObj = p.addWidget(new ComboBox());
  cbObj.setItems({"Dragon", "Box", "Cylinder"});
  cbObj.onItemSelected.bind(this, &Game::onAsset);

  p.addWidget(new Widget());
  p.setLayout(Vertical);
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

    cmdId = (cmdId+1)%MaxFramesInFlight;

    auto&       sync = fence   [cmdId];
    auto&       cmd  = commands[cmdId];
    sync.wait();

    dispatchPaintEvent(surface,texAtlass);
    surfaceMesh[cmdId].update(device,surface);

    Matrix4x4 ret = projMatrix();
    ret.mul(viewMatrix());
    ret.mul(objMatrix());

    struct Push {
      Matrix4x4 mvp;
      uint32_t  meshletCount = 0;
      } push;
    push.mvp          = ret;
    push.meshletCount = mesh.meshletCount;

    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer({{swapchain[swapchain.currentImage()],Vec4(0),Tempest::Preserve}}, {zbuffer, 1.f, Tempest::Preserve});

      /*
      enc.setViewport(0,0,256,256);
      if(useVertex) {
        enc.draw(mesh.vbo, mesh.ibo);
        } else {
        enc.dispatchMeshThreads(mesh.meshletCount);
        }
      enc.setViewport(256,0,256,256);
      */
      if(useVertex) {
        enc.setUniforms(pso,&push,sizeof(push));
        enc.draw(mesh.vbo, mesh.ibo);
        } else {
        enc.setBinding(0, mesh.vbo);
        enc.setBinding(1, mesh.ibo8);
        enc.setUniforms(pso,&push,sizeof(push));
        enc.dispatchMeshThreads(mesh.meshletCount);
        }

      enc.setViewport(0,0,w(),h());
      surfaceMesh[cmdId].draw(enc);
    }

    device.submit(cmd,sync);
    device.present(swapchain);

    auto t = Application::tickCount();
    frameTime[fpsId++] = t-time;
    fpsId = (fpsId+1)%std::extent<decltype(frameTime)>();

    float tx = 0;
    for(auto i:frameTime)
      tx += i;
    setWindowTitle(("FPS = " + std::to_string((1000.f*std::extent<decltype(frameTime)>())/std::max<float>(1, tx))).c_str());
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

void Game::onShaderType(size_t type) {
  device.waitIdle();
  useVertex = bool(type!=0);

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
    auto sh   = AppShader::get("shader.task.sprv");
    auto task = device.shader(sh.data, sh.len);
    sh        = AppShader::get("shader.mesh.sprv");
    auto mesh = device.shader(sh.data, sh.len);
    sh        = AppShader::get("shader.frag.sprv");
    auto frag = device.shader(sh.data, sh.len);
    pso = device.pipeline(Topology::Triangles, rs, task, mesh, frag);
    }
  }

void Game::onAsset(size_t type) {
  device.waitIdle();
  switch (type) {
    case 0:
      mesh = Mesh(device, "assets/dragon.obj");
      break;
    case 1:
      mesh = Mesh(device, "assets/box.obj");
      break;
    case 2:
      mesh = Mesh(device, "assets/cylinder.obj");
      break;
    }
  }

