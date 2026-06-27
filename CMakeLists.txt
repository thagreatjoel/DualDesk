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

# Core library - ONLY ONE add_library!
add_library(DualDeskCore STATIC
    core/src/logger.cpp
    core/src/display_manager.cpp
    core/src/window_tracker.cpp
    core/src/input/input_device.cpp
    core/src/input/input_event.cpp
    core/src/input/input_manager.cpp
    core/src/input/raw_input_handler.cpp
    core/src/input/input_filter.cpp
    core/src/input/input_router.cpp
    core/src/input/input_translator.cpp
    core/src/input/virtual_input.cpp
    core/src/workspace/workspace.cpp
    core/src/workspace/workspace_manager.cpp
    core/src/workspace/window_mover.cpp
    core/src/workspace/window_snapper.cpp
)

target_include_directories(DualDeskCore
    PUBLIC
        core/include
)

target_link_libraries(DualDeskCore
    PUBLIC
        user32
        kernel32
        hid
        setupapi
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