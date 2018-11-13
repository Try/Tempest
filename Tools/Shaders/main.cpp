#include <cstdlib>
#include <string>
#include <fstream>

#include <windows.h>
#include <dirent.h>

bool endsWith(const char* str,const char* need){
  size_t ls = strlen(str);
  size_t ln = strlen(need);
  if(ls<ln)
    return false;
  return strcmp(str+ls-ln,need)==0;
  }

int compile(const char* path,const char* exec,const char* format){
  dirent *entry;
  DIR *dp = opendir(path);
  if(dp==nullptr) {
    return -1;
    }

  while((entry = readdir(dp))) {
    char buf[2048]={};
    if(!endsWith(entry->d_name,".vert") &&
       !endsWith(entry->d_name,".frag"))
      continue;
    std::snprintf(buf,sizeof(buf),format,path,entry->d_name,path,entry->d_name);

    SHELLEXECUTEINFO ShExecInfo = {0};
    ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    ShExecInfo.fMask  = SEE_MASK_NOCLOSEPROCESS;
    ShExecInfo.lpFile = exec;
    ShExecInfo.lpParameters = buf;
    ShExecInfo.nShow = SW_HIDE;
    ShellExecuteEx(&ShExecInfo);
    WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
    CloseHandle(ShExecInfo.hProcess);

    //printf("%s\n",buf);
    }

  closedir(dp);
  return 0;
  }

int generateCpp(const char* path){
  std::ofstream out(std::string(path)+"/shaders.inl");
  dirent *entry;
  DIR *dp = opendir(path);
  if(dp==nullptr) {
    return -1;
    }

  while((entry = readdir(dp))) {
    if(!endsWith(entry->d_name,".sprv"))
      continue;

    out << "static const uint32_t ";
    for(const char* p=entry->d_name;*p;++p){
      char v=*p;
      if(v=='.')
        v='_';
      out << v;
      }
    out << "[] = {";
    std::ifstream sh(std::string(path)+"/"+entry->d_name, std::ifstream::ate | std::ifstream::binary);
    size_t length=sh.tellg();
    sh.seekg(std::ios::beg);

    bool first=true;
    for(size_t i=0;i<length;i+=4){
      uint32_t v=0;
      sh.read(reinterpret_cast<char*>(&v),4);
      if(sh.eof())
        break;
      if(!first)
        out <<", ";
      out << v;
      first=false;
      }
    out <<"};";
    out << std::endl;
    }

  closedir(dp);
  return 0;
  }

int main(int argc,const char** argv){
  if(argc==1)
    return -1;

  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  const char* vk_path=std::getenv("VULKAN_SDK");

  std::string s="";
  s+=vk_path;
  s+="\\Bin\\glslangValidator.exe";

  std::string p="-V \"%s/%s\" -o \"%s/%s.sprv\"";

  compile(argv[1],s.c_str(),p.c_str());
  generateCpp(argv[1]);
  return 0;
  }
