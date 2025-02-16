#include <Tempest/Application>

#include <Tempest/VulkanApi>
#include <Tempest/DirectX12Api>
#include <Tempest/MetalApi>
#include <Tempest/Device>
#include <Tempest/Log>

#include "game.h"

std::unique_ptr<Tempest::AbstractGraphicsApi> mkApi(const char* av, Tempest::ApiFlags flags) {
#if defined(__WINDOWS__) && defined(_MSC_VER)
  if(std::strcmp(av,"-dx12")==0)
    return std::unique_ptr<Tempest::AbstractGraphicsApi>(new Tempest::DirectX12Api{flags});
#endif
#if defined(__OSX__)
  (void)av;
  return std::unique_ptr<Tempest::AbstractGraphicsApi>(new Tempest::MetalApi{flags});
#else
  (void)av;
  return std::unique_ptr<Tempest::AbstractGraphicsApi>(new Tempest::VulkanApi{flags});
#endif
  }

int main(int argc, const char** argv) {
  Tempest::Application app;

  const char* msDev = nullptr;
  auto api = mkApi(argc>1 ? argv[1] : "", Tempest::ApiFlags::Validation);
  auto dev = api->devices();
  for(auto& i:dev)
    if(i.meshlets.meshShader) {
      msDev = i.name;
      }
  if(msDev==nullptr)
    return 0;

  Tempest::Log::i("GL_EXT_mesh_shader");

  app.setFont(Tempest::Application::defaultFont());

  Tempest::Device device{*api,msDev};
  Game            wx(device);

  return app.exec();
  }
