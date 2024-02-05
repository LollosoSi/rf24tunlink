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
			if (current_packet_counter == frames.front()->packets.size()) {
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
			if (current_packet_counter == frames.front()->packets.size()) {
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

bool SelectiveRepeatPacketizer::packetize(TUNMessage &tunmsg) {

	static uint8_t id = 0;
	if (++id == 4) {
		id = 1;
	}

	Frame<RadioPacket> *frm = new Frame<RadioPacket>;
	RadioPacket *pointer = new RadioPacket;

	int cursor = tunmsg.size() - 1;
	int counter = 0;

	int packetcounter = floor(tunmsg.size / 31.0);
	int totalpackets = packetcounter;
	while (cursor >= 0) {

		uint8_t ch = tunmsg.data[cursor--];
		pointer->data[1 + counter++] = ch;

		bool is_last_packet = cursor == -1;
		if (is_last_packet || counter == 31) {
			pointer->size = 1 + counter;
			pointer->data[0] = pack_info(is_last_packet, id,
					is_last_packet ? totalpackets : packetcounter--);
			frm->packets.push_back(pointer);
			if (!is_last_packet) {
				pointer = new RadioPacket;
			}
			counter = 0;
		}

	}

	frames.push_back(frm);

	return (true);
}

bool SelectiveRepeatPacketizer::receive_packet(RadioPacket &rp) {

	// Handle system calls first
	bool first = 0;
	uint8_t id = 0;
	uint8_t seg = 0;
	unpack_info(rp.data[0], first, id, seg);

	// Is this a control packet?
	if (id == 0) {

		// Does this packet carry a NAck or something else?
		// NOTE: NAck could be a confirmation if it carries 0 requests
		if (first) {

			// If it carries more than the info byte, assume these are all requests
			if (rp.size > 1) {
				int counter = 1;
				while (counter < rp.size) {
					bool pkt_boolean = 0;
					uint8_t pkt_id = 0;
					uint8_t pkt_seg = 0;

					unpack_info(rp.data[counter], pkt_boolean, pkt_id, pkt_seg);

					resend_list.push_back(
							frames.front()->packets[(frames.front()->packets.size())
									- pkt_seg]);

					counter++;
				}

			} else if (id == seg) {
				// This is a confirmation! Next frame.
				received_ok();
			} else {
				// Undefined behaviour, wtf
				std::cout
						<< "SelectiveRepeatPacketizer has hit a WTF checkpoint\n";
			}
		} else {
			// This packet isn't carrying requests
		}

	}

	static uint8_t buffer[this->get_mtu()] = { 0 };

	return (false);

}
