cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico2)
set(PICO_PLATFORM rp2350-arm-s)
# set(PICO_PLATFORM rp2350-riscv)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(hstx_sdr C CXX ASM)
pico_sdk_init()

# Internal and external includes
include_directories(
    ${PROJECT_SOURCE_DIR}/include
)

# Upload code
install(CODE "execute_process(
    COMMAND openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c \"adapter speed 5000; program application.elf verify reset exit\"
)")

# Reset RP2040 without programming
add_custom_target(reset
    COMMAND openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c \"init\; reset halt\; rp2040.core1 arp_reset assert 0\; rp2040.core0 arp_reset assert 0\; exit\"
    COMMENT "Resetting"
)

add_executable(application
    src/main.c
    src/inv_modulator.c
)

pico_enable_stdio_usb(application 0)
pico_enable_stdio_uart(application 0)

target_link_libraries(application
    pico_stdlib
    hardware_dma
    hardware_pio
    hardware_structs
)

# Turn on basically all useful warnings
target_compile_options(application PUBLIC
    -Wall
    -Wextra
    -fstack-usage
)

pico_add_extra_outputs(application)
pico_generate_pio_header(application ${PROJECT_SOURCE_DIR}/src/inv_modulator.pio)
