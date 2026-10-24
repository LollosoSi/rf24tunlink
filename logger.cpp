#include "logger.h"
#include <cstdarg>

Logger::Logger() : padSys(nullptr), padTX(nullptr), padRX(nullptr), running(false) {}

Logger& Logger::get() {
    static Logger instance;
    return instance;
}

void Logger::init() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE); // Abilita frecce
    timeout(100);         // Timeout per getch nel thread

    getmaxyx(stdscr, screenH, screenW);
    colW = screenW / 3;

    // Crea PAD grandi (memoria storica)
    padSys = newpad(padHeight, colW);
    padTX  = newpad(padHeight, colW);
    padRX  = newpad(padHeight, colW);

    // Header finti
    wprintw(padSys, "=== SYS ===\n");
    wprintw(padTX,  "=== TX  ===\n");
    wprintw(padRX,  "=== RX  ===\n");
    currentLine = 1;

    running = true;
    inputThread = std::thread(&Logger::inputLoop, this);
    refreshView();
}

void Logger::end() {
    running = false;
    if (inputThread.joinable()) inputThread.join();
    endwin();
}

void Logger::inputLoop() {
    while (running) {
        int ch = getch(); // Legge input main screen
        bool dirty = false;
        
        if (ch == KEY_UP && scrollOffset > 0) {
            scrollOffset--;
            dirty = true;
        } else if (ch == KEY_DOWN && scrollOffset < currentLine - (screenH - 1)) {
            scrollOffset++;
            dirty = true;
        } 
        // Auto-scroll se siamo in fondo e non premiamo su
        else if (scrollOffset != currentLine - (screenH - 1) && ch == ERR) {
             // Opzionale: logica per tornare giù
        }

        if (dirty) refreshView();
    }
}

void Logger::refreshView() {
    // prefresh(pad, pad_y, pad_x, screen_y, screen_x, screen_h, screen_w)
    // Mostra la porzione del pad definita da scrollOffset
    prefresh(padSys, scrollOffset, 0, 0, 0,        screenH-1, colW-1);
    prefresh(padTX,  scrollOffset, 0, 0, colW,     screenH-1, (colW*2)-1);
    prefresh(padRX,  scrollOffset, 0, 0, colW*2,   screenH-1, screenW-1);
}

void Logger::log(int type, const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(mtx);
    
    // Check overflow buffer
    if (currentLine >= padHeight - 1) return; // O implementa ring buffer

    va_list args;
    va_start(args, fmt);
    
    // Scrittura sincronizzata
    if (type == 0) { // SYS
        vw_printw(padSys, fmt, args); wprintw(padSys, "\n");
        wprintw(padTX, "\n"); wprintw(padRX, "\n");
    } else if (type == 1) { // TX
        vw_printw(padTX, fmt, args); wprintw(padTX, "\n");
        wprintw(padSys, "\n"); wprintw(padRX, "\n");
    } else { // RX
        vw_printw(padRX, fmt, args); wprintw(padRX, "\n");
        wprintw(padSys, "\n"); wprintw(padTX, "\n");
    }
    va_end(args);

    currentLine++;
    
    // Auto-scroll: se l'utente era in fondo, rimani in fondo
    bool wasAtBottom = (scrollOffset >= currentLine - screenH);
    if (wasAtBottom || currentLine < screenH) {
        scrollOffset = currentLine - (screenH - 1);
        if (scrollOffset < 0) scrollOffset = 0;
    }
    
    refreshView();
}