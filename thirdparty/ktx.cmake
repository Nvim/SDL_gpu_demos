set(LIBKTX_FEATURE_GL_UPLOAD OFF CACHE BOOL "Disable ktx opengl texture upload")
set(LIBKTX_FEATURE_VK_UPLOAD ON CACHE BOOL "Enable ktx vulkan")
set(BASISU_SUPPORT_OPENCL OFF CACHE BOOL "Disable ktx opencl baseiu")
set(BASISU_SUPPORT_SSE OFF CACHE BOOL "Disable ktx baseiu sse")
set(KTX_FEATURE_DOCS OFF CACHE BOOL "Disable ktx docs")
set(KTX_FEATURE_TESTS OFF CACHE BOOL "Disable ktx unit tests")
set(KTX_FEATURE_JS OFF CACHE BOOL "Disable ktx js")
set(KTX_FEATURE_LOADTEST_APPS OFF CACHE BOOL "Disable ktx load tests")

# only add lib dir, skip other tools
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/ktx")
