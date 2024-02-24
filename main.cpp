/**
 * @author Andrea Roccaccino
 * Created on 03/02/2024
 */

// Defines
// #define UNIT_TEST
// Basic
#include <iostream>
using namespace std;

// Utilities
#include <cstring>
#include <thread>

// Catch CTRL+C Event
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// Custom implementations
#include "settings/DefaultSettings.h"
#include "tun/TUNHandler.h"
#include "packetization/characterstuffing/CharacterStuffingPacketizer.h"
#include "packetization/selectiverepeat/SelectiveRepeatPacketizer.h"
#include "packetization/throughputtester/ThroughputTester.h"
#include "radio/NRF24/RF24Radio.h"
#include "radio/NRF24_DUAL/RF24DualRadio.h"

#include "telemetry/TelemetryPrinter.h"
#include "telemetry/TimeTelemetryElement.h"

TUNHandler *tunh = nullptr;
PacketHandler<RadioPacket> *csp = nullptr;
RadioHandler<RadioPacket> *rh0 = nullptr;
RadioHandler<RadioPacket> *rh1 = nullptr;

#include "unit_tests/FakeRadio.h"
void test_run() {

	Settings::control_packets = false;

	FakeRadio<RadioPacket> fr(0);
	csp = new CharacterStuffingPacketizer();
	Settings::mtu = csp->get_mtu();

	fr.register_packet_handler(csp);

	std::string st[7] =
			{ "Hello, my name is Andrea",
					"And I am the developer behind rf24tunlink",
					"This is a very important test in order to understand how strong and well thought the packetizer is",
					"At the moment we have two packetizers available",
					"One is based on the old simple character stuffing technique with no retransmission",
					"The other is based on ARQ algorithms, in particular, Selective Retransmission. something weird is happening the longer the message. but the packetizer seems to be correct",
					"This message should be exactly long as the MTU size, which for this test is 93, if you don't believe me, you are probably right. But who am I to argue?" };
	int sz = sizeof(st) / sizeof(std::string);
	fr.test(st, sz);

	exit(0);
}

bool running = true;
void ctrlcevent(int s) {
	if (s == 2) {
		printf("\nCaught exit signal\n");
		running = false;
	}
}

int main(int argc, char **argv) {

#ifdef UNIT_TEST
	test_run();
#endif

	if (geteuid()) {
		cout
				<< "Not running as root, make sure you have the required privileges to access SPI ( ) and the network interfaces (CAP_NET_ADMIN) \n";
	} else {
		cout << "Running as root\n";
	}

	if (argc < 2) {
		printf(
				"You need to specify primary or secondary role (./thisprogram [1 or 2])");
		return 0;
	}

	printf("Radio is: %d\n", (int) argv[1][0]);

	bool primary = argv[1][0] == '1';
	cout << "Radio is " << (primary ? "Primary" : "Secondary") << endl;

	// CTRL+C Handler
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = ctrlcevent;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);

	// Initial Setup
	if (primary) {
		strcpy(Settings::address, "192.168.10.1");
		strcpy(Settings::destination, "192.168.10.2");
	} else {
		strcpy(Settings::address, "192.168.10.2");
		strcpy(Settings::destination, "192.168.10.1");
	}
	strcpy(Settings::netmask, "255.255.255.0");

	// Choose a radio
	rh0 = new RF24Radio(primary, Settings::DUAL_RF24::ce_0_pin,
			Settings::DUAL_RF24::csn_0_pin, Settings::DUAL_RF24::channel_0);
	//rh1 = new RF24Radio(primary, Settings::DUAL_RF24::ce_1_pin,
	//		Settings::DUAL_RF24::csn_1_pin, Settings::DUAL_RF24::channel_1);
	//rh0 = new RF24DualRadio(primary);

	// Choose a packetizer
	csp = new SelectiveRepeatPacketizer();
	Settings::mtu = csp->get_mtu();

	// Initialise the interface
	tunh = new TUNHandler();

	// Register everything
	csp->register_tun_handler(tunh);
	tunh->register_packet_handler(csp);
	rh0->register_packet_handler(csp);
	//rh1->register_packet_handler(csp);

	// Start the Radio and register it
	// no action required

	// Start the interface read thread
	tunh->startThread();

	TimeTelemetryElement tte;
	TelemetryPrinter tp(Settings::csv_out_filename);
	tp.add_element(dynamic_cast<Telemetry*>(&tte));
	tp.add_element(dynamic_cast<Telemetry*>(rh0));
	tp.add_element(dynamic_cast<Telemetry*>(csp));
	tp.add_element(dynamic_cast<Telemetry*>(tunh));

	std::thread lp([&] {
		while (running) {
			tp.tick();

			usleep(1000000); // 1 second
			//usleep(500000);
			std::this_thread::yield();
		}
	});
	lp.detach();

	/*
	 std::thread lr([&] {
	 while (running) {
	 rh1->loop(1);
	 std::this_thread::yield();
	 }
	 });
	 lr.detach();*/

	// Program loop (radio loop)
	while (running) {
		//do {
		rh0->loop(1);
		//rh1->loop(1);

		//} while (rh0->is_receiving_data() || rh1->is_receiving_data() || !csp->empty());
		std::this_thread::yield();
		//usleep(10000); // 14% cpu
		//if(csp->empty())
		//	usleep(3000);
		//if(!rh->is_receiving_data() && csp->empty()){
		//	usleep(10000);
		//usleep(10000);
		//	usleep(2);
		//std::this_thread::yield();
		//}

	}
	// Close and free everything
	tunh->stopThread();

	delete tunh;
	delete csp;
	delete rh0;
	//delete rh1;

	return (0);
}
