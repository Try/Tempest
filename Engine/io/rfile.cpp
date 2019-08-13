#include "rfile.h"

#include <Tempest/Except>

#include <windows.h>
#include <stdexcept>

using namespace Tempest;

RFile::RFile(const char *name) {
  std::wstring path;
  const int len=MultiByteToWideChar(CP_UTF8,0,name,-1,nullptr,0);
  if(len>1){
    path.resize(size_t(len-1));
    MultiByteToWideChar(CP_UTF8,0,name,-1,&path[0],int(path.size()));
    }
  handle=implOpen(path.c_str());
  }

RFile::RFile(const std::string &path)
  :RFile(path.c_str()){
  }

RFile::RFile(const char16_t *path) {
  handle=implOpen(reinterpret_cast<const wchar_t*>(path));
  }

RFile::RFile(const std::u16string &path)
  :RFile(path.c_str()){
  }

void* RFile::implOpen(const wchar_t *wstr) {
  void* ret = CreateFileW(wstr,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(ret==HANDLE(LONG_PTR(-1)))
    throw std::system_error(Tempest::SystemErrc::UnableToOpenFile);
  return ret;
  }

RFile::~RFile() {
  if(handle!=nullptr)
    CloseHandle(HANDLE(handle));
  }

size_t RFile::read(void *dest, size_t size) {
  HANDLE fn          = HANDLE(handle);
  DWORD  dwBytesRead = size;
  DWORD  cnt         = ReadFile(fn, dest, size, &dwBytesRead, nullptr) ? dwBytesRead : 0;

  return cnt;
  }

size_t RFile::size() const {
  HANDLE fn = HANDLE(handle);
  return GetFileSize(fn,nullptr);
  }

uint8_t RFile::peek() {
  uint8_t ret = 0;
  HANDLE  fn  = HANDLE(handle);

  LONG current=LONG(SetFilePointer(fn,0,nullptr,FILE_CURRENT));
  if(read(&ret,1)==1) {
    SetFilePointer(fn,current,nullptr,FILE_BEGIN);
    return ret;
    }

  SetFilePointer(fn,current,nullptr,FILE_BEGIN);
  return 0;
  }

size_t RFile::seek(size_t advance) {
  HANDLE fn      = HANDLE(handle);
  LONG   current = LONG(SetFilePointer(fn,0,nullptr,FILE_CURRENT));
  LONG   npos    = LONG(SetFilePointer(fn,LONG(advance),nullptr,FILE_CURRENT));

  if(npos>current)
    return size_t(npos-current);
  return 0;
  }
