#pragma once

#include <Tempest/Assets>
#include <Tempest/Texture2d>

class Resources {
  public:
    explicit Resources(Tempest::Device& device);
    ~Resources();

    template<class T>
    static const T& get(const char* path) {
      const auto& a=inst->asset[path];
      return a.get<T>();
      }

  private:
    static Resources* inst;
    Tempest::Assets asset;
  };
