cmake_minimum_required(VERSION 3.28)
project(sif_port LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
set(FETCHCONTENT_QUIET ON)
# same pin as pt-pc (cmake/Dependencies.cmake:12); static so the port is one executable
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(sdl3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG release-3.4.16 GIT_SHALLOW TRUE)
# same pin and options as pt-pc (cmake/Dependencies.cmake:14-18); cmp\0zlib archives
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(zlib GIT_REPOSITORY https://github.com/madler/zlib.git GIT_TAG v1.3.2 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(sdl3 zlib)

# LZMA SDK decoder (public domain, see THIRD_PARTY.md); cmp\0lzma archives
add_library(lzmadec STATIC third_party/lzma/LzmaDec.c)
target_include_directories(lzmadec PUBLIC third_party/lzma)

# GLSL -> SPIR-V as C word lists (glslc -mfmt=c), included by the renderer
find_program(GLSLC glslc HINTS "$ENV{VULKAN_SDK}/Bin" "C:/VulkanSDK/1.4.363.0/Bin" REQUIRED)
set(SHADER_OUT ${CMAKE_BINARY_DIR}/shaders)
set(SHADER_INCS)
foreach(shader blit.vert blit.frag shape.frag mesh.vert mesh.frag ui.frag)
  add_custom_command(OUTPUT ${SHADER_OUT}/${shader}.inc
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUT}
    COMMAND ${GLSLC} -mfmt=c -O -o ${SHADER_OUT}/${shader}.inc ${CMAKE_SOURCE_DIR}/shaders/${shader}
    DEPENDS ${CMAKE_SOURCE_DIR}/shaders/${shader} VERBATIM)
  list(APPEND SHADER_INCS ${SHADER_OUT}/${shader}.inc)
endforeach()

# vgmstream clHCA (ISC, see THIRD_PARTY.md); built as-is, so upstream warnings are not ours to fix
add_library(clhca STATIC third_party/clhca/clhca.c)
target_include_directories(clhca PUBLIC third_party/clhca)
if(MSVC)
  target_compile_options(clhca PRIVATE /W0)
endif()

# stb_truetype (public domain / MIT, see THIRD_PARTY.md): UI font rasterizer; upstream warnings are not ours to fix
add_library(stb_truetype STATIC src/engine/ui/stb_truetype_impl.c)
target_include_directories(stb_truetype PUBLIC third_party/stb)
if(MSVC)
  target_compile_options(stb_truetype PRIVATE /W0)
endif()

add_library(sif_engine STATIC
  src/engine/audio/audio_output.cpp
  src/engine/audio/cri_utf.cpp
  src/engine/audio/cue_sheet.cpp
  src/engine/audio/decode.cpp
  src/engine/audio/sound_bank.cpp
  src/engine/core/log.cpp
  src/engine/core/crash_report.cpp
  src/engine/core/parallel_for.cpp
  src/engine/formats/blbn.cpp
  src/engine/formats/bmar.cpp
  src/engine/formats/bscm.cpp
  src/engine/formats/btx.cpp
  src/engine/formats/fad.cpp
  src/engine/fs/pac.cpp
  src/engine/fs/vfs.cpp
  src/engine/platform/os.cpp
  src/engine/platform/settings.cpp
  src/engine/render/renderer.cpp
  src/engine/render/scene_model.cpp
  src/engine/ui/ui_screen.cpp
  src/engine/ui/ui_text.cpp
  ${SHADER_INCS})
target_include_directories(sif_engine PUBLIC src PRIVATE ${SHADER_OUT})
target_link_libraries(sif_engine PUBLIC SDL3::SDL3-static zlibstatic lzmadec clhca stb_truetype)
target_compile_definitions(sif_engine PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
if(WIN32)
  target_link_libraries(sif_engine PUBLIC dbghelp)
endif()
if(MSVC)
  target_compile_options(sif_engine PRIVATE /W3 /utf-8 /EHsc)
endif()

# SIF game rules (live play), kept apart from the generic engine
add_library(sif_game STATIC
  src/game/live/chart.cpp
  src/game/live/judge.cpp
  src/game/live/song.cpp
  src/game/mission/mission.cpp
  src/game/save/save.cpp
  src/game/talk/talk.cpp
  src/game/text/messages.cpp)
target_link_libraries(sif_game PUBLIC sif_engine)
if(MSVC)
  target_compile_options(sif_game PRIVATE /W3 /utf-8 /EHsc)
endif()

add_executable(sif WIN32 src/app/main.cpp src/app/input.cpp src/app/live_screen.cpp src/app/options_screen.cpp src/app/result_screen.cpp src/app/song_select.cpp src/app/title_screen.cpp src/app/top_menu.cpp src/app/ui_viewer.cpp)
target_link_libraries(sif PRIVATE sif_game)
if(MSVC)
  target_compile_options(sif PRIVATE /utf-8 /EHsc)
endif()

add_executable(sif_log_test tests/unit/log_test.cpp)
target_link_libraries(sif_log_test PRIVATE sif_engine)

add_executable(sif_vfs_test tests/unit/vfs_test.cpp)
target_link_libraries(sif_vfs_test PRIVATE sif_engine)
if(MSVC)
  target_compile_options(sif_vfs_test PRIVATE /utf-8 /EHsc)
endif()

add_executable(sif_bmar_test tests/unit/bmar_test.cpp)
target_link_libraries(sif_bmar_test PRIVATE sif_engine)
if(MSVC)
  target_compile_options(sif_bmar_test PRIVATE /utf-8 /EHsc)
endif()

add_executable(sif_live_test tests/unit/live_test.cpp)
target_link_libraries(sif_live_test PRIVATE sif_game)
if(MSVC)
  target_compile_options(sif_live_test PRIVATE /utf-8 /EHsc)
endif()

add_executable(sif_mission_test tests/unit/mission_test.cpp)
target_link_libraries(sif_mission_test PRIVATE sif_game)
if(MSVC)
  target_compile_options(sif_mission_test PRIVATE /utf-8 /EHsc)
endif()

add_executable(sif_talk_test tests/unit/talk_test.cpp)
target_link_libraries(sif_talk_test PRIVATE sif_game)
if(MSVC)
  target_compile_options(sif_talk_test PRIVATE /utf-8 /EHsc)
endif()

add_executable(sif_message_test tests/unit/message_test.cpp)
target_link_libraries(sif_message_test PRIVATE sif_game)
if(MSVC)
  target_compile_options(sif_message_test PRIVATE /utf-8 /EHsc)
endif()

add_executable(sif_save_test tests/unit/save_test.cpp)
target_link_libraries(sif_save_test PRIVATE sif_game)
if(MSVC)
  target_compile_options(sif_save_test PRIVATE /utf-8 /EHsc)
endif()

add_executable(sif_fad_test tests/unit/fad_test.cpp)
target_link_libraries(sif_fad_test PRIVATE sif_engine)
if(MSVC)
  target_compile_options(sif_fad_test PRIVATE /utf-8 /EHsc)
endif()

add_executable(sif_input_test tests/unit/input_test.cpp src/app/input.cpp)
target_link_libraries(sif_input_test PRIVATE sif_engine)
if(MSVC)
  target_compile_options(sif_input_test PRIVATE /utf-8 /EHsc)
endif()

add_executable(sif_audio_test tests/unit/audio_test.cpp)
target_link_libraries(sif_audio_test PRIVATE sif_engine)
if(MSVC)
  target_compile_options(sif_audio_test PRIVATE /utf-8 /EHsc)
endif()

if(WIN32)
  # judgement-timing probe: WASAPI loopback sync + scripted key presses (M4 timing parity)
  add_executable(sif_sweep src/tools/sweep.cpp)
  target_link_libraries(sif_sweep PRIVATE sif_game ole32 winmm)
  if(MSVC)
    target_compile_options(sif_sweep PRIVATE /W3 /utf-8 /EHsc)
  endif()
endif()

add_executable(sif_hca2wav src/tools/hca2wav.cpp)
target_link_libraries(sif_hca2wav PRIVATE clhca)
if(MSVC)
  target_compile_options(sif_hca2wav PRIVATE /W3 /utf-8 /EHsc)
endif()
