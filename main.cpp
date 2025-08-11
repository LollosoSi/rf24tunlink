#include <iostream>
#include "config.h"

#include "Settings.h"
#include "rs_codec/RSCodec.h"

#include "activity_signals/activity_led.h"
#include "activity_signals/UdpActivity.h"


#include "generic_structures.h"
#include "system_dialogators/tun/TUNInterface.h"
#include "system_dialogators/uart/UARTHandler.h"

#include "packetizers/Packetizer.h"
#include "radio/RadioInterface.h"

#include "radio/dualrf24/DualRF24.h"
#include "radio/singlerf24/SingleRF24.h"
#include "radio/picorf24/PicoRF24.h"
#include "radio/uartrf/UARTRF.h"

#include "packetizers/harq/HARQ.h"
#include "packetizers/arq/ARQ.h"
#include "packetizers/latency_evaluator/LatencyEvaluator.h"

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

void test(){

	//UARTHandler ua("/dev/ttyACM0");
	/*unsigned long msecs = 0;
	while (true) {
		if (current_millis() > msecs + 1000) {
			msecs = current_millis();
			printf("Mbps: %f\n", (ua.receivedbytes * 8) / 1000000.0f);
			ua.receivedbytes = 0;
		}
		std::this_thread::yield();
	}*/

	Settings S;

	PicoRF24 prf = PicoRF24();
	prf.apply_settings(S);

	std::this_thread::sleep_for(std::chrono::milliseconds(5000));

	prf.apply_settings(S);
	std::this_thread::sleep_for(std::chrono::milliseconds(5000));
}

void catch_setups() {
	/*Do this early in your program's initialization */
	signal(SIGABRT, [](int signal_number) {
		/*Your code goes here. You can output debugging info.
		 If you return from this function, and it was called
		 because abort() was called, your program will exit or crash anyway
		 (with a dialog box on Windows).
		 */
		stop_program=true;
		cv.notify_all();
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
			cv.notify_all();
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
	case Settings::packetizers_available::latency_evaluator:
			return new LatencyEvaluator();
			break;
	case Settings::packetizers_available::arq:
		return new ARQ();
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
	case Settings::radios_available::singlerf24:
		return new SingleRF24();
		break;
	case Settings::radios_available::picorf24:
		return new PicoRF24();
		break;
	case Settings::radios_available::uartrf:
			return new UARTRF();
			break;
	}
}

int main(int argc, char **argv) {

	catch_setups();

	prctl(PR_SET_NAME, "Main Thread", 0, 0, 0);


	//test();
	//return 0;

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
	ActivitySignalInterface* actl = nullptr;
	if(settings.use_activity_led)
		actl = new ActivityLed(settings);
	if(settings.use_udp_activity){
		actl = new UdpActivity(settings);
	}


	TUNInterface TUNI;
	RadioInterface* radio = select_radio_from_settings(settings);
	Packetizer<TunMessage, RFMessage>* packetizer = select_packetizer_from_settings(settings);
	if(actl) packetizer->register_activity_led(actl);

	packetizer->register_rsc(&RSC);
	packetizer->register_tun(&TUNI);
	packetizer->register_radio(radio);
	radio->register_packet_target(packetizer->expose_radio_receiver());
	TUNI.register_packet_target(packetizer->expose_tun_receiver());

	PacketMessageFactory PMF(settings);


	std::function<void()> reload_settings_function([&] {
		RSC.apply_settings(settings);
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

	//while(!stop_program){
	//	printf("Type 'r' to reload settings, type 'q' to quit\n");
		//std::this_thread::sleep_for(1000ms);
//	char a;
//		cin >> a;
//		switch (a) {
//		case 'q':
//			stop_program = true;
//			break;
//		case 'r':
//			read_settings_function();
//			reload_settings_function();
//			break;
//		}
	//	std::this_thread::sleep_for(std::chrono::microseconds(1000));
	//}

	{
		std::unique_lock lock(sleep_mutex);
		cv.wait(lock, [&]{return stop_program;});
	}

	TUNI.stop();
	radio->stop();
	packetizer->stop();
	actl->stop();

	delete packetizer;
	delete radio;
	delete actl;

	return 0;
}
