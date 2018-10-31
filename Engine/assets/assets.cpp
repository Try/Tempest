#include "assets.h"

#ifdef __WINDOWS__
#include <windows.h>
#endif

using namespace Tempest;

Assets::Assets(const char* path) {
  impl.reset(new Directory(path));
  }

Assets::~Assets() {}

Asset Assets::operator[](const char* /*file*/) const {
  return Asset();
  }

Assets::Directory::Directory(const char *name)
#ifndef __WINDOWS__
  :path(name) {
#else
  {
  const int len=MultiByteToWideChar(CP_UTF8,0,name,-1,nullptr,0);
  if(len>1){
    path.resize(size_t(len-1));
    MultiByteToWideChar(CP_UTF8,0,name,-1,&path[0],int(path.size()));
    }
#endif
  }
