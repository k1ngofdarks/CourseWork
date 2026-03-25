file(GLOB_RECURSE LIB_SOURCES "*.cpp")
list(FILTER LIB_SOURCES EXCLUDE REGEX "main.cpp$")
list(FILTER LIB_SOURCES EXCLUDE REGEX "logger_smoke.cpp$")

add_library(tsp_library OBJECT ${LIB_SOURCES})

target_include_directories(tsp_library PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_executable(tsp main.cpp $<TARGET_OBJECTS:tsp_library>)
add_executable(logger_smoke logger_smoke.cpp $<TARGET_OBJECTS:tsp_library>)

target_link_libraries(tsp PRIVATE tsp_library)
target_link_libraries(logger_smoke PRIVATE tsp_library)
