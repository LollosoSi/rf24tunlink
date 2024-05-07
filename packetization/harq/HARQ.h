/*
 * HARQ.h
 *
 *  Created on: 7 May 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../../interfaces/PacketHandler.h"
#include "../../telemetry/Telemetry.h"
#include "../../rs_codec/RSCodec.h"
#include <cmath>

class HARQ: public PacketHandler<RadioPacket>, public Telemetry {
public:
	HARQ();
	virtual ~HARQ();

	inline unsigned char bits_to_mask(const int bits) {return (pow(2,bits)-1);}

	static const int id_bits = 2, segment_bits = 5, last_packet_bits = 1; // @suppress("Multiple variable declaration")
	static const int id_byte_pos_offset = 8-id_bits, segment_byte_pos_offset = 8 - id_bits - segment_bits, last_packet_byte_pos_offset = 8 - id_bits - segment_bits - last_packet_bits; // @suppress("Multiple variable declaration")
	unsigned char id_mask = bits_to_mask(id_bits), segment_mask = bits_to_mask(segment_bits), last_packet_mask = bits_to_mask(last_packet_bits); // @suppress("Multiple variable declaration")

	inline unsigned char pack(unsigned char id, unsigned char segment, bool last_packet){return (0x00|((id&id_mask)<<id_byte_pos_offset)|((segment&segment_mask)<<segment_byte_pos_offset)|(last_packet&last_packet_mask));}
	inline void unpack(unsigned char data, unsigned char &id, unsigned char &segment, bool &last_packet){id=id_mask&(data>>id_byte_pos_offset);segment=segment_mask&(data>>segment_byte_pos_offset); last_packet=last_packet_mask&(data>>last_packet_byte_pos_offset);}

	const int submessage_bytes = 32;
	const int header_bytes = 1;
	const int length_last_packet_bytes=1;
	int bytes_per_submessage = submessage_bytes-Settings::ReedSolomon::k-header_bytes;

	unsigned int get_mtu(){return ((pow(2,segment_bits)*bytes_per_submessage)-length_last_packet_bytes);}

	inline bool next_packet_ready();
	RadioPacket* next_packet();
	inline RadioPacket* get_empty_packet();


protected:

	bool packetize(TUNMessage *tunmsg){

	}
	bool receive_packet(RadioPacket *rp);

	inline void free_frame(Frame<RadioPacket> *frame){};

};

