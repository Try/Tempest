#pragma once

#include <Tempest/MetalApi>

#include <Metal/Metal.hpp>

#include "nsptr.h"

#include <string_view>
#include <vector>

namespace Tempest {
namespace Detail {

class MtPrecompiledLibraries final {
  public:
    MtPrecompiledLibraries(MTL::Device& device, const MetalApi::Options& options);

    NsPtr<MTL::Function> find(std::string_view canonicalMsl,
                              const MetalApi::PrecompiledShaderProfile& runtimeProfile) const;

  private:
    struct Library {
      NsPtr<MTL::Library> impl;
      };

    struct Entry {
      MetalApi::PrecompiledShaderProfile profile;
      MetalApi::PrecompiledShaderKey     key = {};
      size_t                             library = 0;
      bool                               duplicate = false;
      };

    static MetalApi::PrecompiledPlatform currentPlatform();
    static bool valid(const MetalApi::PrecompiledShader& shader);
    static bool sameGenerationProfile(const MetalApi::PrecompiledShaderProfile& a,
                                      const MetalApi::PrecompiledShaderProfile& b);
    static bool sameStage(MTL::FunctionType actual, MetalApi::PrecompiledShaderStage expected);

    std::vector<Library> libraries;
    std::vector<Entry>   entries;
  };

}
}
