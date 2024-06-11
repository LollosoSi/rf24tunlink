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
		Packetizer<TunMessage,RFMessage>::Frame build_out();

		uint64_t received = 0, sent = 0;
		uint64_t received_bytes = 0;

		void process_tun(TunMessage &m);
		void process_packet(RFMessage &m);
		void apply_settings(const Settings& settings) override;
		void send_now(Frame &f);
		unsigned int get_mtu() override {return 100;}

		uint64_t sendtime = 0, diffsum = 0;
		double counter = 0;
};


