/*
 * SelectiveRepeatPacketizer.h
 *
 *  Created on: 4 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../../interfaces/PacketHandler.h"
#include "../../structures/RadioPacket.h"

#include "../../settings/Settings.h"

class SelectiveRepeatPacketizer: public PacketHandler<RadioPacket> {
public:
	SelectiveRepeatPacketizer();
	virtual ~SelectiveRepeatPacketizer();

	bool next_packet_ready();
	RadioPacket* next_packet();
	RadioPacket* get_empty_packet();

protected:
	unsigned int current_packet_counter = 0;
	std::deque<int> resend_list;
	void received_ok(){current_packet_counter = 0; free_frame(frames.front()); delete frames.front(); frames.pop_front();}

	void free_frame(Frame<RadioPacket> *frame);
	bool packetize(TUNMessage &tunmsg);
	bool receive_packet(RadioPacket &rp);

};

