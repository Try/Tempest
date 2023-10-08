#include "rfile.h"

#include <Tempest/TextCodec>
#include <Tempest/Except>

#ifdef __WINDOWS__
#include <windows.h>
#else
#include <cstdio>
#endif

#include <stdexcept>

using namespace Tempest;

RFile::RFile(const char *name) {
#ifdef __WINDOWS__
  std::wstring path;
  const int len=MultiByteToWideChar(CP_UTF8,0,name,-1,nullptr,0);
  if(len>1){
    path.resize(size_t(len-1));
    MultiByteToWideChar(CP_UTF8,0,name,-1,&path[0],int(path.size()));
    }
  handle=implOpen(path.c_str());
#else
  handle=implOpen(name);
#endif
  }

RFile::RFile(const std::string &path)
  :RFile(path.c_str()){
  }

RFile::RFile(const char16_t *path) {
#ifdef __WINDOWS__
  handle = implOpen(reinterpret_cast<const wchar_t*>(path));
#else
  handle = implOpen(TextCodec::toUtf8(path).c_str());
#endif
  }

RFile::RFile(const std::u16string &path)
  :RFile(path.c_str()){
  }

RFile::RFile(RFile &&other)
  :handle(other.handle) {
  other.handle = nullptr;
  }

#if defined(__WINDOWS__)
void* RFile::implOpen(const wchar_t *wstr) {
  void* ret = CreateFileW(wstr,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(ret==HANDLE(LONG_PTR(-1)))
    throw std::system_error(Tempest::SystemErrc::UnableToOpenFile);
  return ret;
  }
#elif defined(__IOS__)
// rfile.mm
#else
void* RFile::implOpen(const char *cstr) {
  void* ret = fopen(cstr,"rb");
  if(ret==nullptr)
    throw std::system_error(Tempest::SystemErrc::UnableToOpenFile);
  return ret;
  }
#endif

RFile::~RFile() {
#ifdef __WINDOWS__
  if(handle!=nullptr)
    CloseHandle(HANDLE(handle));
#else
  if(handle!=nullptr)
    fclose(reinterpret_cast<FILE*>(handle));
#endif
  }

RFile &RFile::operator =(RFile &&other) {
  std::swap(handle,other.handle);
  return *this;
  }

size_t RFile::read(void *dest, size_t size) {
#ifdef __WINDOWS__
  HANDLE fn          = HANDLE(handle);
  DWORD  dwBytesRead = DWORD(size);
  DWORD  cnt         = ReadFile(fn, dest, DWORD(size), &dwBytesRead, nullptr) ? dwBytesRead : 0;

  return cnt;
#else
  return fread(dest,1,size,reinterpret_cast<FILE*>(handle));
#endif
  }

size_t RFile::size() const {
#ifdef __WINDOWS__
  HANDLE fn = HANDLE(handle);
  return GetFileSize(fn,nullptr);
#else
  FILE* f = reinterpret_cast<FILE*>(handle);
  const long curr = ftell(f);
  fseek(f,0,SEEK_END);
  const long size = ftell(f);
  fseek(f,curr,SEEK_SET);
  return size_t(size);
#endif
  }

uint8_t RFile::peek() {
#ifdef __WINDOWS__
  uint8_t ret = 0;
  HANDLE  fn  = HANDLE(handle);

  LONG current=LONG(SetFilePointer(fn,0,nullptr,FILE_CURRENT));
  if(read(&ret,1)==1) {
    SetFilePointer(fn,current,nullptr,FILE_BEGIN);
    return ret;
    }

  SetFilePointer(fn,current,nullptr,FILE_BEGIN);
  return 0;
#else
  uint8_t ch=0;
  if(fread(&ch,1,1,reinterpret_cast<FILE*>(handle))==0)
    return 0;
  if(fseek(reinterpret_cast<FILE*>(handle),-1,SEEK_CUR)==0)
    return ch;
  return 0;
#endif
  }

size_t RFile::seek(size_t advance) {
#ifdef __WINDOWS__
  HANDLE fn      = HANDLE(handle);
  LONG   current = LONG(SetFilePointer(fn,0,nullptr,FILE_CURRENT));
  LONG   npos    = LONG(SetFilePointer(fn,LONG(advance),nullptr,FILE_CURRENT));

  if(INVALID_SET_FILE_POINTER==DWORD(npos))
    return 0;
  if(npos>current)
    return size_t(npos-current);
  return 0;
#else
  if(fseek(reinterpret_cast<FILE*>(handle),long(advance),SEEK_CUR)==0)
    return advance;
  return 0;
#endif
  }

size_t RFile::unget(size_t advance) {
#ifdef __WINDOWS__
  HANDLE fn      = HANDLE(handle);
  LONG   current = LONG(SetFilePointer(fn,0,nullptr,FILE_CURRENT));
  if(LONG(advance)>current)
    advance=size_t(current);
  LONG   npos    = LONG(SetFilePointer(fn,-LONG(advance),nullptr,FILE_CURRENT));

  if(INVALID_SET_FILE_POINTER==DWORD(npos))
    return 0;
  if(npos<current)
    return size_t(current-npos);
  return 0;
#else
  FILE* f = reinterpret_cast<FILE*>(handle);
  const long current = ftell(f);
  if(long(advance)>current)
    advance=size_t(current);
  if(fseek(f,-long(advance),SEEK_CUR)==0)
    return advance;
  return 0;
#endif
  }
