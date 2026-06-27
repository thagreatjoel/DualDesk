cmake_minimum_required(VERSION 3.20)
project(DualDesk VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows specific
if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DNOMINMAX)
    add_definitions(-DUNICODE -D_UNICODE)
endif()

# Core library
add_library(DualDeskCore STATIC
    core/src/logger.cpp
)

target_include_directories(DualDeskCore
    PUBLIC
        core/include
)

target_compile_features(DualDeskCore PUBLIC cxx_std_20)

# Main executable
add_executable(DualDesk
    app/src/main.cpp
)

target_link_libraries(DualDesk
    DualDeskCore
    user32
    kernel32
)

# Set properties
set_target_properties(DualDesk PROPERTIES
    WIN32_EXECUTABLE TRUE
)