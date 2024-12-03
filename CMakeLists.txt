cmake_minimum_required(VERSION 3.15)
project(D-FOT)

set(CMAKE_CXX_STANDARD 11)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("-- Build type: Debug")
    add_compile_options(-g3 -O0)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message("-- Build type: Release")
    add_compile_options(-O3)
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    message("-- Build type: RelWithDebInfo")
    add_compile_options(-g -O2)
else()
    message("-- Build type: Unknown, defaulting to Release")
    add_compile_options(-O3)
endif()

add_compile_options(-fPIC -Wall -Wextra)

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
)

target_link_libraries(dfot kperf sym dl log4cplus boost_system boost_filesystem)
