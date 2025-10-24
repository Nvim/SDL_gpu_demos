include(stb.cmake)

add_library(mikktspace)
# set(CMAKE_C_CREATE_STATIC_LIBRARY ON)
# set(CMAKE_C_COMPILE_OBJECT ON)
target_include_directories(mikktspace PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/mikktspace")
target_sources(mikktspace PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/mikktspace/mikktspace.c")
target_link_libraries(mikktspace PRIVATE m)
