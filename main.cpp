/** Tunlink 3
 * Author: Andrea Roccaccino
 * Created on 11/01/2026
 * Only for private use.
 *
 * Never give up. I didn't.
 */

#include <iostream>

#include <string>
#include <fcntl.h>

#include <thread>
#include <pthread.h>
#include <errno.h>
#include <cstdio>

#include <poll.h>
#include <signal.h>

#include "Modules/High/TUN/TUN.h"
#include "Modules/Processing/HARQ/HARQ.h"

#include "Modules/Low/DUALRF24/DUALRF24.h"

bool running = false;

// To be compiled later using CMake
typedef TunSource SelectedSource;
typedef DUALRF24 SelectedRadio;

struct Pipeline
{
    SelectedSource source;
    // SelectedProcessor processor;
    SelectedRadio radio;
};
Pipeline pipeline;

int pipe_fds[2];
int pipe_fds_2[2];
int pipe_radio_ready_marker[2];
int frame_freed_marker[2];

enum Poll_Items
{
    Source_Poll = 0,
    HARQ_Internal_Poll = 1,
    Radio_Poll = 2,
    Pipe_Poll = 3,
    Frame_Freed_Poll = 4,
    Poll_items_count // Always last item!
};

bool frame_write(uint8_t *data, unsigned int length)
{
    return pipeline.source.write_to_buffer(data, length);
}

void radio_write(uint8_t *out_buffer, unsigned int sequential_payloads, unsigned int bytes_per_payload)
{
    pipeline.radio.radio_write(out_buffer, sequential_payloads, bytes_per_payload);
}

void frame_completed_signal()
{
    uint8_t s = 1;
    write(frame_freed_marker[1], &s, 1);
}

void loop()
{
    // Poll size is the system events + HARQ timers
    unsigned int pollsize = Poll_items_count + buffer_fd_event_number;
    struct pollfd fds[pollsize];
    fds[Source_Poll] = {pipeline.source.get_fd(), POLLIN, 0};
    fds[HARQ_Internal_Poll] = {get_harq_internal_fd_pipe(), POLLIN, 0};

    fds[Radio_Poll] = {pipe_radio_ready_marker[0], POLLIN, 0};

    fds[Frame_Freed_Poll] = {frame_freed_marker[0], POLLIN, 0};

    fds[Pipe_Poll] = {pipe_fds[0], POLLIN, 0};

    for (unsigned int i = 0; i < buffer_fd_event_number; i++)
    {
        fds[Poll_items_count + i].fd = timer_fds[i];
        fds[Poll_items_count + i].events = POLLIN;
        fds[Poll_items_count + i].revents = 0;
    }

    while (running)
    {

        // Do not read TUN if there are no buffers to write into.
        // Leave queued to the kernel
        fds[Source_Poll].events = (!is_fifo_empty()) ? POLLIN : 0;
        int ret = poll(fds, pollsize, -1);
        DLOG("Poll\n");
        if (ret < 0)
        {
            if (errno == EINTR || !running)
                break; // Uscita immediata per segnale
            continue;
        }

        if (ret == 0)
            continue;

        DLOG("Loop\n");

        if (fds[Source_Poll].revents & POLLIN)
        {
            DLOG("Source\n");
            frame_out *out = lock_frame();
            if (out != nullptr)
            {
                int bytes = pipeline.source.read_to_buffer_iovec(get_buffer_iovec(*out), max_segments);
                if (bytes <= 0)
                    continue;

                consume_frame(*out, bytes);
            }else{
                CATASTROPHIC("Frame out pointer is nullptr. This should not happen.");
            }
            if (--ret <= 0)
                continue; // Salta il resto se era l'unico evento
        }

        // Gestione Radio
        if (fds[Radio_Poll].revents & POLLIN)
        {
            DLOG("Radio Poll\n");

            // printf("Ricevo: \n");
            // print_array(segment_beginning(radio_interrupt_buffers[0].segments[0]), radio_interrupt_buffers[0].written_items * bytes_segment_capacity, bytes_segment_capacity);
            uint8_t ready_slot;
            while (read(pipe_radio_ready_marker[0], &ready_slot, 1) > 0)
            {
                consume_radio_packets(ready_slot);
            }

            if (--ret <= 0)
                continue; // Salta il resto se era l'unico evento
        }

        if (fds[HARQ_Internal_Poll].revents & POLLIN)
        {
            HARQInternalEvent event;
            read(frame_freed_marker[0], &event, sizeof(HARQInternalEvent));
            frame_completed(event.buffer_id);

            if (--ret <= 0)
                continue; // Salta il resto se era l'unico evento
        }

        if (fds[Frame_Freed_Poll].revents & POLLIN)
        {

            DLOG("Frame ready poll\n");
            int ready_slot;
            read(frame_freed_marker[0], &ready_slot, 1);
            if (--ret <= 0)
                continue; // Salta il resto se era l'unico evento
        }

        // 3. Scansione Timer (Solo per i restanti eventi)
        for (unsigned int i = Poll_items_count; i < pollsize && ret > 0; ++i)
        {
            if (fds[i].revents & POLLIN)
            {
                DLOG("Timer scanned %d\n", i - Poll_items_count);

                uint64_t expirations;

                // OBBLIGATORIO: Leggi 8 byte per svuotare il timerfd
                ssize_t n = read(fds[i].fd, &expirations, sizeof(expirations));
                if (n == sizeof(uint64_t))
                {
                    // Solo se ho letto davvero un evento chiamo la callback
                    // printf("Timer %d fired\n", i-2);
                    timer_fired(i - Poll_items_count);
                }
                else
                {
                    // Se n == -1 ed errno == EAGAIN, significa che il timer
                    // è stato cancellato (svuotato) da un altro evento (es. ACK)
                    // in questo stesso ciclo di poll. Lo ignoriamo.
                    // DLOG(printf("Salto evento fantasma su timer %d\n", i - Poll_items_count));
                }

                ret--; // Un evento in meno da cercare
            }
        }

        if (fds[Pipe_Poll].revents & POLLIN)
        {
            DLOG("Break poll\n");
            char c;
            read(pipe_fds[0], &c, 1); // Svuota la pipe
            return;                   // Esci dal loop
        }
    }
}

void set_thread_name(const char *name)
{
    return; // Disabled
    int res = pthread_setname_np(pthread_self(), name);
    if (res != 0)
    {
        fprintf(stderr, "Errore set_thread_name: %s\n", strerror(res));
    }
}

void radio_read_IRQ()
{
    DLOG("IRQ call");

    int rbuf;
    while ((rbuf = find_free_radio_input_buffer()) == -1)
    {
        // Sleep... can't do anything else but wait
        std::this_thread::yield();
        DLOG("Radio buffer full");
    }

    radio_interrupt_buffers[rbuf].written_items = pipeline.radio.handle_read(segment_beginning(radio_interrupt_buffers[rbuf].segments[0]), max_radio_buffer_segments);
    radio_interrupt_buffers[rbuf].lock = true;

    //consume_radio_packets(rbuf);
    // Try to process in irq.
     write(pipe_radio_ready_marker[1], &rbuf, 1);
}

void handle_sigint(int sig)
{
    running = false;
    char c = 1;
    write(pipe_fds[1], &c, 1);   // Sveglia il poll
    write(pipe_fds_2[1], &c, 1); // Sveglia il poll
}

int main(int argc, char **argv)
{
    set_thread_name("Main");

    signal(SIGINT, handle_sigint);

INIT_LOG();

    pipe2(pipe_fds, O_CLOEXEC);
    fcntl(pipe_fds[0], F_SETFL, O_NONBLOCK);
    fcntl(pipe_fds[1], F_SETFL, O_NONBLOCK);

    pipe2(pipe_fds_2, O_CLOEXEC);
    fcntl(pipe_fds_2[0], F_SETFL, O_NONBLOCK);
    fcntl(pipe_fds_2[1], F_SETFL, O_NONBLOCK);

    pipe2(pipe_radio_ready_marker, O_CLOEXEC);
    fcntl(pipe_radio_ready_marker[0], F_SETFL, O_NONBLOCK);
    fcntl(pipe_radio_ready_marker[1], F_SETFL, O_NONBLOCK);

    pipe2(frame_freed_marker, O_CLOEXEC);
    fcntl(frame_freed_marker[0], F_SETFL, O_NONBLOCK);
    fcntl(frame_freed_marker[1], F_SETFL, O_NONBLOCK);

    // Verify role from cmd
    if (argc < 2)
    {
        std::cerr << "Utilizzo: " << argv[0] << " [0|1]" << std::endl;
        // return 1;
        role = 1;
    }
    else
    {
        // Convert role (0 o 1)
        role = std::stoi(argv[1]) == 1;
    }

    if (role == 1)
    {
        std::cout << "Role set: 1" << std::endl;
    }
    else
    {
        std::cout << "Role set: 0" << std::endl;
    }

#ifdef PACKET_LOGGER
    char outfile_proto[] = "logs_%s.csv";
    sprintf(outfile_proto, outfile_proto, role ? "PY" : "SY");
    PL.open(outfile_proto);
#endif

    pipeline.source.init(MTU, role);

    initialize_HARQ();

    pipeline.radio.init();

    running = true;
    loop();

// Termina tutto quando il loop finisce
#ifdef PACKET_LOGGER
    PL.close();
#endif

    close(pipe_fds[0]);
    close(pipe_fds[1]);


    END_LOG();
    return 0;
}