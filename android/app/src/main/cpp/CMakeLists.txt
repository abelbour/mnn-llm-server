cmake_minimum_required(VERSION 3.22.1)
project("mnn_server_native")

add_library(mnn_server SHARED
    server_jni.cpp
)

target_include_directories(mnn_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src
)

find_library(log-lib log)
target_link_libraries(mnn_server ${log-lib})
