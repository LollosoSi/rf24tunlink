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


set(rs_codec
    rs_codec/RSCodec.h
    rs_codec/RSCodec.cpp
)
set(reedsolomon
    ReedSolomon/ReedSolomon.h
	ReedSolomon/ReedSolomon.cpp
)
set(settings
    settings/SettingsFileReader.h
    settings/Settings.h
    settings/DefaultSettings.h
)
set(interfaces
    interfaces/RadioHandler.h
    interfaces/Messenger.h
    interfaces/PacketHandler.h
    interfaces/UARTHandler.h
)
set(structures
    structures/RadioPacket.h
)
set(tun
    tun/TUNHandler.h
    tun/TUNHandler.cpp
)
set(packetizers
    packetization/selectiverepeat/SelectiveRepeatPacketizer.h
    packetization/selectiverepeat/SelectiveRepeatPacketizer.cpp
    packetization/selectiverepeat_rs/RSSelectiveRepeatPacketizer.h
    packetization/selectiverepeat_rs/RSSelectiveRepeatPacketizer.cpp
    packetization/characterstuffing/CharacterStuffingPacketizer.h
    packetization/characterstuffing/CharacterStuffingPacketizer.cpp
    packetization/throughputtester/ThroughputTester.h
    packetization/throughputtester/ThroughputTester.cpp
    packetization/harq/HARQ.h
    packetization/harq/HARQ.cpp
)
set(unit_tests
    unit_tests/FakeRadio.h
    unit_tests/FakeRadio.cpp
)
set(telemetry
    telemetry/Telemetry.h
    telemetry/TelemetryPrinter.h
    telemetry/TelemetryPrinter.cpp
    telemetry/TimeTelemetryElement.h
    telemetry/TimeTelemetryElement.cpp
)
set(radio
    radio/NRF24/RF24Radio.h
    radio/NRF24/RF24Radio.cpp
    radio/NRF24_CSMA/RF24CSMARadio.cpp
    radio/NRF24_CSMA/RF24CSMARadio.h
    radio/NRF24_DUAL/RF24DualRadio.h
    radio/NRF24_DUAL/RF24DualRadio.cpp
    radio/LORA/LoraRadio.h
    radio/LORA/LoraRadio.cpp
)
    
# This project will output an executable file
add_executable(${PROJECT_NAME} main.cpp utils.h ${settings} ${interfaces} ${settings} ${structures} ${tun} ${packetizers} ${unit_tests} ${telemetry} ${radio} ${reedsolomon} ${rs_codec})

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
