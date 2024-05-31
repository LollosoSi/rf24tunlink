/*
 * LatencyEvaluator.h
 *
 *  Created on: 31 May 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../Packetizer.h"

class LatencyEvaluator : public Packetizer<TunMessage,RFMessage>{
	public:
		LatencyEvaluator();
		virtual ~LatencyEvaluator();


		void process_tun(TunMessage &m);
		void process_packet(RFMessage &m);
		void apply_settings(const Settings& settings) override;
		void send_now();
		unsigned int get_mtu() override {return 100;}

		uint64_t sendtime = 0;
		uint64_t counter = 0;
};


