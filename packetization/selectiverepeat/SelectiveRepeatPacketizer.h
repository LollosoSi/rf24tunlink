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

	inline bool next_packet_ready();
	RadioPacket* next_packet();
	inline RadioPacket* get_empty_packet();

	unsigned int get_mtu(){return (961);}


	inline static uint8_t pack_info(bool first, uint8_t id, uint8_t segment){
		return (((first << 7) & 0x80) | ((id << 5) & 0x60) | (segment & 0x1F));
	}

	inline static void unpack_info(uint8_t &data, bool &first, uint8_t &id, uint8_t &segment){
		first = (data >> 7) & 0x01;
		id = (data >> 5) & 0x03;
		segment = data & 0x1F;
	}

	bool receive_packet(RadioPacket *rp);
	std::string* telemetry_collect(const unsigned long delta);
	uint8_t *buffer = nullptr;

protected:
	unsigned int current_packet_counter = 0;
	std::deque<RadioPacket*> resend_list;
	inline void received_ok(){current_packet_counter = 0;if(frames.empty()) return; free_frame(frames.front()); frames.pop_front();}

	inline void free_frame(Frame<RadioPacket> *frame){
		delete frame;
	}

	bool packetize(TUNMessage *tunmsg);

	inline bool request_missing_packets(bool *array, unsigned int size);
	inline void response_packet_ok(uint8_t id);
	inline uint8_t get_pack_id(RadioPacket *rp);

	std::string* returnvector = nullptr;
	unsigned long fragments_received = 0, fragments_sent = 0, fragments_resent = 0, frames_completed = 0, fragments_control = 0, bytes_discarded = 0;



};

