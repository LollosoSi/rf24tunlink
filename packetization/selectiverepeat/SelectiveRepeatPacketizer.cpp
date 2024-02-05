/*
 * SelectiveRepeatPacketizer.cpp
 *
 *  Created on: 4 Feb 2024
 *      Author: Andrea Roccaccino
 */

#include "SelectiveRepeatPacketizer.h"

SelectiveRepeatPacketizer::SelectiveRepeatPacketizer() {
	// TODO Auto-generated constructor stub

}

SelectiveRepeatPacketizer::~SelectiveRepeatPacketizer() {
	// TODO Auto-generated destructor stub
}

bool SelectiveRepeatPacketizer::next_packet_ready() {
	return (Settings::control_packets || !frames.empty());
}

RadioPacket* SelectiveRepeatPacketizer::next_packet() {
	if (Settings::control_packets) {
		if (frames.empty()) {
			return (get_empty_packet());
		} else {
			if (current_packet_counter == frames.front()->packets.size() - 1) {
				if (resend_list.empty())
					return (frames.front()->packets[current_packet_counter]);
				else {
					RadioPacket *rs = resend_list.front();
					resend_list.pop_front();
					return (rs);
				}
			} else {
				return (frames.front()->packets[current_packet_counter++]);
			}
		}
	} else {
		if (frames.empty()) {
			// Undefined behaviour
			std::cout << "Requested frame when no one is available\n";
			exit(2);
		} else {
			if (current_packet_counter == frames.front()->packets.size() - 1) {
				if (resend_list.empty())
					return (frames.front()->packets[current_packet_counter]);
				else {
					RadioPacket *rs = resend_list.front();
					resend_list.pop_front();
					return (rs);
				}
			} else {
				return (frames.front()->packets[current_packet_counter++]);
			}
		}
	}

}

void SelectiveRepeatPacketizer::free_frame(Frame<RadioPacket> *frame) {

	while (!frame->packets.empty()) {
		frame->packets.pop_front();
	}

}

RadioPacket* SelectiveRepeatPacketizer::get_empty_packet() {
	static RadioPacket *rp = new RadioPacket { { 0 }, 0 };
	return (rp);
}

bool SelectiveRepeatPacketizer::packetize(TUNMessage *tunmsg) {

	static uint8_t id = 0;
	if (++id == 4) {
		id = 1;
	}

	Frame<RadioPacket> *frm = new Frame<RadioPacket>;
	RadioPacket *pointer = nullptr;

	int tracker = tunmsg->size - 31;

	int packetcounter = floor(tunmsg->size / 31.0);
	int totalpackets = packetcounter;

	int i;
	for (i = totalpackets; i > 0; i--) {
		pointer = new RadioPacket;
		pointer->data[0] = pack_info(false, id, i);
		strncpy((char*) ((pointer->data) + 1),
				(const char*) (tunmsg->data + tracker), 31);
		tracker -= 31;
		pointer->size = 32;
		frm->packets.push_back(pointer);
		//printf("Pacchetto creato: %i %s\n", (int) pointer->size, pointer->data);

	}

	pointer = new RadioPacket;
	pointer->data[0] = pack_info(true, id, totalpackets);
	pointer->size = 1 + (31 + tracker);
	strncpy((char*) ((pointer->data) + 1), (const char*) (tunmsg->data),
			31 + tracker);
	frm->packets.push_back(pointer);
	//printf("Pacchetto creato: %i %s\n", (int) pointer->size, pointer->data);

	frames.push_back(frm);

	return (true);
}

bool SelectiveRepeatPacketizer::request_missing_packets(bool *array,
		unsigned int size) {

	unsigned int findings = 0;

	static RadioPacket *rp_answer = new RadioPacket;

	unsigned int rp_cursor = 0;
	for (unsigned int i = 0; i <= size; i++) {
		if (!array[i]) {
			findings++;

			rp_answer->data[1 + rp_cursor++] = pack_info(0, 0, i);
		}
	}
	if (findings > 0) {
		rp_answer->data[0] = pack_info(true, 0, findings);
		rp_answer->size = 1 + findings;
		resend_list.push_front(rp_answer);
		return (true);
	} else
		return (false);

}

inline void SelectiveRepeatPacketizer::response_packet_ok(uint8_t id) {
	//printf("OK per id %i\n", id);
	static RadioPacket *rp_answer = new RadioPacket;
	rp_answer->size = 1;
	rp_answer->data[0] = pack_info(true, 0, id);
	resend_list.push_front(rp_answer);
}

inline uint8_t SelectiveRepeatPacketizer::get_pack_id(RadioPacket *rp) {
	bool first = 0;
	uint8_t id = 0;
	uint8_t seg = 0;
	unpack_info(rp->data[0], first, id, seg);
	return (id);
}

bool SelectiveRepeatPacketizer::receive_packet(RadioPacket *rp) {

	// Handle system calls first
	bool first = 0;
	uint8_t id = 0;
	uint8_t seg = 0;
	unpack_info(rp->data[0], first, id, seg);

	//printf("Ricevuto pack: %i %s con valori %i %i %i\n", (int) rp->size,
	//		rp->data, (int) first, (int) id, (int) seg);

	// Is this a control packet?
	if (id == 0) {

		// Does this packet carry a NAck or something else?
		// NOTE: NAck could be a confirmation if it carries 0 requests
		if (first) {

			// If it carries more than the info byte, assume these are all requests
			if (rp->size > 1) {
				int counter = 1;
				//printf("Requested packets: ");
				while (counter < rp->size) {
					bool pkt_boolean = 0;
					uint8_t pkt_id = 0;
					uint8_t pkt_seg = 0;

					unpack_info(rp->data[counter], pkt_boolean, pkt_id,
							pkt_seg);

					bool dbg2_pkt_boolean = 0;
					uint8_t dbg2_pkt_id = 0;
					uint8_t dbg2_pkt_seg = 0;
					unpack_info(frames.back()->packets[0]->data[0],
							dbg2_pkt_boolean, dbg2_pkt_id, dbg2_pkt_seg);

					unsigned int position = frames.front()->packets.size() - 1 - pkt_seg;

					bool dbg_pkt_boolean = 0;
					uint8_t dbg_pkt_id = 0;
					uint8_t dbg_pkt_seg = 0;
					unpack_info(frames.front()->packets[position]->data[0],
							dbg_pkt_boolean, dbg_pkt_id, dbg_pkt_seg);

					//printf("| (SEG: %i, carried: %i) %s ", pkt_seg, dbg_pkt_seg,
					//		frames.front()->packets[position]->data);
					resend_list.push_back(frames.front()->packets[position]);

					counter++;
				}
				//printf("\n");

			} else if (seg == get_pack_id(frames.front()->packets.front())) {
				// This is a confirmation! Next frame.
				received_ok();
				//printf("Confirmed packet\n");
			} else {
				// Undefined behaviour, wtf
				std::cout << "SelectiveRepeatPacketizer has hit a WTF checkpoint\n";
			}
		} else {
			// This packet isn't carrying requests
		}

		return (true);
	}

	static uint8_t *buffer = nullptr;
	static bool *received_packets_boolean = nullptr;
	if (buffer == nullptr) {
		buffer = new uint8_t[this->get_mtu()] { 0 };
		received_packets_boolean = new bool[this->get_mtu() / 31] { 0 };
	}

	static uint8_t last_id = 0;
	static uint8_t leftover = 0;

	if (id != last_id) {

		if (first)
			leftover = 31 - (rp->size - 1);

		uint8_t copyposition = (first ? leftover : 0)
				+ (31 * (first ? 0 : seg));
		uint8_t copylength = (rp->size - 1);

		strncpy((char*) (buffer + copyposition), (const char*) (rp->data + 1),
				copylength);

		unsigned int pos = first ? 0 : seg;
		received_packets_boolean[pos] = 1;
	}

	// Was this the last packet
	if (first) {

		if (id == last_id) {
			response_packet_ok(id);
		} else {

			if (request_missing_packets(received_packets_boolean, seg)) {
				//printf("Reception is not okay, trying packet request\n");

			} else {
				// Respond OK
				response_packet_ok(id);
				unsigned int packets_full = (seg);
				unsigned int first_packet_size = 31-leftover;
				unsigned int message_size = (31*packets_full) + first_packet_size;
				//printf("Packets full: %i\tFirst packet size: %i\t Message size: %i\n",packets_full,first_packet_size,message_size);
				static TUNMessage *tm = new TUNMessage { nullptr, 0 };
				tm->data = (buffer + leftover);
				tm->size = message_size;
				//std::cout << "Message length: " << (unsigned int)message_size << std::endl;
				this->tun_handle->send(tm);

				for (unsigned int b = 0; b <= seg; b++)
					received_packets_boolean[b] = false;

				last_id = id;
			}
		}
	}

	return (true);

}
