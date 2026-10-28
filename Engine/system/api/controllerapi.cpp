#include "controllerapi.h"

#include <Tempest/Platform>

#ifdef __OSX__
#include "macoscontrollerapi.h"
#endif

#include <utility>

using namespace Tempest;
using namespace Tempest::Detail;

std::vector<GameController> ControllerApi::controllers() {
#ifdef __OSX__
  MacOSControllerApi::requireMainThread();
  static MacOSControllerApi api;
  std::vector<GameController> result;
  for(auto& state : api.controllers())
    result.push_back(controller(std::move(state)));
  return result;
#else
  return {};
#endif
  }

GameController ControllerApi::controller(std::shared_ptr<ControllerState> state) {
  return GameController(std::move(state));
  }
