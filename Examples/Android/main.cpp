#include <android/log.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <cstdint>

// A packaging smoke test using the platform activity, independent of Tempest's Android backend.
// Fold this into Examples/Empty once that backend is available upstream.
static void draw(ANativeActivity*, ANativeWindow* window) {
  ANativeWindow_setBuffersGeometry(window,0,0,WINDOW_FORMAT_RGBA_8888);
  ANativeWindow_Buffer buffer = {};
  if(ANativeWindow_lock(window,&buffer,nullptr)!=0)
    return;
  auto pixels = static_cast<uint32_t*>(buffer.bits);
  for(int y=0; y<buffer.height; ++y)
    for(int x=0; x<buffer.width; ++x) {
      const bool center = x>buffer.width/3 && x<buffer.width*2/3 &&
                          y>buffer.height/3 && y<buffer.height*2/3;
      pixels[y*buffer.stride+x] = center ? 0xffa1c2d7u : 0xff593314u;
      }
  ANativeWindow_unlockAndPost(window);
  }

extern "C" void ANativeActivity_onCreate(ANativeActivity* activity, void*, size_t) {
  activity->callbacks->onNativeWindowCreated = draw;
  activity->callbacks->onNativeWindowResized = draw;
  activity->callbacks->onNativeWindowRedrawNeeded = draw;
  __android_log_print(ANDROID_LOG_INFO,"TempestExample","Native packaging example started");
  }
