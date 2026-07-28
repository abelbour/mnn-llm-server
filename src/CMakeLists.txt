cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DCPPHTTPLIB_KEEPALIVE_TIMEOUT_SECOND=120)

# MNN root — use env var MNN_ROOT if set, else default to $HOME/MNN
if(DEFINED ENV{MNN_ROOT})
  set(MNN_ROOT "$ENV{MNN_ROOT}" CACHE PATH "MNN root directory")
else()
  set(MNN_ROOT "$ENV{HOME}/MNN" CACHE PATH "MNN root directory")
endif()

set(MNN_INCLUDE_DIR "${MNN_ROOT}/include" CACHE PATH "MNN include directory")
set(MNN_LLM_INCLUDE_DIR "${MNN_ROOT}/transformers/llm/engine/include" CACHE PATH "MNN LLM include directory")
set(MNN_BUILD_DIR "${MNN_ROOT}/build" CACHE PATH "MNN build directory")
set(CPPHTTPLIB_INCLUDE "" CACHE PATH "cpp-httplib include directory")
set(NLOHMANN_INCLUDE "" CACHE PATH "nlohmann json include directory")

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

# httplib fallback
if(NOT CPPHTTPLIB_INCLUDE OR NOT EXISTS "${CPPHTTPLIB_INCLUDE}/httplib.h")
  find_path(CPPHTTPLIB_INCLUDE httplib.h
    HINTS $ENV{HOME}/cpp-httplib /usr/local/include /usr/include
  )
  if(NOT CPPHTTPLIB_INCLUDE)
    message(STATUS "httplib not found, fetching from GitHub")
    include(FetchContent)
    FetchContent_Declare(httplib
      GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
      GIT_TAG v0.15.3
    )
    FetchContent_MakeAvailable(httplib)
    get_target_property(httplib_includes httplib INTERFACE_INCLUDE_DIRECTORIES)
    set(CPPHTTPLIB_INCLUDE "${httplib_includes}")
  endif()
endif()

# nlohmann json fallback
if(NOT NLOHMANN_INCLUDE OR NOT EXISTS "${NLOHMANN_INCLUDE}/nlohmann/json.hpp")
  find_path(NLOHMANN_INCLUDE nlohmann/json.hpp
    HINTS $ENV{HOME}/json /usr/local/include /usr/include
  )
  if(NOT NLOHMANN_INCLUDE)
    message(STATUS "nlohmann/json not found, fetching from GitHub")
    include(FetchContent)
    FetchContent_Declare(json
      GIT_REPOSITORY https://github.com/nlohmann/json.git
      GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(json)
    get_target_property(json_includes nlohmann_json::nlohmann_json INTERFACE_INCLUDE_DIRECTORIES)
    set(NLOHMANN_INCLUDE "${json_includes}")
  endif()
endif()

# Source files
set(SERVER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/server.cpp
)

set(CLI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

# Option: build as shared library (for Android) or executable (default)
option(BUILD_SHARED_LIB "Build as shared library instead of executable" OFF)

if(BUILD_SHARED_LIB)
    # Shared library for Android JNI
    add_library(mnn_server SHARED ${SERVER_SOURCES})
    target_include_directories(mnn_server PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${MNN_INCLUDE_DIR}
        ${MNN_LLM_INCLUDE_DIR}
        ${CPPHTTPLIB_INCLUDE}
        ${NLOHMANN_INCLUDE}
    )
    if(MNN_ROOT)
        target_link_directories(mnn_server PRIVATE ${MNN_BUILD_DIR})
    endif()
    target_link_libraries(mnn_server PRIVATE MNN)
    find_package(Threads REQUIRED)
    target_link_libraries(mnn_server PRIVATE Threads::Threads)
    target_compile_definitions(mnn_server PRIVATE BUILD_SHARED_LIB)
    target_compile_options(mnn_server PRIVATE -Wno-unused-result)

    # JNI for Android
    find_package(JNI)
    if(JNI_FOUND)
        target_include_directories(mnn_server PRIVATE ${JNI_INCLUDE_DIRS})
        target_link_libraries(mnn_server PRIVATE ${JNI_LIBRARIES})
    endif()
else()
    # CLI executable (default)
    add_executable(mnn_server ${CLI_SOURCES} ${SERVER_SOURCES})
    target_include_directories(mnn_server PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${MNN_INCLUDE_DIR}
        ${MNN_LLM_INCLUDE_DIR}
        ${CPPHTTPLIB_INCLUDE}
        ${NLOHMANN_INCLUDE}
    )
    if(MNN_ROOT)
        target_link_directories(mnn_server PRIVATE ${MNN_BUILD_DIR})
    endif()
    target_link_libraries(mnn_server PRIVATE MNN llm)
    find_package(Threads REQUIRED)
    target_link_libraries(mnn_server PRIVATE Threads::Threads)
    target_compile_options(mnn_server PRIVATE -Wno-unused-result)
endif()
