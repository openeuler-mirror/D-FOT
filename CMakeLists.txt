cmake_minimum_required(VERSION 3.15)
project(D-FOT)

set(CMAKE_CXX_STANDARD 11)

option(WITH_DEBUG "debug mode" OFF)

if (WITH_DEBUG)
    message("-- Note: debug mode")
    add_compile_options(-g3 -O0)
endif()

add_compile_options(-O2 -fPIC -Wall -Wextra)

# libkperf
message("-- libkperf library path: ${LIB_KPERF_LIBPATH}")
message("-- libkperf include path: ${LIB_KPERF_INCPATH}")

set(dfot_tuner_sysboost_src
    src/oeaware_plugins/instance.cc
    src/oeaware_plugins/tuner_sysboost.cc
    src/configs.cc
    src/logs.cc
    src/records.cc
    src/utils.cc
    src/startup_opt.cc
)

add_library(dfot SHARED ${dfot_tuner_sysboost_src})

include_directories(dfot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LIB_KPERF_INCPATH}
)

target_link_libraries(dfot kperf sym dl log4cplus boost_system boost_filesystem)
