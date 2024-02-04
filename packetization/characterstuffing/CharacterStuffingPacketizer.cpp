/*
 * CharacterStuffingPacketizer.cpp
 *
 *  Created on: 4 Feb 2024
 *      Author: Andrea Roccaccino
 */

#include "CharacterStuffingPacketizer.h"

#include <iostream>
using namespace std;

CharacterStuffingPacketizer::CharacterStuffingPacketizer() {

	cout << "Character Stuffing Packetizer initialized\n";

}

CharacterStuffingPacketizer::~CharacterStuffingPacketizer() {

}

bool CharacterStuffingPacketizer::next_packet_ready() {
	return (Settings::control_packets || !frames.empty());
}

RadioPacket& CharacterStuffingPacketizer::next_packet() {
	if (Settings::control_packets) {
		if (frames.empty()) {
			return (get_empty_packet());
		} else {
			if (current_packet_counter == frames.front().packets.size()) {
				return (frames.front().packets[current_packet_counter]);
			} else {
				return (frames.front().packets[current_packet_counter++]);
			}
		}
	} else {

		if (frames.empty()) {
			// Undefined behaviour
			std::cout << "Requested frame when no one is available\n";
			exit(2);
		} else {
			if (current_packet_counter == frames.front().packets.size()) {
				return (frames.front().packets[current_packet_counter]);
			} else {
				return (frames.front().packets[current_packet_counter++]);
			}
		}

	}

}

void CharacterStuffingPacketizer::free_frame(Frame<RadioPacket> &frame) {

	while (!frame.packets.empty()) {
		delete[] frame.packets.front().data;
		frame.packets.pop_front();
	}

}

RadioPacket& CharacterStuffingPacketizer::get_empty_packet() {

	static RadioPacket rp = { 0, new uint8_t[1] { 0 } };
	return (rp);

}

