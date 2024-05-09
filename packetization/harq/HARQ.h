/*
 * HARQ.h
 *
 *  Created on: 7 May 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../../interfaces/PacketHandler.h"
#include "../../telemetry/Telemetry.h"
#include "../../rs_codec/RSCodec.h"
#include "../../utils.h"
#include <cmath>
#include <algorithm>

static unsigned char bits_to_mask(const int bits) {
	return (pow(2, bits) - 1);
}

static const int id_bits = 2, segment_bits = 5, last_packet_bits = 1; // @suppress("Multiple variable declaration")
static const int id_max = pow(2, id_bits), segment_max = pow(2, segment_bits); // @suppress("Multiple variable declaration")
static const int id_byte_pos_offset = 8 - id_bits, segment_byte_pos_offset = 8
		- id_bits - segment_bits, last_packet_byte_pos_offset = 8 - id_bits
		- segment_bits - last_packet_bits; // @suppress("Multiple variable declaration")
static unsigned char id_mask = bits_to_mask(id_bits), segment_mask =
		bits_to_mask(segment_bits), last_packet_mask = bits_to_mask(
		last_packet_bits); // @suppress("Multiple variable declaration")

inline unsigned char pack(unsigned char id, unsigned char segment,
		bool last_packet) {
	return (0x00 | ((id & id_mask) << id_byte_pos_offset)
			| ((segment & segment_mask) << segment_byte_pos_offset)
			| (last_packet & last_packet_mask));
}
inline void unpack(unsigned char data, unsigned char &id,
		unsigned char &segment, bool &last_packet) {
	id = id_mask & (data >> id_byte_pos_offset);
	segment = segment_mask & (data >> segment_byte_pos_offset);
	last_packet = last_packet_mask & (data >> last_packet_byte_pos_offset);
}

const int submessage_bytes = 32;
const int header_bytes = 1;
const int length_last_packet_bytes = 1;
const int bytes_per_submessage = submessage_bytes - Settings::ReedSolomon::nsym
		- header_bytes;

class HARQ: public PacketHandler<RadioPacket>, public Telemetry {
public:
	HARQ();
	virtual ~HARQ();

	uint64_t resend_wait_time = 1000;

	std::string* telemetry_collect(const unsigned long delta);

	struct message_reception_identikit {
		uint8_t id = 0;

		uint8_t *buffer;
		int shift = 0;
		int msg_length = 0;

		int total_packets = 0;

		bool received[segment_max] = { 0 };

		uint8_t missing[segment_max] = { 0 };
		int missing_array_length = 0;

		void write_data(uint8_t *data, int size, int segment) {
			memcpy(
					buffer + (bytes_per_submessage * segment)
							+ (segment == 0 ? shift : 0), data, size);
			received[segment] = true;
		}

		bool is_completed() {
			if (total_packets == 0)
				return (false);
			if (id == 0)
				std::cout
						<< "Note: message_reception_identikit has no id. Messages can't be recognised later on.\n";
			missing_array_length = 0;
			for (int i = 0; i < total_packets; i++) {
				if (!received[i]) {
					missing[missing_array_length++] = pack(id, i, false);
				}
			}

			return (missing_array_length == 0);
		}

	};

	message_reception_identikit* make_MSRI(uint8_t id = 0) {
		message_reception_identikit *msri = new message_reception_identikit;
		msri->buffer = new uint8_t[bytes_per_submessage * segment_max];
		msri->id = id;
		return (msri);
	}

	unsigned int get_mtu() {
		return ((pow(2, segment_bits) * bytes_per_submessage)
				- length_last_packet_bytes);
	}

	inline bool next_packet_ready() {
		return (checktimers() || !send_queue.packets.empty()
				|| !send_queue_system.packets.empty());
	}
	RadioPacket* next_packet() {
		if (delete_queue.packets.size() > id_max) {
			delete delete_queue.packets.front();
			delete_queue.packets.pop_front();
		}

		RadioPacket *ret_rp = nullptr;

		if (!send_queue_system.packets.empty()) {
			delete_queue.packets.push_back(
					ret_rp = send_queue_system.packets.front());
			send_queue_system.packets.pop_front();
		} else if (!send_queue.packets.empty()) {
			ret_rp = send_queue.packets.front();
			send_queue.packets.pop_front();
		} else {
			// WTF?
		}

		return (ret_rp);

	}
	inline RadioPacket* get_empty_packet() {
		static RadioPacket *rp = new RadioPacket { { 0 }, 32 };
		return (rp);
	}

protected:

	std::deque<bool> packets_acked;
	std::deque<uint64_t> outgoing_frame_sendtime;

	void findout_add_sendqueue() {
		while (packets_acked.size() < id_max
				&& packets_acked.size() < frames.size()) {
			add_sendqueue(frames[packets_acked.size()]);
			packets_acked.push_back(false);
			outgoing_frame_sendtime.push_back(current_millis());
		}
	}

	inline void add_sendqueue(Frame<RadioPacket> *frp) {
		for (unsigned int i = 0; i < frp->packets.size(); i++)
			send_queue.packets.push_back(frp->packets[i]);
	}

	void flush_acked() {
		while (!packets_acked.empty()) {
			if (packets_acked[0]) {
				free_frame(frames.front());
				frames.pop_front();
				packets_acked.pop_front();
				outgoing_frame_sendtime.pop_front();

			} else
				break;
		}
		findout_add_sendqueue();

	}

	void received_ok(uint8_t id) {
		for (unsigned int i = 0;
				i < (id_max > frames.size() ? frames.size() : id_max); i++) {
			uint8_t p_id, seg;
			bool b;
			unpack(frames[i]->packets[0]->data[0], p_id, seg, b);
			if (id == p_id) {
				packets_acked[i] = true;
				break;
			} else {
				continue;
			}
		}
		flush_acked();
	}

	void catch_lost(uint8_t *lostpackets, int size) {
		for (int i = 0; i < size; i++) {
			uint8_t p_id, p_seg;
			bool p_b;
			unpack(lostpackets[0], p_id, p_seg, p_b);

			uint8_t f_id, f_seg;
			bool f_b;
			for (unsigned int k = 0; k < packets_acked.size(); k++) {
				unpack(frames[k]->packets.front()->data[0], f_id, f_seg, f_b);
				if (p_id == f_id) {
					std::cout << "\nReloading packet: " << frames[k]->packets[frames[k]->packets.size() - 1
																			- p_seg]->data << "\n";
					send_queue.packets.push_back(
							frames[k]->packets[frames[k]->packets.size() - 1
									- p_seg]);
					break;
				}
			}

		}
	}

	bool checktimers() {
		bool result = false;
		uint64_t ms = current_millis();
		for (unsigned int i = 0; i < packets_acked.size(); i++) {
			if (!packets_acked[i]
					&& (ms >= outgoing_frame_sendtime[i] + resend_wait_time)) {
				outgoing_frame_sendtime[i] = ms;
				send_queue.packets.push_back(frames[i]->packets.back());
				result = true;
			}
		}

		return (result);
	}

	RSCodec rsc;

	bool packetize(TUNMessage *tunmsg) {

		Frame<RadioPacket> *f = new Frame<RadioPacket>;

		static uint8_t id = 0;
		id = (id + 1) & id_mask;
		if (id == 0)
			id++;

		int msglen = tunmsg->size;

		int rem = msglen % bytes_per_submessage;
		int packets = ceil(msglen / (float) bytes_per_submessage);
		int arrayshift = bytes_per_submessage - rem;
		if (rem == 0)
			packets++;

		//std::cout << "Rem: " << rem << " Shift: " << arrayshift << " Packets: "
		//		<< packets << "\n";

		uint8_t (*nums)[bytes_per_submessage] =
				reinterpret_cast<uint8_t (*)[bytes_per_submessage]>(tunmsg->data
						- arrayshift);

		RadioPacket *rp = new RadioPacket { { 0 }, 32 };
		rp->data[0] = pack(id, packets, true);
		rp->data[1] = rem;
		memcpy(rp->data + 2, nums[0] + (bytes_per_submessage - rem), rem);
		rsc.efficient_encode(rp->data, rp->size);
		f->packets.push_front(rp);

		for (int i = 1; i < packets; i++) {
			rp = new RadioPacket { { 0 }, 32 };
			rp->data[0] = pack(id, i, false);
			memcpy(rp->data + 1, nums[i], bytes_per_submessage);
			rsc.efficient_encode(rp->data, rp->size);
			f->packets.push_front(rp);
		}

		for (int i = 0; i < packets; i++) {
			std::cout << "Packet : " << i << "\t";
			for (int j = 0; j < 32; j++) {
				std::cout << " " << (int) f->packets[packets - 1 - i]->data[j];
			}
			std::cout << "\n";
		}
		std::cout << "\n";

		frames.push_back(f);
		findout_add_sendqueue();
		return (true);
	}

	bool receive_packet(RadioPacket *rp) {
		if (!rsc.efficient_decode(rp->data, 32)) {
			std::cout << "\tPacket broken\n";
			return (false);
		}

		static std::deque<message_reception_identikit*> receptions;
		static uint8_t waiting_id = 1;

		uint8_t id, segment;
		bool last_packet;
		unpack(rp->data[0], id, segment, last_packet);

		std::cout << "This packet is: ";
		if (id == 0) {

			if (last_packet) {

				// Is this ACK or NACK or BOTH??
				int len = 0;
				while (rp->data[1 + len] != 0) {
					len++;
				}
				if (len != 0) {
					// NACK
					std::cout << " NACK with missing "<<len;
					catch_lost(rp->data + 1, len);
				}
				if (segment != 0) {
					// ACK!
					std::cout << " ACK ";
					received_ok(segment);
				}

				if(len==0 && segment == 0){
					std::cout << " Nothing. What? ";
				}
				std::cout << "\n";
			}

			return (true);
		}

		std::cout << " DATA " << rp->data << "\n";

		message_reception_identikit *work_msri = nullptr;

		for (message_reception_identikit *msri : receptions) {
			if (msri->id == id) {
				work_msri = msri;
				if (work_msri->is_completed()) {
					if (last_packet) {
						//send_ACK_NACK(id);
					} else {
						receptions.erase(std::find(receptions.begin(),receptions.end(),work_msri));
						work_msri=nullptr;
					}
				}
			}
		}

		if (work_msri == nullptr)
			receptions.push_back(work_msri = make_MSRI(id));

		if (last_packet) {
			work_msri->total_packets = segment;
			work_msri->msg_length = rp->data[1]
					+ (bytes_per_submessage * segment);
			work_msri->shift = bytes_per_submessage - rp->data[1];
			work_msri->write_data(rp->data + (2), rp->data[1], 0);
		} else
			work_msri->write_data(rp->data + (1), bytes_per_submessage,
					segment);

		if (work_msri->is_completed()) {
			if (waiting_id == work_msri->id) {
				waiting_id = (waiting_id + 1) & id_mask;
				if (waiting_id == 0)
					waiting_id++;
				std::cout << "Final message: "
						<< work_msri->buffer + work_msri->shift << "\n";
				send_ACK_NACK(id);
			}
		} else {
			send_ACK_NACK(0, work_msri->missing,
					work_msri->missing_array_length);
		}

		return (true);
	}

	// Send ACK : id != 0, Send NACK lost_list is array of pack(...) with size. or nullptr
	void send_ACK_NACK(uint8_t id = 0, uint8_t *lost_list = nullptr, int size =
			0) {
		RadioPacket *rp_ok = new RadioPacket { { pack(0, id, true), 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0 }, 32 };
		if (lost_list)
			memcpy(rp_ok->data + 1, lost_list, size);

		rsc.efficient_encode(rp_ok->data,rp_ok->size);

		send_queue_system.packets.push_back(rp_ok);
	}

	inline void free_frame(Frame<RadioPacket> *frame) {
		while (!frame->packets.empty()) {
			delete frame->packets.front();
			frame->packets.pop_front();
		}
		delete frame;
	}

	Frame<RadioPacket> send_queue;
	Frame<RadioPacket> send_queue_system;

	Frame<RadioPacket> delete_queue;

	std::string *returnvector = nullptr;
	unsigned long fragments_received = 0, fragments_sent = 0, fragments_resent =
			0, frames_completed = 0, fragments_control = 0, bytes_discarded = 0,
			frames_dropped = 0; // @suppress("Multiple variable declaration")

};

