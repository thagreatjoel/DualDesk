add_executable(driver_tester WIN32 driver_tester.cpp)

target_link_libraries(driver_tester
    PRIVATE
        user32
        kernel32
)