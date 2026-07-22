cmake_minimum_required(VERSION 3.15)
project(DualDesk)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DNOMINMAX)
    add_definitions(-DUNICODE)
    add_definitions(-D_UNICODE)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# ============================================================
# Core library
# ============================================================
add_library(DualDeskCore STATIC
    core/src/logger.cpp
    core/src/display_manager.cpp
    core/src/window_tracker.cpp
    core/src/driver_interface.cpp
    core/src/input/input_device.cpp
    core/src/input/input_event.cpp
    core/src/input/input_manager.cpp
    core/src/input/raw_input_handler.cpp
    core/src/input/input_filter.cpp
    core/src/input/input_router.cpp
    core/src/input/input_translator.cpp
    core/src/input/virtual_input.cpp
    core/src/input/eithermouse_plugin.cpp
    core/src/workspace/workspace.cpp
    core/src/workspace/workspace_manager.cpp
    core/src/workspace/window_mover.cpp
    core/src/workspace/window_snapper.cpp
    core/src/cursor/cursor_emulator.cpp
    core/src/cursor/virtual_cursor.cpp
    core/src/display/monitor_border.cpp
)

target_include_directories(DualDeskCore
    PUBLIC
        core/include
        app/include
)

target_link_libraries(DualDeskCore
    PUBLIC
        user32
        kernel32
        hid
        setupapi
        comctl32
        shell32
        gdi32
)

# ============================================================
# DualDesk Dashboard (main.cpp only)
# ============================================================
add_executable(DualDesk WIN32
    app/src/main.cpp
)

target_include_directories(DualDesk
    PRIVATE
        app/include
        core/include
)

target_link_libraries(DualDesk
    PRIVATE
        DualDeskCore
        user32
        kernel32
        comctl32
        shell32
        gdi32
)

set_target_properties(DualDesk PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# ============================================================
# DualMouse Standalone (mouse.cpp only)
# ============================================================
add_executable(DualMouse WIN32
    app/src/mouse.cpp
)

target_include_directories(DualMouse PRIVATE
    core/include
)

target_link_libraries(DualMouse
    PRIVATE
        user32
        gdi32
        comctl32
)

# Add this to the end of CMakeLists.txt

# Copy EitherMouse.exe to build directory
if(EXISTS "${CMAKE_SOURCE_DIR}/EitherMouse.exe")
    configure_file(
        "${CMAKE_SOURCE_DIR}/EitherMouse.exe"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/EitherMouse.exe"
        COPYONLY
    )
    message(STATUS "EitherMouse.exe copied to build directory")
else()
    message(WARNING "EitherMouse.exe not found in root directory")
endif()