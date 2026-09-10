# Android packaging

A small NativeActivity packaging example. It will move into `Examples/Empty` when the Android backend is available upstream.

With JDK 17, Gradle 8.9, Ninja and the Android SDK configured (`ANDROID_HOME`), install SDK 35, build-tools 35.0.0, NDK 27.0.12077973 and CMake 3.22.1. Replace `/path/to/ndk` below with the NDK installation directory.

```sh
cmake -S Examples/Android -B build/android-example -G Ninja -DCMAKE_TOOLCHAIN_FILE=/path/to/ndk/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DCMAKE_BUILD_TYPE=Release
cmake --build build/android-example --target TempestExample-apk
adb install -r build/android-example/TempestExample-apk/build/outputs/apk/release/TempestExample-apk-release.apk
adb shell am start -n org.tempest.example/android.app.NativeActivity
```

Use `-DCMAKE_BUILD_TYPE=Debug` for a debug APK. Gradle must be on `PATH`, under `GRADLE_HOME`, or selected with `TEMPEST_ANDROID_GRADLE_EXECUTABLE`. Release APKs use the local debug key unless the `TEMPEST_KEYSTORE`, `TEMPEST_KEY_ALIAS`, `TEMPEST_STORE_PASSWORD` and `TEMPEST_KEY_PASSWORD` environment variables are set.

In an application's existing CMakeLists.txt, after defining its shared-library target and adding Tempest:

```cmake
if(ANDROID)
  add_android_apk(MyGame-apk
    CODE MyGame
    PACKAGE_NAME org.example.mygame
    MANIFEST AndroidManifest.xml)
endif()
```

The application owns the manifest; `.in` templates are also supported. NativeActivity's `android.app.lib_name` must match the library's `OUTPUT_NAME` without `lib` or `.so`. Use `CMAKE_ARGUMENTS` to pass project-specific CMake options into Gradle's native build.

The helper generates one `build.gradle` and points Gradle at this same CMake project. Its inner native build skips packaging generation. The generated directory can also be imported into Android Studio. No separate packaging CMake project or checked-in Gradle wrapper is needed, and desktop builds do not look for Android tools.
