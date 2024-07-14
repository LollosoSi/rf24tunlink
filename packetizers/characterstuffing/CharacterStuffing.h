/*
 * CharacterStuffing.h
 *
 *  Created on: 27 May 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../Packetizer.h"

class CharacterStuffing : public Packetizer<TunMessage,RFMessage> {

	protected:
		Settings const* settings = nullptr;
		unsigned int mtu = 100;
		uint8_t magic_char = 235;

	public:
		CharacterStuffing();
		virtual ~CharacterStuffing();

		void process_tun(TunMessage &m);
		void process_packet(RFMessage &m);
		void apply_settings(const Settings &settings);
		unsigned int get_mtu();
};
