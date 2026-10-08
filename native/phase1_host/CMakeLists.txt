cmake_minimum_required(VERSION 3.24)
project(cordel_phase1_host VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# This backend is an experiment, not a CORDEL production dependency decision.
# SDL remains in the build tree; URL_HASH pins the exact source archive.
include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
set(SDL_OFFSCREEN ON CACHE BOOL "" FORCE)
option(CORDEL_OFFSCREEN_ONLY "Build without desktop development packages" OFF)
if(CORDEL_OFFSCREEN_ONLY)
  set(SDL_UNIX_CONSOLE_BUILD ON CACHE BOOL "No visible desktop in reference environment" FORCE)
  set(SDL_X11 OFF CACHE BOOL "" FORCE)
  set(SDL_WAYLAND OFF CACHE BOOL "" FORCE)
  set(SDL_KMSDRM OFF CACHE BOOL "" FORCE)
  # SDL itself includes SDL_opengl.h. Its Linux feature probe instead expects
  # system GL/gl.h, which this minimal environment lacks. All host GL symbols
  # are loaded dynamically; EGL uses SDL's bundled Khronos declarations.
  set(HAVE_OPENGL TRUE CACHE BOOL "Use SDL's bundled OpenGL declarations" FORCE)
endif()
FetchContent_Declare(SDL3
  URL https://codeload.github.com/libsdl-org/SDL/tar.gz/d9d5536704d585616d4db3c8ba3c4ff6fc2757e1
  URL_HASH SHA256=44b617821ea70cb6515a05f962d4133a5e32c1011c98a2f1daea01a187279a43
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(SDL3)

add_library(cordel_logic INTERFACE)
target_include_directories(cordel_logic INTERFACE include)
add_library(cordel_scene src/scene/scene.cpp)
target_link_libraries(cordel_scene PUBLIC cordel_logic)
target_include_directories(cordel_scene PRIVATE third_party)
add_executable(cordel-native-host
  src/main.cpp src/platform/platform.cpp src/render/renderer.cpp
  src/diagnostics/diagnostics.cpp src/runtime.cpp src/self_test.cpp)
target_link_libraries(cordel-native-host PRIVATE cordel_scene SDL3::SDL3-static)
target_include_directories(cordel-native-host PRIVATE tests)
target_compile_definitions(cordel-native-host PRIVATE
  CORDEL_DEFAULT_SCENE="${CMAKE_CURRENT_SOURCE_DIR}/../../examples/cordel_viewport/game/assets/cordel_scene.gltf")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(cordel-native-host PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
  target_compile_options(cordel_scene PRIVATE -Wall -Wextra -Wpedantic)
endif()
include(CTest)
if(BUILD_TESTING)
  add_executable(cordel-native-tests tests/test_logic.cpp)
  target_link_libraries(cordel-native-tests PRIVATE cordel_scene)
  target_compile_definitions(cordel-native-tests PRIVATE
    CORDEL_DEFAULT_SCENE="${CMAKE_CURRENT_SOURCE_DIR}/../../examples/cordel_viewport/game/assets/cordel_scene.gltf")
  add_test(NAME native_logic COMMAND cordel-native-tests)
  # Python is required only for the cross-reference test, never for the host.
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_test(NAME shared_python_goldens COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/goldens.py --check)
endif()

# Phase 1.3 optional narrative boundary. No interpreter is linked into the host.
find_package(Threads REQUIRED)
add_library(cordel_narrative
  src/narrative/json.cpp src/narrative/protocol.cpp src/narrative/client.cpp)
target_link_libraries(cordel_narrative PUBLIC cordel_logic Threads::Threads)
target_sources(cordel-native-host PRIVATE src/narrative/session.cpp src/narrative/experiment.cpp)
target_link_libraries(cordel-native-host PRIVATE cordel_narrative)
target_compile_definitions(cordel-native-host PRIVATE
  CORDEL_NARRATIVE_SCHEMA="${CMAKE_CURRENT_SOURCE_DIR}/../../narrative/phase1_adapter/protocol/schema.json"
  CORDEL_NARRATIVE_BOOTSTRAP="${CMAKE_CURRENT_SOURCE_DIR}/../../narrative/phase1_adapter/worker/bootstrap.py"
  CORDEL_NARRATIVE_PYTHON="${CMAKE_CURRENT_SOURCE_DIR}/../../lib/py3-linux-x86_64/python")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(cordel_narrative PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()
if(BUILD_TESTING)
  add_executable(cordel-protocol-tests tests/test_protocol.cpp)
  target_link_libraries(cordel-protocol-tests PRIVATE cordel_narrative)
  target_compile_definitions(cordel-protocol-tests PRIVATE
    CORDEL_NARRATIVE_SCHEMA="${CMAKE_CURRENT_SOURCE_DIR}/../../narrative/phase1_adapter/protocol/schema.json")
  add_test(NAME narrative_protocol COMMAND cordel-protocol-tests)
  add_test(NAME narrative_python_protocol COMMAND ${Python3_EXECUTABLE} -m unittest discover
    -s ${CMAKE_CURRENT_SOURCE_DIR}/../../narrative/phase1_adapter/tests -p test_protocol.py -v)
endif()
