
cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico_w CACHE STRING "Board type")
include(pico_sdk_import.cmake)
set(FREERTOS_KERNEL_PATH "C:/FreeRTOS-Kernel")
set(FREERTOS_KERNEL_PATH "C:/FreeRTOS-Kernel")
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

project(EstacaoEnchente_RTOS C CXX ASM)
# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(EstacaoEnchente_RTOS EstacaoEnchente_RTOS.c 
        lib/ssd1306.c
        lib/matriz_leds.c)

pico_set_program_name(EstacaoEnchente_RTOS "EstacaoEnchente_RTOS")
pico_set_program_version(EstacaoEnchente_RTOS "0.1")

# Generate PIO header
pico_generate_pio_header(EstacaoEnchente_RTOS ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(EstacaoEnchente_RTOS 0)
pico_enable_stdio_usb(EstacaoEnchente_RTOS 1)

# Add the standard library to the build
target_link_libraries(EstacaoEnchente_RTOS
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(EstacaoEnchente_RTOS PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/lib
)

# Add any user requested libraries
target_link_libraries(EstacaoEnchente_RTOS 
        hardware_pio
        pico_stdlib 
        hardware_gpio
        hardware_i2c
        hardware_pwm
        hardware_adc
        hardware_pio
        FreeRTOS-Kernel         # Kernel do FreeRTOS
        FreeRTOS-Kernel-Heap4   # Gerenciador de memoria
        )

pico_add_extra_outputs(EstacaoEnchente_RTOS)