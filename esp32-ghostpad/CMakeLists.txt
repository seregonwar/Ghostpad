cmake_minimum_required(VERSION 3.16)

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.private.defaults")
    set(SDKCONFIG_DEFAULTS sdkconfig.defaults sdkconfig.private.defaults)
else()
    set(SDKCONFIG_DEFAULTS sdkconfig.defaults)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ghostpad_bridge)
