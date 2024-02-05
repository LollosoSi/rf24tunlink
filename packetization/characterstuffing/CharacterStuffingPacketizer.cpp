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
	return (Settings::control_packets || frames.size() > 0);
}

RadioPacket* CharacterStuffingPacketizer::next_packet() {
	if (Settings::control_packets) {
		if (frames.empty()) {
			return (get_empty_packet());
		} else {
			if (current_packet_counter == frames.front()->packets.size()) {
				received_ok();
			}
			return (frames.front()->packets[current_packet_counter++]);

		}
	} else {

		if (frames.empty()) {
			// Undefined behaviour
			std::cout << "Requested frame when no one is available\n";
			exit(2);
		} else {
			RadioPacket *rp =
					(frames.front()->packets[current_packet_counter++]);
			if (current_packet_counter == frames.front()->packets.size()) {
				static Frame<RadioPacket> *deletequeue = nullptr;

				if (deletequeue != nullptr) {
					free_frame(deletequeue);
					delete deletequeue;
				}
				deletequeue = frames.front();

				received_ok();
			}
			return (rp);
		}

	}

}

void CharacterStuffingPacketizer::free_frame(Frame<RadioPacket> *frame) {

	while (!frame->packets.empty()) {
		frame->packets.pop_front();
	}

}

RadioPacket* CharacterStuffingPacketizer::get_empty_packet() {

	static RadioPacket *rp = new RadioPacket { {0}, 1 };
	return (rp);

}

bool CharacterStuffingPacketizer::packetize(TUNMessage &tunmsg) {

	//printf("To packetize: %s of size %i\n", tunmsg.data, tunmsg.size);

	Frame<RadioPacket> *frm = new Frame<RadioPacket>;

	RadioPacket *pointer = new RadioPacket;

	unsigned int cursor = 0;
	int packetcursor = 0;
	while (cursor < tunmsg.size) {

		if (tunmsg.data[cursor] == radio_escape_char) {
			if (packetcursor + 2 <= 32) {
				pointer->data[packetcursor++] = tunmsg.data[cursor];
				pointer->data[packetcursor++] = radio_escape_char;
				if (packetcursor == 32) {
					pointer->size = packetcursor;
					frm->packets.push_back(pointer);
					pointer = new RadioPacket;
					packetcursor = 0;
				}
			} else {
				pointer->size = packetcursor;
				frm->packets.push_back(pointer);
				pointer = new RadioPacket;
				packetcursor = 0;
			}
		} else {
			pointer->data[packetcursor++] = tunmsg.data[cursor];
			if (packetcursor == 32) {
				pointer->size = packetcursor;
				frm->packets.push_back(pointer);
				pointer = new RadioPacket;
				packetcursor = 0;
			}
		}

		cursor++;
	}

	pointer->data[packetcursor++] = radio_escape_char;
	pointer->size = packetcursor;
	frm->packets.push_back(pointer);
	packetcursor = 0;

	frames.push_back(frm);

	//std::cout << "Packets: " << frm->packets.size() << std::endl;

	return (true);
}

/**
 * Read packets from the radio and compose messages.
 * Malformed messages will be rejected from the TUN interface
 * Empty messages will just contain an escape sequence
 */
bool CharacterStuffingPacketizer::receive_packet(RadioPacket &rp) {

	//std::cout << "Received packet data, size: " << (int)rp.size << "\n";

	static unsigned int message_size = 0;
	static uint8_t *buffer = nullptr;
	if (buffer == nullptr) {
		buffer = new uint8_t[Settings::mtu * 2] { 0 };
	}

	static TUNMessage tm = { new uint8_t[Settings::mtu * 2], 0 };

	for (int i = 0; i < rp.size; i++) {
		if (rp.data[i] == radio_escape_char
				&& (i + 1 == rp.size ?
						true : (rp.data[i + 1] != radio_escape_char))) {
			if (message_size != 0) {
				// Write message and set stats
				tm.size = message_size;
				strncpy((char*) tm.data, (const char*) buffer, message_size);
				if (tun_handle->send(tm)) {
					//statistics_packets_ok++;

				} else {
					//statistics_packets_corrupted++;
				}
			} else {
				//statistics_packets_control++;
			}

			message_size = 0;
		} else {
			buffer[message_size++] = rp.data[i];

			// Skip the next character because this is an escape
			if (rp.data[i] == radio_escape_char)
				i++;
		}
	}

	return (true);
}
