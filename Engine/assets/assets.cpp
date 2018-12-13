#include "assets.h"

#ifdef __WINDOWS__
#include <windows.h>
#endif

#include <Tempest/Device>
#include <Tempest/Pixmap>
#include <Tempest/Texture2d>
#include <Tempest/Sprite>
#include <Tempest/Font>

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

struct Assets::FontFile : Asset::Impl {
  FontFile(Font&& f,Assets::str_path&& path,Directory& owner)
    :owner(owner),fpath(path),fnt(std::move(f)) {}

  const void* get(const std::type_info& t) override {
    if(t==typeid(Font))
      return &fnt;
    return nullptr;
    }

  const str_path& path() const override {
    return fpath;
    }

  Directory&             owner;
  const Assets::str_path fpath;
  Font                   fnt;
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
  {
    auto a=implOpenTry<Pixmap,TextureFile>(fpath);
    if(a.second)
      return a.first;
  }
  {
    auto a=implOpenTry<Font,FontFile>(fpath);
    if(a.second)
      return a.first;
  }
  return Asset();
  }

template<class ClsAsset,class File>
std::pair<Asset,bool> Assets::Directory::implOpenTry(Assets::str_path& fpath) { // std::optional?
  try {
    ClsAsset p(fpath);
    try {
      auto ptr=std::make_shared<File>(std::move(p),std::move(fpath),*this);
      return std::make_pair(Asset(std::move(ptr)),true);
      }
    catch(...) {
      // fail, but asset file is valid
      return std::make_pair(Asset(),false);
      }
    }
  catch(...){
    // unable to load -> next codec
    }
  return std::make_pair(Asset(),false);
  }
