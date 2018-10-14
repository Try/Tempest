#include <Tempest/SystemApi>
#include <Tempest/VulkanApi>

#include <Tempest/Device>
#include <Tempest/Painter>
#include <Tempest/Window>
#include <Tempest/Fence>
#include <Tempest/Semaphore>
#include <Tempest/VertexBuffer>

#include <vector>

#include "game.h"

int main() {
  Tempest::VulkanApi api;
  Game               wx(api);

  Tempest::SystemApi::exec();

  return 0;
  }
