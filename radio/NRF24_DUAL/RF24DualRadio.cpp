/*
 * RF24DualRadio.cpp
 *
 *  Created on: 19 Feb 2024
 *      Author: Andrea Roccaccino
 */

#include "RF24DualRadio.h"

RF24DualRadio::RF24DualRadio(bool radio_role) :
		Telemetry("Dual-RF24") {
	role = radio_role;

	register_elements(new std::string[6] { "AVG ARC", "Packets Out",
			"Packets In", "Radio Bytes Out", "Radio Bytes In", "Data Rate" },
			6);

	returnvector = new std::string[6] { std::to_string(0), std::to_string(0),
			std::to_string(0), std::to_string(0), std::to_string(0),
			std::to_string(0) };

	setup();

}

RF24DualRadio::~RF24DualRadio() {
	stop();
}

std::string* RF24DualRadio::telemetry_collect(const unsigned long delta) {
	if (count_arc != 0)
		returnvector[0] = (std::to_string(sum_arc / count_arc));
	else
		returnvector[0] = (std::to_string(0));
	returnvector[1] = (std::to_string(count_arc));
	returnvector[2] = (std::to_string(packets_in));
	returnvector[3] = (std::to_string(radio_bytes_out));
	returnvector[4] = (std::to_string(radio_bytes_in));
	// implemented in check_fault() - returnvector[5] = (std::to_string(radio->getDataRate()));

	sum_arc = count_arc = packets_in = radio_bytes_out = radio_bytes_in = 0;
	return (returnvector);
}

void RF24DualRadio::setup() {
	if (!radio_0) {
		radio_0 = new RF24(Settings::DUAL_RF24::ce_0_pin,
				Settings::DUAL_RF24::csn_0_pin, Settings::DUAL_RF24::spi_speed);
	}
	if (!radio_1) {
		radio_1 = new RF24(Settings::DUAL_RF24::ce_1_pin,
				Settings::DUAL_RF24::csn_1_pin, Settings::DUAL_RF24::spi_speed);
	}

	reset_radio();
	printf("Reset after setup\n");
}

void RF24DualRadio::set_speed_pa_retries() {
	Settings::DUAL_RF24::radio_delay = Settings::DUAL_RF24::select_delay();
	Settings::DUAL_RF24::radio_retries = Settings::DUAL_RF24::select_retries();

	radio_0->setPALevel(Settings::DUAL_RF24::radio_power);
	radio_0->setDataRate(Settings::DUAL_RF24::data_rate);
	radio_0->setRetries(Settings::DUAL_RF24::radio_delay,
			Settings::DUAL_RF24::radio_retries);
	radio_1->setPALevel(Settings::DUAL_RF24::radio_power);
	radio_1->setDataRate(Settings::DUAL_RF24::data_rate);
	radio_1->setRetries(Settings::DUAL_RF24::radio_delay,
			Settings::DUAL_RF24::radio_retries);
}

void RF24DualRadio::loop(unsigned long delta) {

	while (fill_buffer_tx()) {

	}
	if (send_tx()) {

	}
	while (read()) {

	}

}

void RF24DualRadio::stop() {
}

bool RF24DualRadio::read() {

	bool result = false;
	uint8_t pipe = 0;

	static RadioPacket *rp = new RadioPacket;

	if ((result = (radio_0->available(&pipe)))) {

		rp->size = radio_0->getDynamicPayloadSize();
		radio_0->read(rp, rp->size);

		switch (pipe) {
		default:
			// Radio isn't working properly, try reset
			result = false;
			reset_radio();
			printf("Bad pipe received. Radio reset\n");

			break;

		case 0:
		case 1:
			radio_bytes_in += rp->size;

			receiving_data = !(rp->size == 1 && rp->data[0] == 0);

			if (!receiving_data) {
				break;
			}
			this->packet_received(rp);
			break;

		case 2:
			// Secondary radio received a control packet!
			printf("-----> Move request to %i\n", rp->data[0]);
			//radio->flush_rx();
			//radio->flush_tx();

			radio_bytes_in += rp->size;
			//process_control_packet(rp);
			break;
		}
		if (pipe != 0 && pipe != 1)
			printf("Pipe %i\n", pipe);
		packets_in++;
		//last_packet = current_millis();

	}

	return (result);
}

bool RF24DualRadio::send_tx() {

	if (radio_1->txStandBy()) {
		// Transmit was successful, everything is okay
		sum_arc += radio_1->getARC();
		count_arc++;
		return (true);
	} else {
		//radio->reUseTX();
		//radio->flush_tx();
		return (false);
	}

}

bool RF24DualRadio::fill_buffer_tx() {

// Check if radio FIFO TX is full, if yes, skip.
// Also check if the next packet is available
	static uint8_t pk = 0;
	if (radio_1->isFifo(true, false)) {
		pk = 0;
		return (false);
	}

	RadioPacket *rp = nullptr;
	if (!(rp = next_packet()))
		return (false);

//if (rp->size > 1) {
//	printf("Loading packet:\n");
//	print_hex(rp->data + 1, rp->size - 1);
//}
	//usleep(10000);

	if (radio_1->writeFast(rp->data, rp->size, false)) {
		radio_bytes_out += rp->size;
		return (true);
	} else {
		return (false);
	}

}

void RF24DualRadio::check_fault() {
	rf24_datarate_e dt_0 = radio_0->getDataRate(), dt_1 =
			radio_1->getDataRate();
	if (radio_0->failureDetected || radio_1->failureDetected
			|| (dt_0 != Settings::DUAL_RF24::data_rate)
			|| (dt_1 != Settings::DUAL_RF24::data_rate)
			|| (!radio_0->isChipConnected()) || (!radio_1->isChipConnected())) {
		radio_0->failureDetected = 0;
		radio_1->failureDetected = 0;
		reset_radio();
		printf("Reset after radio failure detected\n");
		return;
	}
}

void RF24DualRadio::reset_radio() {

	uint8_t t = 1;
	while (!radio_0->begin()) {
		std::cout << "Radio 0 is not responsive" << std::endl;
		usleep(10000);
// delay(2);
		if (!t++) {
			// handleRadioUnresponsiveTimeout();
		}
	}

	t = 1;
	while (!radio_1->begin()) {
		std::cout << "Radio 1 is not responsive" << std::endl;
		usleep(10000);
		// delay(2);
		if (!t++) {
			// handleRadioUnresponsiveTimeout();
		}
	}

	radio_0->flush_rx();
	radio_0->flush_tx();

	radio_1->flush_rx();
	radio_1->flush_tx();

	radio_0->setAddressWidth(Settings::DUAL_RF24::address_bytes);
	radio_1->setAddressWidth(Settings::DUAL_RF24::address_bytes);

	set_speed_pa_retries();

// save on transmission time by setting the radio to only transmit the
// number of bytes we need to transmit
// radio->setPayloadSize(Settings::payload_size);
	radio_0->setAutoAck(false);
	radio_1->setAutoAck(false);

// to use ACK payloads, we need to enable dynamic payload lengths (for all nodes)
	radio_0->enableDynamicPayloads(); // ACK payloads are dynamically sized
	radio_1->enableDynamicPayloads();

// Acknowledgement packets have no payloads by default. We need to enable
// this feature for all nodes (TX & RX) to use ACK payloads.
	//radio->enableAckPayload();

	radio_0->setChannel(Settings::DUAL_RF24::channel_0);
	radio_1->setChannel(Settings::DUAL_RF24::channel_1);

	radio_0->setCRCLength(Settings::DUAL_RF24::crc_length);
	radio_1->setCRCLength(Settings::DUAL_RF24::crc_length);

	if (role) {
		radio_0->openReadingPipe(1, Settings::DUAL_RF24::address_0_1[0]);
		radio_1->openReadingPipe(1, Settings::DUAL_RF24::address_1_1[0]);

		radio_1->openWritingPipe(Settings::DUAL_RF24::address_0_2[0]);

	} else {
		radio_0->openReadingPipe(1, Settings::DUAL_RF24::address_0_2[0]);
		radio_1->openReadingPipe(1, Settings::DUAL_RF24::address_1_2[0]);

		radio_1->openWritingPipe(Settings::DUAL_RF24::address_0_1[0]);
	}

	radio_0->printPrettyDetails();
	radio_1->printPrettyDetails();

	/*printf("Radio details! P-Variant: %s\tValid: %s\tCarrier: %s\tRPD: %s\n",
	 radio->isPVariant() ? "YES" : "NO",
	 radio->isValid() ? "YES" : "NO, WHAT???",
	 radio->testCarrier() ? "BUSY" : "FREE",
	 radio->testRPD() ? "BUSY" : "FREE");*/

	radio_1->stopListening();
	radio_0->startListening();
}
