# Building `sample-project-android` into an APK on Windows 10 

This guide assumes you already have this layout and nothing else:

```
D:\dev\rbfx-android\                 <- the directory you created
├── rbfx\                            <- clone #1: github.com/rbfx/rbfx (already built natively)
└── sample-project-android\          <- clone #2: github.com/rbfx/sample-project
    ├── CMakeLists.txt
    ├── Project\
    └── android\                     <- the Gradle project we will build
```

Every command below is a **Windows 10 Command Prompt (`cmd.exe`)** command. No Bash, no MSYS2, no PowerShell, without automated build via ex: `rebuild.sh`.

`rebuild.sh` only automates five things. This document does them by hand:

| `rebuild.sh` does | You will do |
| --- | --- |
| discovers Android SDK / NDK / JDK | Step 3 (set environment variables) |
| exports `Urho3D_DIR`, `CMAKE_PREFIX_PATH`, `RBFX_ANDROID_JAVA_DIR`, `ci_arch` | Step 3 |
| copies `rbfx\bin\CoreData` into `android\assets\` and writes `assets\ResourceRoot.ini` | Step 4 |
| runs `cmake --build` for the **Windows** engine | **not needed** for the APK (see note below) |
| runs `./gradlew assembleDebug` | Steps 5–6 |

> **Note — the native Windows build is not reused by the APK.** `Urho3D_DIR` points at
> `rbfx\CMake\Urho3D\share\Urho3D\Urho3DConfig.cmake`, which does
> `add_subdirectory(<rbfx source>)`, so Gradle's CMake step recompiles the whole engine
> **for Android** from the rbfx sources into `android\.cxx\`. The only thing from your earlier
> native build that this flow can use is the host `PackageTool.exe` (optional — Python 3 works
> as well). Practical consequence: you need the rbfx **source tree**, and you must satisfy the
> `PackageTool` check at configure time (Step 3.4).

---

## Step 1 — Verify the two clones

```bat
cd /d D:\dev\rbfx-android
dir /b
```

You should see `rbfx` and `sample-project-android`.

```bat
cd /d D:\dev\rbfx-android\sample-project-android
git submodule update --init --recursive
```

This pulls `Plugins/Core.SamplePlugin`. Without it the CMake configure step aborts with
`Missing required file: .../Plugins/Core.SamplePlugin/CMakeLists.txt`.

Sanity checks (none of these may print `NOT FOUND`):

```bat
if not exist "D:\dev\rbfx-android\rbfx\CMake\Urho3D\share\Urho3D\Urho3DConfig.cmake" echo NOT FOUND: Urho3DConfig.cmake
if not exist "D:\dev\rbfx-android\rbfx\bin\CoreData" echo NOT FOUND: CoreData
if not exist "D:\dev\rbfx-android\rbfx\Source\ThirdParty\SDL\android-project\app\src\main\java\org\libsdl\app\SDLActivity.java" echo NOT FOUND: SDL java glue
if not exist "D:\dev\rbfx-android\sample-project-android\Project\Data" echo NOT FOUND: Project\Data
```

---

## Step 2 — Install the external toolchains

The APK needs: **JDK 17**, **Android SDK (platform 35 + build-tools 35)**, **Android NDK**,
**CMake ≥ 3.25**, plus **Gradle 8.14.3** (because the repo does not ship a wrapper — see Step 5).

JDK 17 (skip if `java -version` already reports 17):

```bat
winget install -e --id Microsoft.OpenJDK.17
```

Android SDK command-line tools (needed only if you do not have Android Studio). Download
`commandlinetools-win-*.zip` from <https://developer.android.com/studio#command-line-tools-only>,
extract it to `%LOCALAPPDATA%\Android\Sdk\cmdline-tools\latest`, then:

```bat
set "SDKM=%LOCALAPPDATA%\Android\Sdk\cmdline-tools\latest\bin\sdkmanager.bat"
"%SDKM%" --licenses
"%SDKM%" "platform-tools" "platforms;android-35" "build-tools;35.0.0" "ndk;27.0.12077973" "cmake;3.31.6"
```

`ndk;27.0.12077973` is the NDK this project is known-good with (any other 27.x is normally fine,
but then adjust `ANDROID_NDK_HOME` in Step 3). `cmake;3.31.6` satisfies the `3.25.0+` requirement
declared in `android\build.gradle`; a system CMake ≥ 3.25 on `PATH` also works.

Gradle 8.14.3 (the version pinned by the project's wrapper properties):

```bat
curl -L -o "%TEMP%\gradle-8.14.3-bin.zip" https://services.gradle.org/distributions/gradle-8.14.3-bin.zip
if not exist C:\gradle mkdir C:\gradle
tar -xf "%TEMP%\gradle-8.14.3-bin.zip" -C C:\gradle
set "PATH=C:\gradle\gradle-8.14.3\bin;%PATH%"
gradle.bat -v
```

Python 3 is also needed (see Step 3.4). If `python --version` fails:

```bat
winget install -e --id Python.Python.3.12
```

---

## Step 3 — Set the environment variables

These replace everything `rebuild.sh` exports. Paste this whole block into the same `cmd.exe`
window you will build in. Adjust `ROOT` and the JDK path to your machine.

```bat
set "ROOT=D:\dev\rbfx-android"
set "ENGINE=%ROOT%\rbfx"
set "PROJECT=%ROOT%\sample-project-android"

rem --- Android toolchain ---------------------------------------------------
set "ANDROID_HOME=%LOCALAPPDATA%\Android\Sdk"
set "ANDROID_SDK_ROOT=%ANDROID_HOME%"
set "ANDROID_NDK_HOME=%ANDROID_HOME%\ndk\27.0.12077973"
set "JAVA_HOME=C:\Program Files\Microsoft\jdk-17.0.19.10-hotspot"

rem --- rbfx paths (CMake wants forward slashes) ---------------------------
set "Urho3D_DIR=%ENGINE:\=/%/CMake/Urho3D/share/Urho3D"
set "CMAKE_PREFIX_PATH=%Urho3D_DIR%"
set "RBFX_ANDROID_JAVA_DIR=%ENGINE:\=/%/Source/ThirdParty/SDL/android-project/app/src/main/java"

rem --- one ABI only (omit this to build armeabi-v7a + arm64-v8a + x86_64) --
set "ci_arch=arm64"

rem --- do NOT set ci_lib_type: the engine must be built shared ------------
set "ci_lib_type="
```

Verify — all four must resolve:

```bat
echo %ANDROID_HOME%
echo %ANDROID_NDK_HOME%
echo %JAVA_HOME%
echo %Urho3D_DIR%
if not exist "%Urho3D_DIR%\Urho3DConfig.cmake" echo WRONG Urho3D_DIR
if not exist "%RBFX_ANDROID_JAVA_DIR%\org\libsdl\app\SDLActivity.java" echo WRONG RBFX_ANDROID_JAVA_DIR
```

### 3.1 Why each variable is mandatory

* `ANDROID_HOME` — AGP refuses to configure with `SDK location not found` otherwise
  (alternative: a `local.properties` file containing `sdk.dir=D\:\\dev\\...`).
* `ANDROID_NDK_HOME` — pins the NDK when several are installed. Without it AGP picks its
  default and you can get `NDK not configured` or ABI mismatches.
* `JAVA_HOME` — Gradle 8.14.3 / AGP 8.13 need JDK 17 or newer; JDK 17 is the version this
  project is verified with.
* `Urho3D_DIR` **and** `CMAKE_PREFIX_PATH` — must point at the directory that *contains*
  `Urho3DConfig.cmake`, i.e. `rbfx\CMake\Urho3D\share\Urho3D`. Pointing at `rbfx\CMake` or at
  `rbfx-build` fails with `Could not find a package configuration file provided by "Urho3D"`.
* `RBFX_ANDROID_JAVA_DIR` — **required.** `android\build.gradle` defaults this to
  `../rbfx/Source/...` or `../3rdParty/rbfx/Source/...` *relative to `sample-project-android\`*,
  and neither exists in a sibling-clone layout. Without the variable, `SDLActivity.java` is not
  compiled and the build fails with `package org.libsdl.app does not exist`.
* `ci_arch=arm64` — maps to `abiFilters 'arm64-v8a'`. Unset means all three ABIs and roughly
  a 3× longer build.

Persist them for future sessions with `setx` (does **not** affect the current window):

```bat
setx ANDROID_HOME "%LOCALAPPDATA%\Android\Sdk"
setx ANDROID_SDK_ROOT "%LOCALAPPDATA%\Android\Sdk"
setx ANDROID_NDK_HOME "%LOCALAPPDATA%\Android\Sdk\ndk\27.0.12077973"
setx JAVA_HOME "C:\Program Files\Microsoft\jdk-17.0.19.10-hotspot"
setx Urho3D_DIR "D:/dev/rbfx-android/rbfx/CMake/Urho3D/share/Urho3D"
setx CMAKE_PREFIX_PATH "D:/dev/rbfx-android/rbfx/CMake/Urho3D/share/Urho3D"
setx RBFX_ANDROID_JAVA_DIR "D:/dev/rbfx-android/rbfx/Source/ThirdParty/SDL/android-project/app/src/main/java"
```

### 3.2 `ci_lib_type`

`build.gradle` computes `-DBUILD_SHARED_LIBS=ON` unless `ci_lib_type` is exactly `lib`. A static
engine will not work here (SDLActivity loads `libUrho3D.so` at runtime), so leave it unset or
empty. If a previous experiment set it globally, clear it with `set "ci_lib_type="`.

### 3.3 Output names come from the folder name

There is no `settings.gradle`, so the Gradle project is named after the directory holding
`build.gradle` — `android`. The APK is therefore `build\outputs\apk\debug\android-debug.apk`.

### 3.4 Satisfy the `PackageTool` check (pick one of the two ways)

At configure time `create_pak()` looks for `PackageTool`. It is never built for Android
(`Source\Tools` returns early when `DESKTOP` is false), so it falls back to
`Source/Tools/PackageTool/PackageTool.py` with Python 3. If you have Python 3 → nothing to do.
If you do not want Python, point it at the native tool from your earlier engine build:

```bat
set "PACKAGE_TOOL_EXECUTABLE=%ROOT%\rbfx-build\bin\RelWithDebInfo\PackageTool.exe"
if not exist "%PACKAGE_TOOL_EXECUTABLE%" echo WRONG PACKAGE_TOOL_EXECUTABLE
```

Without either, CMake stops with `PackageTool not found. Build tools natively, set
PACKAGE_TOOL_EXECUTABLE, or install Python3`.

---

## Step 4 — Put the engine resources inside the APK

`android\build.gradle` packages `android\assets\`, `Project\`, and
`..\StoreArt\Android\Icons`. The engine needs `CoreData` there, plus a `ResourceRoot.ini`
that describes paths *inside* the APK (program dir is `/apk/`), not on disk.

```bat
cd /d "%PROJECT%\android"
if not exist assets mkdir assets
xcopy /E /I /Y "%ENGINE%\bin\CoreData" "assets\CoreData"
> "assets\ResourceRoot.ini" echo # Resource roots inside the APK, program dir is /apk/.
>> "assets\ResourceRoot.ini" echo CoreData=CoreData
>> "assets\ResourceRoot.ini" echo Data=Data
```

Check:

```bat
dir /b assets
type assets\ResourceRoot.ini
```

Expect `CoreData`, `ResourceRoot.ini` (plus the tracked `.gitignore`), and exactly these three
lines from `type`:

```
# Resource roots inside the APK, program dir is /apk/.
CoreData=CoreData
Data=Data
```

`CoreData\` contains hundreds of files, so `xcopy` takes a few seconds.

> Do not use `sample-project-android\ResourceRoot.ini` (the one at the project root) for the APK.
> It points to `../rbfx/bin/CoreData` and `Project/Cache`, which are meaningless on a device.
> Gradle does not package it anyway.

---

## Step 5 — Create the Gradle wrapper (one time)

`sample-project`'s `.gitignore` contains the line `gradle*`, so a fresh clone has **no**
`gradlew`, no `gradlew.bat`, and no `gradle\wrapper\gradle-wrapper.jar`. Create them with the
Gradle 8.14.3 you installed in Step 2:

```bat
cd /d "%PROJECT%\android"
gradle.bat wrapper --gradle-version 8.14.3
```

Result:

```bat
dir /b gradlew.bat gradle\wrapper
```

The `wrapper` task configures `build.gradle` (so `ANDROID_HOME`, `Urho3D_DIR`, and
`ci_arch` must already be set) and downloads AGP 8.13.0 from `google()`/`mavenCentral()`.

*Skip this step entirely* if you prefer to keep using the standalone binary — just replace
`gradlew.bat` with `gradle.bat` in Step 6. The wrapper is only a convenience; it pins
8.14.3, which is the version AGP 8.13.0 expects.

---

## Step 6 — Build the APK

From `%PROJECT%\android`, in the same `cmd.exe` window:

```bat
cd /d "%PROJECT%\android"
gradlew.bat --console=plain assembleDebug
```

First run: Gradle downloads the 8.14.3 distribution, resolves AGP, then CMake configures
`arm64-v8a` and compiles the whole engine + sample project with Ninja into
`android\.cxx\Debug\<hash>\arm64-v8a\`. Expect roughly 5–15 minutes on a modern machine
(the reference build took 5m52s) and several GB inside `.cxx\`. Later runs are incremental
and take well under a minute.

A successful run ends with:

```
> Task :assembleDebug
BUILD SUCCESSFUL in 5m 52s
37 actionable tasks: 15 executed, 22 up-to-date
```

Warnings are expected and harmless, e.g.:

* `warning: 'OffsetTileData' overrides a member function but is not marked 'override'`
* `[CXX5304] This version only understands SDK XML versions up to 3 ... version 4 was encountered`
  (only a mismatch between CMake and newer `cmdline-tools`)
* `Deprecated Gradle features were used ... incompatible with Gradle 9.0`

---

## Step 7 — Verify and install

```bat
dir build\outputs\apk\debug
```

You should see `android-debug.apk` (~58 MB for `arm64-v8a`) and `output-metadata.json`.

Install to a connected, USB-debugging-enabled device:

```bat
"%ANDROID_HOME%\platform-tools\adb.exe" devices
"%ANDROID_HOME%\platform-tools\adb.exe" install -r build\outputs\apk\debug\android-debug.apk
```

Launch it from the device's app drawer, or:

```bat
"%ANDROID_HOME%\platform-tools\adb.exe" shell am start -n io.urho3d/.UrhoActivity
"%ANDROID_HOME%\platform-tools\adb.exe" logcat -s Urho3D SDL
```

### Release variant

```bat
gradlew.bat assembleRelease
```

`android\build.gradle` sets `minifyEnabled true` but declares **no** `signingConfig`, so the
output in `build\outputs\apk\release\` is unsigned and cannot be installed until you sign it:

```bat
"%ANDROID_HOME%\build-tools\35.0.0\apksigner.bat" sign ^
  --ks my-release.jks --ks-key-alias mykey ^
  --out app-release-signed.apk build\outputs\apk\release\android-release-unsigned.apk
```

The debug build needs no signing — AGP signs it with the default debug keystore.

---

## Clean rebuild

```bat
cd /d "%PROJECT%\android"
gradlew.bat --stop
gradlew.bat clean
rmdir /s /q .cxx
gradlew.bat assembleDebug
```

Delete `%USERPROFILE%\.gradle\caches` only as a last resort — it re-downloads everything.

---

## Troubleshooting

| Symptom | Cause / fix |
| --- | --- |
| `'gradlew.bat' is not recognized` | The wrapper is git-ignored. Run Step 5. |
| `SDK location not found` | `ANDROID_HOME` not set in *this* window (or set with `setx` in a window opened before it). Re-run Step 3, or create `android\local.properties` with `sdk.dir=D\:\\Users\\you\\AppData\\Local\\Android\\Sdk`. |
| `Could not find a package configuration file provided by "Urho3D"` | `Urho3D_DIR` / `CMAKE_PREFIX_PATH` wrong. Must be `...\rbfx\CMake\Urho3D\share\Urho3D` (the folder containing `Urho3DConfig.cmake`), with forward slashes. |
| `package org.libsdl.app does not exist` / `SDLActivity` unresolved | `RBFX_ANDROID_JAVA_DIR` unset. See 3.1. Alternative: `mklink /J "%PROJECT%\rbfx" "%ENGINE%"` so the `../rbfx` default resolves. |
| `PackageTool not found. Build tools natively, set PACKAGE_TOOL_EXECUTABLE, or install Python3` | Install Python 3 or set `PACKAGE_TOOL_EXECUTABLE` (3.4). |
| `Missing required file: .../Plugins/Core.SamplePlugin/CMakeLists.txt` | Run `git submodule update --init --recursive` in `sample-project-android`. |
| `NDK not configured` / `No version of NDK matched the requested version` | Set `ANDROID_NDK_HOME` to an installed `ndk\<version>` folder, or add `ndkVersion '27.0.12077973'` inside `android { }`. |
| `error: undefined reference to ...` from Urho3D when linking the app | Something set `ci_lib_type=lib` (static). Clear it: `set "ci_lib_type="` and delete `android\.cxx`. |
| `java.lang.OutOfMemoryError: Java heap space` during `mergeDebugNativeLibs` | Raise `org.gradle.jvmargs` in `android\gradle.properties` (already `-Xmx4096m`; try `-Xmx8192m`). |
| `ninja: error: ... Filename longer than 260 characters` | Enable long paths (`reg add HKLM\SYSTEM\CurrentControlSet\Control\FileSystem /v LongPathsEnabled /t REG_DWORD /d 1 /f`, then reboot) or move `%ROOT%` closer to `C:\`. |
| Build succeeds but the app crashes on launch | `assets\CoreData` or `assets\ResourceRoot.ini` missing (Step 4), or the APK's ABI does not match the device — check `adb shell getprop ro.product.cpu.abi`. |
| CMake picks the wrong CMake / `CMake 3.25.0+ is required` | Install `cmake;3.31.6` via `sdkmanager` (Step 2) or remove older SDK CMake folders so AGP selects the newer one. |
| Stale/odd CMake errors after changing env vars | Incremental `.cxx` cache holds the old values: `gradlew.bat --stop`, `rmdir /s /q .cxx`, rebuild. |

---

## Reference — environment variables consumed by this build

| Variable | Read by | Purpose |
| --- | --- | --- |
| `ANDROID_HOME` / `ANDROID_SDK_ROOT` | AGP | SDK location |
| `ANDROID_NDK_HOME` | AGP | NDK pin |
| `JAVA_HOME` | `gradlew.bat` | JDK 17 for Gradle |
| `Urho3D_DIR` | `android\build.gradle` → `-DUrho3D_DIR=` | `find_package(Urho3D)` config dir (in-source engine build) |
| `CMAKE_PREFIX_PATH` | `android\build.gradle` → `-DCMAKE_PREFIX_PATH=` | same, as package prefix |
| `RBFX_ANDROID_JAVA_DIR` | `android\build.gradle` | `java.srcDirs` entry that supplies `org.libsdl.app.*` |
| `ci_arch` | `android\build.gradle` | `arm64` → `arm64-v8a`, `arm` → `armeabi-v7a`, `x64` → `x86_64`, `x86` → `x86`; unset → three ABIs |
| `ci_lib_type` | `android\build.gradle` | must be unset/empty so `BUILD_SHARED_LIBS=ON` |
| `PACKAGE_TOOL_EXECUTABLE` | rbfx CMake (`create_pak`) | host `PackageTool.exe` instead of the Python script |

Fixed values already baked into the project (no action needed):

* `compileSdk 35`, `targetSdk 35`, `minSdk 24`, `namespace io.urho3d`
* `-DANDROID_STL=c++_static -DANDROID_PLATFORM=android-21 -DANDROID_ARM_MODE=arm -DANDROID=1`
* `packaging.jniLibs.useLegacyPackaging true` — SDLActivity enumerates libs from
  `nativeLibraryDir`, so `.so` files must be extracted from the APK
* `org.gradle.jvmargs=-Xmx4096m -XX:MaxMetaspaceSize=768m`
