#pragma once

#include <stdint.h>

//#define WAIT_WINDOW_COMPLETION
//#define ONE_AT_A_TIME

constexpr bool reset_on_duplicates = false;

// #define USE_SCRAMBLE_KEY

#ifdef USE_SCRAMBLE_KEY
constexpr bool use_scramble_keys = true;
#else
constexpr bool use_scramble_keys = false;
#endif

#include "../../../logging.h"

constexpr uint8_t bits_id = 2;
constexpr uint8_t bits_segment = 5;
constexpr uint8_t bits_lastpacket = 1;

constexpr uint8_t bytes_data = 29;

constexpr uint8_t bytes_ECC = 3;
// Minimum 2 bytes

// Default: 5000
constexpr unsigned int segment_timeout_us = 6000;
constexpr unsigned int segment_retries_max = 5000;
constexpr unsigned int frame_timeout_ms = 500;

constexpr unsigned int max_radio_buffer_segments = 8;
constexpr unsigned int radio_input_buffer_num = 32;

// ###########################################################################

constexpr unsigned int frame_timeout_us = frame_timeout_ms * 1000;

#include <ctime>

unsigned long long system_millis()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

constexpr uint8_t bytes_header = 2;
constexpr uint8_t bytes_segment_capacity = bytes_data - bytes_header;

// Bit shift: 1 << bits equals 2^bits
constexpr unsigned int max_segments = 1u << bits_segment;
constexpr unsigned int max_id = 1u << bits_id;

constexpr unsigned int bytes_payload = bytes_data + bytes_ECC;

constexpr unsigned int MTU = (bytes_segment_capacity * max_segments) - 1; // -1 to store the last packet length byte



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

