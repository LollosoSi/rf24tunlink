cmake_minimum_required(VERSION 3.12)

# Set some basic project attributes
project (rf24tunlink2
	VERSION 0.1
	DESCRIPTION "TUN Link Driver")

# Performance flags (do not use for debugging, obviously)
#add_compile_options(-Ofast -Wall -march=native -fno-signed-zeros -fno-trapping-math -frename-registers -funroll-loops) # passing the compiler a `-pthread` flag doesn't work here

#set(CMAKE_BUILD_TYPE Debug) 

# AddressSanitizer flags, only needed for debugging
#set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -g")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -g")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
#set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")

# detect the CPU make and type
include(~/rf24libs/RF24/cmake/detectCPU.cmake) # sets the variable SOC accordingly

# auto-detect what driver to use
# auto-detect can be overriden using `cmake .. -D RF24_DRIVER=<supported driver>`
include(~/rf24libs/RF24/cmake/AutoConfig_RF24_DRIVER.cmake)

find_library(RF24 rf24 REQUIRED)
message(STATUS "using RF24 library: ${RF24}")

# Include directories
include_directories(${CMAKE_SOURCE_DIR})

# Find the gpiod library
find_package(PkgConfig REQUIRED)
pkg_check_modules(GPIOD REQUIRED libgpiodcxx)

# Project files
set(rs_codec
    rs_codec/RSCodec.h
    rs_codec/RSCodec.cpp
    rs_codec/ReedSolomon/ReedSolomon.h
	rs_codec/ReedSolomon/ReedSolomon.cpp
)

set(settings
    Settings.h
)

set(generics
    generic_structures.h
    timer_handlers.h
)

set(activity_signals
	activity_signals/ActivitySignalInterface.h
	activity_signals/activity_led.h
	activity_signals/UdpActivity.h
)

set(dialogators
    system_dialogators/tun/TUNInterface.h
    system_dialogators/tun/TUNInterface.cpp
    system_dialogators/uart/UARTHandler.h
    packetizers/Packetizer.h
    packetizers/latency_evaluator/LatencyEvaluator.h
    packetizers/latency_evaluator/LatencyEvaluator.cpp
    radio/RadioInterface.h
)

set(radios
	radio/dualrf24/DualRF24.h
	radio/dualrf24/DualRF24.cpp
	radio/singlerf24/SingleRF24.h
	radio/singlerf24/SingleRF24.cpp
	radio/picorf24/PicoRF24.h
	radio/picorf24/PicoRF24.cpp
	radio/uartrf/UARTRF.h
	radio/uartrf/UARTRF.cpp
)

set(packetizers
	packetizers/harq/HARQ.h
	packetizers/harq/HARQ.cpp
	packetizers/arq/ARQ.h
	packetizers/arq/ARQ.cpp
	packetizers/characterstuffing/CharacterStuffing.h
	packetizers/characterstuffing/CharacterStuffing.cpp
)



set(sourcefiles
	utils.h
	crc.h
	${reedsolomon}
	${rs_codec}
	${settings}
	${generics}
	${dialogators}
	${radios}
	${packetizers}
	${activity_signals}
)

# This project will output an executable file
add_executable(${PROJECT_NAME} main.cpp ${sourcefiles})

# Link the gpiod library
target_link_libraries(${PROJECT_NAME} PRIVATE ${GPIOD_LIBRARIES})

# Include directories for gpiod
target_include_directories(${PROJECT_NAME} PRIVATE ${GPIOD_INCLUDE_DIRS})

# Create a simple configuration header
configure_file(config.h.in config.h)

# Include the configuration header in the build
target_include_directories(${PROJECT_NAME} PUBLIC "${PROJECT_BINARY_DIR}")
    
    # avoid including interrupt.h when pigpio is not available
    if("${LibPIGPIO}" STREQUAL "LibPIGPIO-NOTFOUND")
        target_compile_definitions(${PROJECT_NAME} PUBLIC RF24_NO_INTERRUPT)
    endif()

    # link the RF24 lib to the target. Notice we specify pthread as a linked lib here
    if("${RF24_DRIVER}" STREQUAL "MRAA")
        target_link_libraries(${PROJECT_NAME} PUBLIC ${RF24} pthread ${LibMRAA})
    elseif("${RF24_DRIVER}" STREQUAL "wiringPi")
        # wiringPi additionally needs to link to crypt and shm_open libraries
        target_link_libraries(${PROJECT_NAME} PUBLIC ${RF24} pthread ${LibWiringPi} crypt rt)
    elseif("${RF24_DRIVER}" STREQUAL "pigpio" OR NOT "${LibPIGPIO}" STREQUAL "LibPIGPIO-NOTFOUND")
        # linking to pigpio requires pthread to be listed as last linked lib
        target_link_libraries(${PROJECT_NAME} PUBLIC ${RF24} ${LibPIGPIO} pthread)
    else() # not using MRAA or wiringPi drivers
        target_link_libraries(${PROJECT_NAME} PUBLIC ${RF24} pthread)
    endif()

