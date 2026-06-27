set(APP_SOURCES
    src/main.cpp
    src/application.cpp
)

set(APP_HEADERS
    include/dualddesk/application.h
    include/dualddesk/main.h
    include/dualddesk/version.h
)

add_executable(DualDesk WIN32 ${APP_SOURCES} ${APP_HEADERS})

target_include_directories(DualDesk
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(DualDesk
    PRIVATE
        DualDeskCore
)

# Link Windows libraries
target_link_libraries(DualDesk
    PRIVATE
        user32
        shell32
        advapi32
)

# Set application properties
set_target_properties(DualDesk PROPERTIES
    WIN32_EXECUTABLE TRUE
    VERSION ${PROJECT_VERSION}
)

# Copy runtime files
if(MSVC)
    add_custom_command(TARGET DualDesk POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:spdlog::spdlog>
            $<TARGET_FILE_DIR:DualDesk>
        COMMENT "Copying spdlog.dll"
    )
endif()