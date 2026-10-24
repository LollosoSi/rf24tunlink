#pragma once

#include "HARQ_constants.h"
#include "HARQ_data_structures.h"

#include "../../../logging.h"

constexpr unsigned int packet_eaters_size = max_id - 1;

frame_out tun_frames[packet_eaters_size];
PacketEater packet_eaters[packet_eaters_size];

void init_frames()
{
    for (unsigned int i = 0; i < packet_eaters_size; i++)
    {
        initialize_frame_out(tun_frames[i]);
        packet_eaters[i].init();
    }
}

#include <atomic>
#include <cstring>
#include <assert.h>

// Usiamo una dimensione della FIFO leggermente maggiore per non perdere un buffer reale
// a causa della regola "One Slot Wasted" dei ring buffer lock-free.
constexpr unsigned int fifo_size = packet_eaters_size + 1; 

// Array FIFO degli indici liberi (dimensione aumentata di 1)
uint8_t free_frames_array[fifo_size] = {0}; 
bool frames_completed_array[packet_eaters_size]; // Stato logico per debug/check

// Indici Atomici
std::atomic<uint8_t> free_head{0}; // Dove TX PRELEVA (Lock)
std::atomic<uint8_t> free_tail{0}; // Dove RX INSERISCE (Free)

// Helper per avanzare l'indice circolare
inline uint8_t advance_index(uint8_t index) {
    return (index + 1) % fifo_size;
}

// ==========================================
// LOGICA FIFO
// ==========================================

bool is_fifo_empty() {
    // Se Head == Tail, non c'è nulla da leggere
    return free_head.load(std::memory_order_acquire) == free_tail.load(std::memory_order_acquire);
}

bool is_fifo_full() {
    // Se Tail+1 raggiunge Head, è pieno
    uint8_t current_tail = free_tail.load(std::memory_order_relaxed);
    uint8_t next_tail = advance_index(current_tail);
    return next_tail == free_head.load(std::memory_order_acquire);
}

void initialize_free_frames_array()
{
    memset(frames_completed_array, true, packet_eaters_size);

    // 1. Reset Punteri
    free_head = 0;
    free_tail = 0;

    // 2. RIEMPI LA FIFO! 
    // All'avvio tutti i buffer (0..N-1) sono liberi.
    // Simuliamo che RX li stia liberando tutti ora.
    for (unsigned int i = 0; i < packet_eaters_size; i++)
    {
        free_frames_array[free_tail] = i;
        free_tail = advance_index(free_tail); 
    }
    // Ora la FIFO è PIENA (contiene tutti gli ID), pronta per essere consumata da TX.
    

}

// Chiamato da RX (Produttore di frame liberi)
// "Restituisco un buffer usato alla coda dei liberi"
void free_frame(uint8_t buffer_id) 
{
    uint8_t current_tail = free_tail.load(std::memory_order_relaxed);
    uint8_t next_tail = advance_index(current_tail);

    // CORREZIONE: Qui controllo se è PIENA. Se è piena, non posso aggiungere altro.
    if (next_tail == free_head.load(std::memory_order_acquire)) {
        CATASTROPHIC("FIFO FULL inside free_frame! Logic error or double free. Lost frame %d", buffer_id);
        return;
    }

    // RIMOSSO: if(free_frames_array[current_tail]) -> Errore, 0 è un ID valido!

    // 1. Scrivi il dato
    free_frames_array[current_tail] = buffer_id;

    // 2. Pubblica l'indice (Release)
    free_tail.store(next_tail, std::memory_order_release);
    
    frames_completed_array[buffer_id] = true;
}

// Chiamato da TX (Consumatore di frame liberi)
// "Prendo un buffer libero per scriverci i dati del TUN"
frame_out* lock_frame() 
{
    // CORREZIONE: Qui controllo se è VUOTA. Se è vuota, non c'è nulla da prendere.
    if (is_fifo_empty()) {
        // Backpressure: Nessun buffer libero disponibile
        return nullptr; 
    }

    uint8_t current_head = free_head.load(std::memory_order_relaxed);
    
    // 1. Leggi il dato
    uint8_t buf_id = free_frames_array[current_head];

    // 2. Avanza la testa
    uint8_t next_head = advance_index(current_head);
    
    // 3. Pubblica il nuovo indice (Release)
    free_head.store(next_head, std::memory_order_release);

    frames_completed_array[buf_id] = false;
    return &tun_frames[buf_id];
}


bool is_buffer_completed(unsigned int buf_id)
{
    return frames_completed_array[buf_id];
}

unsigned int current_out_buffer = packet_eaters_size-1;

bool next_locked_frame()
{
//    if (frames_completed_array[current_out_buffer])
        for (unsigned int i = 0; i < packet_eaters_size; i++)
        {
            unsigned int r = (current_out_buffer + i) % packet_eaters_size;
            if (!is_buffer_completed(r))
            {
                current_out_buffer = r;
                return true;
            }
        }
//        else{
//            DLOG("Current frame (%d) is not completed", current_out_buffer);
//      }

    DLOG("No frames need to be finished.");
    return false;
}

// returns true is next is ready
bool current_frame_completed()
{

    free_frame(current_out_buffer);
    return next_locked_frame();
    //frames_completed_array[current_out_buffer] = true;
    //if (++completed_frames == (packet_eaters_size))
    //{
    //    completed_frames = 0;
    //    memcpy(free_frames_array, free_frames_array_default_copy, packet_eaters_size);
    //    memset(free_frames_by_index, true, packet_eaters_size);
    //    memset(frames_completed_array, true, packet_eaters_size);

//        free_frames = packet_eaters_size;

//        DLOG("All inputs freed. Waiting for more data.\n");
//    }
}

frame_out &current_frame_out()
{
    return tun_frames[current_out_buffer];
}