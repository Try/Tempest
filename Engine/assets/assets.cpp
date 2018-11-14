#include "assets.h"

#ifdef __WINDOWS__
#include <windows.h>
#endif

#include <Tempest/Device>
#include <Tempest/Pixmap>
#include <Tempest/Texture2d>

using namespace Tempest;

struct Assets::TextureFile : Asset::Impl {
  TextureFile(Pixmap&& p,Tempest::Device& device):device(device),value(std::move(p)) {}
  void* get(const std::type_info& t) override {
    if(t==typeid(Pixmap))
      return &value;
    if(t==typeid(Texture2d)) {
      if(tex.isEmpty())
        tex = device.loadTexture(value,true);
      return &tex;
      }
    return nullptr;
    }

  Tempest::Device& device;
  Pixmap           value;
  Texture2d        tex;
  };

Assets::Assets(const char* path, Tempest::Device &dev) {
  impl.reset(new Directory(path,dev));
  }

Assets::~Assets() {}

Asset Assets::operator[](const char* file) const {
  return impl->open(file);
  }

Assets::Directory::Directory(const char *name, Device &dev)
#ifndef __WINDOWS__
  :path(name),device(dev) {
  if(path.size()!=0 && path.back()!='/')
    path.push_back('/');
#else
  :device(dev){
  int len=MultiByteToWideChar(CP_UTF8,0,name,-1,nullptr,0);
  if(len>1){
    if(name[len-2]!='/')
      len++;
    path.resize(size_t(len-1));
    MultiByteToWideChar(CP_UTF8,0,name,-1,reinterpret_cast<wchar_t*>(&path[0]),int(path.size()));
    if(name[len-2]!='/')
      path.back()='/';
    }
#endif
  }

Asset Assets::Directory::open(const char *file) {
#ifndef __WINDOWS__
  auto fpath=path+file;
#else
  int len=MultiByteToWideChar(CP_UTF8,0,file,-1,nullptr,0);
  if(len<=1)
    return Asset();
  str_path fpath(path.size()+size_t(len-1),'\0');
  memcpy(&fpath[0],path.c_str(),path.size()*sizeof(wchar_t));
  MultiByteToWideChar(CP_UTF8,0,file,-1,reinterpret_cast<wchar_t*>(&fpath[path.size()]),len-1);
#endif
  auto i=files.find(fpath);
  if(i!=files.end()){
    return i->second;
    }

  Asset a=implOpen(fpath);
  files.emplace(std::move(fpath),a);
  return a;
  }

Asset Assets::Directory::implOpen(const Assets::str_path &fpath) {
  try {
    Tempest::Pixmap p(fpath);
    return Asset( new TextureFile(std::move(p),device) );
    }
  catch(...){
    // not image
    }
  return Asset();
  }
