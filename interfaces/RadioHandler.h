/*
 * RadioHandler.h
 *
 *  Created on: 6 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "PacketHandler.h"

#include "../settings/Settings.h"

template<typename packet>
class RadioHandler{

public:
	RadioHandler(){}
	virtual ~RadioHandler(){}

	bool register_packet_handler(PacketHandler<packet> *pkt_handler_pointer) {
			if (this->pkt_handle == nullptr) {
				this->pkt_handle = pkt_handler_pointer;
				return (true);
			} else {
				return (false);
			}
		}

	virtual void setup() = 0;
	virtual void loop(unsigned long delta) = 0;

protected:
	PacketHandler<packet>* pkt_handle = nullptr;


	virtual void stop() = 0;

	bool has_next_packet(){return (pkt_handle->next_packet_ready());}
	packet* next_packet(){return (pkt_handle->next_packet());}

	bool packet_received(packet* pa){return (((Messenger<packet>*)pkt_handle)->send(pa));}


};
