# Building the SIF port (Windows)

This builds `sif.exe`, the PC port of "Love Live! School Idol Festival ~after school ACTIVITY~ Wai-Wai!Home Meeting!!" (PS4, CUSA24620). The build contains no game data. To play, you also need your own dump of the game; see [PLAY.md](PLAY.md).

## What you need

| Tool | Notes |
|---|---|
| Windows 10 or 11, 64-bit, with a GPU and driver that support Vulkan | The renderer uses SDL3's GPU API on Vulkan |
| **Visual Studio 2026** (Community is enough) | Workload **Desktop development with C++**, plus the component **C++ Clang Compiler for Windows** (`clang-cl`). The workload brings CMake (3.28 or newer) and Ninja |
| **Vulkan SDK** (LunarG) | Only its shader compiler `glslc` is used. The installer sets `VULKAN_SDK`, which the build uses to find it |
| **Git**, and internet access for the first configure | CMake downloads SDL3 (`release-3.4.16`) and zlib (`v1.3.2`) from GitHub |
| Python 3.14 (optional) | Only for the research tools in `tools/`, not for the game |

## Steps

1. Get the code:

   ```bash
   git clone https://github.com/alfarell/love-live-sif-pc.git
   ```

2. Open **x64 Native Tools Command Prompt for VS 2026** from the Start menu, so `clang-cl`, CMake and Ninja are on `PATH`, and go to the `repository` folder of the clone:

   ```bash
   cd love-live-sif-pc\repository
   ```

3. Configure and build the release version:

   ```bash
   cmake --preset release
   ```

   ```bash
   cmake --build ../work/build/release --target sif
   ```

   The result is `love-live-sif-pc\work\build\release\sif.exe`: one file, with SDL3, zlib and the shaders built in.

The `debug` preset works the same way (`cmake --preset debug`, then `cmake --build ../work/build/debug`). Leaving out `--target sif` also builds the unit tests and tools.

## Tests (optional)

Each test is a program in the build folder. The ones that read the game take your dump folder as an argument, for example:

```bash
..\work\build\debug\sif_live_test.exe "D:\Games\PS4\Love Live School Idol Festival\CUSA24620"
```

Each prints `… ok` or the failed checks. `sif_video_test` also takes `--all` after the folder, which decodes every picture of every movie (slow in Debug).

## When it fails

| Message | Cause and fix |
|---|---|
| `Could not find program glslc` / `GLSLC-NOTFOUND` | The Vulkan SDK is missing, or the prompt was opened before installing it. Install it and open a new prompt |
| `CMAKE_C_COMPILER: clang-cl is not a full path and was not found` | Not the "x64 Native Tools" prompt, or the Clang component is missing in the Visual Studio Installer |
| Odd compiler or linker errors that mention `gcc`, `mingw` or `w64devkit` | Another compiler (MinGW, w64devkit, Strawberry Perl …) comes first on `PATH`. Remove it from `PATH` for this prompt, delete `..\work\build\<preset>`, and configure again |
| `Failed to clone repository … SDL` or `zlib` | No Git, or no internet during the first configure |
| `'vswhere.exe' is not recognized …` when the prompt opens | Harmless: Visual Studio's prompt script looks for an optional helper. The build still works |
| Changed tools, or the build folder is in a strange state | Delete `..\work\build\<preset>` and run both steps again |

The build writes only inside the clone (`work\build\`). Nothing is installed system-wide.
