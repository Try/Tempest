#include <Tempest/Application>

#include <Tempest/VulkanApi>
#include <Tempest/DirectX12Api>
#include <Tempest/MetalApi>
#include <Tempest/Device>
#include <Tempest/Log>

#include "game.h"

std::unique_ptr<Tempest::AbstractGraphicsApi> mkApi(const char* av) {
#if defined(__WINDOWS__) && defined(_MSC_VER)
  if(std::strcmp(av,"-dx12")==0)
    return std::unique_ptr<Tempest::AbstractGraphicsApi>(new Tempest::DirectX12Api{Tempest::ApiFlags::Validation});
#endif
#if defined(__OSX__)
  (void)av;
  return std::unique_ptr<Tempest::AbstractGraphicsApi>(new Tempest::MetalApi{Tempest::ApiFlags::Validation});
#else
  (void)av;
  return std::unique_ptr<Tempest::AbstractGraphicsApi>(new Tempest::VulkanApi{Tempest::ApiFlags::Validation});
#endif
  }

int main(int argc, const char** argv) {
  Tempest::Application app;

  const bool emulated = false;

  const char* msDev = nullptr;
  auto api = mkApi(argc>1 ? argv[1] : "");
  auto dev = api->devices();
  for(auto& i:dev)
    if(i.meshlets.meshShader!=emulated && i.meshlets.meshShaderEmulated==emulated) {
      msDev = i.name;
      }
  for(auto& i:dev)
    if(i.meshlets.meshShader!=emulated && i.meshlets.meshShaderEmulated==emulated && i.type==Tempest::DeviceType::Discrete) {
      msDev = i.name;
      }
  if(msDev==nullptr)
    return 0;

  Tempest::Log::i(msDev);
  Tempest::Log::i(emulated ? "mesh-shader-emulated" : "GL_EXT_mesh_shader");
  Tempest::Device device{*api,msDev};
  Game            wx(device);

  return app.exec();
  }
