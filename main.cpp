#include <iostream>
#include "config.h"

#include "Settings.h"
#include "rs_codec/RSCodec.h"

#include "generic_structures.h"
#include "system_dialogators/tun/TUNInterface.h"
#include "packetizers/Packetizer.h"
#include "radio/RadioInterface.h"

#include "radio/dualrf24/DualRF24.h"

#include "packetizers/harq/HARQ.h"

// Thread naming
#include <sys/prctl.h>

// Catch CTRL+C Event
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// Exit conditions
#include <mutex>
#include <condition_variable>

#include <functional>

std::mutex sleep_mutex;
std::condition_variable cv;
bool ready_to_exit = false;

bool stop_program = false;

void catch_setups() {
	/*Do this early in your program's initialization */
	signal(SIGABRT, [](int signal_number) {
		/*Your code goes here. You can output debugging info.
		 If you return from this function, and it was called
		 because abort() was called, your program will exit or crash anyway
		 (with a dialog box on Windows).
		 */
		stop_program=true;
		std::cout << "SIGABORT caught. Ouch.\n";
	});

	// CTRL+C Handler
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = [](int s) {
		std::cout << "Caught a signal, number:\t" << (int) s << std::endl;
		if (s == 2) {
			printf("Caught exit signal\n");
			// TODO: Handle signals gracefully
			stop_program=true;
		}
	};
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);
}

Packetizer<TunMessage, RFMessage>* select_packetizer_from_settings(const Settings &settings) {
	Settings::packetizers_available psel = settings.find_packetizer_index();
	switch (psel) {
	default:
		std::cout << "Packetizer not found in code. Did you forget to register it in the main function?\n";
		throw std::invalid_argument("Invalid packetizer in switch case");
		exit(1);
		break;
	case Settings::packetizers_available::harq:
		return new HARQ();
		break;
	}
}

RadioInterface* select_radio_from_settings(const Settings &settings) {
	Settings::radios_available rsel = settings.find_radio_index();
	switch (rsel) {
	default:
		std::cout << "Radio not found in code. Did you forget to register it in the main function?\n";
		throw std::invalid_argument("Invalid radio in switch case");
		exit(1);
		break;
	case Settings::radios_available::dualrf24:
		return new DualRF24();
		break;
	}
}

int main(int argc, char **argv) {

	catch_setups();

	prctl(PR_SET_NAME, "Main Thread", 0, 0, 0);

	Settings settings;

	if (geteuid())
		std::cout << "Not running as root, make sure you have the required privileges to access SPI ( ) and the network interfaces (CAP_NET_ADMIN) \n";

	std::cout << "rf24tunlink 2 BETA\t" << "Version " << rf24tunlink2_VERSION_MAJOR << "." << rf24tunlink2_VERSION_MINOR << std::endl;

	if (argc < 2) {
		std::cout << "You need to include at least one settings file (./thisprogram settings.txt more_settings.txt more_more_settings.txt)\nPrinting tunlink_default.txt for you in the working directory\n";
		settings.to_file("tunlink_default.txt");
		return (0);
	}

	std::function<void()> read_settings_function([&] {
		for (int i = 1; i < argc; i++)
			settings.read_settings(argv[i]);

		settings.print_all_settings(std::cout, false);
	});
	read_settings_function();


	RSCodec RSC(settings);

	TUNInterface TUNI;
	RadioInterface* radio = select_radio_from_settings(settings);
	Packetizer<TunMessage, RFMessage>* packetizer = select_packetizer_from_settings(settings);

	packetizer->register_rsc(&RSC);
	packetizer->register_tun(&TUNI);
	packetizer->register_radio(radio);
	radio->register_packet_target(packetizer->expose_radio_receiver());
	TUNI.register_packet_target(packetizer->expose_tun_receiver());

	PacketMessageFactory PMF(settings);


	std::function<void()> reload_settings_function([&] {
		// MTU can only be calcd after applying the settings
		packetizer->apply_settings(settings);
		settings.mtu = packetizer->get_mtu();
		PMF = PacketMessageFactory(settings);

		packetizer->register_message_factory(&PMF);
		radio->register_message_factory(&PMF);

		TUNI.apply_settings(settings); // The read thread will start here
		radio->apply_settings(settings);
	});
	reload_settings_function();

	// If needed, reload at runtime
	//read_settings_function();
	//reload_settings_function();

	while(!stop_program){

		std::this_thread::sleep_for(1000ms);
	}

	TUNI.stop();
	radio->stop();
	packetizer->stop();

	delete packetizer;
	delete radio;

	return 0;
}
