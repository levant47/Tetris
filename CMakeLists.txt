cmake_minimum_required(VERSION 3.24)
project(tetris)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-write-strings")

link_directories("${PROJECT_SOURCE_DIR}\\lib")

include_directories("${PROJECT_SOURCE_DIR}")

add_executable(tetris src/main.cpp)

target_link_libraries(tetris SDL2main.lib SDL2.lib SDL2_ttf.lib)

# copy DLLs from lib into output
add_custom_command(TARGET tetris POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}\\lib\\SDL2.dll" $<TARGET_FILE_DIR:tetris>)
add_custom_command(TARGET tetris POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}\\lib\\SDL2_ttf.dll" $<TARGET_FILE_DIR:tetris>)

# copy resources from res into output
add_custom_command(TARGET tetris POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}\\res" $<TARGET_FILE_DIR:tetris>\\res)
