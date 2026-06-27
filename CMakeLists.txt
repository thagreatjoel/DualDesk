cmake_minimum_required(VERSION 3.15)
project(DualDesk)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DNOMINMAX)
    add_definitions(-DUNICODE)
    add_definitions(-D_UNICODE)
endif()

# Core library
add_library(DualDeskCore STATIC
    core/src/logger.cpp
    core/src/display_manager.cpp
    core/src/window_tracker.cpp
)

target_include_directories(DualDeskCore
    PUBLIC
        core/include
)

# Main executable
add_executable(DualDesk WIN32
    app/src/main.cpp
)

target_link_libraries(DualDesk
    DualDeskCore
    user32
    kernel32
)