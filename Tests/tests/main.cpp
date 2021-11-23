#include <gtest/gtest.h>

#include "utils/renderdoc.h"

int main(int argc, char *argv[]) {
  RenderDoc rdoc;
  ::testing::InitGoogleTest(&argc, argv);

  RenderDoc::start();
  auto ret = RUN_ALL_TESTS();
  RenderDoc::stop();
  return ret;
  }
