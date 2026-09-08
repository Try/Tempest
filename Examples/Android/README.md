# Android packaging

This example packages a native CMake target into an APK. It draws a gold rectangle using Android's built-in NativeActivity, without Java sources, game assets or the Tempest Android backend. Events, Vulkan swapchains and controllers are separate work.

## Build

Install JDK 17, Gradle 8.9, CMake 3.22 or newer, and Ninja. Install Android SDK packages `platforms;android-35`, `build-tools;35.0.0`, `ndk;27.0.12077973` and `cmake;3.22.1`. Set `JAVA_HOME` and `ANDROID_HOME`, and put Gradle on `PATH` or set `GRADLE_HOME` to its installation directory.

From the repository root:

```sh
cmake -S Examples/Android -B build/android-example -G Ninja
cmake --build build/android-example --target TempestExample-apk
adb install -r build/android-example/TempestExample/build/outputs/apk/release/TempestExample-release.apk
adb shell am start -n org.tempest.example/android.app.NativeActivity
```

Release is the default; select debug with `-DTEMPEST_ANDROID_BUILD_TYPE=Debug` when configuring. Release APKs use the local debug signing key unless distribution signing is configured below.

Generation needs only CMake and its build tool. It creates one `build.gradle` and a manifest in the build directory. There is no root/app split, `settings.gradle`, `gradle.properties`, wrapper JAR or wrapper script to maintain. Command-line builds use the installed Gradle; set `TEMPEST_ANDROID_GRADLE_EXECUTABLE` to its executable if discovery fails.

In Android Studio, import `build/android-example/TempestExample` and select the local Gradle 8.9 installation if prompted. If you prefer a wrapper, generate it in that build directory with `gradle -p build/android-example/TempestExample wrapper --gradle-version 8.9`. Generated files stay out of the source repository. See the [Gradle wrapper documentation](https://docs.gradle.org/current/userguide/gradle_wrapper.html).

## Use in another application

Create a separate packaging project with `project(... LANGUAGES NONE)`, include `Engine/cmake/TempestAndroid.cmake`, and call `tempest_android_application` as in this example. `NATIVE_SOURCE_DIR` points to the existing native CMake project, not the packaging project. Gradle configures that native project with the NDK, avoiding recursive packaging generation.

The native project builds a shared library and calls `tempest_android_native_target` to retain `ANativeActivity_onCreate` and enable 16 KiB page alignment. Desktop builds do not invoke the packaging function and need no Android tools.

Required arguments: `APPLICATION_ID`, `NATIVE_SOURCE_DIR`, `NATIVE_TARGET`, `LIBRARY_NAME`. The library name must match the target's `OUTPUT_NAME`, without `lib` or `.so`.

Optional configuration:

- `LABEL`, `VERSION_CODE`, `VERSION_NAME`: app metadata.
- `MANIFEST`: an application-owned manifest for a custom activity, permissions or device requirements.
- `JAVA_DIRS`, `RESOURCE_DIRS`, `ASSET_DIRS`: source directories.
- `DEPENDENCIES`, `CMAKE_ARGUMENTS`, `CPP_FLAGS`, `PROGUARD_FILES`, `NO_COMPRESS`: lists.
- `SHRINK_RELEASE`, `REPACKAGE`: enable shrinking or force ZIP repackaging for large asset bundles.
- `ASSET_PROPERTY`: a Gradle property naming an additional asset directory.

Paths are relative to the packaging CMakeLists.txt. The default activity is `android.app.NativeActivity`; Java sources and JNI keep rules are not injected automatically. Apps using a custom backend must supply its manifest, Java sources and keep rules explicitly. AndroidX apps can pass `-Pandroid.useAndroidX=true` to Gradle or configure it in their user-level Gradle properties.

Tool versions and ABIs are `TEMPEST_ANDROID_*` CMake cache settings. Gradle properties `tempestVersionCode` and `tempestVersionName` override versions; `PROPERTY_PREFIX` changes the prefix.

## Distribution signing

Create a signing key once and reuse it for every update. Keep it outside the repository and back it up securely with its password. With JDK 17's `bin` on `PATH`, this command prompts for the password and certificate details:

```sh
keytool -genkeypair -v -storetype PKCS12 -keystore /path/to/app-release.p12 -alias release -keyalg RSA -keysize 2048 -validity 10000
```

Set all four environment variables before running the APK build target: `TEMPEST_KEYSTORE` (absolute keystore path), `TEMPEST_KEY_ALIAS`, `TEMPEST_STORE_PASSWORD` and `TEMPEST_KEY_PASSWORD`. For PKCS12, use the same password for both. Gradle reads them at build time; secrets are not written to generated files or the CMake cache. `SIGNING_ENV_PREFIX` changes the `TEMPEST` prefix.

PowerShell, after creating the key:

```powershell
$env:TEMPEST_KEYSTORE = 'C:/Keys/app-release.p12'
$env:TEMPEST_KEY_ALIAS = 'release'
$env:TEMPEST_STORE_PASSWORD = [System.Net.NetworkCredential]::new('', (Read-Host 'Keystore password' -AsSecureString)).Password
$env:TEMPEST_KEY_PASSWORD = $env:TEMPEST_STORE_PASSWORD
cmake --build build/android-example --target TempestExample-apk
$env:TEMPEST_STORE_PASSWORD = $null
$env:TEMPEST_KEY_PASSWORD = $null
```

Bash, after creating the key:

```sh
export TEMPEST_KEYSTORE='/path/to/app-release.p12'
export TEMPEST_KEY_ALIAS='release'
read -r -s -p 'Keystore password: ' TEMPEST_STORE_PASSWORD
echo
export TEMPEST_STORE_PASSWORD
export TEMPEST_KEY_PASSWORD="$TEMPEST_STORE_PASSWORD"
cmake --build build/android-example --target TempestExample-apk
unset TEMPEST_STORE_PASSWORD TEMPEST_KEY_PASSWORD
```

Never commit keystores or passwords. A differently signed APK cannot update an existing installation. See Android's [signing guide](https://developer.android.com/studio/publish/app-signing).
