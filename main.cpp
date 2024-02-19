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

TUNHandler *tunh = nullptr;
PacketHandler<RadioPacket> *csp = nullptr;
RadioHandler<RadioPacket> *rh = nullptr;

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
	rh = new RF24DualRadio(primary);

	// Choose a packetizer
	csp = new SelectiveRepeatPacketizer();
	Settings::mtu = csp->get_mtu();

	// Initialise the interface
	tunh = new TUNHandler();

	// Register everything
	csp->register_tun_handler(tunh);
	tunh->register_packet_handler(csp);
	rh->register_packet_handler(csp);

	// Start the Radio and register it
	// no action required

	// Start the interface read thread
	tunh->startThread();

	std::thread lp([&] {
		while (running) {
			std::system("clear");
			const std::string *telemetrydata = ((dynamic_cast<Telemetry*>(rh)))->telemetry_collect(
			1000);
			const std::string *elementnames =
					((dynamic_cast<Telemetry*>(rh)))->get_element_names();
			printf("\t\t%s Telemetry entries\n", (dynamic_cast<Telemetry*>(rh))->get_name().c_str());
			for (int i = 0; i < ((dynamic_cast<Telemetry*>(rh)))->size(); i++) {
				printf("%s: %s\t", elementnames[i].c_str(),
						telemetrydata[i].c_str());
			}
			printf("\n\n");

			telemetrydata =
					((dynamic_cast<Telemetry*>(csp)))->telemetry_collect(1000);
			elementnames =
					((dynamic_cast<Telemetry*>(csp)))->get_element_names();
			printf("\t\t%s Telemetry entries\n", (dynamic_cast<Telemetry*>(csp))->get_name().c_str());
			for (int i = 0; i < ((dynamic_cast<Telemetry*>(csp)))->size();
					i++) {
				printf("%s: %s\t", elementnames[i].c_str(),
						telemetrydata[i].c_str());
			}
			printf("\n\n");

			telemetrydata =
					((dynamic_cast<Telemetry*>(tunh)))->telemetry_collect(1000);
			elementnames =
					((dynamic_cast<Telemetry*>(tunh)))->get_element_names();
			printf("\t\t%s Telemetry entries\n", (dynamic_cast<Telemetry*>(tunh))->get_name().c_str());
			for (int i = 0; i < ((dynamic_cast<Telemetry*>(tunh)))->size();
					i++) {
				printf("%s: %s\t", elementnames[i].c_str(),
						telemetrydata[i].c_str());
			}
			printf("\n\n");

			usleep(1000000);
			std::this_thread::yield();
		}
	});
	lp.detach();

	// Program loop (radio loop)
	while (running) {
		//do {
			rh->loop(1);
		//} while (rh->is_receiving_data() || !csp->empty());
		std::this_thread::yield();
		// usleep(10000); // 14% cpu
		//if(csp->empty())
		//	usleep(10000);
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
	delete rh;

	return (0);
}
