/*
 * ThroughputTester.h
 *
 *  Created on: 11 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../../interfaces/PacketHandler.h"
#include "../../structures/RadioPacket.h"

#include "../../settings/Settings.h"

#include "../../telemetry/Telemetry.h"

#include "../../rs_codec/RSCodec.h"

#include <vector>
#include <array>
#include <functional>
#include <algorithm>

struct error_test{
	bool representation[32*8]={1};
	uint32_t popularity = 1;
};

class ThroughputTester: public PacketHandler<RadioPacket>, public Telemetry {
public:
	ThroughputTester();
	virtual ~ThroughputTester();

	inline bool next_packet_ready() {
		return (true);
	}

	RadioPacket* next_packet();
	inline RadioPacket* get_empty_packet() {
		return (nullptr);
	}

	unsigned int get_mtu() {
		return 9999;
	}


	std::string* telemetry_collect(const unsigned long delta);

	RSCodec* rsc = nullptr;

protected:
	RadioPacket test_packets[10];

	std::string* returnvector = nullptr;

	std::vector<error_test*> error_structs;

	unsigned long received_packets_length = 0;
	RadioPacket received_packets[10000];

	bool is_valid(RadioPacket &rp) {
		if (rp.size == 32) {
			for (int j = 1; j < 32; j++)
				if (rp.data[j] != rp.data[0])
					return (false);
			return (true);
		} else
			return (false);
	}

	bool packetize(TUNMessage *tunmsg) {

		// This class is dedicated to testing, no real data is allowed

		return (true);
	}

	bool receive_packet(RadioPacket *rp);

	virtual inline void free_frame(Frame<RadioPacket> *frame) {
	}


};

