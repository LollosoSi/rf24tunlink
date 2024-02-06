/**
 * @author Andrea Roccaccino
 * Created on 03/02/2024
 */

// Defines
 #define UNIT_TEST

// Basic
#include <iostream>
using namespace std;

// Utilities
#include <cstring>

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


TUNHandler *tunh = nullptr;
PacketHandler<RadioPacket> *csp = nullptr;


#include "unit_tests/FakeRadio.h"
void test_run(){
	Settings::control_packets = false;

	FakeRadio<RadioPacket> fr(50);
	csp = new SelectiveRepeatPacketizer();


	fr.register_packet_handler(csp);



	std::string st[5]={"Ciao", "Sono", "Andrea", "e sto provando se questo modulo funziona, e se e quanti dati perde o recupera, non sarà un compito semplice però credo che ci si possa riuscire","A Quanto pare sono riuscito nel mio intento"};
	int sz = sizeof(st)/sizeof(std::string);
	fr.test(st,sz);

	exit(0);
}

bool running = true;
void ctrlcevent(int s) {
	if(s == 2){
		printf("\nCaught exit signal\n");
		running = false;
	}
}

int main(int argc, char **argv) {

#ifdef UNIT_TEST
	test_run();
#endif

	if (geteuid()){
		cout << "Not running as root, make sure you have the required privileges to access SPI ( ) and the network interfaces (CAP_NET_ADMIN) \n";
	}else{
		cout << "Running as root\n";
	}

	// CTRL+C Handler
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = ctrlcevent;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);

	// Initial Setup
	bool role = 1;
	if (role) {
		strcpy(Settings::address, "192.168.10.1");
		strcpy(Settings::destination, "192.168.10.2");
	} else {
		strcpy(Settings::address, "192.168.10.2");
		strcpy(Settings::destination, "192.168.10.1");
	}
	strcpy(Settings::netmask, "255.255.255.0");

	Settings::mtu = 1000;

	// Initialise the interface
	tunh = new TUNHandler();

	// Choose a packetizer and register it
	csp = new CharacterStuffingPacketizer();
	tunh->register_packet_handler(csp);

	// Start the Radio and register it
	// TODO: radio

	// Start the interface read thread
	tunh->startThread();

	// Program loop (radio loop)
	while (running) {
		usleep(1000);
	}

	// Close and free everything
	tunh->stopThread();

	delete tunh;
	delete csp;

	return (0);
}
