/*
 * CharacterStuffingPacketizer.h
 *
 *  Created on: 4 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include <iostream>

#include "../../interfaces/Messenger.h"
#include "../../structures/TUNMessage.h"
#include "../../structures/RadioPacket.h"

class CharacterStuffingPacketizer: public Messenger<TUNMessage>,
		public Messenger<RadioPacket> {
public:
	CharacterStuffingPacketizer();
	~CharacterStuffingPacketizer();

protected:
	//Messenger<TUNMessage> *tun_handle = nullptr;
	//Messenger<RadioPacket> *radio_handle = nullptr;
	bool receive_message(TUNMessage &tunmsg) {
		// Packetize frame
		return (true);
	}
	bool receive_message(RadioPacket &rp) {
		return (true);
	}
};

