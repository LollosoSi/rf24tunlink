#pragma once

#include "HARQ_constants.h"
#include "HARQ_timers_latest.h"
#include "HARQ_mandatory_interfaces.h"
#include "HARQ_data_structures.h"

static bool busy = false;
static frame_out tun_frame;
static PacketEater packet_eater;

static bool is_buffer_free()
{
    return !busy;
}

static iovec *get_buffer_iovec()
{
    return tun_frame.data_pointers;
}

static void set_timer(TIMER_ACTIONS action, unsigned int segments = 1)
{
    set_buffer_timer(action, ((4 + segments) * segment_timeout_us));
}

static void initialize_HARQ()
{
    initialize_frame_out(tun_frame);
    initialize_segment_space_frame_in(packet_eater.input);
}

static void consume_frame(unsigned int length)
{
    busy = true;
    unsigned int segments;
    radio_write(tun_frame.buffer[0].header, segments = set_frame_data(tun_frame, length), bytes_payload);
    set_timer(SEGMENT_RESEND, segments);
    set_buffer_timer(FRAME_EXPIRED, frame_timeout_us);
}

static void frame_completed()
{
    clear_buffer_timer(FRAME_EXPIRED);
    clear_buffer_timer(SEGMENT_RESEND);
    busy = false;
}

static void consume_segment(segment &msg)
{
    uint8_t id, seg;
    bool lp;

    unpack(msg.header[0], id, seg, lp);

    if (id == 0)
    {
        if (seg != 0)
            return;
        uint8_t how_many_requests = msg.data[bytes_segment_capacity - 1];
        if (how_many_requests == 0)
        {
            // ACK!
            bool ver_id = tun_frame.id == msg.data[1];
            bool ver_CRC = tun_frame.CRC == msg.data[0];
            if (ver_id && ver_CRC)
            {
                frame_completed();
            }
            else
            {
                // Bad ack. we could ignore it or resend all
                radio_write(tun_frame.buffer[0].header, tun_frame.last_segment_number + 1, bytes_payload);
                set_timer(SEGMENT_RESEND, tun_frame.last_segment_number + 1);
            }
        }
        else if (how_many_requests < (bytes_segment_capacity - 1))
        {
            // NACK!
            unsigned int queued = 0;
            segment *queue_out_segments_nack[bytes_segment_capacity - 1];
            for (uint8_t i = 0; i < how_many_requests; i++)
            {
                uint8_t id_r, seg_r;
                bool fp_r;
                unpack(msg.data[i], id_r, seg_r, fp_r);

                if (id_r != tun_frame.id || seg_r > tun_frame.last_segment_number)
                {
                    return;
                }

                queue_out_segments_nack[queued++] = &tun_frame.buffer[seg_r];
            }
            for (uint8_t i = 0; i < queued; i++)
            {
                radio_write(queue_out_segments_nack[i]->header, 1, bytes_payload);
            }
            set_timer(SEGMENT_RESEND, queued);
        }
    }
    else
    {
        packet_eater.consume_packet(msg, id, seg, lp);
        if (lp)
        {
            segment response;
            make_ack_nack(response, packet_eater);
            radio_write(response.header, 1, bytes_payload);

            if (packet_eater.is_finished() && !packet_eater.input.consumed)
            {
                packet_eater.input.consumed = true;
                frame_write(packet_eater.input.message, packet_eater.input.msglen);
            }
        }
    }
}

static void consume_radio_packets(const unsigned int buffer)
{
    for (unsigned int i = 0; i < radio_interrupt_buffers[buffer].written_items; i++)
        consume_segment(radio_interrupt_buffers[buffer].segments[i]);
    radio_interrupt_buffers[buffer].lock = false;
}

void timer_fired(TIMER_ACTIONS action)
{
    if (busy)
        switch (action)
        {
        case FRAME_EXPIRED:
            frame_completed();
            break;

        case SEGMENT_RESEND:
            // Only send the last segment to trigger NACK
            radio_write(tun_frame.buffer[tun_frame.last_segment_number].header, 1, bytes_payload);
            set_timer(SEGMENT_RESEND);

            break;
        }
}