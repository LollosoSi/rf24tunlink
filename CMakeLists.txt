cmake_minimum_required(VERSION 3.10)

set(CMAKE_BUILD_TYPE Debug) 

# Set some basic project attributes
project (rf24tunlink
	VERSION 0.1
	DESCRIPTION "A RF24 TUN Link")

cmake_minimum_required(VERSION 3.12)

add_compile_options(-Ofast -Wall) # passing the compiler a `-pthread` flag doesn't work here

# detect the CPU make and type
include(~/rf24libs/RF24/cmake/detectCPU.cmake) # sets the variable SOC accordingly

# auto-detect what driver to use
# auto-detect can be overriden using `cmake .. -D RF24_DRIVER=<supported driver>`
include(~/rf24libs/RF24/cmake/AutoConfig_RF24_DRIVER.cmake)

find_library(RF24 rf24 REQUIRED)
message(STATUS "using RF24 library: ${RF24}")

# conditionally append "interruptConfigure" to the EXAMPLES_LIST
if("${RF24_DRIVER}" STREQUAL "MRAA" OR "${RF24_DRIVER}" STREQUAL "wiringPi" OR "${LibPIGPIO}" STREQUAL "LibPIGPIO-NOTFOUND")
    message(STATUS "Skipping interruptConfigure.cpp example as it is incompatible with selected driver library")
else() # not using MRAA or wiringPi drivers (or pigpio lib was found)
    list(APPEND EXAMPLES_LIST interruptConfigure)
endif()


    #make a target
    
# This project will output an executable file
add_executable(${PROJECT_NAME} main.cpp settings/Settings.h settings/DefaultSettings.h interfaces/Messenger.h interfaces/PacketHandler.h structures/RadioPacket.h tun/TUNHandler.h tun/TUNHandler.cpp packetization/selectiverepeat/SelectiveRepeatPacketizer.h packetization/selectiverepeat/SelectiveRepeatPacketizer.cpp packetization/characterstuffing/CharacterStuffingPacketizer.h packetization/characterstuffing/CharacterStuffingPacketizer.cpp unit_tests/FakeRadio.h unit_tests/FakeRadio.cpp telemetry/Telemetry.h interfaces/RadioHandler.h radio/NRF24/RF24Radio.h radio/NRF24/RF24Radio.cpp radio/LORA/LoraRadio.h radio/LORA/LoraRadio.cpp utils.h)

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
