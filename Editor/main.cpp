#include <Tempest/VulkanApi>

#include <Tempest/Window>
#include <Tempest/Application>

#include <vector>

#include "mainwindow.h"

int main() {
  Tempest::VulkanApi api;
  MainWindow         wx(api);

  Tempest::Application app;
  return app.exec();
  }
