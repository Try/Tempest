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

using namespace Tempest;

Log::Context& Log::getCtx() {
  static Context ctx;
  return ctx;
  }

void Log::flush(Mode m, char *&out, size_t &count) {
  Context& c = getCtx();
  if(count!=sizeof(c.buffer)){
    *out = '\0';

#ifdef __ANDROID__
    switch (m) {
      case Error:
        __android_log_print(ANDROID_LOG_ERROR, "app", "%s", c.buffer);
        break;
      case Debug:
        __android_log_print(ANDROID_LOG_DEBUG, "app", "%s", c.buffer);
        break;
      case Info:
      default:
        __android_log_print(ANDROID_LOG_INFO,  "app", "%s", c.buffer);
        break;
      }
#elif defined(__WINDOWS_PHONE__)
#if !defined(_DEBUG)
  OutputDebugStringA(c.buffer);
  OutputDebugStringA("\r\n");
#endif
#else
#if defined(_MSC_VER) && !defined(_NDEBUG)
  (void)m;
  OutputDebugStringA(c.buffer);
  OutputDebugStringA("\r\n");
#else
  if( m==Error ){
    std::cerr << c.buffer << std::endl;
    std::cerr.flush();
    }
    else
    std::cout << c.buffer << std::endl;
#endif
#endif
    }
  out   = c.buffer;
  count = sizeof(c.buffer);
  }

void Log::printImpl(Mode m,char* out, size_t count){
  flush(m,out,count);
  }

void Log::write(Log::Mode m, char *&out, size_t &count, const std::string &msg) {
  write(m,out,count,msg.c_str());
  }

void Log::write(Mode m, char *&out, size_t &count, float msg) {
  char sym[16];
  snprintf(sym,sizeof(sym),"%f",double(msg));
  write(m,out,count,sym);
  }

void Log::write(Mode m, char *&out, size_t &count, double msg) {
  char sym[16];
  snprintf(sym,sizeof(sym),"%f",msg);
  write(m,out,count,sym);
  }

void Log::write(Log::Mode m, char *&out, size_t &count, const void *msg) {
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
  write(m,out,count,sym+pos);
  }

void Log::write(Log::Mode m, char *&out, size_t &count, std::thread::id msg) {
  std::hash<std::thread::id> h;
  write(m,out,count,h(msg));
  }

void Log::write(Mode m, char*& out, size_t& count, int16_t msg){
  writeInt(m,out,count,msg);
  }

void Log::write(Log::Mode m, char *&out, size_t &count, uint16_t msg) {
  writeUInt(m,out,count,msg);
  }

void Log::write(Mode m, char*& out, size_t& count, int32_t msg){
  writeInt(m,out,count,msg);
  }

void Log::write(Log::Mode m, char *&out, size_t &count, uint32_t msg) {
  writeUInt(m,out,count,msg);
  }

void Log::write(Log::Mode m, char *&out, size_t &count, int64_t msg) {
  writeInt(m,out,count,msg);
  }

void Log::write(Log::Mode m, char *&out, size_t &count, uint64_t msg) {
  writeUInt(m,out,count,msg);
  }

void Log::write(Mode m, char*& out, size_t& count, const char* msg){
  while(*msg){
    if(count<=1)
      flush(m,out,count);
    if(*msg=='\n'){
      flush(m,out,count);
      ++msg;
      } else {
      *out = *msg;
      ++out;
      ++msg;
      --count;
      }
    }
  }

void Log::write(Log::Mode m, char *&out, size_t &count, char msg) {
  if(count<=1)
    flush(m,out,count);
  *out = msg;
  ++out;
  ++msg;
  --count;
  }

void Log::write(Log::Mode m, char *&out, size_t &count, int8_t msg) {
  writeInt(m,out,count,msg);
  }

void Log::write(Log::Mode m, char *&out, size_t &count, uint8_t msg) {
  writeUInt(m,out,count,msg);
  }

template< class T >
void Log::writeInt(Mode m, char*& out, size_t& count, T msg){
  char sym[32];
  sym[sizeof(sym)-1] = '\0';
  int pos = sizeof(sym)-2;

  if(msg<0){
    write(m,out,count,'-');
    msg = -msg;
    }

  while(pos>=0){
    sym[pos] = msg%10+'0';
    msg/=10;
    if(msg==0)
      break;
    --pos;
    }
  write(m,out,count,sym+pos);
  }

template< class T >
void Log::writeUInt(Mode m, char*& out, size_t& count, T msg){
  char sym[32];
  sym[sizeof(sym)-1] = '\0';
  int pos = sizeof(sym)-2;

  while(pos>=0){
    sym[pos] = msg%10+'0';
    msg/=10;
    if(msg==0)
      break;
    --pos;
    }
  write(m,out,count,sym+pos);
  }
