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

#include "../../telemetry/Telemetry.h"

#include <cmath>
#include <cstring>

class SelectiveRepeatPacketizer: public PacketHandler<RadioPacket>, public Telemetry {
public:
	SelectiveRepeatPacketizer();
	virtual ~SelectiveRepeatPacketizer();

	bool next_packet_ready();
	RadioPacket* next_packet();
	RadioPacket* get_empty_packet();

	int get_mtu(){return (31*pow(2,5));}


	uint8_t pack_info(bool first, uint8_t id, uint8_t segment){
		return (((first << 7) & 0x80) | ((id << 5) & 0x60) | (segment & 0x1F));
	}

	void unpack_info(uint8_t &data, bool &first, uint8_t &id, uint8_t &segment){
		first = (data >> 7) & 0x01;
		id = (data >> 5) & 0x03;
		segment = data & 0x1F;
	}

	bool receive_packet(RadioPacket *rp);
	std::string* telemetry_collect(const unsigned long delta);


protected:
	unsigned int current_packet_counter = 0;
	std::deque<RadioPacket*> resend_list;
	void received_ok(){current_packet_counter = 0; free_frame(frames.front()); delete frames.front(); frames.pop_front();}

	void free_frame(Frame<RadioPacket> *frame);
	bool packetize(TUNMessage *tunmsg);

	bool request_missing_packets(bool *array, unsigned int size);
	inline void response_packet_ok(uint8_t id);
	inline uint8_t get_pack_id(RadioPacket *rp);

	std::string* returnvector = nullptr;
	unsigned int fragments_received = 0, fragments_sent = 0, fragments_resent = 0, frames_completed = 0, fragments_control = 0;


};

