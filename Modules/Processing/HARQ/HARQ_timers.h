#pragma once

#include "HARQ_secondary_constants.h"

#include <iostream>
#include <stdint.h>

#include <sys/timerfd.h>
#include <unistd.h>

// Number of timers (one per buffer)
constexpr unsigned int buffer_fd_event_number = max_id - 1;
int timer_fds[buffer_fd_event_number] = {-1};
unsigned int buffer_send_retries[buffer_fd_event_number] = {0};
static void initialize_buffer_fds()
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

void set_buffer_timer(int buf_id, int timeout_us)
{
    struct itimerspec new_value{};
    new_value.it_value.tv_sec = timeout_us / 1000000;
    new_value.it_value.tv_nsec = (timeout_us % 1000000) * 1000;
    // it_interval a 0 significa: scatta una volta sola
    timerfd_settime(timer_fds[buf_id], 0, &new_value, NULL);

#ifdef DEBUG_LOGS
    printf("Timer %d impostato: %d us\n", buf_id, timeout_us);
#endif
}

bool is_timer_active(int buf_id) {
    struct itimerspec current_value;
    if (timerfd_gettime(timer_fds[buf_id], &current_value) == -1) return false;
    // Se secondi o nanosecondi residui > 0, il timer è attivo
    return (current_value.it_value.tv_sec > 0 || current_value.it_value.tv_nsec > 0);
}

void clear_buffer_timer(int buf_id)
{
    struct itimerspec stop_value{};
    timerfd_settime(timer_fds[buf_id], 0, &stop_value, NULL);

    uint64_t exp;
    // read non blocca se hai usato TFD_NONBLOCK, ma restituirà -1
    if (read(timer_fds[buf_id], &exp, sizeof(exp)) == -1)
    {
        // Se errno è EAGAIN, significa che non c'erano eventi da leggere: perfetto.
    }

    buffer_send_retries[buf_id] = 0;
#ifdef DEBUG_LOGS
    printf("Rimosso timer %d\n", buf_id);
#endif
}