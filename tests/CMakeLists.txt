add_executable(tests)
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS 
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
  "${PROJECT_SOURCE_DIR}/src/common/*.cpp" # DON'T BUILD MAIN.CPP
)
target_sources(tests PRIVATE ${SOURCES})
target_include_directories(tests PUBLIC "${PROJECT_SOURCE_DIR}/thirdparty")
target_include_directories(tests PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_precompile_headers(tests PUBLIC "${PROJECT_SOURCE_DIR}/src/pch.h")

target_link_libraries(tests PRIVATE
  Catch2::Catch2WithMain
  SDL3::SDL3
  glm::glm 
  imgui
  fastgltf
  spdlog::spdlog
  stb::stb 
  cxx_setup
  mikktspace
  ktx
  # cxx_setup
)

include(CTest)
include("${PROJECT_SOURCE_DIR}/thirdparty/Catch2/extras/Catch.cmake")
catch_discover_tests(tests)
