#include "rfile.h"

#if defined(__IOS__)

#include <Tempest/TextCodec>
#include <Tempest/Except>

#import  <UIKit/UIKit.h>

using namespace Tempest;

void* RFile::implOpen(const char *cstr) {
  if(cstr==nullptr || cstr[0]=='/') {
    void* ret = fopen(cstr,"rb");
    if(ret==nullptr)
      throw std::system_error(Tempest::SystemErrc::UnableToOpenFile);
    return ret;
    }
  
  @autoreleasepool {
    NSString *dir = [[NSBundle mainBundle] resourcePath];
    std::string full = [dir UTF8String];
    full += "/";
    full += cstr;
    return implOpen(full.c_str());
    }
  }

#endif
