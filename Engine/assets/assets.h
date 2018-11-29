#pragma once

#include <Tempest/Platform>
#include <Tempest/Asset>
#include <Tempest/TextureAtlas>

#include <memory>
#include <unordered_set>

namespace Tempest {

class Device;

class Assets final {
#ifdef __WINDOWS__
  using str_path=std::u16string;
#else
  using str_path=std::string;
#endif
  public:
    Assets(const char *path,Tempest::Device& dev);
    ~Assets();

    Asset operator[](const char* file) const;

    struct Provider {
      virtual ~Provider(){}
      virtual Asset open(const char* file)=0;
      };

  private:
    struct AssetHash {
      size_t operator()(const Asset& a) const{
        return a.hash;
        }
      size_t operator()(const str_path& a) const{
        std::hash<str_path> v;
        return v(a);
        }
      };

    struct Directory:Provider {
      Directory(const char* path,Tempest::Device &dev);
      ~Directory() override=default;

      Asset open(const char* file) override;
      Asset implOpen(str_path &&path);

      str_path                           path;
      Tempest::Device&                   device;
      Tempest::TextureAtlas              atlas;
      std::vector<Asset>                 files;
      //std::unordered_map<str_path,Asset> files;

      };

    struct TextureFile;

    std::unique_ptr<Provider> impl;
  };

}
