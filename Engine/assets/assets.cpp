#include "assets.h"

#ifdef __WINDOWS__
#include <windows.h>
#include <shlwapi.h>
#endif

#ifdef __LINUX__
#include <libgen.h>
#endif

#include <Tempest/Device>
#include <Tempest/Pixmap>
#include <Tempest/Texture2d>
#include <Tempest/Sprite>
#include <Tempest/Font>
#include <Tempest/TextCodec>

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

struct Assets::ShaderFile : Asset::Impl {
  ShaderFile(Shader&& p,Assets::str_path&& path,Directory& owner)
    :owner(owner),fpath(path),value(std::move(p)) {
    }

  static Shader tryOpen(Directory& owner,const Assets::str_path& fpath) {
    return owner.device.loadShader(fpath.c_str());
    }

  const void* get(const std::type_info& t) override {
    if(t==typeid(Tempest::Shader))
      return &value;
    return nullptr;
    }

  const str_path& path() const override {
    return fpath;
    }

  Directory&             owner;
  const Assets::str_path fpath;
  Shader                 value;
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
  :path(modulePath()+name),device(dev),atlas(dev) {
#else
  :device(dev),atlas(dev){
  path = modulePath()+TextCodec::toUtf16(name);
#endif
  if(path.size()!=0 && path.back()!='/')
    path.push_back('/');
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

Asset Assets::Directory::implOpen(str_path&& fpath) {
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
  {
    auto a=implOpenTry<ShaderFile>(fpath);
    if(a.second)
      return a.first;
  }
  return Asset();
  }

Assets::str_path Assets::Directory::modulePath() {
#if defined(__WINDOWS__)
  WCHAR path[MAX_PATH]={};
  GetModuleFileNameW(nullptr, path, MAX_PATH);

  PathRemoveFileSpecW(path);
  str_path str = reinterpret_cast<char16_t*>(path);
  if(str.size()>0 &&  str.back()!='\\')
     str.push_back('\\');
  return str;
#elif defined(__LINUX__)
  char path[PATH_MAX]={};
  ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
  const char *path;
  if(count != -1) {
    str_path str = dirname(path);
    if(str.size()>0 &&  str.back()!='/')
       str.push_back('/');
    }
  return "";
#else
#error "TODO"
#endif
  }

template<class ClsAsset,class File>
std::pair<Asset,bool> Assets::Directory::implOpenTry(str_path& fpath) { // std::optional?
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

template<class File>
std::pair<Asset,bool> Assets::Directory::implOpenTry(str_path& fpath) { // std::optional?
  try {
    auto p = File::tryOpen(*this,fpath);
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
