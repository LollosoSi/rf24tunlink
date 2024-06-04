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
#include <unistd.h>

#include <cmath>

#include "../telemetry/Telemetry.h"
#include "../utils.h"

template<typename packet>
class FakeRadio: Messenger<TUNMessage> {
public:
	FakeRadio(int chance_loss, int chance_corruption) {
		this->chance_loss = chance_loss;
		this->chance_corruption = chance_corruption;
	}
	virtual ~FakeRadio() {
	}

	int chance_loss = 0;

	int chance_corruption = 5;

	void test(const std::string *messages, unsigned int msize) {
		srand(time(NULL));

		printf(
				"About to send these messages with simulated %i percent packet loss\n",
				chance_loss);
		TUNMessage *tunmessages = new TUNMessage[msize];
		for (unsigned int i = 0; i < msize; i++) {
			tunmessages[i].data = new uint8_t[messages[i].length()];
			tunmessages[i].size = messages[i].length();
			printf("len %d\n",tunmessages[i].size);
			std::strcpy((char*) tunmessages[i].data,
					(const char*) messages[i].c_str());
			std::cout << messages[i] << std::endl;
			((Messenger<TUNMessage>*) pkt_ref)->send(tunmessages + i);
		}
		std::cout << "- - - - - - Begin transmission - - - - - - \n";

		int sent = 0;
		int cut = 0;

		packet *p = nullptr;
		uint64_t start_ms = current_millis();
		while (current_millis() - start_ms < 100) {
			if (pkt_ref->next_packet_ready()) {
				p = pkt_ref->next_packet();
				if (p) {

					if ((rand() % 100) > chance_loss) {
						sent++;
						for (int i = 0; i < p->size; i++) {
							if ((rand() % 100) < chance_corruption) {
								p->data[i] = p->data[i]
										* ((int) pow(2, (int) (rand() % 8)));
							}
						}
						((Messenger<RadioPacket>*) pkt_ref)->send(p);
					} else {
						cut++;
					}
				}

				//usleep(10000);
			}
		}

		std::cout << "\n\tSent: " << sent << "   Lost: " << cut << std::endl;

		print_telemetry();
		for (unsigned int i = 0; i < msize; i++)
			delete [] tunmessages[i].data;
		delete [] tunmessages;

	}

	void print_telemetry() {
		const std::string *telemetrydata =
				((dynamic_cast<Telemetry*>(pkt_ref)))->telemetry_collect(1000);
		const std::string *elementnames =
				((dynamic_cast<Telemetry*>(pkt_ref)))->get_element_names();
		printf("Telemetry entries: %i\n",
				((dynamic_cast<Telemetry*>(pkt_ref)))->size());
		for (int i = 0; i < ((dynamic_cast<Telemetry*>(pkt_ref)))->size();
				i++) {
			printf("%s: %s\t", elementnames[i].c_str(),
					telemetrydata[i].c_str());
		}
		printf("\n");
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
	bool receive_message(TUNMessage *tunmsg) {
		tunmsg->data[tunmsg->size] = '\0';
		printf("Lettura:\t%s\n", tunmsg->data);
		return (true);
	}
private:
	PacketHandler<packet> *pkt_ref = nullptr;

};

