#include <Tempest/SystemApi>
#include <Tempest/VulkanApi>

#include <Tempest/Device>
#include <Tempest/Painter>
#include <Tempest/Window>
#include <Tempest/Fence>
#include <Tempest/Semaphore>
#include <Tempest/VertexBuffer>

#include <vector>

#include "mainwindow.h"

int main() {
  Tempest::VulkanApi api;
  MainWindow         wx(api);

  Tempest::SystemApi::exec();

  return 0;
  }
