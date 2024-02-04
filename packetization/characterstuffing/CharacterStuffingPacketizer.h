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


class CharacterStuffingPacketizer : public PacketHandler<RadioPacket> {
public:
	CharacterStuffingPacketizer();
	~CharacterStuffingPacketizer();

	bool next_packet_ready();
	RadioPacket& next_packet();
	RadioPacket& get_empty_packet();

protected:
	unsigned int current_packet_counter = 0;
	void received_ok(){current_packet_counter = 0; free_frame(frames.front()); frames.pop_front();}
	void free_frame(Frame<RadioPacket>& frame);

};
