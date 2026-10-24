#pragma once

#include <iostream>
#include <stdint.h>

#include <sys/timerfd.h>
#include <unistd.h>

enum TIMER_ACTIONS
{
    SEGMENT_RESEND = 0,
    FRAME_EXPIRED = 1
};

// Number of timers
constexpr unsigned int buffer_fd_event_number = 2 * (max_id - 1);
int timer_fds[buffer_fd_event_number] = {-1};
unsigned int buffer_send_retries[buffer_fd_event_number] = {0};
inline void initialize_buffer_fds()
{
    for (unsigned int i = 0; i < buffer_fd_event_number; i++)
    {
        timer_fds[i] = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
        if (timer_fds[i] == -1)
        {
            perror("Errore creazione timerfd");
            exit(EXIT_FAILURE);
        }
    }
}

TIMER_ACTIONS unpack_id_action_timer(unsigned int timerid, uint8_t &id)
{
    id = timerid / 2;
    return (timerid % 2 == 0) ? SEGMENT_RESEND : FRAME_EXPIRED;
}

#include <mutex>

// Unico mutex per proteggere l'accesso ai timerfd
static std::mutex timer_mtx;

void set_buffer_timer(TIMER_ACTIONS action, unsigned int buf_id, int timeout_us)
{

    unsigned int target = (2 * buf_id) + action;

    std::lock_guard<std::mutex> lock(timer_mtx); // <--- LOCK

    struct itimerspec new_value{};
    new_value.it_value.tv_sec = timeout_us / 1000000;
    new_value.it_value.tv_nsec = (timeout_us % 1000000) * 1000;

    timerfd_settime(timer_fds[target], 0, &new_value, NULL);

    // buffer_send_retries NON va resettato qui se è un retry,
    // ma se è un nuovo pacchetto sì. Gestiscilo con cura.
    // buffer_send_retries[buf_id] = 0;
}

void clear_buffer_timer(TIMER_ACTIONS action, unsigned int buf_id)
{
    unsigned int target = (2 * buf_id) + action;

    std::lock_guard<std::mutex> lock(timer_mtx); // <--- LOCK

    uint64_t exp;
    struct itimerspec stop_value{}; // Valore zero disarma

    timerfd_settime(timer_fds[target], 0, &stop_value, NULL);

    // Svuota eventuali eventi pendenti
    while (read(timer_fds[target], &exp, sizeof(exp)) > 0)
        ;

    buffer_send_retries[target] = 0; // Ora è thread-safe
}

// Anche la lettura dei retry deve essere protetta se fatta da fuori
unsigned int get_retries(TIMER_ACTIONS action, unsigned int buf_id)
{
    unsigned int target = (2 * buf_id) + action;
    std::lock_guard<std::mutex> lock(timer_mtx);
    return buffer_send_retries[target];
}

unsigned int increment_retries(TIMER_ACTIONS action, unsigned int buf_id)
{
    unsigned int target = (2 * buf_id) + action;
    std::lock_guard<std::mutex> lock(timer_mtx);

    return buffer_send_retries[target]++;
}

bool is_timer_active(TIMER_ACTIONS action, unsigned int buf_id)
{
    unsigned int target = (2 * buf_id) + action;

    std::lock_guard<std::mutex> lock(timer_mtx); // <--- LOCK

    struct itimerspec current_value;
    if (timerfd_gettime(timer_fds[target], &current_value) == -1)
        return false;
    // Se secondi o nanosecondi residui > 0, il timer è attivo
    return (current_value.it_value.tv_sec > 0 || current_value.it_value.tv_nsec > 0);
}
