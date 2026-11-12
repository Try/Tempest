#pragma once

#include "macoscontroller.h"

#include <vector>

namespace Tempest::Detail {

class MacosControllerApi final {
  public:
    MacosControllerApi();
    ~MacosControllerApi();
    MacosControllerApi(const MacosControllerApi&) = delete;
    MacosControllerApi& operator=(const MacosControllerApi&) = delete;

    static void requireMainThread();
    std::vector<std::shared_ptr<MacosController>> controllers() const;

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
  };

}
