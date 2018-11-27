#include "wfile.h"

#include <windows.h>
#include <stdexcept>

using namespace Tempest;

WFile::WFile(const char *name) {
  std::wstring path;
  const int len=MultiByteToWideChar(CP_UTF8,0,name,-1,nullptr,0);
  if(len>1){
    path.resize(size_t(len-1));
    MultiByteToWideChar(CP_UTF8,0,name,-1,&path[0],int(path.size()));
    }
  handle=implOpen(path.c_str());
  }

WFile::WFile(const std::string &path)
  :WFile(path.c_str()){
  }

WFile::WFile(const char16_t *path) {
  handle=implOpen(reinterpret_cast<const wchar_t*>(path));
  }

WFile::WFile(const std::u16string &path)
  :WFile(path.c_str()){
  }

void* WFile::implOpen(const wchar_t *wstr) {
  void* ret = CreateFileW(wstr,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(ret==HANDLE(LONG_PTR(-1)))
    throw std::runtime_error("failed to open file!");
  return ret;
  }

WFile::~WFile() {
  if(handle!=nullptr)
    CloseHandle(HANDLE(handle));
  }

size_t WFile::write(const void *val, size_t size) {
  HANDLE fn           = HANDLE(handle);
  DWORD dwBytesWriten = size;
  return WriteFile( fn, val, size, &dwBytesWriten, nullptr) ? dwBytesWriten : 0;
  }

bool WFile::flush() {
  HANDLE fn = HANDLE(handle);
  return FlushFileBuffers(fn)==TRUE;
  }
