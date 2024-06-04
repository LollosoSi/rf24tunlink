/*
 * unit_tests.h
 *
 *  Created on: 23 May 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../packetization/harq/HARQ.h"
#include "../interfaces/UARTHandler.h"
#include "FakeRadio.h"

#include <string>
#include <sstream>

void test_run() {

	/*UARTHandler ua("/dev/ttyACM0");
	 unsigned long msecs = 0;
	 while(true){
	 if(current_millis() > msecs+1000){
	 msecs=current_millis();
	 printf("Mbps: %f\n", (ua.receivedbytes*8)/1000000.0f);
	 ua.receivedbytes=0;
	 }
	 std::this_thread::yield();
	 }*/

	Settings::control_packets = false;

	FakeRadio<RadioPacket> fr(0, 0);
	PacketHandler<RadioPacket> *csp = new HARQ();
	Settings::mtu = csp->get_mtu();

	fr.register_packet_handler(csp);

	int sw = 0;
	int sz = 1;

	for (int k = 0; k < csp->get_mtu(); k++) {

		std::string st[sz];
		std::stringstream ss;

		printf("Len %d\n",csp->get_mtu()-k-sw);
		for (unsigned int i = 0; i < csp->get_mtu()-k-sw; i++) {
			st[0].append(1,(char) (65 + (i % (91 - 65))));
		}
		fr.test(st, sz);
	}



	exit(0);
}
