cmake_minimum_required(VERSION 3.20)
project(GhostpadNative VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# GLFW
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
  GIT_SHALLOW TRUE
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Dear ImGui
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG docking
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# nlohmann/json
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(json)

find_package(OpenGL REQUIRED)

set(SOURCES
  src/main.cpp
  src/ui/app.cpp
  src/ui/home_screen.cpp
  src/ui/consoles_screen.cpp
  src/ui/settings_screen.cpp
  src/ui/beeper_screen.cpp
  src/ui/systemstate_screen.cpp
  src/ui/projects_screen.cpp
  src/ui/input_redirect_screen.cpp
  src/ui/controller_screen.cpp
  src/ui/credits_screen.cpp
  src/ui/pad_visualizer.cpp
  src/network/ghostpad_client.cpp
  src/network/beeper_client.cpp
  src/network/ssm_client.cpp
  src/network/payload_deploy.cpp
  src/network/network_scanner.cpp
  src/storage/console_store.cpp
  src/storage/settings_store.cpp
  src/storage/project_store.cpp
  src/input/keyboard_input.cpp
  src/input/gamepad_input.cpp
  src/input/macro_engine.cpp
  ${IMGUI_SOURCES}
)

add_executable(ghostpad-native ${SOURCES})
target_include_directories(ghostpad-native PRIVATE
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
  src
)
target_compile_definitions(ghostpad-native PRIVATE IMGUI_DEFINE_MATH_OPERATORS)
target_link_libraries(ghostpad-native PRIVATE glfw OpenGL::GL nlohmann_json::nlohmann_json)

if(WIN32)
  target_link_libraries(ghostpad-native PRIVATE ws2_32)
  target_sources(ghostpad-native PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp)
elseif(APPLE)
  find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
  find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
  find_library(COREVIDEO_FRAMEWORK CoreVideo REQUIRED)
  target_link_libraries(ghostpad-native PRIVATE ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK} ${COREVIDEO_FRAMEWORK})
endif()

if(APPLE)
  set_target_properties(ghostpad-native PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.ghostpad.native"
    MACOSX_BUNDLE_BUNDLE_NAME "Ghostpad Native"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
  )
endif()
