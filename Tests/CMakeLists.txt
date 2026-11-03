# /**
#  * @file    CMakeLists.txt
#  * @brief   Build and register the portable host test executable.
#  * @details Compiles the host runner with hardware-independent application modules.
#  *          Strict warnings are errors; generated UART examples stay in the build tree.
#  */
cmake_minimum_required(VERSION 3.22)
project(encoder_rpm_host_tests LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
enable_testing()

add_executable(encoder_tests
    test_main.c
    test_encoder_math.c
    test_speed_monitor.c
    test_telemetry.c
    test_scenarios.c
    ../App/Src/encoder_math.c
    ../App/Src/speed_monitor.c
    ../App/Src/telemetry.c
)
target_include_directories(encoder_tests PRIVATE ../App/Inc)
target_compile_definitions(encoder_tests PRIVATE
    UNIT_TEST
    UART_SAMPLE_OUTPUT_PATH="${CMAKE_CURRENT_BINARY_DIR}/uart_sample_output.txt"
)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(encoder_tests PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
        -Wdouble-promotion -Wcast-align -Wstrict-prototypes -Wmissing-prototypes
        -Werror
    )
elseif(MSVC)
    target_compile_options(encoder_tests PRIVATE /W4 /WX)
else()
    message(FATAL_ERROR "Unsupported warning options for this host compiler")
endif()

add_test(NAME encoder_rpm_host COMMAND encoder_tests)
