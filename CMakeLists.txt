cmake_minimum_required(VERSION 3.30)

project(sdlcube LANGUAGES CXX)

# CPP Setup
set(CMAKE_CXX_STANDARD 20)
add_library(cxx_setup INTERFACE)
target_compile_features(cxx_setup INTERFACE cxx_std_20)
target_include_directories(cxx_setup INTERFACE ${PROJECT_SOURCE_DIR})
target_compile_options(cxx_setup INTERFACE -Wall -Wpedantic -Wextra)

# Expected installed libs
find_package(SDL3 REQUIRED)
find_package(SDL3_image REQUIRED)
find_package(glm REQUIRED)
find_package(simdjson REQUIRED)

add_subdirectory(${PROJECT_SOURCE_DIR}/thirdparty)
# Imgui
add_library(imgui)
target_include_directories(imgui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui")
target_sources(imgui PRIVATE 
"${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/imgui.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/imgui_demo.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/imgui_draw.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/imgui_tables.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/imgui_widgets.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/backends/imgui_impl_sdlgpu3.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/backends/imgui_impl_sdl3.cpp"
)

# Fastgltf
set(FASTGLTF_COMPILE_AS_CPP20 YES CACHE "" BOOL)
set(FASTGLTF_ENABLE_TESTS NO CACHE "" BOOL)
set(FASTGLTF_ENABLE_EXAMPLES NO CACHE "" BOOL)
set(FASTGLTF_ENABLE_DOCS NO CACHE "" BOOL)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/fastgltf")

target_link_libraries(imgui PUBLIC SDL3::SDL3)

# Spdlog
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE})
set(SPDLOG_BUILD_EXAMPLE OFF CACHE "" BOOL)
set(SPDLOG_BUILD_BENCH OFF CACHE "" BOOL)
set(SPDLOG_BUILD_TESTS OFF CACHE "" BOOL)
set(SPDLOG_BUILD_SHARED OFF CACHE "" BOOL)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/spdlog")

# SDL
# add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/SDL")

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
)
