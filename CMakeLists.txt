cmake_minimum_required(VERSION 3.16)
project(MathDuelRaylib CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(EMSCRIPTEN)
    message(STATUS "Платформа: WebAssembly (Emscripten)")
else()
    message(FATAL_ERROR "Этот CMakeLists настроен строго под EMSCRIPTEN. Используйте emcmake cmake ..")
endif()

include(FetchContent)
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
)

set(PLATFORM "Web" CACHE STRING "Сборка под HTML5")
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(raylib)

add_executable(game src/game.cpp)

target_link_libraries(game PRIVATE raylib)

set_target_properties(game PROPERTIES
    SUFFIX ".html"
    LINK_FLAGS "-sUSE_GLFW=3 -sASYNCIFY -sEXPORTED_FUNCTIONS=['_main','_StartGameSession','_RegisterGestureResult'] -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString'] --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/src/shell.html"
)

add_custom_command(
    TARGET game
    POST_BUILD
    COMMAND ${CMAKE_CXX_COMPILER} "${CMAKE_CURRENT_SOURCE_DIR}/src/hand_worker.js" -O2 -o "${CMAKE_CURRENT_BINARY_DIR}/hand_worker.js"
    COMMENT "Минификация hand_worker.js и копирование в директорию сборки..."
)
