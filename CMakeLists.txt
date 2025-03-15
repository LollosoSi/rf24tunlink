cmake_minimum_required(VERSION 3.13)

# initialize pico-sdk from GIT
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_FETCH_FROM_GIT on)

include(pico_sdk_import.cmake)
include(lib/RF24/CMakeLists.txt)

project(usb_serial_example CXX C ASM)

pico_sdk_init()

if (TARGET tinyusb_device)
    add_executable(usb_serial_example
            main.cpp)

    # pull in common dependencies
    target_link_libraries(usb_serial_example RF24 pico_stdlib pico_multicore pico_sync hardware_spi hardware_gpio)
	target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})

    # enable usb output, disable uart output
    pico_enable_stdio_usb(usb_serial_example 1)
    pico_enable_stdio_uart(usb_serial_example 0)

    # create map/bin/hex/uf2 file etc.
    pico_add_extra_outputs(usb_serial_example)
    
elseif(PICO_ON_DEVICE)
    message(WARNING "not building usb_serial_example because TinyUSB submodule is not initialized in the SDK")
endif()
