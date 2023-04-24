#pragma once

#include <functional>
#include <string>
#include <cstdint>

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

    static bool scan(const char16_t*       path,std::function<void(const std::u16string&,FileType)> cb);
    static bool scan(const std::u16string& path,std::function<void(const std::u16string&,FileType)> cb);
  };

}
