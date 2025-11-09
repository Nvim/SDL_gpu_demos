include(stb.cmake)
include(ktx.cmake)
include(sdl.cmake)

# Imgui
add_library(imgui)
target_include_directories(imgui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/imgui")
target_sources(imgui PRIVATE 
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_demo.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_draw.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_tables.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_widgets.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_sdlgpu3.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_sdl3.cpp"
)

# Fastgltf
set(FASTGLTF_COMPILE_AS_CPP20 YES CACHE BOOL "Compile fastgltf as C++20" )
set(FASTGLTF_ENABLE_TESTS NO CACHE BOOL "Disable fastgltf tests" )
set(FASTGLTF_ENABLE_EXAMPLES NO CACHE BOOL "Disable fastgltf examples" )
set(FASTGLTF_ENABLE_DOCS NO CACHE BOOL "disable fastgltf docs" )
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/fastgltf")

target_link_libraries(imgui PUBLIC SDL3::SDL3)

# Spdlog
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "Disable SPDLOG example builds")
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "Disable SPDLOG benchmark builds")
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "Disable SPDLOG test builds")
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "Disable SPDLOG shared library builds")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/spdlog")

# mikktspace
add_library(mikktspace)
target_include_directories(mikktspace PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/mikktspace")
target_sources(mikktspace PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/mikktspace/mikktspace.c")
target_link_libraries(mikktspace PRIVATE m)
