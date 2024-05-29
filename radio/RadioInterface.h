/*
 * Radio.h
 *
 *  Created on: 27 May 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../../generic_structures.h"

class RadioInterface : public SystemInterface<RFMessage>, public SyncronizedShutdown {

	protected:
		SystemInterface<RFMessage>* packetizer = nullptr;
		PacketMessageFactory* pmf = nullptr;

	public:
		RadioInterface() = default;
		virtual ~RadioInterface(){}

		virtual void input_finished() = 0;
		virtual bool input(RFMessage &m) = 0;
		virtual void apply_settings(const Settings &settings) override {
			SystemInterface<RFMessage>::apply_settings(settings);
		}
		virtual void stop_module() = 0;

		void register_packet_target(SystemInterface<RFMessage>* receiver) {
			packetizer = receiver;
		}

		void register_message_factory(PacketMessageFactory* pmfr) {
			pmf = pmfr;
		}

};
