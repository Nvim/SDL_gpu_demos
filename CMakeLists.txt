cmake_minimum_required(VERSION 3.30)

project(sdl_demos LANGUAGES CXX C)

option(ENABLE_ASAN "Enable ASan" OFF)
option(BUILD_SDL "Build a vendored version of SDL3" OFF)

# CPP Setup
set(CMAKE_CXX_STANDARD 20)
add_library(cxx_setup INTERFACE)
target_compile_features(cxx_setup INTERFACE cxx_std_20)
target_include_directories(cxx_setup INTERFACE "${PROJECT_SOURCE_DIR}/src")
target_compile_options(cxx_setup INTERFACE -Wall -Wpedantic -Wextra)

# Expected installed libs
if (NOT BUILD_SDL)
  find_package(SDL3 REQUIRED)
endif()
find_package(glm REQUIRED)
find_package(simdjson REQUIRED)

# Add submodules directory
add_subdirectory(${PROJECT_SOURCE_DIR}/thirdparty)

# Project executable
add_executable(${PROJECT_NAME})
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty")

target_compile_definitions(${PROJECT_NAME} PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)

# Compile flags per configuration
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:-Wall -Wextra -Wpedantic -fno-omit-frame-pointer>
    $<$<CONFIG:Release>:-Wall -Wextra -Wpedantic>
    $<$<CONFIG:RelWithDebInfo>:-Wall -Wextra -Wpedantic>
)

if(ENABLE_ASAN)
    message(STATUS "ASan enabled")

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address)
    else()
        message(WARNING "ASan not supported on this compiler")
    endif()
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC
  SDL3::SDL3
  glm::glm 
  imgui
  fastgltf
  spdlog::spdlog
  stb::stb 
  cxx_setup
  mikktspace
  ktx
)

target_precompile_headers(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h")

# Tests:
include(CTest)
add_subdirectory(${PROJECT_SOURCE_DIR}/tests)
