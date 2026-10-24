#pragma once

#include "HARQ_secondary_constants.h"

#include <time.h>

extern bool write_output(uint8_t *data, unsigned int length);

// Returns the number of frames queued for sending from that buffer
unsigned int consume_frame(const unsigned int &buffer, const unsigned int &length)
{
    const uint8_t id = buffer + 1;
    auto &current_buf = input_buffers[buffer]; // Riferimento locale
    auto &bufdata = current_buf.buffer;

    current_buf.key_idx = next_key(buffer);

    // Calcolo intero veloce O(1)
    int segments = (length + bytes_segment_capacity - 1) / bytes_segment_capacity;
    int remainder = length % bytes_segment_capacity;
    bool need_to_negative_len = remainder > bytes_segment_capacity - 2;
    if (remainder == 0 || need_to_negative_len)
    {
        segments++;
    }
    int8_t signed_remainder = remainder;
    if (need_to_negative_len)
        signed_remainder = -1;

    // printf("remainder %d\n", remainder);

    current_buf.last_segment_number = segments - 1;
    current_buf.CRC = gencrc_iovec(current_buf.data_pointers, segments, length);

    PKLOGGER(PL.log_messaggio_completo(TX, buffer + 1, current_buf.CRC, length, segments));

    for (int n = 0; n < segments - 1; n++)
    {
        auto &frame = bufdata[n];
        // Pack and encode
        frame.header[0] = pack(id, n, false);
        rs_encode_key(frame.header, current_buf.key_idx);
        PKLOGGER(PL.write_log(TX, buffer + 1, DATA, n));
        DDATA(print_array(frame.header, bytes_payload, bytes_payload));
    }

    // Pack and encode the last segment
    bufdata[segments - 1].data[bytes_segment_capacity - 1] = current_buf.CRC;
    bufdata[segments - 1].data[bytes_segment_capacity - 2] = signed_remainder;
    bufdata[segments - 1].header[0] = pack(id, segments - 1, true);
    rs_encode_key(bufdata[segments - 1].header, current_buf.key_idx);

    DDATA(print_array(bufdata[segments - 1].header, bytes_payload, bytes_payload));

    PKLOGGER(PL.write_log(TX, buffer + 1, LAST, segments - 1));

#ifdef ONE_AT_A_TIME
    one_at_a_time_input(buffer);
    return 0;
#else
    set_buffer_timer(buffer, (segments * segment_timeout_us) + (2 * segment_timeout_us));

    current_buf.ms_start = system_millis();

    return segments;
#endif
}

// This function assumes the nack array in data has been completed
static void finalize_nack_packet(unsigned int index)
{
    system_nack_replies[index].header[0] = pack(0, 0, false);
    rs_encode_key(system_nack_replies[index].header, 0);
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
            DLOG(
                printf("Preventing duplicate segment request in same NACK batch\n");)
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
unsigned long last_nack_ms[contemporary_outbound_buffers];
static bool insert_ack_or_nack_for_buffer(uint8_t &buf_id, bool &mismatched_crc, bool only_ACK_if_finished = false)
{
    unsigned long curms = system_millis();
    mismatched_crc = false;
    if (inserted_replies < max_reply_queue_length)
    {
        bool should_ack = true;
        reception_buffer &buffer_ref = reception_buffers[buf_id];
        for (unsigned int i = 0; i <= buffer_ref.last_segment_number; i++)
        {
            if (!buffer_ref.segments_received[i])
            {
                if (curms < last_nack_ms[buf_id] + 2)
                {
                    CATASTROPHIC(printf("Not NACKing - Too fast for buf %d\n", buf_id);)
                    return false;
                }

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
                    } printf(" %d(%d)", buf_id + 1, i);)
                if (!only_ACK_if_finished)
                {
                    insert_nack_item(buf_id + 1, i);
                }
                should_ack = false;
            }
        }
        if (!should_ack)
            last_nack_ms[buf_id] = system_millis();
        DLOG(
            if (!should_ack) {
                printf("\n");
            })

        if (should_ack)
        {
            // ACK! Make header, CRC8 and encode
            DLOG(printf("Rispondiamo ACK - ");)
            uint8_t msg_crc = gencrc(buffer_ref.message, buffer_ref.msglen);
            if (msg_crc == buffer_ref.received_CRC)
            {
                if (inserted_system_ack_replies < max_system_replies_length && inserted_replies < max_reply_queue_length)
                {
                    unsigned int locked_idx = inserted_system_ack_replies++;
                    system_ack_replies[locked_idx].header[0] = pack(0, 0, true);
                    system_ack_replies[locked_idx].data[0] = buf_id + 1;
                    system_ack_replies[locked_idx].data[1] = buffer_ref.last_segment_number;
                    system_ack_replies[locked_idx].data[bytes_segment_capacity - 1] = msg_crc;
                    rs_encode_key(system_ack_replies[locked_idx].header, buffer_ref.key);
                    replies[inserted_replies++].message_pointer = &system_ack_replies[locked_idx];
                    DLOG(printf("Fatto\n"););
                }
                else
                {
                    CATASTROPHIC(
                        printf("Ran out of ACK (%d) or replies (%d) space.\n", inserted_system_ack_replies < max_system_replies_length, inserted_replies < max_reply_queue_length);)
                }
            }
            else
            {
                CATASTROPHIC(printf("CRC mismatch at last segment! Resetting\n"));
                reception_buffer_reset(&buffer_ref);
                mismatched_crc = true;
                return false;
            }

            if (msg_crc != buffer_ref.CRC && buffer_ref.consumed)
            {
                CATASTROPHIC(
                    printf("Different CRC with consumed frame, imposing to consume.\n");)
                buffer_ref.consumed = false;
                PKLOGGER(
                    PL.write_log(TX, buf_id + 1, Packet_Types::WTF, segment_number_last, "Different CRC on consumed frame");)
            }
            buffer_ref.CRC = msg_crc;

            PKLOGGER(
                PL.write_log(TX, buf_id + 1, Packet_Types::ACK, segment_number_last);)
            // printf("Questo messaggio era lungo %d e ha CRC %d\n", buffer_ref.msglen, counter);
            return true; // Return true so the caller knows they have to consume the frame
        }
        else
        {
            // NACK! Our information about the missing segments is in place, just make the header and encode
            DLOG(
                // printf("Messaggio scorretto, NACK.\n");
            )
            PKLOGGER(
                PL.write_log(TX, buf_id + 1, NACK, segment_number_last);)
            return false;
        }
    }

    return false; // We did not ACK nor NACK, but the caller must only know the reception isn't completed (need more data or ran out of space)
}

unsigned int current_incoming_id = 0;

static bool consume_packet(segment &msg, uint8_t &key_idx, unsigned int &buffer_to_consume_result)
{
    uint8_t id;
    uint8_t segment;
    bool last_packet;

    unpack(msg, id, segment, last_packet);

    DLOG(
        printf("Packet in: ID %d SEG %d LAST %d\n", id, segment, last_packet);

    );

    bool is_system = id == 0;

    if (is_system)
    {
        bool is_ack = last_packet;
        if (segment == 0)
        {

            if (is_ack)
            {
                // ACK
                uint8_t msg_id = msg.data[0];
                uint8_t last_segment_num = msg.data[1];
                uint8_t msg_crc = msg.data[bytes_segment_capacity - 1];

                bool id_valid = msg_id > 0 && msg_id < max_id;
                uint8_t buf_id = msg_id - 1;
                if (id_valid)
                {
                    if (is_buffer_free(buf_id))
                    {
                        DLOG(printf("ACK: Already free ID %d\n", msg_id));
                        return false;
                    }
                    bool key_valid = key_idx == input_buffers[buf_id].key_idx || !use_scramble_keys;
                    bool crc_valid = msg_crc == input_buffers[buf_id].CRC;
                    bool segments_valid = last_segment_num == input_buffers[buf_id].last_segment_number;
                    if (key_valid && crc_valid && segments_valid)
                    {
                        // Wohoo! We successfully transferred the frame.
                        DLOG(printf("ACK: Confermato ID %d, SEG %d, CRC %d, ID %d, KEY %d\n", msg_id, msg.data[0], msg.data[bytes_segment_capacity - 1], msg_id, key_idx));
                        //  Everything corresponds. We successfully transferred a message!
                        //  Hooray! Let's free that buffer for new data to come in.
                        PKLOGGER(PL.write_log(RX, msg_id, ACK, input_buffers[buf_id].last_segment_number));
                        // TODO: Free upper level buffer and reschedule
                        if (!is_buffer_free(buf_id))
                        {
                            clear_buffer_timer(buf_id);
                            on_frame_completed(buf_id);
                        }
                    }
                    else if (key_valid)
                    {
                        CATASTROPHIC(printf("ACK failed: CRC: %d, Segments: %d, ID: %d, KEY: %d\n", crc_valid, segments_valid, msg_id, key_idx););
                        // Something is wrong with this ACK - might be an ack that does not have anything to do with the current message
                        // We want to reschedule all current segments out.
                        // If this is a duplicate ACK, there is no hurt.
                        // If the message was partially delivered, we will get a NACK.
                        for (unsigned int i = 0; i <= input_buffers[buf_id].last_segment_number; i++)
                            if (reschedule_segment(buf_id, i))
                            {
                                PKLOGGER(PL.write_log(TX, buf_id + 1, RESEND_AFTER_INVALID_ACK, i));
                                continue;
                            }
                            else
                                break;

                        set_buffer_timer(buf_id, (input_buffers[buf_id].last_segment_number * segment_timeout_us) + (3 * segment_timeout_us));
                    }
                    else
                    {
                        CATASTROPHIC(printf("ACK failed: invalid key\n"););
                    }
                }
                else
                {
                    CATASTROPHIC(printf("Invalid ID in ACK data\n"););
                }
            }
            else
            {
                // NACK
                DLOG(printf("NACK: rinvio "));

                // NACK
                // We're asked to reschedule N segments.
                // Let's get to it immediately:
                uint8_t how_many = msg.data[bytes_segment_capacity - 1];
                if (how_many > bytes_segment_capacity - 1 || how_many == 0)
                {
                    CATASTROPHIC(printf("Invalid nack, were requested %d segments.\n", how_many));
                    PKLOGGER(PL.write_log(RX, buf_id + 1, WTF, 0, "Invalid NACK"));
                    return false;
                }

                unsigned int rescheduled = 0;
                uint8_t r_id, r_segment;
                bool r_lpkt;

                uint8_t r_buf;

                for (unsigned int i = 0; i < how_many; i++)
                {
                    unpack(msg.data[i], r_id, r_segment, r_lpkt);

                    r_buf = r_id - 1;
                    if (r_buf != current_frame_out)
                    {
                        CATASTROPHIC(printf("NACK contained invalid request, removing %d queued packets\n", rescheduled););
                        inserted_replies -= rescheduled;
                        return false;
                    }

                    DLOG(printf("%d(%d) ", r_id, r_segment));

                    if (r_buf < contemporary_outbound_buffers)
                    {
                        if (r_segment <= input_buffers[r_buf].last_segment_number)
                        {
                            if (reschedule_segment(r_buf, r_segment))
                            {
                                rescheduled++;
                                set_buffer_timer(r_buf, (rescheduled * segment_timeout_us) + (2 * segment_timeout_us));

                                PKLOGGER(PL.write_log(TX, r_buf, RESEND_ASKED, i));

                                continue;
                            }
                            else
                                break;
                        }
                        else
                        {
                            CATASTROPHIC(printf("Requested segments that don't exist ID: %d( seg: %d : max %d).\n", r_id, r_segment, input_buffers[r_buf].last_segment_number));
                            PKLOGGER(PL.write_log(RX, r_buf, INVALID_NACK, 0, "Inexistent segments"));
                            inserted_replies -= rescheduled;
                            return false;
                        }
                    }
                    else
                    {
                        CATASTROPHIC(printf("Requested segment in buffer that doesn't exist buf: %d( seg: %d).\n", r_buf, r_segment));
                        PKLOGGER(PL.write_log(RX, r_buf, INVALID_NACK, 0, "Inexistent buffer"));
                        inserted_replies -= rescheduled;
                        return false;
                    }
                    DLOG(printf("\n"));
                }
                DLOG(printf("\n"));
            }
        }
        else
        {
            CATASTROPHIC(printf("Non zero segment with system message\n"););
        }
    }
    else
    {
        // Buffer id
        uint8_t buf_id = id - 1;

        reception_buffer &current_buffer = reception_buffers[buf_id];

        if ((key_idx != current_buffer.key) && use_scramble_keys)
        {
            reception_buffer_reset(&current_buffer);

            current_buffer.key = key_idx;
            DLOG(printf("Buffer reset: new key %d\n", key_idx););
        }

        if (current_incoming_id != buf_id)
        {
            reception_buffer_reset(&reception_buffers[current_incoming_id]);
            reception_buffer_reset(&reception_buffers[buf_id]);

            current_buffer.key = key_idx;
            DLOG(printf("Buffer reset: new id %d\n", buf_id););
            current_incoming_id = buf_id;
        }

        if (last_packet)
        {
            // How many data bytes is this last segment carrying?
            int8_t seglen = msg.data[bytes_segment_capacity - 2];
            if (seglen >= bytes_segment_capacity)
            {
                // Corrupted packet!
                CATASTROPHIC(printf("Last segment corrupted: seglen %d .\n", seglen));
                PKLOGGER(PL.write_log(NONE, buf_id + 1, RECEPTION_RESET, segment, "Segment Already Marked"));
                return false;
            }
            unsigned int calculated_msg_len = (segment * bytes_segment_capacity) + seglen;

            // If the msglen was already calculated, see if it is different.
            // Chances are we lost all previous segments and tx is trying to send a new message
            // So reset the message now, copy the data and ask for all the missing segments.
            if (calculated_msg_len != reception_buffers[buf_id].msglen && reception_buffers[buf_id].msglen != 0)
            {
                PKLOGGER(PL.write_log(NONE, buf_id + 1, RECEPTION_RESET, segment, "Message Length Mismatch"));
                reception_buffer_reset(reception_buffers + buf_id);
                current_buffer.key = key_idx;
                DLOG(printf("Buffer reset: Length mismatch\n"));
            }

            if (calculated_msg_len > MTU)
            {
                CATASTROPHIC("MSG LEN > MTU. WHAT! %d\n", calculated_msg_len);
                return false;
            }

            reception_buffers[buf_id].msglen = calculated_msg_len;
            reception_buffers[buf_id].last_segment_number = segment;
            reception_buffers[buf_id].received_CRC = msg.data[bytes_segment_capacity - 1];
            current_buffer.segments_received[segment] = true;

            // Don't copy if seglen is negative
            if (seglen > 0)
            {
                // Copy data
                memcpy(reception_buffers[buf_id].message + (bytes_segment_capacity * segment), msg.data, seglen);
            }
            else
            {
                DLOG(printf("Negative Seglen: %d\n", seglen););
            }
            bool mismatched_crc = false;
            if (insert_ack_or_nack_for_buffer(buf_id, mismatched_crc))
            {
                // We should consume the frame!
                if (!reception_buffers[buf_id].consumed)
                {
                    reception_buffers[buf_id].consumed = true;
                    DLOG(printf("Messaggio da consumare: LEN %d, CRC %d, ID %d, KEY %d\n", reception_buffers[buf_id].msglen, reception_buffers[buf_id].CRC, id, key_idx));
                    PKLOGGER(PL.log_messaggio_completo(RX, buf_id + 1, reception_buffers[buf_id].CRC, reception_buffers[buf_id].msglen));

                    // TODO: send to the upper level interface
                    buffer_to_consume_result = buf_id;
                    return true;
                }
            }
            else if (mismatched_crc)
            {
                // Don't copy if seglen is negative
                if (seglen > 0)
                {
                    // Copy data
                    memcpy(reception_buffers[buf_id].message + (bytes_segment_capacity * segment), msg.data, seglen);
                }
                reception_buffers[buf_id].msglen = calculated_msg_len;
                reception_buffers[buf_id].last_segment_number = segment;
                reception_buffers[buf_id].received_CRC = msg.data[bytes_segment_capacity - 1];
                current_buffer.segments_received[segment] = true;
                insert_ack_or_nack_for_buffer(buf_id, mismatched_crc, false);
            }
        }
        else
        {
            if (current_buffer.segments_received[segment] && reset_on_duplicates)
            {
                reception_buffer_reset(&current_buffer);
                current_buffer.key = key_idx;
                DLOG(printf("Buffer reset: duplicate\n"));
            }

            if (!current_buffer.segments_received[segment])
            {
                // Copy data
                memcpy(reception_buffers[buf_id].message + (bytes_segment_capacity * segment), msg.data, bytes_segment_capacity);
            }
            current_buffer.segments_received[segment] = true;

            if ((current_buffer.last_segment_number > 0) && !(reception_buffers[buf_id].consumed))
            {
                bool m = false;
                // Find out if needs ACK
                if (insert_ack_or_nack_for_buffer(buf_id, m, true))
                {
                    // We should consume the frame!
                    if (!reception_buffers[buf_id].consumed)
                    {
                        reception_buffers[buf_id].consumed = true;
                        DLOG(printf("Messaggio da consumare (dati): LEN %d, CRC %d, Id %d, KEY %d\n", reception_buffers[buf_id].msglen, reception_buffers[buf_id].CRC, id, key_idx));
                        PKLOGGER(PL.log_messaggio_completo(RX, buf_id + 1, reception_buffers[buf_id].CRC, reception_buffers[buf_id].msglen));

                        // TODO: send to the upper level interface
                        buffer_to_consume_result = buf_id;
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

static bool consume_packets(const unsigned int &buffer, unsigned int *buffers_to_consume, unsigned int &buffer_to_consume_count)
{

    buffer_to_consume_count = 0;
    unsigned int buffer_to_consume_result = 0;
    uint8_t key_idx;
    for (unsigned int i = 0; i < radio_interrupt_buffers[buffer].written_items; i++)
    {

        if (rs_decode_key(radio_interrupt_buffers[buffer].segments[i].header, key_idx))
        {
            if (consume_packet(radio_interrupt_buffers[buffer].segments[i], key_idx, buffer_to_consume_result))
            {
                // buffers_to_consume[buffer_to_consume_count++] = buffer_to_consume_result;
                if (!write_output(reception_buffers[buffer_to_consume_result].message, reception_buffers[buffer_to_consume_result].msglen))
                {
                    print_reception_buffer_state(buffer_to_consume_result);
                }
                // reception_buffers[buffer_to_consume_result].last_segment_number = 0;
            }
        }
    }

    // Call after consuming all incoming data.
    // Not here. Do that in main or the RX.
    // finalize_nack_and_flush();

    radio_interrupt_buffers[buffer].lock = false;
    return buffer_to_consume_count > 0;
}

static void timer_fired(unsigned int buf_id)
{

    if (is_timer_active(buf_id))
    {
        DLOG("Timer fired, but it is still ticking! Skipping.\n");
        return;
    }
    // Timer associated with a buffer has fired.
    // See if we want to reschedule the segment or scrap the frame
    if (!is_buffer_free(buf_id))
    {
        if (buffer_send_retries[buf_id]++ < segment_retries_max && (frame_timeout_ms + input_buffers[buf_id].ms_start > system_millis()))
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

            on_frame_completed(buf_id);
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
}

static unsigned int replies_duplicates()
{

    unsigned int initial_replies = inserted_replies;
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

                if (j + 1 < inserted_replies--)
                {
                    replies[j].message_pointer = replies[j + 1].message_pointer;
                    j--;
                }
                found++;
            }
        }
    }

    CATASTROPHIC(
        if (found != 0) {
            printf("\nOutbound replies contain %d duplicates. (possibly less. must be ZERO duplicates). Duplicates removed: replies first vs now: %d | %d\n", found, initial_replies, inserted_replies);
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
