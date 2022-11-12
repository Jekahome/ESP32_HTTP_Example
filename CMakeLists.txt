# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
FILE(GLOB_RECURSE partitions ${CMAKE_SOURCE_DIR}/main/partitions/partitions.csv)
set(SDKCONFIG_DEFAULTS ${partitions})
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")


include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Esp32_wifi)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -Wno-unused-variable -Wno-unused-parameter -Wno-unused-but-set-variable -Werror=unused-label")
 
