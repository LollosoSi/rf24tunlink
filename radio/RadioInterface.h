/*
 * Radio.h
 *
 *  Created on: 27 May 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../generic_structures.h"

class RadioInterface : public SystemInterface<RFMessage>, public SyncronizedShutdown {

	protected:
		SystemInterface<RFMessage>* packetizer = nullptr;
		PacketMessageFactory* pmf = nullptr;

	public:
		RadioInterface() = default;
		virtual ~RadioInterface(){}

		virtual inline void input_finished() = 0;
		virtual inline bool input(RFMessage &m) = 0;
		virtual inline void apply_settings(const Settings &settings) override {
			SystemInterface<RFMessage>::apply_settings(settings);
		}
		virtual inline void stop_module() = 0;

		inline void register_packet_target(SystemInterface<RFMessage>* receiver) {
			packetizer = receiver;
		}

		inline void register_message_factory(PacketMessageFactory* pmfr) {
			pmf = pmfr;
		}

};
