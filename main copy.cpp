#include <iostream>

#define USE_READ_THREAD

const bool use_IRQ_RF24 = true;

#include <poll.h>

#include "PacketLogger.h"

#include "debugutils.h"

#include "Modules/High/TUN/TUN.h"
#include "Modules/Processing/HARQ/HARQ.h"

#include "Modules/Low/DUALRF24/DUALRF24.h"

#include <signal.h>

#define test_perf_iovec

typedef TunSource SelectedSource; // da compilare dopo con CMake
// typedef HARQ SelectedProcessor; // da compilare dopo con CMake
typedef DUALRF24 SelectedRadio; // da compilare dopo con CMake

// const unsigned int tMTU = 65500;

#ifndef test_perf_iovec
uint8_t tdata[MTU];
#endif

// uint8_t tdata1[MTU];

struct Pipeline
{
    SelectedSource source;
    // SelectedProcessor processor;
    SelectedRadio radio;
};
Pipeline pipeline;

bool running = true;

struct SwapPair
{
    uint8_t *a;
    uint8_t *b;
    void swap()
    {
        uint8_t tmp = *a;
        *a = *b;
        *b = tmp;
    }
};

SwapPair ip_swap_map[4];

void precompute_ip_swap_iovec(iovec *iov, int iovcnt)
{
    // 1. Reset preventivo per evitare swap su vecchi puntatori
    for (int k = 0; k < 4; k++)
        ip_swap_map[k] = {nullptr, nullptr};

    int count = 0;
    for (int i = 0; i < iovcnt; i++)
    {
        uint8_t *base = static_cast<uint8_t *>(iov[i].iov_base);
        for (size_t j = 0; j < iov[i].iov_len; j++)
        {
            if (count >= 12 && count < 16)
                ip_swap_map[count - 12].a = &base[j];
            else if (count >= 16 && count < 20)
                ip_swap_map[count - 16].b = &base[j];

            count++;
            if (count >= 20)
                return; // Esci appena hai mappato i 20 byte
        }
    }
}

void fast_swap_precalcd()
{
    // 2. Controllo validità prima dello swap
    for (int i = 0; i < 4; i++)
    {
        if (ip_swap_map[i].a && ip_swap_map[i].b)
            ip_swap_map[i].swap();
    }
}

void copy_to_iovec(iovec *dest, unsigned int iovcnt, uint8_t *src, unsigned int bytes)
{
    unsigned int count = 0;
    for (unsigned int i = 0; i < iovcnt; i++)
    {
        for (unsigned int j = 0; j < dest[i].iov_len; j++)
        {
            static_cast<uint8_t *>(dest[i].iov_base)[j] = src[count++];
            if (count == bytes)
                return;
        }
    }
}

int pipe_fds[2];
int pipe_fds_2[2];
int pipe_radio_ready_marker[2];
int frame_freed_marker[2];

enum Poll_Items
{
    Source_Poll = 0,
    Radio_Poll = 1,
    Pipe_Poll = 2,
    Frame_Freed_Poll = 3,
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
#ifdef USE_READ_THREAD
    fds[Radio_Poll] = {pipe_radio_ready_marker[0], POLLIN, 0};
#else
    fds[Radio_Poll] = {pipeline.radio.get_fd(), POLLIN, 0};
#endif

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
        fds[Source_Poll].events = (is_buffer_free()) ? POLLIN : 0;
        int ret = poll(fds, pollsize, -1);
        if (ret < 0)
        {
            if (errno == EINTR || !running)
                break; // Uscita immediata per segnale
            continue;
        }

        if (ret == 0)
            continue;

        if (fds[Source_Poll].revents & POLLIN)
        {

            int bytes = pipeline.source.read_to_buffer_iovec(get_buffer_iovec(), max_segments);
            if (bytes <= 0)
                continue;

            consume_frame(bytes);
            if (--ret <= 0)
                continue; // Salta il resto se era l'unico evento
        }

        // Gestione Radio
        if (fds[Radio_Poll].revents & POLLIN)
        {
#ifdef HARQ_SINGLEBUFFER
#ifdef USE_READ_THREAD
            // printf("Ricevo: \n");
            // print_array(radio_interrupt_buffers[0].segments[0].header, radio_interrupt_buffers[0].written_items * bytes_segment_capacity, bytes_segment_capacity);
            uint8_t ready_slot;
            while (read(pipe_radio_ready_marker[0], &ready_slot, 1) > 0)
            {
                consume_packets(ready_slot);
            }
#else
            radio_interrupt_buffers[0].written_items = pipeline.radio.handle_read(radio_interrupt_buffers[0].segments[0].header, max_radio_buffer_segments);
            radio_interrupt_buffers[0].lock = true;
            consume_packets(pipeline, 0);
#endif
#else

#ifdef USE_READ_THREAD
            // printf("Ricevo: \n");
            // print_array(radio_interrupt_buffers[0].segments[0].header, radio_interrupt_buffers[0].written_items * bytes_segment_capacity, bytes_segment_capacity);
            uint8_t ready_slot;
            while (read(pipe_radio_ready_marker[0], &ready_slot, 1) > 0)
            {
                consume_radio_packets(ready_slot);
            }
#else
            radio_interrupt_buffers[0].written_items = pipeline.radio.handle_read(radio_interrupt_buffers[0].segments[0].header, max_radio_buffer_segments);
            radio_interrupt_buffers[0].lock = true;
            consume_radio_packets(0);

#endif
#endif

            if (--ret <= 0)
                continue; // Salta il resto se era l'unico evento
        }

        if (fds[Frame_Freed_Poll].revents & POLLIN)
        {
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
                uint64_t expirations;

                // OBBLIGATORIO: Leggi 8 byte per svuotare il timerfd
                ssize_t n = read(fds[i].fd, &expirations, sizeof(expirations));
                if (n == sizeof(uint64_t))
                {
                    // Solo se ho letto davvero un evento chiamo la callback
                    // printf("Timer %d fired\n", i-2);
                    timer_fired((TIMER_ACTIONS)(i - Poll_items_count));
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
            char c;
            read(pipe_fds[0], &c, 1); // Svuota la pipe
            break;                    // Esci dal loop
        }
    }
}

#include <thread> // Per std::thread
#include <pthread.h>
#include <errno.h>
#include <cstdio>

void set_thread_name(const char *name)
{
    return; // Disabled
    int res = pthread_setname_np(pthread_self(), name);
    if (res != 0)
    {
        fprintf(stderr, "Errore set_thread_name: %s\n", strerror(res));
    }
}

const int poll_size = 2;
struct pollfd fdr[poll_size];

void radio_listener_poll_thread()
{

    do
    {

        if (!use_IRQ_RF24)
        {
            int ret = poll(fdr, poll_size, -1);
            if (ret < 0)
            {
                if (errno == EINTR || !running)
                    break; // Uscita immediata per segnale
                continue;
            }

            if (ret == 0)
                continue;
        }

        if ((fdr[0].revents & POLLIN) || use_IRQ_RF24)
        {
            int rbuf;
            while ((rbuf = find_free_radio_input_buffer()) == -1)
            {
                // Sleep... can't do anything else but wait
                std::this_thread::yield();
                printf("Radio buffer full\n");
            }

            radio_interrupt_buffers[rbuf].written_items = pipeline.radio.handle_read(radio_interrupt_buffers[rbuf].segments[0].header, max_radio_buffer_segments);
            radio_interrupt_buffers[rbuf].lock = false;

            write(pipe_radio_ready_marker[1], &rbuf, 1);
        }

        // Gestione Pipe di comando/shutdown (Uscita)
        if (fdr[1].revents & POLLIN)
        {
            char dummy[256];
            read(pipe_fds_2[0], dummy, sizeof(dummy)); // Svuota la pipe
            // Gestisci eventuale comando di shutdown o TX prioritario
        }
    } while (running && !use_IRQ_RF24);
}

#include <string>
#include <fcntl.h>

void handle_sigint(int sig)
{
    running = false;
    char c = 1;
    write(pipe_fds[1], &c, 1);   // Sveglia il poll
    write(pipe_fds_2[1], &c, 1); // Sveglia il poll
}

std::thread radio_thread;
int main(int argc, char **argv)
{
    set_thread_name("Main");

    signal(SIGINT, handle_sigint);

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

    // Initialize CRC stuff
    init_scramble_keypairs();
    init_lookup_table();

    // test_crc_scramble();

    // Verifica che ci sia almeno l'argomento del ruolo
    if (argc < 2)
    {
        std::cerr << "Utilizzo: " << argv[0] << " [0|1]" << std::endl;
        // return 1;
        role = 1;
    }
    else
    {
        // Converte l'argomento in intero (0 o 1)
        role = std::stoi(argv[1]) == 1;
    }

    if (role == 1)
    {
        std::cout << "Ruolo impostato: 1" << std::endl;
    }
    else
    {
        std::cout << "Ruolo impostato: 0" << std::endl;
    }

#ifdef PACKET_LOGGER
    char outfile_proto[] = "logs_%s.csv";
    sprintf(outfile_proto, outfile_proto, role ? "PY" : "SY");
    PL.open(outfile_proto);
#endif

    pipeline.source.init(MTU, role);

    initialize_HARQ();

    pipeline.radio.init();

// Catch sigterm sigkill e Ctrl+C, imposta running=false;

// Compila la pipeline e inizializza gli oggetti
#ifdef USE_READ_THREAD
    fdr[0] = {pipeline.radio.get_fd(), POLLIN, 0};
    fdr[1] = {pipe_fds_2[0], POLLIN, 0};
    if (!use_IRQ_RF24)
    {
        radio_thread = std::thread(radio_listener_poll_thread); // Lancia il loop con la pipeline
    }
#endif
    loop();

// Termina tutto quando il loop finisce
#ifdef PACKET_LOGGER
    PL.close();
#endif

#ifdef USE_READ_THREAD
    if (!use_IRQ_RF24)
        radio_thread.join();
#endif
    close(pipe_fds[0]);
    close(pipe_fds[1]);

    return 0;
}