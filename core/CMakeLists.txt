set(CORE_SOURCES
    src/logger.cpp
    src/config_manager.cpp
    src/error_handler.cpp
    src/event_dispatcher.cpp
    src/thread_pool.cpp
)

set(CORE_HEADERS
    include/dualddesk/core/logger.h
    include/dualddesk/core/config_manager.h
    include/dualddesk/core/error_handler.h
    include/dualddesk/core/event_dispatcher.h
    include/dualddesk/core/thread_pool.h
    include/dualddesk/core/types.h
)

add_library(DualDeskCore STATIC ${CORE_SOURCES} ${CORE_HEADERS})

target_include_directories(DualDeskCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(DualDeskCore
    PUBLIC
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        Threads::Threads
)

target_compile_definitions(DualDeskCore
    PUBLIC
        DUADDESK_CORE_EXPORTS
)

# Set properties for MSVC
if(MSVC)
    target_compile_options(DualDeskCore PRIVATE /W4)
endif()