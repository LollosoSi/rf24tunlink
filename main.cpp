/**
 * @author Andrea Roccaccino
 * Created on 03/02/2024
 */

// Defines
// #define UNIT_TEST
// Basic
#include <iostream>
using namespace std;

#include "settings/Settings.h"

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
#include "packetization/selectiverepeat_rs/RSSelectiveRepeatPacketizer.h"
#include "packetization/harq/HARQ.h"

#include "radio/NRF24/RF24Radio.h"
#include "radio/NRF24_DUAL/RF24DualRadio.h"
#include "radio/NRF24_CSMA/RF24CSMARadio.h"
#include "radio/NRF24_DUAL_THREADSAFE/RF24DualThreadsafeRadio.h"

#include "telemetry/TelemetryPrinter.h"
#include "telemetry/TimeTelemetryElement.h"

#include "interfaces/UARTHandler.h"

#include "settings/SettingsFileReader.h"

#include "rs_codec/RSCodec.h"

TUNHandler *tunh = nullptr;
PacketHandler<RadioPacket> *csp = nullptr;
RadioHandler<RadioPacket> *rh0 = nullptr;
RadioHandler<RadioPacket> *rh1 = nullptr;

#include "unit_tests/FakeRadio.h"
void test_run() {

	Settings::control_packets = false;

	FakeRadio<RadioPacket> fr(0, 10);
	csp = new HARQ();
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

#include <signal.h>

extern "C" void handle_aborts(int signal_number) {
	/*Your code goes here. You can output debugging info.
	 If you return from this function, and it was called
	 because abort() was called, your program will exit or crash anyway
	 (with a dialog box on Windows).
	 */
	std::cout << "SIGABORT caught. Ouch.\n";
}

int main(int argc, char **argv) {

	/*Do this early in your program's initialization */
	signal(SIGABRT, &handle_aborts);

#ifdef UNIT_TEST
	test_run();
#endif

	if (geteuid()) {
		cout << "Not running as root, make sure you have the required privileges to access SPI ( ) and the network interfaces (CAP_NET_ADMIN) \n";
	} else {
		cout << "Running as root\n";
	}

	if (argc < 2) {
		printf(
				"You need to include at least one settings file (./thisprogram settings.txt more_settings.txt more_more_settings.txt)\n");
		return (0);
	}

	// Initial Setup
	if (Settings::RF24::primary) {
		strcpy(Settings::address, "192.168.10.1");
		strcpy(Settings::destination, "192.168.10.2");
	} else {
		strcpy(Settings::address, "192.168.10.2");
		strcpy(Settings::destination, "192.168.10.1");
	}
	strcpy(Settings::netmask, "255.255.255.0");

	for (int i = 1; i < argc; i++) {
		read_settings(argv[i]);
	}


	printf("Radio is: %i\n", (int) Settings::RF24::primary);
	//bool primary = argv[1][0] == '1';
	cout << "Radio is " << (Settings::RF24::primary ? "Primary" : "Secondary") << endl;

	// CTRL+C Handler
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = ctrlcevent;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);

	// Choose a radio
	switch (Settings::radio_handler) {
	default:
	case 0:
		rh0 = new RF24Radio(Settings::RF24::primary, Settings::RF24::ce_pin,
				Settings::RF24::csn_pin, Settings::RF24::channel);
		break;
	case 1:
		rh0 = new RF24Radio(!Settings::RF24::primary,
				Settings::DUAL_RF24::ce_0_pin, Settings::DUAL_RF24::csn_0_pin,
				Settings::DUAL_RF24::channel_0);
		rh1 = new RF24Radio(Settings::RF24::primary,
				Settings::DUAL_RF24::ce_1_pin, Settings::DUAL_RF24::csn_1_pin,
				Settings::DUAL_RF24::channel_1);
		break;
	case 2:
		rh0 = new RF24DualRadio(Settings::RF24::primary);
		break;

	case 3:
		// Not yet implemented
		printf(
				"This Radio handler is not available! Please either finish implementation or pick another radio\n");
		exit(1);
		break;
	case 4:
		// Not yet implemented
		rh0 = new RF24CSMARadio(Settings::RF24::primary, Settings::RF24::ce_pin,
				Settings::RF24::csn_pin, Settings::RF24::channel);
		break;
	case 5:
		// Dual Threadsafe
		rh0 = new RF24DualThreadsafeRadio(Settings::RF24::primary);
		break;
	}

	// Choose a packetizer
	switch (Settings::mode) {
	default:
		csp = new SelectiveRepeatPacketizer();
		break;
	case 0:
		csp = new CharacterStuffingPacketizer();
		break;
	case 1:
		csp = new SelectiveRepeatPacketizer();
		break;
	case 2:
		csp = new ThroughputTester(true);
		break;
	case 3:
		csp = new ThroughputTester(false);
		break;
	case 4:
		csp = new RSSelectiveRepeatPacketizer();
		break;
	case 5:
		csp = new HARQ();
		break;

	}

	Settings::mtu = csp->get_mtu();
	printf("\t\tMTU:%d\n",csp->get_mtu());

	// Initialise the interface
	tunh = new TUNHandler();

	// Register everything
	csp->register_tun_handler(tunh);
	tunh->register_packet_handler(csp);
	rh0->register_packet_handler(csp);
	if (rh1 != nullptr)
		rh1->register_packet_handler(csp);

	// Start the Radio and register it
	// no action required

	// Start the interface read thread
	tunh->startThread();

	TimeTelemetryElement tte;
	TelemetryPrinter tp(Settings::csv_out_filename);
	tp.add_element(dynamic_cast<Telemetry*>(&tte));
	tp.add_element(dynamic_cast<Telemetry*>(rh0));
	if (rh1 != nullptr)
		tp.add_element(dynamic_cast<Telemetry*>(rh1));
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
	if(Settings::radio_handler==5){
		while(running){
			usleep(100000);
			std::this_thread::yield();
		}
		reinterpret_cast<RF24DualThreadsafeRadio*>(rh0)->stop_quit();

	}else{
		if (rh1 != nullptr) {

				std::thread lrh1([&] {
					while (running) {
						rh1->loop(1);
						//usleep(100);
						std::this_thread::yield();
					}
				});
				lrh1.detach();
			}
			while (running) {
				rh0->loop(1);

				//usleep(100);
				std::this_thread::yield();

			}
	}

	// Close and free everything
	tunh->stopThread();


	delete tunh;
	delete csp;
	delete rh0;
	if (rh1 != nullptr) {
		delete rh1;
	}

	return (0);
}
