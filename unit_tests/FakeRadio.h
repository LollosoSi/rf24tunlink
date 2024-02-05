/*
 * FakeRadio.h
 *
 *  Created on: 5 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../interfaces/PacketHandler.h"
#include <string>
#include <cstring>
#include <iostream>
#include <stdio.h>

template<typename packet>
class FakeRadio : Messenger<TUNMessage> {
public:
	FakeRadio(int chance){chance_loss = chance;}
	virtual ~FakeRadio(){}

	int chance_loss = 50;

	void test(const std::string* messages, unsigned int msize){
		srand(time(NULL));

		printf("About to send these messages with simulated %i percent packet loss\n", chance_loss);
		TUNMessage *tunmessages = new TUNMessage[msize];
		for(unsigned int i = 0; i < msize; i++){
			tunmessages[i].data = new uint8_t[messages[i].length()];
			tunmessages[i].size = messages[i].length();
			std::strcpy((char*)tunmessages[i].data, (const char*)messages[i].c_str());
			std::cout << messages[i] << std::endl;
			((Messenger<TUNMessage>*)pkt_ref)->send(tunmessages[i]);
		}
		std::cout << "- - - - - - Begin transmission - - - - - - \n";

		int sent = 0;
		int cut = 0;

		packet *p = nullptr;
		while(pkt_ref->next_packet_ready()){
			p = pkt_ref->next_packet();
			if(rand()%100 > chance_loss){
				sent++;
				((Messenger<RadioPacket>*)pkt_ref)->send(*p);
			}else{
				cut++;
			}

		}

		std::cout << "\n\tSent: " << sent << "   cut: " << cut << std::endl;




	}

	bool register_packet_handler(PacketHandler<packet> *pkt_handler_pointer) {
		if (this->pkt_ref == nullptr) {
			this->pkt_ref = pkt_handler_pointer;
			pkt_ref->register_tun_handler(this);
			return (true);
		} else {
			return (false);
		}
	}
protected:
	bool receive_message(TUNMessage &tunmsg){
		printf("%s\n",tunmsg.data);

		return (true);
	}
private:
	PacketHandler<packet> *pkt_ref = nullptr;
};

