cmake_minimum_required(VERSION 3.16)
project(mnn-server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DCPPHTTPLIB_KEEPALIVE_TIMEOUT_SECOND=120)

set(MNN_ROOT "/root/MNN" CACHE PATH "MNN root directory")
set(MNN_INCLUDE_DIR "${MNN_ROOT}/include" CACHE PATH "MNN include directory")
set(MNN_LLM_INCLUDE_DIR "${MNN_ROOT}/transformers/llm/engine/include" CACHE PATH "MNN LLM include directory")
set(MNN_BUILD_DIR "${MNN_ROOT}/build" CACHE PATH "MNN build directory")
set(CPPHTTPLIB_INCLUDE "" CACHE PATH "cpp-httplib include directory")

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${MNN_INCLUDE_DIR}
    ${MNN_LLM_INCLUDE_DIR}
    ${CPPHTTPLIB_INCLUDE}
)

link_directories(${MNN_BUILD_DIR})

add_executable(mnn-server main.cpp)

target_link_libraries(mnn-server
    MNN
    llm
    pthread
)

target_compile_options(mnn-server PRIVATE -Wno-unused-result)
