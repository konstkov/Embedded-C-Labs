# Minimum required CMake version
cmake_minimum_required(VERSION 3.13)


# Pull in the Pico SDK (must be after setting CMake version but before project())
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Set board type (pico or pico_w)
set(PICO_BOARD pico_w)

# Set the project name and languages
project(blink C CXX ASM)

# Set C and C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK (must come after project())
pico_sdk_init()

# Optional: compiler warnings
add_compile_options(
        -Wall
        -Wno-format
        -Wno-unused-function
        -Wno-maybe-uninitialized
)

# Add your source file
add_executable(${PROJECT_NAME}
        main.c
)

# Link standard SDK libraries
target_link_libraries(${PROJECT_NAME}
        pico_stdlib
        hardware_pwm
        hardware_gpio
)

# Enable UART output, disable USB output
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)

# Generate additional output formats (UF2, bin, etc.)
pico_add_extra_outputs(${PROJECT_NAME})

