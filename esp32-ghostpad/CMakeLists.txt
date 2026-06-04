cmake_minimum_required(VERSION 3.16)

# Use sdkconfig.defaults.<target> if it exists for the configured target,
# otherwise fall back to sdkconfig.defaults.
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig")
    file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig" _ghostpad_target_line
         REGEX "^CONFIG_IDF_TARGET=\"?([a-zA-Z0-9_]+)\"?$")
    if(_ghostpad_target_line MATCHES "CONFIG_IDF_TARGET=\"?([a-zA-Z0-9_]+)\"?")
        set(_ghostpad_target "${CMAKE_MATCH_1}")
        if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults.${_ghostpad_target}")
            set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults.${_ghostpad_target}")
        else()
            set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
        endif()
    else()
        set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
    endif()
else()
    set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
endif()

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.private.defaults")
    set(SDKCONFIG_DEFAULTS ${SDKCONFIG_DEFAULTS} "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.private.defaults")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ghostpad_bridge)
