/*
 * PacketHandler.h
 *
 *  Created on: 4 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include <iostream>

#include <deque>

#include "Messenger.h"
#include "../structures/TUNMessage.h"
#include "../structures/RadioPacket.h"


template<typename packet>
struct Frame {
	std::deque<packet*> packets;
};

typedef Messenger<TUNMessage> tun;

template<typename packet>
class PacketHandler: public Messenger<TUNMessage>, public Messenger<packet>{

public:
	PacketHandler() {};
	virtual ~PacketHandler() {
	}

	virtual inline bool next_packet_ready() = 0;
	virtual packet* next_packet() = 0;
	virtual inline packet* get_empty_packet() = 0;
	virtual unsigned int get_mtu() = 0;

	bool register_tun_handler(tun *tun_handler_pointer) {
		if (this->tun_handle == nullptr) {
			this->tun_handle = tun_handler_pointer;
			return (true);
		} else {
			return (false);
		}
	}

	bool receive_message(TUNMessage *tunmsg) {
		// Packetize frame
		return (packetize(tunmsg));
	}
	bool receive_message(RadioPacket *rp) {
		return (receive_packet(rp));
	}


protected:
	tun *tun_handle = nullptr;

	std::deque<Frame<packet>*> frames;

	virtual bool packetize(TUNMessage *tunmsg) = 0;
	virtual bool receive_packet(RadioPacket *rp) = 0;

	virtual inline void free_frame(Frame<packet> *frame) = 0;

};
