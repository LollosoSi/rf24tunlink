/**
 * @author Andrea Roccaccino
 * Created on 03/02/2024
 */

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

bool running = true;
void ctrlcevent(int s) {
	printf("Caught signal %d\n", s);
	running = false;
}

int main(int argc, char **argv) {

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
		strcpy(Settings::address, "192.168.1.1");
		strcpy(Settings::destination, "192.168.1.2");
	} else {
		strcpy(Settings::address, "192.168.1.2");
		strcpy(Settings::destination, "192.168.1.1");
	}
	strcpy(Settings::netmask, "255.255.255.0");

	Settings::mtu = 900;

	// Initialise the interface
	TUNHandler tunh = TUNHandler();

	// Choose a packetizer and register it
	CharacterStuffingPacketizer csp = CharacterStuffingPacketizer();
	tunh.register_packet_handler(&csp);

	// Start the Radio and register it
	// TODO: radio

	// Start the interface read thread
	tunh.startThread();

	// Program loop (radio loop)
	while (running) {

	}

	// Close and free everything
	tunh.stopThread();

}
