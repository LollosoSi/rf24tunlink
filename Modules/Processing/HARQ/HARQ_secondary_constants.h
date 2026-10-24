#pragma once

#include "RSCodec.h"

#include "HARQ_constants.h"

#include <assert.h>

#include <cstring>

#include <sys/uio.h>

constexpr unsigned int max_radio_buffer_segments = 9;
constexpr unsigned int radio_input_buffer_num = 4;

constexpr uint8_t bytes_header = 1;
constexpr uint8_t bytes_segment_capacity = bytes_data - bytes_header;

// Bit shift: 1 << bits equals 2^bits
constexpr unsigned int max_segments = 1u << bits_segment;
constexpr unsigned int max_id = 1u << bits_id;

constexpr unsigned int MTU = (bytes_segment_capacity * max_segments) - 2; // -1 Is the last packet length byte and -1 for crc

constexpr unsigned int bytes_payload = bytes_data + bytes_ECC;

#include "HARQ_timers.h"

// - - - - - - - - - - - - - -

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
static void unpack(const uint8_t &data, uint8_t &id, uint8_t &segment, bool &last_packet)
{
    id = id_mask & (data >> id_byte_pos_offset);
    segment = segment_mask & (data >> segment_byte_pos_offset);
    last_packet = last_packet_mask & (data >> last_packet_byte_pos_offset);
}

// - - - - - - - - - - - -

// Segment and outbound buffers

constexpr unsigned int contemporary_outbound_buffers = max_id - 1;

#pragma pack(push, 1)
struct segment
{
    uint8_t header[bytes_header];
    uint8_t data[bytes_segment_capacity];
    uint8_t trailer[bytes_ECC];
};
#pragma pack(pop)

static void unpack(const segment data_seg, uint8_t &id, uint8_t &segment, bool &last_packet)
{
    unpack(data_seg.header[0], id, segment, last_packet);
}

#pragma pack(push, 1)
struct frame_input
{
    unsigned long ms_start = 0;
    uint8_t key_idx = 0;
    uint8_t CRC = 0;
    uint8_t last_segment_number = 0;
    iovec data_pointers[max_segments];
    segment buffer[max_segments];
};
#pragma pack(pop)

frame_input input_buffers[contemporary_outbound_buffers];

static uint8_t next_key(const uint8_t buffer_idx)
{
    return (input_buffers[buffer_idx].key_idx + 1) % scramble_keys_number;
}

//  - - - - - - - - - - - - - - 
// Batch reply system

constexpr unsigned int max_reply_queue_length = MTU * 2 * contemporary_outbound_buffers;
constexpr unsigned int max_system_replies_length = MTU * contemporary_outbound_buffers;

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

static bool reschedule_segment(const unsigned int &buffer, const unsigned int &segment)
{
    if (inserted_replies < max_reply_queue_length)
    {
        replies[inserted_replies++].message_pointer = &input_buffers[buffer].buffer[segment];
        return true; // Successfully rescheduled
    }
    // Couldn't reschedule
    return false;
}

// Frame lock - unlock logic
// buffer 0 must be ID 1

uint8_t completed_frames = 0;
uint8_t free_frames = contemporary_outbound_buffers;
uint8_t free_frames_array_default_copy[contemporary_outbound_buffers] = {0};
uint8_t free_frames_array[contemporary_outbound_buffers] = {0};
bool free_frames_by_index[contemporary_outbound_buffers] = {true};

bool frame_ready_oneatatime[contemporary_outbound_buffers];

static void initialize_free_frames_array()
{
    for (unsigned int i = 0; i < contemporary_outbound_buffers; i++)
    {
        free_frames_array[i] = (contemporary_outbound_buffers)-1 - i;
        free_frames_array_default_copy[i] = (contemporary_outbound_buffers)-1 - i;
    }
    memset(free_frames_by_index, true, contemporary_outbound_buffers);
    memset(frame_ready_oneatatime, false, contemporary_outbound_buffers);
}

static bool is_buffer_free(unsigned int buf_id)
{
    assert(buf_id < (contemporary_outbound_buffers));
    return free_frames_by_index[buf_id];
}

static bool any_buffers_free()
{
    return free_frames > 0;
}

static void free_frame(uint8_t buffer_id)
{
    if (is_buffer_free(buffer_id))
    {
        CATASTROPHIC(printf("Double free for frame %d\n", buffer_id););
        return;
    }
    free_frames_array[free_frames++] = buffer_id;
    free_frames_by_index[buffer_id] = true;
    frame_ready_oneatatime[buffer_id] = false;
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

bool out_completed = true;
static unsigned int current_frame_out = 0;
static void schedule_frame(uint8_t buf_id)
{
    DLOG(printf("Scheduling frame buf %d\n", buf_id););
    for (unsigned int i = 0; i <= input_buffers[buf_id].last_segment_number; i++)
    {
        reschedule_segment(buf_id, i);
    }

    set_buffer_timer(buf_id, ((input_buffers[buf_id].last_segment_number + 1) * segment_timeout_us) + (4 * segment_timeout_us));
    input_buffers[buf_id].ms_start = system_millis();
    out_completed = false;
    current_frame_out = buf_id;
}

static void schedule_next_frame_if_available()
{
    if (out_completed)
    {
        for (unsigned int i = 0; i < contemporary_outbound_buffers; i++)
        {
            if (frame_ready_oneatatime[(current_frame_out + i) % contemporary_outbound_buffers])
            {
                schedule_frame((current_frame_out + i) % contemporary_outbound_buffers);
                return;
            }
        }
    }
}

static void one_at_a_time_input(uint8_t buf_id)
{
    frame_ready_oneatatime[buf_id] = true;
    schedule_next_frame_if_available();
}

static void on_frame_completed(uint8_t buf_id)
{
#ifdef ONE_AT_A_TIME
    out_completed = true;
    frame_ready_oneatatime[buf_id] = false;
    if (++completed_frames == (contemporary_outbound_buffers))
    {
        completed_frames = 0;
        memcpy(free_frames_array, free_frames_array_default_copy, contemporary_outbound_buffers);
        memset(free_frames_by_index, true, contemporary_outbound_buffers);
        free_frames = contemporary_outbound_buffers;

        DLOG(printf("All inputs freed. Waiting for more data.\n"));
    }
    current_frame_out = (current_frame_out + 1) % contemporary_outbound_buffers;
    schedule_next_frame_if_available();
#elif defined(WAIT_WINDOW_COMPLETION)
    if (++completed_frames == (contemporary_outbound_buffers))
    {
        completed_frames = 0;
        memcpy(free_frames_array, free_frames_array_default_copy, contemporary_outbound_buffers);
        memset(free_frames_by_index, true, contemporary_outbound_buffers);
        free_frames = contemporary_outbound_buffers;

        DLOG(printf("All inputs freed. Waiting for more data.\n"));
    }
#else
    free_frame(buf_id);
#endif
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
        input_buffers[j].data_pointers[pointerslen - 1] = {input_buffers[j].buffer[pointerslen - 1].data, bytes_segment_capacity - 2}; // The last packet is mandatory with one less byte
    }

    initialize_free_frames_array();
    initialize_buffer_fds();
}

// - - - - - - - - - - -
// Reception buffers

#pragma pack(push, 1)
struct reception_buffer
{
    uint8_t key = 255;
    uint8_t CRC = 0;
    uint8_t received_CRC = 0;
    unsigned int msglen = 0;
    uint8_t last_segment_number = 0;
    bool consumed = false; // If we should consume or only ACK when receiving the same last segment
    bool segments_received[max_segments] = {false};
    uint8_t message[MTU] = {0};
};
#pragma pack(pop)

reception_buffer reception_buffers[contemporary_outbound_buffers];

static void reception_buffer_reset(reception_buffer *buffer)
{
    buffer->msglen = 0;
    memset(buffer->segments_received, false, max_segments);
    buffer->consumed = false;
    buffer->last_segment_number = 0;
    buffer->key = 255;
    buffer->CRC = 0;
    buffer->received_CRC = 0;
}

static void print_reception_buffer_state(uint8_t buf_id)
{
    reception_buffer *buffer = reception_buffers + buf_id;
    printf("Reception buffer state: ");
    printf("\n\tConsumed: %d\tMSGLEN: %d\tCRC: %d\tSegments: %d\tID: %d, KEY: %d\n", buffer->consumed, buffer->msglen, buffer->CRC, buffer->last_segment_number, buf_id + 1, buffer->key);
    printf("Status of reception:");
    for (int i = 0; i <= buffer->last_segment_number; i++)
        printf(" %d", buffer->segments_received[i]);
    printf("\nIf you see any ZERO here, something went horribly wrong.\n");
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

// - - - - - - - - - -
