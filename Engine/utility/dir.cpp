#include "dir.h"

#include <Tempest/Platform>

#ifdef __WINDOWS__
#include <windows.h>
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

  std::string tmp;
  while( FindNextFileW(hFind, &ffd)!=0 ){
    const int len = WideCharToMultiByte(CP_UTF8,0,ffd.cFileName,-1,nullptr,0,nullptr,nullptr);
    if(len<=0)
      continue;

    tmp.resize(size_t(len));
    WideCharToMultiByte(CP_UTF8,0,ffd.cFileName,-1,&tmp[0],int(tmp.size()),nullptr,nullptr);

    if(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      cb(tmp,FT_Dir); else
      cb(tmp,FT_File);
    }
  if( GetLastError() != ERROR_NO_MORE_FILES )
    return false;
#else
  dirent *dp;
  DIR *dirp;

  dirp = opendir( s.c_str() );

  while (dirp) {
    errno = 0;
    if ((dp = readdir(dirp)) != NULL) {
      std::u16string str;

      uint8_t* s = (uint8_t*)dp->d_name;
      str = SystemAPI::toUtf16((char*)s);

      FileType type=FT_File;
      //struct stat statbuf;
      if(dp->d_type==DT_DIR)
        type=FT_Dir;
      cb( str,type );
      //Log() <<"'"<< dp->d_name <<"': " << len;
      } else {
      if( errno == 0 ) {
        closedir(dirp);
        return true;
        }
      closedir(dirp);
      return false;
      }
    }
#endif
  return true;
  }
