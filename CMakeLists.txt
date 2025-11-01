cmake_minimum_required(VERSION 3.30)

project(sdlcube LANGUAGES CXX C)

# CPP Setup
set(CMAKE_CXX_STANDARD 20)
add_library(cxx_setup INTERFACE)
target_compile_features(cxx_setup INTERFACE cxx_std_20)
target_include_directories(cxx_setup INTERFACE "${PROJECT_SOURCE_DIR}/src")
target_compile_options(cxx_setup INTERFACE -Wall -Wpedantic -Wextra)

# Expected installed libs
find_package(SDL3 REQUIRED)
find_package(SDL3_image REQUIRED)
find_package(glm REQUIRED)
find_package(simdjson REQUIRED)

add_subdirectory(${PROJECT_SOURCE_DIR}/thirdparty)

# Project executable
add_executable(${PROJECT_NAME})
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty")

target_compile_definitions(${PROJECT_NAME} PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)

target_link_libraries(${PROJECT_NAME} PUBLIC
  SDL3_image::SDL3_image 
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
