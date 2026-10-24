#pragma once

#include "HARQ_constants.h"

#include "RSCodec.h"

#include <sys/uio.h>

#include <cstring>

#pragma pack(push, 1)
struct segment
{
    uint8_t data[bytes_segment_capacity];
    uint8_t header[bytes_header];
    uint8_t trailer[bytes_ECC];
};
#pragma pack(pop)

inline uint8_t *segment_beginning(segment &seg)
{
    return seg.data;
}

#pragma pack(push, 1)
struct frame_out
{
    unsigned long issued_ms = 0;
    uint8_t SEQ = 0;
    uint8_t id = 0;
    uint8_t CRC = 0;
    uint8_t last_segment_number = 0;
    iovec data_pointers[max_segments];
    segment buffer[max_segments];
};
#pragma pack(pop)

void initialize_frame_out(frame_out &out)
{
    for (unsigned int i = 0; i < max_segments - 1; i++)
    {
        out.data_pointers[i].iov_base = out.buffer[i].data;
        out.data_pointers[i].iov_len = bytes_segment_capacity;
    }
    out.data_pointers[max_segments - 1].iov_base = out.buffer[max_segments - 1].data;
    out.data_pointers[max_segments - 1].iov_len = bytes_segment_capacity - 2;
}

#pragma pack(push, 1)
struct frame_in
{
    uint8_t CRC = 0;
    uint8_t received_CRC = 0;
    uint8_t last_segment_number = 0;
    unsigned int msglen = 0;
    unsigned int received_segments_count = 0;
    bool consumed = false; // If we should consume or only ACK when receiving the same last segment
    bool segments_received[max_segments];
    uint8_t *segment_space_MTU_pointers[max_segments];
    uint8_t message[MTU];
};
#pragma pack(pop)

void initialize_segment_space_frame_in(frame_in &buffer)
{
    for (unsigned int i = 0; i < max_segments; i++)
        buffer.segment_space_MTU_pointers[i] = buffer.message + (bytes_segment_capacity * i);
}

void reset_frame_in(frame_in &buffer)
{
    buffer.CRC = 0;
    buffer.received_CRC = 0;
    buffer.last_segment_number = 0;
    buffer.msglen = 0;
    buffer.received_segments_count = 0;
    buffer.consumed = 0;
    memset(buffer.segments_received, false, max_segments);
    // Not needed to overwrite MTU. Actually much of this stuff probably isn't needed
}

// Radio input

#pragma pack(push, 1)
struct single_radio_buffer
{
    bool lock = false;
    uint8_t written_items = 0;
    segment segments[max_radio_buffer_segments];
};
#pragma pack(pop)

single_radio_buffer radio_interrupt_buffers[radio_input_buffer_num];

int find_free_radio_input_buffer()
{
    for (unsigned int i = 0; i < radio_input_buffer_num; i++)
    {
        if (!radio_interrupt_buffers[i].lock)
        {
            radio_interrupt_buffers[i].lock = true;
            return i;
        }
    }
    return -1;
}

#ifdef multibuffer_eater

struct PacketEater
{
    uint8_t last_received_SEQ = 255;

    const static unsigned int contemporary_SEQ_inputs = 2;

    unsigned int current_id[contemporary_SEQ_inputs] = {256, 256};

    uint8_t current_SEQ[contemporary_SEQ_inputs] = {255, 255};
    frame_in input[contemporary_SEQ_inputs];
    unsigned int active_input = 0;

    void init()
    {
        for (unsigned int i = 0; i < contemporary_SEQ_inputs; i++)
            initialize_segment_space_frame_in(input[i]);
    }

    int find_seq(uint8_t seq)
    {
        for (unsigned int i = 0; i < contemporary_SEQ_inputs; i++)
            if (current_SEQ[i] == seq)
                return i;
        return -1;
    }

    void consume_packet(segment &msg, uint8_t id, uint8_t segment, bool lp, uint8_t recv_SEQ)
    {

        if (id >= max_id)
        {
            DLOG("Consuming packet - Invalid id %d", id);
            return;
        }

        int returned = find_seq(recv_SEQ);
        if (returned == -1)
        {
            active_input = (active_input + 1) % contemporary_SEQ_inputs;
            current_SEQ[active_input] = recv_SEQ;

            reset(active_input);
            current_id[active_input] = id;
            DLOG("Consuming packet - RESET - NEW SEQ %d", recv_SEQ);
        }
        else
        {
            active_input = returned;
            DLOG("Consuming packet - Writing to SEQ %d", recv_SEQ);
        }

        if (id != current_id[active_input])
        {
            reset(active_input);
            current_id[active_input] = id;
            DLOG("Consuming packet - RESET - Different id");
        }

        if (!lp)
        {
            // if (input.consumed)
            //{
            //     reset();
            // }

            if (!input[active_input].segments_received[segment])
            {
                // New data in
                input[active_input].received_segments_count++;
            }
            else if (reset_on_duplicates)
            {
                reset(active_input);
                DLOG("Consuming packet - RESET - Duplicate data");
            }
            input[active_input].segments_received[segment] = true;
            memcpy(input[active_input].segment_space_MTU_pointers[segment], msg.data, bytes_segment_capacity);
            DLOG("Consuming packet - Received data | ID: %d, Segment: %d", id, segment);
        }
        else
        {
            uint8_t frame_crc_received = msg.data[bytes_segment_capacity - 2];
            int8_t seglen = msg.data[bytes_segment_capacity - 1];
            unsigned int msglen = (bytes_segment_capacity * segment) + seglen;

            if (msglen > MTU || seglen > (bytes_segment_capacity - 2))
            {
                DLOG("Consuming packet - Invalid LP segment size");
                return;
            }

            if (input[active_input].consumed && (((segment + 1) != input[active_input].received_segments_count) || (input[active_input].msglen != msglen)))
            {
                DLOG("New packet, resetting\n");
                reset(active_input);
            }

            if (!input[active_input].segments_received[segment])
            {
                // New data in
                input[active_input].received_segments_count++;
            }

            if (seglen > 0)
            {
                memcpy(input[active_input].segment_space_MTU_pointers[segment], msg.data, seglen);
            }

            input[active_input].segments_received[segment] = true;
            input[active_input].last_segment_number = segment;
            input[active_input].msglen = msglen;
            input[active_input].received_CRC = frame_crc_received;

            if (is_finished())
            {
                input[active_input].CRC = gencrc(input[active_input].message, msglen);
                if (input[active_input].CRC != input[active_input].received_CRC)
                {
                    DLOG("Consuming packet - RESET - CRC Failed! ID %d, Expected/CRC %d, LEN %d", id, input[active_input].received_CRC, input[active_input].CRC, msglen);

                    reset(active_input);

                    if (seglen > 0)
                    {
                        memcpy(input[active_input].segment_space_MTU_pointers[segment], msg.data, seglen);
                    }

                    input[active_input].segments_received[segment] = true;
                    input[active_input].last_segment_number = segment;
                    input[active_input].msglen = msglen;
                    input[active_input].received_CRC = frame_crc_received;
                    input[active_input].received_segments_count = 1;
                }
                else
                {
                    DLOG("Consuming packet - Ready for consumption! ID %d, CRC %d, LEN %d", id, input[active_input].CRC, msglen);
                }
            }
            else
            {
                DLOG("Consuming packet - %d segments are still missing", input[active_input].last_segment_number - input[active_input].received_segments_count - 1);
            }
        }
    }

    bool is_finished()
    {
        return ((input[active_input].received_segments_count - 1) == input[active_input].last_segment_number);
    }

    unsigned int write_nack_bytes(uint8_t *bytes_to_write, const unsigned int max_writes = (bytes_segment_capacity - 1))
    {
        // For each segment missing, specify position and ID so we can catch errors or support interleaving.
        unsigned int writes = 0;
        for (unsigned int i = 0; i < input[active_input].last_segment_number && max_writes > writes; i++)
        {
            if (!input[active_input].segments_received[i])
                bytes_to_write[writes++] = pack(current_id[active_input], i, false);
        }
        return writes;
    }

    void reset(unsigned int index)
    {
        reset_frame_in(input[index]);
    }

    uint8_t get_SEQ()
    {
        return current_SEQ[active_input];
    }

    uint8_t get_id_active()
    {
        return current_id[active_input];
    }

    frame_in &get_input_active()
    {
        return input[active_input];
    }
};

#else

struct PacketEater
{
    unsigned int last_received_CRC = 256;

    const static unsigned int contemporary_CRC_inputs = 1;

    unsigned int current_id[contemporary_CRC_inputs] = {256};

    unsigned int current_CRC[contemporary_CRC_inputs] = {256};
    frame_in input[contemporary_CRC_inputs];
    unsigned int active_input = 0;

    void init()
    {
        for (unsigned int i = 0; i < contemporary_CRC_inputs; i++)
            initialize_segment_space_frame_in(input[i]);
    }

    int find_seq(uint8_t seq)
    {
        for (unsigned int i = 0; i < contemporary_CRC_inputs; i++)
            if (current_CRC[i] == seq)
                return i;
        return -1;
    }

    void consume_packet(segment &msg, uint8_t id, uint8_t segment, bool lp, uint8_t recv_SEQ){
        frame_in& current_in = input[active_input];
        if(current_id[active_input] != 255 && current_id[active_input] != id){
            reset(active_input);
        }

        if(current_CRC[active_input] == 256){
            LOGRX("Resetting for new CRC");
            reset(active_input);
            current_CRC[active_input] = recv_SEQ;
        }

        if(lp){
            int8_t seglen = msg.data[bytes_segment_capacity-1];
            
            unsigned int msglen = (bytes_segment_capacity * segment) + seglen;
            if(msglen > MTU || seglen > bytes_segment_capacity-1){
                CATASTROPHIC("Bad segment (mtu or msglen) skipping.");
                return;
            }
            if(current_in.consumed && ( (segment+1) != current_in.received_segments_count || (msglen != current_in.msglen) )){
                LOGRX("Resetting: consumed and different LP");
                reset(active_input);
            }

            current_in.msglen = msglen;
            current_in.received_CRC = msg.header[1];
            current_in.last_segment_number = segment;
        } 
        
        if(current_CRC[active_input] != recv_SEQ){
            if(!current_in.consumed){
               CATASTROPHIC("Unfinished packets have different CRC. Guess corruption or frame was terminated");
            }
            reset(active_input);
            current_CRC[active_input] = msg.header[1];

        }

        if(current_id[active_input] == 255){
            current_id[active_input] = id;
        }

        if(!current_in.segments_received[segment]){
            current_in.received_segments_count++;
        }
        current_in.segments_received[segment] = true;
        LOGRX("Flagging segment: %d, as %d!", segment, current_in.segments_received[segment]);

        memcpy(current_in.message + (bytes_segment_capacity * segment), msg.data, lp ? (msg.data[bytes_segment_capacity-1] > 0 ? msg.data[bytes_segment_capacity-1] : 0) : bytes_segment_capacity);
        
        if(is_finished()){
            fill_CRC();
            if(get_CRC() != get_CRC_calcd()){
                CATASTROPHIC("Recomposed CRC is not as received! %d vs %d", get_CRC_calcd(), get_CRC());
                // Do something about it.

            }
        }
    }


    /**void consume_packet(segment &msg, uint8_t id, uint8_t segment, bool lp, uint8_t recv_SEQ)
    {

        if (id >= max_id)
        {
            DLOG("Consuming packet - Invalid id %d", id);
            return;
        }

        int returned = find_seq(recv_SEQ);
        if (returned == -1)
        {
            active_input = (active_input + 1) % contemporary_SEQ_inputs;
            current_SEQ[active_input] = recv_SEQ;

            reset(active_input);
            current_id[active_input] = id;
            DLOG("Consuming packet - RESET - NEW SEQ %d", recv_SEQ);
        }
        else
        {
            active_input = returned;
            DLOG("Consuming packet - Writing to SEQ %d", recv_SEQ);
        }

        if (id != current_id[active_input])
        {
            reset(active_input);
            current_id[active_input] = id;
            DLOG("Consuming packet - RESET - Different id");
        }

        if (!lp)
        {
            // if (input.consumed)
            //{
            //     reset();
            // }

            if (input[active_input].segments_received[segment] && reset_on_duplicates)
            {
                reset(active_input);

                DLOG("Consuming packet - RESET - Duplicate data");
            }

            input[active_input].received_segments_count++;

            input[active_input].segments_received[segment] = true;
            memcpy(input[active_input].segment_space_MTU_pointers[segment], msg.data, bytes_segment_capacity);
            DLOG("Consuming packet - Received data | ID: %d, Segment: %d", id, segment);
        }
        else
        {
            uint8_t frame_crc_received = msg.data[bytes_segment_capacity - 2];
            int8_t seglen = msg.data[bytes_segment_capacity - 1];
            unsigned int msglen = (bytes_segment_capacity * segment) + seglen;

            if (msglen > MTU || seglen > (bytes_segment_capacity - 2))
            {
                DLOG("Consuming packet - Invalid LP segment size");
                return;
            }

            if (input[active_input].consumed && (((segment + 1) != input[active_input].received_segments_count) || (input[active_input].msglen != msglen)))
            {
                DLOG("New packet, resetting\n");
                reset(active_input);
            }

            if (!input[active_input].segments_received[segment])
            {
                // New data in
                input[active_input].received_segments_count++;
            }

            if (seglen > 0)
            {
                memcpy(input[active_input].segment_space_MTU_pointers[segment], msg.data, seglen);
            }

            input[active_input].segments_received[segment] = true;
            input[active_input].last_segment_number = segment;
            input[active_input].msglen = msglen;
            input[active_input].received_CRC = frame_crc_received;

            if (is_finished())
            {

                fill_CRC();
                if (input[active_input].CRC != input[active_input].received_CRC)
                {
                    CATASTROPHIC("Consuming packet - RESET - CRC Failed! ID %d, Expected/CRC %d, LEN %d", id, input[active_input].received_CRC, input[active_input].CRC, msglen);

                    reset(active_input);

                    if (seglen > 0)
                    {
                        memcpy(input[active_input].segment_space_MTU_pointers[segment], msg.data, seglen);
                    }

                    input[active_input].segments_received[segment] = true;
                    input[active_input].last_segment_number = segment;
                    input[active_input].msglen = msglen;
                    input[active_input].received_CRC = frame_crc_received;
                    input[active_input].received_segments_count = 1;
                }
                else if(!input[active_input].consumed)
                {
                    DLOG("Consuming packet - Ready for consumption! ID %d, CRC %d, LEN %d", id, input[active_input].CRC, msglen);
                } else{
                    DLOG("Consuming packet - Already consumed! ID %d, CRC %d, LEN %d", id, input[active_input].CRC, msglen);

                }
            }
            else
            {
                DLOG("Consuming packet - %d segments are still missing", input[active_input].last_segment_number - input[active_input].received_segments_count - 1);
            }
        }
    } */

    bool is_finished()
    {
        bool ret = ((input[active_input].received_segments_count - 1) == input[active_input].last_segment_number) && input[active_input].msglen > 0;

        return ret;
    }

    uint8_t fill_CRC(){
        return input[active_input].CRC = gencrc(input[active_input].message, input[active_input].msglen);
    }

    unsigned int write_nack_bytes(uint8_t *bytes_to_write, const unsigned int max_writes = (bytes_segment_capacity - 1))
    {
        // For each segment missing, specify position and ID so we can catch errors or support interleaving.
        unsigned int writes = 0;
        for (unsigned int i = 0; i <= input[active_input].last_segment_number && max_writes > writes; i++)
        {
            if (!input[active_input].segments_received[i]){
                DLOG("Missing seg %d of ID %d, position %d", i, current_id[active_input], writes);
                bytes_to_write[writes++] = pack(current_id[active_input], i, false);

            }
        }
        return writes;
    }

    void reset(unsigned int index)
    {
        reset_frame_in(input[index]);
        current_CRC[index] = 256;
        current_id[index] = 255;

    }

    uint8_t get_CRC()
    {
        return current_CRC[active_input];
    }

    uint8_t get_CRC_calcd(){
        return input[active_input].CRC;
    }

    uint8_t get_id_active()
    {
        return current_id[active_input];
    }

    frame_in &get_input_active()
    {
        return input[active_input];
    }
};

#endif

bool make_ack_nack(segment &writable_system_space, PacketEater &receiver)
{
    unsigned int missing = writable_system_space.data[bytes_segment_capacity - 1] = receiver.write_nack_bytes(writable_system_space.data, (bytes_segment_capacity - 1));
    writable_system_space.header[0] = pack(0, 0, true);
    

    if (missing == 0)
    {
        // This is an ACK
        LOGRX("ACKING! ID %d, CRC %d, LEN %d, SgN: %d", receiver.get_id_active(), receiver.get_CRC_calcd(), receiver.get_input_active().msglen, receiver.get_input_active().last_segment_number);

        // Embed CRC
        writable_system_space.data[0] = receiver.get_CRC_calcd();

        // Embed ID
        writable_system_space.data[1] = receiver.get_id_active();

        // Embed the last segment number
        writable_system_space.data[2] = receiver.get_input_active().last_segment_number;
    }
    else
    {
        LOGRX("Not ACKING! ID %d with CRC %d is missing %d", receiver.get_id_active(), receiver.get_CRC(), missing);
    }

    writable_system_space.header[1] = missing == 0 ? receiver.get_CRC_calcd() : receiver.get_CRC();

    rs_encode(segment_beginning(writable_system_space));

    // Explanation: write_nack_bytes will return 0 if the packet is completed.
    // Header: 0x01
    // ACK Structure: Data byte 0: CRC of the received frame, Data byte 1: (number of segments-1), Last data byte = 0 (number of segments to request)
    // NACK Structure: Last data byte = number of segments to request, All the other data bytes pack(id, segment, 0)

    return missing == 0;
}

unsigned int set_frame_data(frame_out &input_frame, unsigned int length)
{
    static uint8_t id_counter = 0;
    id_counter = 1 + ((id_counter + 1) % (max_id - 1));
    input_frame.id = id_counter;
    

    input_frame.issued_ms = system_millis();
    
    // Calcolo intero veloce O(1)
    int segments = (length + bytes_segment_capacity - 1) / bytes_segment_capacity;
    int remainder = length % bytes_segment_capacity;
    if (remainder == 0)
    {
        segments++;
    }

    input_frame.last_segment_number = segments - 1;
    input_frame.CRC = gencrc_iovec(input_frame.data_pointers, segments, length);
    input_frame.SEQ = input_frame.CRC;

    for (int n = 0; n < segments - 1; n++)
    {
        // Pack and encode
        input_frame.buffer[n].header[0] = pack(id_counter, n, false);
        input_frame.buffer[n].header[1] = input_frame.SEQ;
        rs_encode(segment_beginning(input_frame.buffer[n]));
    }

    // Pack and encode the last segment
    input_frame.buffer[segments - 1].data[bytes_segment_capacity - 1] = remainder;
    input_frame.buffer[segments - 1].header[0] = pack(id_counter, segments - 1, true);
    input_frame.buffer[segments - 1].header[1] = input_frame.SEQ;
    rs_encode(segment_beginning(input_frame.buffer[segments - 1]));

    // DDATA(print_array(segment_beginning(bufdata[segments - 1]), bytes_payload, bytes_payload));

    return segments;
}
