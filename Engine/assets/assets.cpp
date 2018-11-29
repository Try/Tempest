#include "assets.h"

#ifdef __WINDOWS__
#include <windows.h>
#endif

#include <Tempest/Device>
#include <Tempest/Pixmap>
#include <Tempest/Texture2d>
#include <Tempest/Sprite>

using namespace Tempest;

struct Assets::TextureFile : Asset::Impl {
  TextureFile(Pixmap&& p,Assets::str_path&& path,Directory& owner)
    :owner(owner),fpath(path),value(std::move(p)) {}

  const void* get(const std::type_info& t) override {
    if(t==typeid(Pixmap))
      return &getValue();

    if(t==typeid(Texture2d)) {
      if(tex.isEmpty())
        tex = owner.device.loadTexture(getValue(),true);
      value=Pixmap();
      return &tex;
      }

    if(t==typeid(Sprite)) {
      if(spr.isEmpty())
        spr = owner.atlas.load(getValue());
      value=Pixmap();
      return &spr;
      }
    return nullptr;
    }

  const str_path& path() const override {
    return fpath;
    }

  const Pixmap& getValue(){
    if(!value.isEmpty())
      return value;
    try {
      value=Pixmap(fpath);
      return value;
      }
    catch(...) {
      // not enought memory for asset
      // TODO: fallback assets
      return value;
      }
    }

  Directory&             owner;
  const Assets::str_path fpath;
  Pixmap                 value;
  Texture2d              tex;
  Sprite                 spr;
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
  :path(name),device(dev),atlas(dev) {
  if(path.size()!=0 && path.back()!='/')
    path.push_back('/');
#else
  :device(dev),atlas(dev){
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
  /*
  auto i=files.find(fpath);
  if(i!=files.end()){
    return i->second;
    }*/

  AssetHash ah;
  auto hash=ah(fpath);

  for(auto& i:files){
    if(i.hash==hash && i.impl && i.impl->path()==fpath)
      return i;
    }

  Asset a=implOpen(std::move(fpath));
  files.emplace_back(a);
  return a;
  }

Asset Assets::Directory::implOpen(Assets::str_path&& fpath) {
  try {
    Tempest::Pixmap p(fpath);
    return Asset(new TextureFile(std::move(p),std::move(fpath),*this));
    }
  catch(...){
    // not image
    }
  return Asset();
  }
