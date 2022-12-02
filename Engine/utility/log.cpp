#include "log.h"

#include <Tempest/Platform>

#ifdef __ANDROID__
#include <android/log.h>
#endif

#if defined(__WINDOWS_PHONE__) || defined(_MSC_VER)
#include <windows.h>
#define snprintf sprintf_s
#endif

#include <iostream>
#include <cstdio>
#include <cstring>

using namespace Tempest;

void Log::setOutputCallback(std::function<void (Mode, const char*)> f) {
  std::lock_guard<std::recursive_mutex> g(globals().sync);
  globals().outFn = f;
  }

Log::Globals& Log::globals() {
  static Globals g;
  return g;
  }

void Log::flush(Context& ctx, char *&out, size_t &count) {
  if(count==sizeof(ctx.buffer))
    return;

  std::lock_guard<std::recursive_mutex> g(globals().sync);
  {
    *out = '\0';
#ifdef __ANDROID__
    switch(ctx.mode) {
      case Error:
        __android_log_print(ANDROID_LOG_ERROR, "app", "%s", ctx.buffer);
        break;
      case Debug:
        __android_log_print(ANDROID_LOG_DEBUG, "app", "%s", ctx.buffer);
        break;
      case Info:
      default:
        __android_log_print(ANDROID_LOG_INFO,  "app", "%s", ctx.buffer);
        break;
      }
#elif defined(__WINDOWS_PHONE__)
#if !defined(_DEBUG)
  OutputDebugStringA(ctx.buffer);
  OutputDebugStringA("\r\n");
#endif
#else
#if defined(_MSC_VER) && !defined(_NDEBUG)
  (void)ctx;
  OutputDebugStringA(ctx.buffer);
  OutputDebugStringA("\r\n");
#else
  if(ctx.mode==Error){
    std::cerr << ctx.buffer << std::endl;
    std::cerr.flush();
    }
    else
    std::cout << ctx.buffer << std::endl;
#endif
#endif
    }

  if(globals().outFn) {
    char cpy[sizeof(ctx.buffer)] = {};
    std::memcpy(cpy,ctx.buffer,count);
    out   = ctx.buffer;
    count = sizeof(ctx.buffer);
    globals().outFn(ctx.mode,ctx.buffer);
    } else {
    out   = ctx.buffer;
    count = sizeof(ctx.buffer);
    }
  }

void Log::printImpl(Context& ctx, char* out, size_t count) {
  flush(ctx,out,count);
  }

void Log::write(Context& ctx, char *&out, size_t &count, const std::string &msg) {
  write(ctx,out,count,msg.c_str());
  }

void Log::write(Context& ctx, char*& out, size_t& count, std::string_view msg) {
  for(size_t i=0; i<msg.size(); ++i) {
    if(count<=1)
      flush(ctx,out,count);

    if(msg[i]=='\n') {
      flush(ctx,out,count);
      } else {
      *out = msg[i];
      ++out;
      --count;
      }
    }
  }

void Log::write(Context& ctx, char *&out, size_t &count, float msg) {
  char sym[16];
  snprintf(sym,sizeof(sym),"%f",double(msg));
  write(ctx,out,count,sym);
  }

void Log::write(Context& ctx, char *&out, size_t &count, double msg) {
  char sym[16];
  snprintf(sym,sizeof(sym),"%f",msg);
  write(ctx,out,count,sym);
  }

void Log::write(Context& ctx, char *&out, size_t &count, const void *msg) {
  char sym[sizeof(void*)*2+3];
  sym[sizeof(sym)-1] = '\0';
  int pos = sizeof(sym)-2;

  uintptr_t ptr = uintptr_t(msg);
  for(size_t i=0; i<sizeof(void*)*2; ++i){
    auto num = ptr%16;
    sym[pos] = num<=9 ? char(num+'0') : char(num-10+'a');
    ptr/=16;
    --pos;
    }
  sym[pos] = 'x';
  --pos;
  sym[pos] = '0';
  write(ctx,out,count,sym+pos);
  }

void Log::write(Context& ctx, char*& out, size_t& count, std::thread::id msg) {
  std::hash<std::thread::id> h;
  write(ctx,out,count,uint64_t(h(msg)));
  }

void Log::write(Context& ctx, char*& out, size_t& count, int16_t msg){
  writeInt(ctx,out,count,msg);
  }

void Log::write(Context& ctx, char *&out, size_t& count, uint16_t msg) {
  writeUInt(ctx,out,count,msg);
  }

void Log::write(Context& ctx, char*& out, size_t& count, const int32_t& msg){
  writeInt(ctx,out,count,msg);
  }

void Log::write(Context& ctx, char*& out, size_t& count, const uint32_t& msg) {
  writeUInt(ctx,out,count,msg);
  }

void Log::write(Context& ctx, char*& out, size_t& count, const int64_t& msg) {
  writeInt(ctx,out,count,msg);
  }

void Log::write(Context& ctx, char*& out, size_t& count, const uint64_t& msg) {
  writeUInt(ctx,out,count,msg);
  }

void Log::write(Context& ctx, char*& out, size_t& count, const char* msg){
  while(*msg) {
    if(count<=1)
      flush(ctx,out,count);
    if(*msg=='\n'){
      flush(ctx,out,count);
      ++msg;
      } else {
      *out = *msg;
      ++out;
      ++msg;
      --count;
      }
    }
  }

void Log::write(Context& ctx, char *&out, size_t &count, char msg) {
  if(count<=1)
    flush(ctx,out,count);
  *out = msg;
  ++out;
  ++msg;
  --count;
  }

void Log::write(Context& ctx, char *&out, size_t &count, int8_t msg) {
  writeInt(ctx,out,count,msg);
  }

void Log::write(Context& ctx, char *&out, size_t &count, uint8_t msg) {
  writeUInt(ctx,out,count,msg);
  }

template< class T >
void Log::writeInt(Context& ctx, char*& out, size_t& count, T msg){
  char sym[32];
  sym[sizeof(sym)-1] = '\0';
  int pos = sizeof(sym)-2;

  if(msg<0){
    write(ctx,out,count,'-');
    msg = -msg;
    }

  while(pos>=0){
    sym[pos] = msg%10+'0';
    msg/=10;
    if(msg==0)
      break;
    --pos;
    }
  write(ctx,out,count,sym+pos);
  }
