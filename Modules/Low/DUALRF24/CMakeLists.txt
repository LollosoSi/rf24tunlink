# 1. Trova i file sorgente
file(GLOB RF24_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# 2. Dipendenze: Threads, RF24 e libgpiodcxx
find_package(Threads REQUIRED)
find_library(RF24_LIB rf24 REQUIRED)

find_package(PkgConfig REQUIRED)
# Cerca specificamente la versione C++ (gpiodcxx)
pkg_check_modules(GPIODCXX REQUIRED libgpiodcxx>=2.0)

# 3. Crea la libreria
add_library(radio_lib STATIC ${RF24_SOURCES})

# 4. Configura Target e Linkaggio
target_link_libraries(radio_lib 
    PUBLIC 
        ${RF24_LIB} 
        Threads::Threads 
        ${GPIODCXX_LIBRARIES}  # Linka libgpiodcxx
)

# 5. Includi Directory
target_include_directories(radio_lib 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${GPIODCXX_INCLUDE_DIRS}
)

# 6. Opzioni di compilazione
target_compile_options(radio_lib PRIVATE -Ofast)