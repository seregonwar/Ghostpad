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

# Download IconsFontAwesome6.h and fa-solid-900.ttf
if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/IconsFontAwesome6.h")
  message(STATUS "Downloading IconsFontAwesome6.h...")
  file(DOWNLOAD "https://raw.githubusercontent.com/juliettef/IconFontCppHeaders/main/IconsFontAwesome6.h"
       "${CMAKE_CURRENT_BINARY_DIR}/IconsFontAwesome6.h" SHOW_PROGRESS)
endif()

if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/fa-solid-900.ttf")
  message(STATUS "Downloading fa-solid-900.ttf...")
  file(DOWNLOAD "https://github.com/FortAwesome/Font-Awesome/raw/6.5.1/webfonts/fa-solid-900.ttf"
       "${CMAKE_CURRENT_BINARY_DIR}/fa-solid-900.ttf" SHOW_PROGRESS)
endif()

if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/stb_image.h")
  message(STATUS "Downloading stb_image.h...")
  file(DOWNLOAD "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h"
       "${CMAKE_CURRENT_BINARY_DIR}/stb_image.h" SHOW_PROGRESS)
endif()

# Generate embedded headers using Python
find_package(Python3 REQUIRED COMPONENTS Interpreter)
message(STATUS "Running embed_assets.py to generate header files...")
execute_process(
  COMMAND ${Python3_EXECUTABLE} 
          "${CMAKE_CURRENT_SOURCE_DIR}/scripts/embed_assets.py"
          "${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf"
          "${CMAKE_CURRENT_BINARY_DIR}/fa-solid-900.ttf"
          "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/dualsense_solid_black.png"
          "${CMAKE_CURRENT_BINARY_DIR}/generated_assets"
  RESULT_VARIABLE embed_result
)
if(NOT embed_result EQUAL 0)
  message(FATAL_ERROR "Failed to run embed_assets.py to generate header files")
endif()

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
  src/storage/profile_store.cpp
  src/storage/project_store.cpp
  src/input/keyboard_input.cpp
  src/input/gamepad_input.cpp
  src/input/macro_engine.cpp
  src/ui/gif_export.cpp
  ${IMGUI_SOURCES}
)

add_executable(ghostpad-native ${SOURCES})
target_include_directories(ghostpad-native PRIVATE
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/generated_assets
  src
)
target_compile_definitions(ghostpad-native PRIVATE
  IMGUI_DEFINE_MATH_OPERATORS
)
target_link_libraries(ghostpad-native PRIVATE glfw OpenGL::GL nlohmann_json::nlohmann_json)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/ghostpad-settings.json
  ${CMAKE_CURRENT_BINARY_DIR}/ghostpad-settings.json
  COPYONLY
)

# Compiler warning flags
if(MSVC)
  target_compile_options(ghostpad-native PRIVATE /W4)
else()
  target_compile_options(ghostpad-native PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
  target_link_libraries(ghostpad-native PRIVATE ws2_32 iphlpapi)
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
  set(RESOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/ghostpad-settings.json
  )
  set_source_files_properties(${RESOURCE_FILES} PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources
  )
  target_sources(ghostpad-native PRIVATE ${RESOURCE_FILES})
endif()

# ============================================================================
#                                 TESTS TARGET
# ============================================================================

set(TEST_SOURCES
  src/tests/test_main.cpp
  src/storage/console_store.cpp
  src/storage/settings_store.cpp
  src/storage/profile_store.cpp
  src/storage/project_store.cpp
  src/network/network_scanner.cpp
)

add_executable(ghostpad-tests ${TEST_SOURCES})
target_include_directories(ghostpad-tests PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  src
)
set_target_properties(ghostpad-tests PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)
target_link_libraries(ghostpad-tests PRIVATE nlohmann_json::nlohmann_json)

if(MSVC)
  target_compile_options(ghostpad-tests PRIVATE /W4)
else()
  target_compile_options(ghostpad-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
  target_link_libraries(ghostpad-tests PRIVATE ws2_32 iphlpapi)
endif()

