#pragma once

#include <functional>

namespace Tempest {

class Dir {
  public:
    Dir();

    enum FileType : uint8_t {
      FT_Dir  = 1,
      FT_File = 3
      };
    static bool scan(const char*        path,std::function<void(const std::string&,FileType)> cb);
    static bool scan(const std::string& path,std::function<void(const std::string&,FileType)> cb);
  };

}
