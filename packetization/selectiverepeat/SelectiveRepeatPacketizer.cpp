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
					int rs = resend_list.front();
					resend_list.pop_front();
					return (frames.front()->packets[rs]);
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
					int rs = resend_list.front();
					resend_list.pop_front();
					return (frames.front()->packets[rs]);
				}
			} else {
				return (frames.front()->packets[current_packet_counter++]);
			}
		}
	}

}

void SelectiveRepeatPacketizer::free_frame(Frame<RadioPacket> *frame) {

	while (!frame->packets.empty()) {
		delete[] frame->packets.front()->data;
		frame->packets.pop_front();
	}

}

RadioPacket* SelectiveRepeatPacketizer::get_empty_packet() {
	static RadioPacket *rp = new RadioPacket{ new uint8_t[1] { 0 }, 0, 0 };
	return (rp);
}

bool SelectiveRepeatPacketizer::packetize(TUNMessage &tunmsg) {

	// Not implemented

	return (false);
}

bool SelectiveRepeatPacketizer::receive_packet(RadioPacket &rp) {

	// Not implemented

	return (false);

}
