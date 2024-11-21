#include "wfile.h"

#include <Tempest/Except>
#include <Tempest/TextCodec>
#include "utility/smallarray.h"

#ifdef __WINDOWS__
#include <windows.h>
#else
#include <cstdio>
#endif

#include <cstring>
#include <system_error>

using namespace Tempest;

WFile::WFile(const char *name) {
#ifdef __WINDOWS__
  std::wstring path;
  const int len = MultiByteToWideChar(CP_UTF8,0,name,-1,nullptr,0);
  if(len>1){
    path.resize(size_t(len-1));
    MultiByteToWideChar(CP_UTF8,0,name,-1,&path[0],int(path.size()));
    }
  handle = implOpen(path.c_str());
#else
  handle = implOpen(name);
#endif
  }

WFile::WFile(std::string_view name) {
#ifdef __WINDOWS__
  std::wstring path;
  const int len = MultiByteToWideChar(CP_UTF8,0,name.data(),int(name.size())+1,nullptr,0);
  if(len>1){
    path.resize(size_t(len-1));
    MultiByteToWideChar(CP_UTF8,0,name.data(),int(name.size())+1,&path[0],int(path.size()));
    }
  handle = implOpen(path.c_str());
#else
  Detail::SmallArray<char,256> path(name.size()+1);
  std::memcpy(path.get(), name.data(), name.size()*sizeof(char));
  path[name.size()] = '\0';
  handle = implOpen(path.get());
#endif
  }

WFile::WFile(const char16_t *path) {
#ifdef __WINDOWS__
  handle = implOpen(reinterpret_cast<const wchar_t*>(path));
#else
  handle = implOpen(TextCodec::toUtf8(path).c_str());
#endif
  }

WFile::WFile(std::u16string_view name) {
#ifdef __WINDOWS__
  Detail::SmallArray<wchar_t,256> path(name.size()+1);
  std::memcpy(path.get(), name.data(), name.size()*sizeof(wchar_t));
  path[name.size()] = '\0';
  handle = implOpen(path.get());
#else
  handle = implOpen(TextCodec::toUtf8(name).c_str());
#endif
  }

WFile::WFile(WFile &&other)
  :handle(other.handle){
  other.handle = nullptr;
  }

#ifdef __WINDOWS__
void* WFile::implOpen(const wchar_t *wstr) {
  void* ret = CreateFileW(wstr,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(ret==HANDLE(LONG_PTR(-1)))
    throw std::system_error(Tempest::SystemErrc::UnableToOpenFile);
  return ret;
  }
#else
void* WFile::implOpen(const char *cstr) {
  void* ret = fopen(cstr,"wb");
  if(ret==nullptr)
    throw std::system_error(Tempest::SystemErrc::UnableToOpenFile);
  return ret;
  }
#endif

WFile::~WFile() {
#ifdef __WINDOWS__
  if(handle!=nullptr)
    CloseHandle(HANDLE(handle));
#else
  if(handle!=nullptr)
    fclose(reinterpret_cast<FILE*>(handle));
#endif
  }

WFile &WFile::operator =(WFile &&other) {
  std::swap(handle,other.handle);
  return *this;
  }

size_t WFile::write(const void *val, size_t size) {
#ifdef __WINDOWS__
  HANDLE fn           = HANDLE(handle);
  DWORD dwBytesWriten = DWORD(size);
  return WriteFile( fn, val, DWORD(size), &dwBytesWriten, nullptr) ? dwBytesWriten : 0;
#else
  return fwrite(val,1,size,reinterpret_cast<FILE*>(handle));
#endif
  }

bool WFile::flush() {
#ifdef __WINDOWS__
  HANDLE fn = HANDLE(handle);
  return FlushFileBuffers(fn)==TRUE;
#else
  return fflush(reinterpret_cast<FILE*>(handle))==0;
#endif
  }
