#pragma once

#include <stdint.h>

#include <mutex>
#include <thread>

extern bool frame_write(uint8_t* message, unsigned int length);
extern void radio_write(uint8_t* sequential_data, unsigned int items, unsigned int packet_length);



void radio_write_threadsafe(uint8_t* sequential_data, unsigned int items, unsigned int packet_length){
    // "static" significa che questo mutex è condiviso tra tutte le chiamate a questa funzione
    // indipendentemente da quale thread la chiami.
    static std::mutex radio_mtx; 
    
    // RAII: Il lock viene acquisito qui
    std::lock_guard<std::mutex> lock(radio_mtx);
    
    // Se un altro thread arriva qui mentre stiamo scrivendo (per 2ms),
    // il sistema operativo lo metterà a dormire (0% CPU) e lo sveglierà
    // appena questa parentesi graffa si chiude.
    radio_write(sequential_data, items, packet_length);
    
    // Il lock viene rilasciato automaticamente qui (distruttore di lock_guard)
}

extern void frame_completed_signal();