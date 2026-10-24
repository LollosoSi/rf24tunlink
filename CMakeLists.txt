cmake_minimum_required(VERSION 3.10)

project(tunlink3-alpha)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Release)

# Imposta il tipo di build predefinito a Debug se non specificato
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
  # Abilita ASAN per compilazione e linking
  # Definisci i flag come lista (CMake usa il punto e virgola come separatore)
  set(ASAN_FLAGS "-fsanitize=address" "-fno-omit-frame-pointer" "-g")

endif()



# Aggiunge simboli di debug e rimuove ottimizzazioni per g++
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -fno-signed-zeros -fno-trapping-math -frename-registers -funroll-loops -march=native")


set(RADIO_MODULE "DUALRF24" CACHE STRING "Seleziona il modulo radio")
set_property(CACHE RADIO_MODULE PROPERTY STRINGS "DUALRF24" "DUMMY")

message(STATUS "Configurazione modulo radio: ${RADIO_MODULE}")

add_subdirectory(Modules/Low/${RADIO_MODULE})

set(SOURCES
    main.cpp
    Modules/Processing/HARQ/ReedSolomon/ReedSolomon.cpp
    logger.cpp
)

find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIRS})

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ncurses
target_link_libraries(${PROJECT_NAME} PRIVATE ${CURSES_LIBRARIES})

# Collega la libreria generica creata dal sottomodulo
target_link_libraries(${PROJECT_NAME} PRIVATE radio_lib)


if(ASAN_FLAGS)

  # Applica gli stessi flag all'eseguibile principale
  target_compile_options(tunlink3-alpha PRIVATE ${ASAN_FLAGS})
  target_link_options(tunlink3-alpha PRIVATE ${ASAN_FLAGS})

endif()

