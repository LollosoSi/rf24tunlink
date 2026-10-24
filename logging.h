#pragma once
#include <cstdio>

//#define ncurses_log

#ifdef ncurses_log

#include "logger.h"

#else

//#define DISABLE_LOGGING

// --- CONFIGURAZIONE: Commenta/Decommenta per filtrare ---
#ifndef DISABLE_LOGGING

#define ENABLE_DLOG
#define ENABLE_TX
#define ENABLE_RX
#define ENABLE_TIMER

#endif

#define ENABLE_CATASTROPHIC

// --- IMPLEMENTAZIONE ---

#define INIT_LOG()       
#define END_LOG()        

#ifdef ENABLE_DLOG
    #define DLOG(fmt, ...) printf("[DLOG] : " fmt "\n", ##__VA_ARGS__); fflush(stdout);
#else
    #define DLOG(...)
#endif

#ifdef ENABLE_TX
    #define LOGTX(fmt, ...) printf("[TX]   : " fmt "\n", ##__VA_ARGS__); fflush(stdout);
#else
    #define LOGTX(...)
#endif

#ifdef ENABLE_RX
    #define LOGRX(fmt, ...) printf("[RX]   : " fmt "\n", ##__VA_ARGS__); fflush(stdout);
#else
    #define LOGRX(...)
#endif

#ifdef ENABLE_TIMER
    #define TIMERLOG(fmt, ...) printf("[TMR]  : " fmt "\n", ##__VA_ARGS__); fflush(stdout);
#else
    #define TIMERLOG(...)
#endif

#ifdef ENABLE_CATASTROPHIC
    #define CATASTROPHIC(fmt, ...) fprintf(stderr, "[CRIT] : " fmt "\n", ##__VA_ARGS__); fflush(stderr);
#else
    #define CATASTROPHIC(...)
#endif

#endif