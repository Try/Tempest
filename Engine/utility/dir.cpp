#include "dir.h"

#include <Tempest/Platform>
#include <Tempest/TextCodec>

#include <memory>

#if defined(__WINDOWS__) || defined(__WINDOWS_PHONE__)
#include <windows.h>
#else
#include <errno.h>
#include <dirent.h>
#endif

using namespace Tempest;

Dir::Dir() {  
  }

bool Dir::scan(const std::string &path, std::function<void (const std::string &, Dir::FileType)> cb) {
  return scan(path.c_str(),cb);
  }

bool Dir::scan(const char *name, std::function<void(const std::string&,FileType)> cb) {
#if defined(__WINDOWS__) || defined(__WINDOWS_PHONE__)
  WIN32_FIND_DATAW ffd;
  HANDLE hFind = INVALID_HANDLE_VALUE;

  std::wstring path;
  const int len=MultiByteToWideChar(CP_UTF8,0,name,-1,nullptr,0);
  if(len>1){
    path.resize(size_t(len+1));
    MultiByteToWideChar(CP_UTF8,0,name,-1,&path[0],int(path.size()));
    path[path.size()-2]='/';
    path[path.size()-1]='*';
    }

  hFind = FindFirstFileExW( path.c_str(),
                            FindExInfoStandard, &ffd,
                            FindExSearchNameMatch, nullptr, 0);
  if(INVALID_HANDLE_VALUE==hFind)
    return false;
  const std::unique_ptr<void, decltype(&FindClose)> searchHandle(hFind, &FindClose);

  std::string tmp;
  while( FindNextFileW(hFind, &ffd)!=0 ){
    const int len = WideCharToMultiByte(CP_UTF8,0,ffd.cFileName,-1,nullptr,0,nullptr,nullptr);
    if(len<=1)
      continue;

    tmp.resize(size_t(len-1));
    WideCharToMultiByte(CP_UTF8,0,ffd.cFileName,-1,&tmp[0],int(tmp.size()),nullptr,nullptr);

    if(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      cb(tmp,FT_Dir); else
      cb(tmp,FT_File);
    }
  if( GetLastError() != ERROR_NO_MORE_FILES )
    return false;
#else
  const std::unique_ptr<DIR, decltype(&closedir)> dirp(opendir(name), &closedir);
  if(!dirp)
    return false;

  std::string tmp;
  for(;;) {
    errno = 0;
    const auto* dp = readdir(dirp.get());
    if(dp==nullptr)
      return errno==0;

    tmp = dp->d_name;
    if(dp->d_type==DT_DIR)
      cb(tmp,FT_Dir); else
      cb(tmp,FT_File);
    }
#endif
  return true;
  }

bool Dir::scan(const std::u16string &path, std::function<void (const std::u16string &, Dir::FileType)> cb) {
  return scan(path.c_str(),cb);
  }

bool Dir::scan(const char16_t *path, std::function<void (const std::u16string &, Dir::FileType)> cb) {
#if defined(__WINDOWS__) || defined(__WINDOWS_PHONE__)
  WIN32_FIND_DATAW ffd;
  HANDLE hFind = INVALID_HANDLE_VALUE;

  std::u16string rg = path;
  rg += u"/*";

  hFind = FindFirstFileExW( reinterpret_cast<const WCHAR*>(rg.c_str()),
                            FindExInfoStandard, &ffd,
                            FindExSearchNameMatch, nullptr, 0);
  if(INVALID_HANDLE_VALUE==hFind)
    return false;
  const std::unique_ptr<void, decltype(&FindClose)> searchHandle(hFind, &FindClose);

  while( FindNextFileW(hFind, &ffd)!=0 ){
    std::u16string path=reinterpret_cast<const char16_t*>(ffd.cFileName);
    if(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      cb(path,FT_Dir); else
      cb(path,FT_File);
    }
  if( GetLastError() != ERROR_NO_MORE_FILES )
    return false;
#else
  std::string name = TextCodec::toUtf8(path);
  const std::unique_ptr<DIR, decltype(&closedir)> dirp(opendir(name.c_str()), &closedir);
  if(!dirp)
    return false;

  std::u16string tmp;
  for(;;) {
    errno = 0;
    const auto* dp = readdir(dirp.get());
    if(dp==nullptr)
      return errno==0;

    tmp = TextCodec::toUtf16(dp->d_name);
    if(dp->d_type==DT_DIR)
      cb(tmp,FT_Dir); else
      cb(tmp,FT_File);
    }
#endif
  return true;
  }
