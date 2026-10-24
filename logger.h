#pragma once
#include <ncurses.h>
#include <mutex>
#include <thread>
#include <atomic>

enum LogType { L_SYS, L_TX, L_RX };

class Logger {
    WINDOW *padSys, *padTX, *padRX; // Usa pad invece di win
    std::mutex mtx;
    std::thread inputThread;
    std::atomic<bool> running;
    
    int padHeight = 2000; // Buffer storico
    int currentLine = 0;  // Dove stiamo scrivendo
    int scrollOffset = 0; // Dove stiamo guardando
    int screenH, screenW, colW;

    Logger();
    void inputLoop(); // Gestisce tastiera
    void refreshView(); // Aggiorna visualizzazione

public:
    static Logger& get();
    void init();
    void end();
    void log(int type, const char* fmt, ...); // Usa int per semplicità
};

// --- MACRO ADATTATE ---
//#define DISABLE_LOGGING // Commenta per attivare

#ifndef DISABLE_LOGGING
    #define INIT_LOG()       Logger::get().init()
    #define END_LOG()        Logger::get().end()
    
    // Tutti usano il Logger centralizzato per sync visuale
    #define DLOG(fmt, ...)   Logger::get().log(L_SYS, "[DLOG]: " fmt, ##__VA_ARGS__)
    #define LOGTX(fmt, ...)  Logger::get().log(L_TX,  "[TX]  : " fmt, ##__VA_ARGS__)
    #define LOGRX(fmt, ...)  Logger::get().log(L_RX,  "[RX]  : " fmt, ##__VA_ARGS__)
    #define TIMERLOG(fmt, ...) Logger::get().log(L_SYS, "[TMR] : " fmt, ##__VA_ARGS__)
    #define CATASTROPHIC(fmt, ...) Logger::get().log(L_SYS, "[CRIT]: " fmt, ##__VA_ARGS__)
#else
    #define INIT_LOG()
    #define END_LOG()
    #define DLOG(...)
    #define LOGTX(...)
    #define LOGRX(...)
    #define TIMERLOG(...)
    #define CATASTROPHIC(...)
#endif