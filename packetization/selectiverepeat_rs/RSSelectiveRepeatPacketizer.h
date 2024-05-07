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

#include "../../utils.h"

#include "../../rs_codec/RSCodec.h"

#include <cmath>
#include <cstring>

class RSSelectiveRepeatPacketizer: public PacketHandler<RadioPacket>, public Telemetry {
public:
	RSSelectiveRepeatPacketizer();
	virtual ~RSSelectiveRepeatPacketizer();

	RSCodec* rsc = nullptr;

	inline bool next_packet_ready();
	RadioPacket* next_packet();
	inline RadioPacket* get_empty_packet();

	unsigned int get_mtu(){return ((Settings::ReedSolomon::k-1)*pow(2,8-3)-4);}


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
	std::deque<RadioPacket*> resend_list_static;
	std::deque<RadioPacket*> delete_list;
	inline void received_ok(){reset_cnt = 1; current_packet_counter = 0;if(frames.empty()) return; last_frame_change = current_millis(); free_frame(frames.front()); frames.pop_front();}

	inline void free_frame(Frame<RadioPacket> *frame){

		while(!delete_list.empty()){
			//delete delete_list.front();
			delete_list.pop_front();
		}

		delete frame;
	}

	bool packetize(TUNMessage *tunmsg);

	inline RadioPacket* request_missing_packets(bool *array, unsigned int size);
	inline void response_packet_ok(uint8_t id);
	inline uint8_t get_pack_id(RadioPacket *rp);

	std::string* returnvector = nullptr;
	unsigned long fragments_received = 0, fragments_sent = 0, fragments_resent = 0, frames_completed = 0, fragments_control = 0, bytes_discarded = 0, frames_dropped = 0;

	uint64_t last_frame_change = 0;

	bool reset_cnt = 1;


};

