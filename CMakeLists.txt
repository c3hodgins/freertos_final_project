cmake_minimum_required(VERSION 3.14)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD pico2 CACHE STRING "Board type")

if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()

set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()

if( NOT DEFINED PICO_SDK_PATH )
	set( PICO_SDK_PATH ${USERHOME}/.pico-sdk )
endif()

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(programming_assignment_3 C CXX ASM)

pico_sdk_init()

set(FREERTOS_KERNEL_PATH "${USERHOME}/.FreeRTOS-Kernel")
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2350_ARM_NTZ/FreeRTOS_Kernel_import.cmake)

add_executable(programming_assignment_3 programming-assignment-3.c )

pico_enable_stdio_uart(programming_assignment_3 0)
pico_enable_stdio_usb(programming_assignment_3 1)

target_link_libraries(programming_assignment_3
    FreeRTOS-Kernel-Heap4
    #FreeRTOS-Kernel-Static
    hardware_adc
    pico_stdlib)

target_include_directories(programming_assignment_3 PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(programming_assignment_3)