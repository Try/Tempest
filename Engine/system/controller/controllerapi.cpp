#include "controllerapi.h"

#include <Tempest/Platform>
#include <Tempest/Application>

#ifdef __OSX__
#include "macoscontrollerapi.h"
#endif

#include <utility>

using namespace Tempest;
using namespace Tempest::Detail;

std::vector<GameController> Application::controllers() {
  return ControllerApi::controllers();
  }

std::vector<GameController> ControllerApi::controllers() {
#ifdef __OSX__
  MacosControllerApi::requireMainThread();
  static MacosControllerApi api;
  std::vector<GameController> result;
  for(auto& state : api.controllers())
    result.push_back(GameController(std::move(state)));
  return result;
#else
  return {};
#endif
  }
