#include <iostream>

#include "Settings.h"

#include "tuntaphandler.h"

#include <linux/if.h>
#include <linux/if_tun.h>

#include <cstring>
#include <string>

#include <fcntl.h>
#include <sys/ioctl.h>  // Per ioctl
#include <unistd.h>     // Per close

#include <bitset>


radiopacket testpacket { (uint8_t) 0xE2 };

RF24 radio(25, 0);



void justtest(bool primary) {

	while (!radio.begin(Settings::ce_pin, 0)) {
		cout << "Radio can't begin\n";
	}

	radio.setRetries(Settings::radio_delay, Settings::radio_retries);
	radio.setChannel(Settings::channel);
	radio.setCRCLength(Settings::crclen);
	radio.setAddressWidth(3);
	radio.setPALevel(RF24_PA_MAX);
	radio.setDataRate(RF24_2MBPS);
	radio.setAutoAck(Settings::auto_ack);

	// to use ACK payloads, we need to enable dynamic payload lengths (for all nodes)
	if (Settings::dynamic_payloads)
		radio.enableDynamicPayloads();  // ACK payloads are dynamically sized
	else
		radio.disableDynamicPayloads();

	// Acknowledgement packets have no payloads by default. We need to enable
	// this feature for all nodes (TX & RX) to use ACK payloads.
	if (Settings::ack_payloads)
		radio.enableAckPayload();
	else
		radio.disableAckPayload();


	if (primary) {
		radio.openReadingPipe(1, '1');
		radio.openWritingPipe('0');
	} else {
		radio.openReadingPipe(1, '0');
		radio.openWritingPipe('1');
		radio.startListening();
	}

	radio.printPrettyDetails();

	if (primary) {
		radio.write(&testpacket, 32);
		cout << "OUT: " << radioppppprintchararray((uint8_t*) &testpacket, 32);
	}

	radiopacket rp;
	bool running = true;
	while (running) {

		if (radio.available()) {
			radio.read(&rp, 32);
			cout << "IN: " << radioppppprintchararray((uint8_t*) &rp, 32);
		}

	}

}


int main(int argc, char **argv) {

	if (argc < 2) {
		printf(
				"You need to specify primary or secondary role (./thisprogram 1 or 2)");
		return 0;
	}

	for (int i = 0; i < 31; i++) {
		testpacket.data[i] = (uint8_t) (0x0);
	}
	testpacket.data[0] = (uint8_t) (0x55);
	testpacket.data[1] = (uint8_t) (0x55);

	printf("Radio is: %d\n", (int) argv[1][0]);

	bool primary = argv[1][0] == '1';
	cout << "Radio is " << (primary ? "Primary" : "Secondary") << endl;

	//justtest(primary);
	//return 0;

	tuntaphandler ttp(primary);

	if (primary) {

			const char * dd = "abcdefghilmnopqrstuvwz1234567890987654321012345678901234567890";
			printf("Adding testpacket, crc %d\n", gencrc((uint8_t*)dd,62));
			//radioppppprintchararray((uint8_t*)dd, 46);
			ttp.rpth.handleData((uint8_t*)dd, 62);
			//ttp.pth.packetstackqueue.push(&testpacket);
		}

	ttp.TunnelThread();

	ttp.stop_interface();
	return 0;
}
