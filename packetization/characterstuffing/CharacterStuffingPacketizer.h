/*
 * CharacterStuffingPacketizer.h
 *
 *  Created on: 4 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include <iostream>

#include "../../interfaces/PacketHandler.h"
#include "../../structures/RadioPacket.h"

#include "../../settings/Settings.h"

#include "../../telemetry/Telemetry.h"


class CharacterStuffingPacketizer : public PacketHandler<RadioPacket>, public Telemetry {
public:
	CharacterStuffingPacketizer();
	~CharacterStuffingPacketizer();

	unsigned int get_mtu(){return (6000);}

	bool next_packet_ready();
	RadioPacket* next_packet();
	RadioPacket* get_empty_packet();

	bool receive_packet(RadioPacket *rp);

	std::string* telemetry_collect(const unsigned long delta);


protected:
	const uint8_t radio_escape_char = '/';
	unsigned int current_packet_counter = 0;
	void received_ok(){current_packet_counter = 0; frames.pop_front();}
	void free_frame(Frame<RadioPacket>* frame);
	bool packetize(TUNMessage *tunmsg);

	std::string* returnvector = nullptr;
	unsigned int fragments_sent = 0, fragments_received = 0, fragments_control = 0, frames_completed = 0, frames_failed = 0;

};

