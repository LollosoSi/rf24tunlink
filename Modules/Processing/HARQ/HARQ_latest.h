#pragma once

#include "HARQ_constants.h"
#include "HARQ_timers_latest.h"
#include "HARQ_mandatory_interfaces.h"
#include "HARQ_data_structures.h"
#include "HARQ_internal_events.h"
// #include "HARQ_signature_checking.h" // Checks for duplicates, but it's too expensive

#include "HARQ_input_buffer_tun_handler.h"

#include <string.h>

unsigned int timer_id_when_set = -1;
// bool expecting_timer_0 = false, expecting_timer_1 = false;

iovec *get_buffer_iovec(frame_out &f)
{
    return f.data_pointers;
}

void set_timer_segments(unsigned int segments = 1)
{
    TIMERLOG("Setting timer action %d", SEGMENT_RESEND);
    // expecting_timer_1 = true;

    // set_buffer_timer(action, ((4 + segments) * segment_timeout_us));
    // set_buffer_timer(SEGMENT_RESEND, current_out_buffer, segment_timeout_us);
	// FIX: Timer is based on ongoing segments + reply margin
	set_buffer_timer(SEGMENT_RESEND, current_out_buffer, (2+segments)*segment_timeout_us);
}

void set_timer_frame(unsigned int us)
{
    TIMERLOG("Setting timer action %d", FRAME_EXPIRED);
    // expecting_timer_0 = true;
    //  set_buffer_timer(action, ((4 + segments) * segment_timeout_us));
    set_buffer_timer(FRAME_EXPIRED, current_out_buffer, frame_timeout_us);
}

void initialize_HARQ()
{
    init_frames();

    initialize_fd_internalevents();

    initialize_free_frames_array();

    initialize_buffer_fds();
    DLOG("HARQ Initialized");
}

extern void frame_completed(unsigned int buffer);

void send_out_current_frame()
{
    if (current_frame_out().issued_ms + frame_timeout_ms < system_millis())
    {
        // Too late! Boo. This frame was never born
        frame_completed(current_out_buffer);
        DLOG("Dropping late frame. This was never sent!");
        return;
    }

    radio_write_threadsafe(segment_beginning(current_frame_out().buffer[0]), current_frame_out().last_segment_number + 1, bytes_payload);

    LOGTX("Frame ID %d sent in %d segments", current_frame_out().id, current_frame_out().last_segment_number + 1);

    //set_timer_segments(1);
	// FIX: wait based on the number of segments
	set_timer_segments(current_frame_out().last_segment_number + 1);
    set_timer_frame(frame_timeout_us);
}

void consume_frame(frame_out &out, unsigned int length)
{
    // Preparing
    set_frame_data(out, length);

    // Log prima dell'invio
    LOGTX("Buffered Frame. Len: %d bytes", length);
    if (is_buffer_completed(current_out_buffer) || &out == &current_frame_out())
        if (next_locked_frame())
            send_out_current_frame();
}

unsigned int resend_retries = 0;

void frame_completed(unsigned int buffer)
{
    if (buffer != current_out_buffer)
    {
        DLOG("Frame completed fired for buffer %d while current is %d", buffer, current_out_buffer);
        return;
    }

    if (is_buffer_completed(current_out_buffer))
        return;

    LOGTX("Frame ID %d Completed. Freeing buffer.", current_frame_out().id);
    clear_buffer_timer(FRAME_EXPIRED, current_frame_out().id - 1);
    clear_buffer_timer(SEGMENT_RESEND, current_frame_out().id - 1);
    resend_retries = 0;

    // Find the next buffered frame and send!
    if (current_frame_completed())
        send_out_current_frame();

    frame_completed_signal();
}

void consume_segment(segment &msg)
{
    frame_out &tun_frame = current_frame_out();

    uint8_t id, seg;
    bool lp;

    unpack(msg.header[0], id, seg, lp);

    uint8_t SEQ_number = msg.header[1];

    // Log verbose per debug pacchetti
    LOGRX("Pkt In: ID %d, CRC %d, Seg %d, LP %d", id, SEQ_number, seg, lp);

    if (id == 0)
    {
        if (seg != 0)
        {
            CATASTROPHIC("Protocol Violation: System packet with Seg %d != 0", seg);
            return;
        }
        if (SEQ_number != tun_frame.SEQ)
        {
            uint8_t how_many_requests = msg.data[bytes_segment_capacity - 1];
            CATASTROPHIC("Different SEQ number in system packet. Expected/found: %d/%d. Ignoring. Was ACK or NACK? Requests : %d", tun_frame.SEQ, SEQ_number, how_many_requests);

            return;
        }
        if (lp)
        {

            uint8_t how_many_requests = msg.data[bytes_segment_capacity - 1];
            if (how_many_requests == 0)
            {
                // ACK!

                bool ver_CRC = tun_frame.CRC == msg.data[0];
                bool ver_id = tun_frame.id == msg.data[1];
                bool ver_segment = tun_frame.last_segment_number == msg.data[2];
                if (ver_id && ver_CRC && ver_segment)
                {
                    LOGRX("Valid ACK received for Frame ID %d", tun_frame.id);
                    // send_internal_event(FRAME_COMPLETED, current_out_buffer);
                    frame_completed(current_out_buffer);
                }
                else
                {
                    CATASTROPHIC("Bad ACK received! Expected ID %d/CRC %d/SEG %d, got ID %d/CRC %d/SEG %d",
                                 tun_frame.id, tun_frame.CRC, tun_frame.last_segment_number, msg.data[1], msg.data[0], msg.data[2]);

                    // Bad ack. we could ignore it or resend all
                    radio_write_threadsafe(segment_beginning(tun_frame.buffer[0]), tun_frame.last_segment_number + 1, bytes_payload);
                    set_timer_segments(tun_frame.last_segment_number + 1);
                    // frame_completed();
                }
            }
            else if (how_many_requests < (bytes_segment_capacity - 1))
            {
                // NACK!
                LOGRX("NACK received: requesting %d segments", how_many_requests);

                unsigned int queued = 0;
                segment *queue_out_segments_nack[bytes_segment_capacity - 1];
                for (uint8_t i = 0; i < how_many_requests; i++)
                {
                    uint8_t id_r, seg_r;
                    bool fp_r;
                    unpack(msg.data[i], id_r, seg_r, fp_r);

                    if (id_r != tun_frame.id || seg_r > tun_frame.last_segment_number)
                    {
                        CATASTROPHIC("Invalid NACK content: ID %d (Curr: %d), Seg %d (Last: %d)",
                                     id_r, tun_frame.id, seg_r, tun_frame.last_segment_number);
                        return;
                    }

                    queue_out_segments_nack[queued++] = &tun_frame.buffer[seg_r];
                }

                LOGTX("Resending %d segments due to NACK", queued);
                uint8_t outbuffer[queued*bytes_payload];
				for (uint8_t i = 0; i < queued; i++)
                {
					// Copy to buffer instead of multiple mutex calls
                    // radio_write_threadsafe(segment_beginning(*queue_out_segments_nack[i]), 1, bytes_payload);
                	memcpy(outbuffer+(i*bytes_payload), segment_beginning(*queue_out_segments_nack[i]), bytes_payload);	
				}
				radio_write_threadsafe(outbuffer, queued, bytes_payload);
                set_timer_segments(queued);
            }
        }
        else
        {
            CATASTROPHIC("Protocol Violation: System packet with LP %d != 1", lp);
        }
    }
    else
    {
        if ((id - 1) > packet_eaters_size)
        {
            CATASTROPHIC("Segment with ID larger than the pool! ID: %d", id);
        }

        static uint8_t last_id = max_id;
        // if (1 + ((last_id + 1) % (max_id - 1)) == id)
        //{
        //     packet_eaters[id - 1].reset();
        // }
        last_id = id;
        // uint8_t eater_id = id - 1; // Disabled
        uint8_t eater_id = 0;

        PacketEater &current_eater = packet_eaters[eater_id];
        current_eater.consume_packet(msg, id, seg, lp, SEQ_number);
        if(current_eater.is_finished()){
			
			// Either if frame is completed with or without LP flag write to TUN once and ACK
			if (!current_eater.get_input_active().consumed)
			{
			    LOGRX("Message ID %d reassembled (Len %d). Writing to TUN.", id, current_eater.get_input_active().msglen);
			    current_eater.get_input_active().consumed = true;
			    frame_write(current_eater.get_input_active().message, current_eater.get_input_active().msglen);
			}
			
			segment response;
			LOGRX("Response is: %s", make_ack_nack(response, current_eater) ? "ACK" : "NACK");
			radio_write_threadsafe(segment_beginning(response), 1, bytes_payload);
			
		}else if(lp){
			LOGRX("LP received for ID %d. Sending response.", id);
			// Still unfinished, but we got a last packet, send NACK immediately
			segment response;
			LOGRX("Response is: %s", make_ack_nack(response, current_eater) ? "ACK" : "NACK");
			radio_write_threadsafe(segment_beginning(response), 1, bytes_payload);
	
		}
		
		/** Old block - AI suggested fix above here
		if (lp)
        {
            LOGRX("LP received for ID %d. Sending response.", id);

            if (current_eater.is_finished() && !current_eater.get_input_active().consumed)
            {
                LOGRX("Message ID %d reassembled (Len %d). Writing to TUN.", id, current_eater.get_input_active().msglen);
                current_eater.get_input_active().consumed = true;
                frame_write(current_eater.get_input_active().message, current_eater.get_input_active().msglen);
            }

            segment response;
            LOGRX("Response is: %s", make_ack_nack(response, current_eater) ? "ACK" : "NACK");
            radio_write_threadsafe(segment_beginning(response), 1, bytes_payload);
        }else if (current_eater.is_finished())
        {
            segment response;
            LOGRX("Response is: %s", make_ack_nack(response, current_eater) ? "ACK" : "NACK");
            radio_write_threadsafe(segment_beginning(response), 1, bytes_payload);
        }**/
    }
}

inline void consume_radio_packets(const unsigned int buffer)
{
    for (unsigned int i = 0; i < radio_interrupt_buffers[buffer].written_items; i++)
        if (rs_decode(segment_beginning(radio_interrupt_buffers[buffer].segments[i])))
        {
            consume_segment(radio_interrupt_buffers[buffer].segments[i]);
        }
        else
        {
            DLOG("Scrapped corrupted radio packet.");
        }

    radio_interrupt_buffers[buffer].lock = false;
}

void timer_fired(unsigned int timer_index)
{
    uint8_t fid = 0;
    TIMER_ACTIONS action = unpack_id_action_timer(timer_index, fid);
    if (fid != current_out_buffer)
    {
        DLOG("Skipping timer for different id");
        return;
    }

    if (is_buffer_completed(current_out_buffer))
    {
        DLOG("Skipping timer, buffer is completed");
        return;
    }

    switch (action)
    {
    case FRAME_EXPIRED:

        // if (expecting_timer_0)
        //{
        CATASTROPHIC("TIMER: Frame ID %d EXPIRED (Timeout).", current_out_buffer);
        //    expecting_timer_0 = false;
        frame_completed(current_out_buffer);
        //}

        break;

    case SEGMENT_RESEND:
        // if (expecting_timer_1)
        //{
        //     expecting_timer_1 = false;

        if (increment_retries(SEGMENT_RESEND, current_out_buffer) < segment_retries_max)
        {
            TIMERLOG("TIMER: Resend Triggered. Pinging last segment of ID %d.", current_frame_out().id);
            // Only send the last segment to trigger NACK
            radio_write_threadsafe(segment_beginning(current_frame_out().buffer[current_frame_out().last_segment_number]), 1, bytes_payload);
            set_timer_segments();
        }
        else
        {
            TIMERLOG("TIMER: Resend Triggered but expired. ID %d.", current_out_buffer);
            frame_completed(current_out_buffer);
        }
        //}
        break;
    }
}