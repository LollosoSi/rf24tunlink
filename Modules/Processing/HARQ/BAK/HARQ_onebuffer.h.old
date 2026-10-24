#pragma once

#define HARQ_SINGLEBUFFER
// #define WAIT_WINDOW_COMPLETION



#ifdef HARQ_SINGLEBUFFER

constexpr bool RESET_ON_DUPLICATES = false;

#include <iostream>
#include <stdint.h>

#include <cmath>

#include <cstring>

#include <sys/uio.h>

#include "HARQ_constants.h"
#include "CRC.h"
#include "RSCodec.h"

#include <sys/timerfd.h>
#include <unistd.h>

#include "../../../debugutils.h"
#include "../../../PacketLogger.h"

// Secondary constants (non user defined)

constexpr uint8_t radio_input_buffer_num = 2;
constexpr uint8_t max_radio_buffer_segments = 3;

constexpr uint8_t bytes_header = 1;
constexpr uint8_t bytes_segment_capacity = bytes_data - bytes_header;

// Bit shift: 1 << bits equals 2^bits
constexpr unsigned int max_segments = 1u << bits_segment;
constexpr unsigned int max_id = 1u << bits_id;

constexpr unsigned int MTU = (bytes_segment_capacity * max_segments) - 1; // -1 Is the last packet length byte

constexpr unsigned int bytes_payload = bytes_data + bytes_ECC;

constexpr unsigned int max_reply_queue_length = 4 * MTU;
constexpr unsigned int max_system_replies_length = (MTU);

constexpr unsigned int max_input_buffers_frame = 1;
constexpr unsigned int max_frames = max_input_buffers_frame;

// PACK AND UNPACK header utils
constexpr uint8_t bits_to_mask(uint8_t bits)
{
    return (1u << bits) - 1;
}

// Bit math for pack and unpack
constexpr unsigned int id_byte_pos_offset = 8 - bits_id;
constexpr unsigned int segment_byte_pos_offset = 8 - bits_id - bits_segment;
constexpr unsigned int last_packet_byte_pos_offset = 8 - bits_id - bits_segment - bits_lastpacket;
constexpr unsigned int id_mask = bits_to_mask(bits_id);
constexpr unsigned int segment_mask = bits_to_mask(bits_segment);
constexpr unsigned int last_packet_mask = bits_to_mask(bits_lastpacket);

static uint8_t pack(const uint8_t &id, const uint8_t &segment,
                    const bool &last_packet)
{
    return (0x00 | ((id & id_mask) << id_byte_pos_offset) | ((segment & segment_mask) << segment_byte_pos_offset) | (last_packet & last_packet_mask));
}
// Corrects segment size for the last packet to prevent overflow
static void unpack(const uint8_t &data, uint8_t &id, uint8_t &segment, bool &last_packet)
{
    id = id_mask & (data >> id_byte_pos_offset);
    segment = segment_mask & (data >> segment_byte_pos_offset);
    last_packet = last_packet_mask & (data >> last_packet_byte_pos_offset);
}

// - - - - - - - - - - - -

// Number of timers (one per buffer)
constexpr unsigned int buffer_fd_event_number = max_frames;
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

bool is_timer_active(int buf_id)
{
    struct itimerspec current_value;
    if (timerfd_gettime(timer_fds[buf_id], &current_value) == -1)
        return false;
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

#pragma pack(push, 1)
struct segment
{
    uint8_t header[bytes_header];
    uint8_t data[bytes_segment_capacity];
    uint8_t trailer[bytes_ECC];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct frame_input
{
    uint8_t CRC = 0;
    uint8_t last_segment_number = 0;
    bool queued_for_resend[max_segments] = {0};
    iovec data_pointers[max_segments];
    segment buffer[max_segments];
};
#pragma pack(pop)

frame_input input_buffers[max_input_buffers_frame];

// Frame lock - unlock logic

uint8_t completed_frames = 0;
uint8_t free_frames = max_frames;
uint8_t free_frames_array_default_copy[max_frames] = {0};
uint8_t free_frames_array[max_frames] = {0};
bool free_frames_by_index[max_frames] = {true};

static void initialize_free_frames_array()
{
    for (unsigned int i = 0; i < max_frames; i++)
    {
        free_frames_array[i] = (max_frames)-1 - i;
        free_frames_array_default_copy[i] = (max_frames)-1 - i;
    }
}

#ifdef WAIT_WINDOW_COMPLETION

static void on_frame_completed()
{
    if (++completed_frames == (max_frames))
    {
        completed_frames = 0;
        memcpy(free_frames_array, free_frames_array_default_copy, max_frames);
        memset(free_frames_by_index, true, max_frames);
        free_frames = max_frames;
#ifdef DEBUG_LOGS
        printf("All inputs freed. Waiting for more data.\n");
#endif
    }
}

#endif

static bool is_buffer_free(unsigned int buf_id)
{
    assert(buf_id < (max_frames));
    return free_frames_by_index[buf_id];
}

static bool any_buffers_free()
{
    return free_frames > 0;
}

static void free_frame(uint8_t buffer_id)
{
    free_frames_array[free_frames++] = buffer_id;
    free_frames_by_index[buffer_id] = true;
    // printf("Unlocked frame: %d\n", buffer_id);
    // printf("Free frames now: ");
    // print_array(free_frames_array, free_frames, free_frames);
}

static frame_input *lock_frame(uint8_t &buf_id)
{
    buf_id = free_frames_array[--free_frames];
    free_frames_by_index[buf_id] = false;

    // printf("Locked frame: %d\n", buf_id);
    // printf("Free frames now: ");
    // print_array(free_frames_array, free_frames, free_frames);
    return &input_buffers[buf_id];
}

// - - - - - - - - - - - -

static void setup_data_pointers()
{
    unsigned int bufferslen = sizeof(input_buffers) / sizeof(frame_input);

    for (unsigned int j = 0; j < bufferslen; j++)
    {
        unsigned int pointerslen = sizeof(input_buffers[j].data_pointers) / sizeof(iovec);
        for (unsigned int i = 0; i < pointerslen - 1; i++)
        {
            input_buffers[j].data_pointers[i] = {input_buffers[j].buffer[i].data, bytes_segment_capacity};
        }
        input_buffers[j].data_pointers[pointerslen - 1] = {input_buffers[j].buffer[pointerslen - 1].data, bytes_segment_capacity - 1}; // The last packet is mandatory with one less byte
    }

    initialize_free_frames_array();
    initialize_buffer_fds();
}

#pragma pack(push, 1)
struct reception_buffer
{
    uint8_t ID = 0;
    uint8_t CRC = 0;
    unsigned int msglen = 0;
    uint8_t last_segment_number = 0;
    bool consumed = false; // If we should consume or only ACK when receiving the same last segment
    bool segments_received[max_segments] = {false};
    uint8_t message[MTU] = {0};
};
#pragma pack(pop)

reception_buffer incoming_buffer;

static void reception_buffer_reset(reception_buffer *buffer = &incoming_buffer)
{
    buffer->msglen = 0;
    memset(buffer->segments_received, false, max_segments);
    buffer->consumed = false;
    buffer->last_segment_number = 0;
    buffer->ID = 0;
}

#pragma pack(push, 1)
struct single_radio_buffer
{
    bool lock = false;
    uint8_t written_items = 0;
    segment segments[max_radio_buffer_segments];
};
#pragma pack(pop)

single_radio_buffer radio_interrupt_buffers[radio_input_buffer_num];

#pragma pack(push, 1)
struct reply_queue
{
    segment *message_pointer = nullptr;
};
#pragma pack(pop)

reply_queue replies[max_reply_queue_length];
unsigned int inserted_replies = 0;

segment system_ack_replies[max_system_replies_length];
unsigned int inserted_system_ack_replies = 0;

segment system_nack_replies[max_system_replies_length];
unsigned int queued_nack_items = 0;
unsigned int queued_nack_packets = 0;
constexpr unsigned int max_nack_entries = (bytes_segment_capacity - 1);

// This function assumes the nack array in data has been completed
static void finalize_nack_packet(unsigned int index)
{
    system_nack_replies[index].header[0] = pack(0, 0, false);
    rs_encode(system_nack_replies[index].header);
}

static void finalize_nack_and_flush()
{

    if (queued_nack_items > 0)
    {

        unsigned int current_pkt = queued_nack_items / max_nack_entries;
        unsigned int byte_offset = queued_nack_items % max_nack_entries;

        // Se ci sono dati residui (non siamo all'inizio di un nuovo pacchetto)
        if (byte_offset != 0)
        {
            finalize_nack_packet(current_pkt); // Punto al pacchetto attuale
            queued_nack_packets++;
        }

        for (int i = 0; i < queued_nack_packets; i++)
        {
            if (inserted_replies < max_reply_queue_length)
            {
                replies[inserted_replies++].message_pointer = &system_nack_replies[i];
            }
            else
                break;
        }
    }
}

static void insert_nack_item(uint8_t id, uint8_t segment)
{
    // 1. Individua posizione
    unsigned int current_pkt = queued_nack_items / max_nack_entries;
    unsigned int byte_offset = queued_nack_items % max_nack_entries;

    uint8_t candidate = pack(id, segment, 0);

    // Return if already present
    for (int i = 0; i < queued_nack_items; i++)
    {
        unsigned int test_pkt = (queued_nack_items - 1 - i) / max_nack_entries;
        unsigned int test_byte_offset = (queued_nack_items - 1 - i) % max_nack_entries;
        if (system_nack_replies[test_pkt].data[test_byte_offset] == candidate)
        {
#ifdef DEBUG_LOGS
            printf("Preventing duplicate segment request in same NACK batch\n");
#endif
            return;
        }
    }

    // 2. Scrittura (usando l'id nell'header se necessario o nel payload)
    // Se id=0 è un messaggio di sistema, il pacchetto trasporta i segmenti mancanti
    system_nack_replies[current_pkt].data[byte_offset] = candidate;

    // Aggiorna contatore interno (ultimo byte usato come segnaposto/count)
    system_nack_replies[current_pkt].data[max_nack_entries] = byte_offset + 1;

    // 3. Controllo chiusura pacchetto
    if (byte_offset == (max_nack_entries - 1))
    {
        // L'indice dell'id del buffer va gestito (qui suppongo header[0] gestito da pack)
        finalize_nack_packet(current_pkt);
        queued_nack_packets++;
    }

    queued_nack_items++;
}

static bool reschedule_segment(const unsigned int &buffer, const unsigned int &segment)
{
    if (inserted_replies < max_reply_queue_length && !input_buffers[buffer].queued_for_resend[segment])
    {
        replies[inserted_replies++].message_pointer = &input_buffers[buffer].buffer[segment];
        input_buffers[buffer].queued_for_resend[segment] = true;
        return true; // Successfully rescheduled
    }

    // Couldn't reschedule
    return false;
}

static bool insert_ack_or_nack_for_buffer(uint8_t &segment_number_last, bool only_ACK_if_finished = false)
{
    if (inserted_replies < max_reply_queue_length)
    {
        bool should_ack = true;
        reception_buffer &buffer_ref = incoming_buffer;
        for (unsigned int i = 0; i <= segment_number_last; i++)
        {
            if (!buffer_ref.segments_received[i])
            {
                DLOG(
                    if (should_ack) {
                        if (only_ACK_if_finished)
                        {
                            printf("Not NACKing - Missing:");
                        }
                        else
                        {
                            printf("NACK - Manca:");
                        }
                    } printf(" %d(%d)", incoming_buffer.ID, i););
                if (!only_ACK_if_finished)
                {
                    insert_nack_item(buffer_ref.ID, i);
                }
                should_ack = false;
            }
        }
        DLOG(
            if (!should_ack) {
                printf("\n");
            });

        if (should_ack)
        {
            // ACK! Make header, CRC8 and encode
            DLOG(printf("Rispondiamo ACK\n"));
            uint8_t msg_crc = gencrc(buffer_ref.message, buffer_ref.msglen);
            if (inserted_system_ack_replies < max_system_replies_length && inserted_replies < max_reply_queue_length)
            {
                unsigned int locked_idx = inserted_system_ack_replies++;
                system_ack_replies[locked_idx].header[0] = pack(0, 0, true);
                system_ack_replies[locked_idx].data[0] = incoming_buffer.ID;
                system_ack_replies[locked_idx].data[1] = segment_number_last;
                system_ack_replies[locked_idx].data[bytes_segment_capacity - 1] = msg_crc;
                rs_encode(system_ack_replies[locked_idx].header);
                replies[inserted_replies++].message_pointer = &system_ack_replies[locked_idx];
            }
            else
            {
                CATASTROPHIC(printf("Ran out of ACK (%d) or replies (%d) space.\n", inserted_system_ack_replies < max_system_replies_length, inserted_replies < max_reply_queue_length));
            }

            if (msg_crc != buffer_ref.CRC && buffer_ref.consumed)
            {
                CATASTROPHIC(printf("Different CRC with consumed frame, imposing to consume.\n"));
                buffer_ref.consumed = false;
                PKLOGGER(PL.write_log(TX, buf_id + 1, Packet_Types::WTF, segment_number_last, "Different CRC on consumed frame"));
            }
            buffer_ref.CRC = msg_crc;

            PKLOGGER(PL.write_log(TX, buf_id + 1, Packet_Types::ACK, segment_number_last));
            // printf("Questo messaggio era lungo %d e ha CRC %d\n", buffer_ref.msglen, counter);
            return true; // Return true so the caller knows they have to consume the frame
        }
        else
        {
            // NACK! Our information about the missing segments is in place, just make the header and encode
            DLOG(printf("Messaggio scorretto, NACK.\n"));
            PKLOGGER(PL.write_log(TX, buf_id + 1, NACK, segment_number_last));
            return false;
        }
    }
    else
    {
        CATASTROPHIC(printf("Ran out of reply space (%d).\n", inserted_replies));
    }

    return false; // We did not ACK nor NACK, but the caller must only know the reception isn't completed (need more data or ran out of space)
}

static uint8_t current_id = 0;

// Returns the number of frames queued for sending from that buffer
unsigned int consume_frame(const unsigned int &buffer, const unsigned int &length)
{
    static uint8_t rolling_helper = 0;

    current_id = 1 + ((++rolling_helper) % (max_id - 1));
    rolling_helper = current_id - 1;

    auto &current_buf = input_buffers[buffer]; // Riferimento locale
    auto &bufdata = current_buf.buffer;

    // Calcolo intero veloce O(1)
    int segments = (length + bytes_segment_capacity - 1) / bytes_segment_capacity;
    int remainder = length % bytes_segment_capacity;
    if (remainder == 0)
        segments++;

    // printf("remainder %d\n", remainder);

    current_buf.last_segment_number = segments - 1;
    current_buf.CRC = gencrc_iovec(current_buf.data_pointers, segments, length);

    PKLOGGER(PL.log_messaggio_completo(TX, buffer + 1, current_buf.CRC, length, segments));

    for (int n = 0; n < segments - 1; n++)
    {
        auto &frame = bufdata[n];
        // Pack and encode
        frame.header[0] = pack(current_id, n, false);
        rs_encode(frame.header);
        PKLOGGER(PL.write_log(TX, buffer + 1, DATA, n));
        DDATA(print_array(frame.header, bytes_payload, bytes_payload));
    }

    // Pack and encode the last segment
    bufdata[segments - 1].data[bytes_segment_capacity - 1] = remainder;
    bufdata[segments - 1].header[0] = pack(current_id, segments - 1, true);
    rs_encode(bufdata[segments - 1].header);

    DDATA(print_array(bufdata[segments - 1].header, bytes_payload, bytes_payload));
    PKLOGGER(PL.write_log(TX, buffer + 1, LAST, segments - 1));

    set_buffer_timer(buffer, (segments * segment_timeout_us) + (2 * segment_timeout_us));

    return segments;
}

static bool consume_packet_single(const segment &msg, unsigned int &buffer_to_consume)
{
    uint8_t id, segment;
    bool last_packet = false;

    unpack(msg.header[0], id, segment, last_packet);

    DLOG(printf("Dati in ingresso: ID %d | segmento %d | l_pkt %d\n", id, segment, last_packet));
    DDATA(print_array(msg.header, bytes_payload, bytes_payload));

    if (id != 0)
    {
        if (id > max_id)
        {
            // Something went horrybly wrong
            CATASTROPHIC(printf("Received id over the max: %d\n", id));
            return false;
        }

        if (id != incoming_buffer.ID)
        {
            // Different ID, should reset.
            DLOG(printf("New ID, input reset.\n"));
            PKLOGGER(PL.write_log(NONE, incoming_buffer.ID, RECEPTION_RESET, 0, "New ID"));

            reception_buffer_reset();
            incoming_buffer.ID = id;
        }

        if (!last_packet)
        {
            // We are dealing with a data packet

            if (incoming_buffer.segments_received[segment] && RESET_ON_DUPLICATES)
            {
                // Duplicate packet. Reset.
                DLOG(printf("Duplicate segment, input reset.\n"));
                PKLOGGER(PL.write_log(NONE, id, RECEPTION_RESET, 0, "Duplicate segment"));

                reception_buffer_reset();
            }
            incoming_buffer.segments_received[segment] = true;

            // Copy to memory!
            memcpy(incoming_buffer.message + (bytes_segment_capacity * segment), msg.data, bytes_segment_capacity);

            if (incoming_buffer.last_segment_number > 0)
            {
                // If this variable is set, we received the last segment at least once.
                // Which means this is the result of a NACK, so we should check if this was the last one.
                if (insert_ack_or_nack_for_buffer(incoming_buffer.last_segment_number, true))
                {
                    if (!incoming_buffer.consumed)
                    {
                        incoming_buffer.consumed = true;

                        DLOG(printf("Messaggio da consumare (dopo dati): LEN %d, CRC %d\n", incoming_buffer.msglen, incoming_buffer.CRC));
                        PKLOGGER(PL.log_messaggio_completo(RX, incoming_buffer.ID, incoming_buffer.CRC, incoming_buffer.msglen));

                        // TODO: send to the upper level interface
                        buffer_to_consume = 0;
                        return true;
                    }
                }
            }
        }
        else
        {
            // We are dealing with a LAST packet. Carries more info!
            uint8_t seglen = msg.data[bytes_segment_capacity - 1]; // How many data bytes is this last segment carrying?
            if (seglen >= bytes_segment_capacity)
            {
                // Corrupted packet!
                CATASTROPHIC(printf("Last segment corrupted: seglen %d .\n", seglen));
                PKLOGGER(PL.write_log(NONE, incoming_buffer.ID, RECEPTION_RESET, segment, "Segment over capacity"));
                return false;
            }
            unsigned int calculated_msg_len = (segment * bytes_segment_capacity) + seglen;

            PKLOGGER(PL.write_log(RX, incoming_buffer.ID, LAST, segment));

            // If the msglen was already calculated, see if it is different.
            // Chances are we lost all previous segments and tx is trying to send a new message
            // So reset the message now, copy the data and ask for all the missing segments.
            if (calculated_msg_len != incoming_buffer.msglen && incoming_buffer.msglen != 0)
            {
                PKLOGGER(PL.write_log(NONE, incoming_buffer.ID, RECEPTION_RESET, segment, "Message Length Mismatch"));
                reception_buffer_reset();
            }

            incoming_buffer.msglen = calculated_msg_len;
            incoming_buffer.last_segment_number = segment;

            if (!incoming_buffer.segments_received[segment])
            {
                // Marking as received
                incoming_buffer.segments_received[segment] = true;
            }

            // Copy to memory
            memcpy(incoming_buffer.message + (bytes_segment_capacity * segment), msg.data, seglen);

            if (insert_ack_or_nack_for_buffer(incoming_buffer.last_segment_number))
            {
                // We should consume the frame!
                if (!incoming_buffer.consumed)
                {
                    incoming_buffer.consumed = true;

                    DLOG(printf("Messaggio da consumare: LEN %d, CRC %d\n", incoming_buffer.msglen, incoming_buffer.CRC));
                    PKLOGGER(PL.log_messaggio_completo(RX, incoming_buffer.ID, incoming_buffer.CRC, incoming_buffer.msglen));

                    // TODO: send to the upper level interface
                    buffer_to_consume = 0;
                    return true;
                }
            }
        }
    }
    else
    {
        // Let's handle ACK or NACK
        if (last_packet)
        {
            // Dealing with ACK
            // The id is contained in segment now

            if (segment != 0)
            {
                CATASTROPHIC(printf("Segment != 0 (%d). bad ACK\n", segment));
                PKLOGGER(PL.write_log(RX, segment, WTF, 0, "ACK segment verification failed"));

                return false;
            }
            uint8_t msg_id = msg.data[0];
            if(msg_id == 0 || msg_id > max_id){
                 CATASTROPHIC(printf("ID (%d). bad ACK\n", msg_id));
                PKLOGGER(PL.write_log(RX, msg_id, WTF, 0, "ACK ID verification failed"));

                return false;
            }
            if (msg_id == current_id)
            {

                // ACK
                // If the transmission is successful, we will match the full message CRC
                // and the number of segments that was sent (-1 to avoid overflow)
                bool checkCRC = input_buffers[0].CRC == msg.data[bytes_segment_capacity - 1];
                bool checkSegments = input_buffers[0].last_segment_number == msg.data[1];
                if (checkCRC && checkSegments)
                {
                    DLOG(printf("ACK: Confermato ID %d, SEG %d, CRC %d\n", current_id, msg.data[1], msg.data[bytes_segment_capacity - 1]));
                    //  Everything corresponds. We successfully transferred a message!
                    //  Hooray! Let's free that buffer for new data to come in.
                    PKLOGGER(PL.write_log(RX, current_id, ACK, input_buffers[buf_id].last_segment_number));
                    // TODO: Free upper level buffer and reschedule
                    // if(!is_buffer_free(buf_id)){
                    clear_buffer_timer(0);
#ifdef WAIT_WINDOW_COMPLETION
                    on_frame_completed();
#else
                    free_frame(0);
#endif
                    //}
                }
                else
                {
                    DLOG(
                        printf("ACK invalido.\n");
                        if (!checkCRC)
                            printf("Controllo CRC fallito: atteso %d | trovato %d\t", input_buffers[0].CRC, msg.data[bytes_segment_capacity - 1]);
                        if (!checkSegments)
                            printf("Controllo dimensione segmenti fallito\t");
                        printf("\n"););

                    PKLOGGER(
                        char out_str[] = "CRC Check: %d | Seg Check: %d";
                        sprintf(out_str, out_str, checkCRC, checkSegments);
                        PL.write_log(RX, current_id, INVALID_ACK, 0, out_str););

                    unsigned int rescheduled = 0;
                    // Something is wrong with this ACK - might be an ack that does not have anything to do with the current message
                    // We want to reschedule all current segments out.
                    // If this is a duplicate ACK, there is no hurt.
                    // If the message was partially delivered, we will get a NACK.
                    for (unsigned int i = 0; i <= input_buffers[0].last_segment_number; i++)
                    {
                        if (reschedule_segment(0, i))
                        {
                            rescheduled++;
                            PKLOGGER(PL.write_log(TX, current_id, RESEND_AFTER_INVALID_ACK, i));
                            continue;
                        }
                        else
                            break;

                        set_buffer_timer(0, (rescheduled * segment_timeout_us) + (2 * segment_timeout_us));
                    }
                }
            }
            else
            {
                CATASTROPHIC(printf("ACK about free buffer.\n"));
                PKLOGGER(PL.write_log(RX, buf_id + 1, FREE_ACK, 0));
            }
        }
        else
        {
            // Dealing with NACK
            // Segment must be ZERO
            if (segment != 0)
            {
                CATASTROPHIC(printf("Nonzero segment id (%d). bad NACK\n", segment));
                PKLOGGER(PL.write_log(RX, buf_id + 1, WTF, 0, "Larger id than pool"));
                return false;
            }

            DLOG(printf("NACK: rinvio "));

            // NACK
            // We're asked to reschedule N segments.
            // Let's get to it immediately:
            uint8_t how_many = msg.data[bytes_segment_capacity - 1];
            if (how_many > bytes_segment_capacity - 1 || how_many == 0)
            {
                CATASTROPHIC(printf("Invalid nack, %d segments requested.\n", how_many));
                PKLOGGER(PL.write_log(RX, 0, WTF, 0, "Invalid NACK, too many"));
                return false;
            }

            unsigned int rescheduled = 0;
            uint8_t r_id, r_segment;
            bool r_lpkt;

            for (unsigned int i = 0; i < how_many; i++)
            {
                unpack(msg.data[i], r_id, r_segment, r_lpkt);

                DLOG(printf("%d(%d) ", r_id, r_segment));

                if (r_id == current_id)
                {
                    if (r_segment <= input_buffers[0].last_segment_number)
                    {
                        if (reschedule_segment(0, r_segment))
                        {
                            rescheduled++;
                            

                            PKLOGGER(PL.write_log(TX, r_id, RESEND_ASKED, i));
                        }
                        else{
                            break;
                        }
                    }
                    else
                    {
                        CATASTROPHIC(printf("Requested segments that don't exist ID: %d( seg: %d : max %d).\n", r_id, r_segment, input_buffers[0].last_segment_number));
                        PKLOGGER(PL.write_log(RX, r_id, INVALID_NACK, 0, "Inexistent segments"));
                        break;
                    }
                }
                else
                {
                    CATASTROPHIC(printf("Requested segment in inactive buffer. %d(%d)\n", r_id, r_segment));
                    PKLOGGER(PL.write_log(RX, r_id, INVALID_NACK, 0, "Inactive buffer"));
                    break;
                }
                DLOG(printf("\n"));
            }
            DLOG(printf("\n"));

            if(rescheduled > 0)
                set_buffer_timer(0, (rescheduled * segment_timeout_us) + (2 * segment_timeout_us));
        }
    }

    return false;
}

static void timer_fired(unsigned int buf_id)
{

    if (is_timer_active(buf_id))
    {
        CATASTROPHIC("Timer fired, but it is still ticking! Skipping.\n");
        return;
    }
    // Timer associated with a buffer has fired.
    // See if we want to reschedule the segment or scrap the frame
    if (!is_buffer_free(buf_id))
    {
        if (buffer_send_retries[buf_id]++ < segment_retries_max)
        {
            // Reschedule
            DLOG(printf("Buffer %d resent.\n", buf_id));
            PKLOGGER(PL.write_log(TX, buf_id + 1, RESEND_TIMEOUT, input_buffers[buf_id].last_segment_number));

            reschedule_segment(buf_id, input_buffers[buf_id].last_segment_number);
            set_buffer_timer(buf_id, segment_timeout_us + (2 * segment_timeout_us));
        }
        else
        {
            // Scrap the frame
            DLOG(printf("Buffer %d has expired.\n", buf_id));
            PKLOGGER(PL.write_log(NONE, buf_id + 1, EXPIRED, input_buffers[buf_id].last_segment_number));
            clear_buffer_timer(buf_id);
#ifdef WAIT_WINDOW_COMPLETION
            on_frame_completed();
#else
            free_frame(buf_id);
#endif
        }
    }
    else
    {
        DLOG(printf("Buffer %d is free, timer skipped.\n", buf_id));
    }
}

static void reset_reply_counters()
{
    inserted_replies = 0;
    inserted_system_ack_replies = 0;
    queued_nack_items = 0;
    queued_nack_packets = 0;

    for(int i = 0; i<max_frames; i++){
        memset(input_buffers[i].queued_for_resend, false, max_segments);
        
    }
}

static unsigned int replies_duplicates()
{
    unsigned int found = 0;
    for (unsigned int i = 0; i < inserted_replies; i++)
    {
        for (unsigned int j = i + 1; j < inserted_replies; j++)
        {
            if (replies[i].message_pointer == replies[j].message_pointer)
            {
                CATASTROPHIC(
                    if (found == 0)
                        printf("Duplicate items:");
                    uint8_t id, seg;
                    bool fpkt;
                    unpack(replies[i].message_pointer->header[0], id, seg, fpkt);
                    printf(" %d(%d, %d)", id, seg, fpkt););

                found++;
            }
        }
    }
    CATASTROPHIC(
        if (found != 0) {
            printf("\nOutbound replies contain %d duplicates. (possibly less. must be ZERO duplicates).", found);
        });
    return found;
}

static int find_free_radio_input_buffer()
{
    for (int i = 0; i < radio_input_buffer_num; i++)
    {
        if (!radio_interrupt_buffers[i].lock)
        {
            return i;
        }
    }
    return -1;
}

#endif