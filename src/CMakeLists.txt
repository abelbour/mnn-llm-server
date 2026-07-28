cmake_minimum_required(VERSION 3.16)
project(mnn-server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DCPPHTTPLIB_KEEPALIVE_TIMEOUT_SECOND=120)

# MNN root — use env var MNN_ROOT if set, else default to /root/MNN
if(DEFINED ENV{MNN_ROOT})
  set(MNN_ROOT "$ENV{MNN_ROOT}" CACHE PATH "MNN root directory")
else()
  set(MNN_ROOT "/root/MNN" CACHE PATH "MNN root directory")
endif()

set(MNN_INCLUDE_DIR "${MNN_ROOT}/include" CACHE PATH "MNN include directory")
set(MNN_LLM_INCLUDE_DIR "${MNN_ROOT}/transformers/llm/engine/include" CACHE PATH "MNN LLM include directory")
set(MNN_BUILD_DIR "${MNN_ROOT}/build" CACHE PATH "MNN build directory")
set(CPPHTTPLIB_INCLUDE "" CACHE PATH "cpp-httplib include directory")
set(NLOHMANN_INCLUDE "" CACHE PATH "nlohmann json include directory")

# Auto-discover httplib and nlohmann if paths are empty
if(NOT CPPHTTPLIB_INCLUDE OR CPPHTTPLIB_INCLUDE STREQUAL "")
  find_path(CPPHTTPLIB_INCLUDE httplib.h)
endif()
if(NOT NLOHMANN_INCLUDE OR NLOHMANN_INCLUDE STREQUAL "")
  find_path(NLOHMANN_INCLUDE nlohmann/json.hpp)
endif()

# Fallback discovery if the default paths don't exist
if(NOT EXISTS "${MNN_INCLUDE_DIR}/MNN/MNNForwardType.h")
  find_path(MNN_INCLUDE_DIR MNN/MNNForwardType.h)
endif()
if(NOT EXISTS "${MNN_LLM_INCLUDE_DIR}/llm/llm.hpp")
  find_path(MNN_LLM_INCLUDE_DIR llm/llm.hpp)
endif()
if(NOT EXISTS "${MNN_BUILD_DIR}/libMNN.so" AND NOT EXISTS "${MNN_BUILD_DIR}/libMNN.a")
  find_library(MNN_LIBRARY MNN)
  find_library(LLM_LIBRARY llm)
  if(MNN_LIBRARY)
    get_filename_component(MNN_BUILD_DIR "${MNN_LIBRARY}" DIRECTORY)
  endif()
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${MNN_INCLUDE_DIR}
    ${MNN_LLM_INCLUDE_DIR}
    ${CPPHTTPLIB_INCLUDE}
    ${NLOHMANN_INCLUDE}
)

link_directories(${MNN_BUILD_DIR})

add_executable(mnn-server main.cpp)

target_link_libraries(mnn-server
    MNN
    llm
    pthread
)

target_compile_options(mnn-server PRIVATE -Wno-unused-result)
