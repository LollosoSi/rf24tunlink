/*
 * RF24Radio.cpp
 *
 *  Created on: 6 Feb 2024
 *      Author: Andrea Roccaccino
 */

#include "RF24Radio.h"

RF24Radio::RF24Radio(bool primary) :
		Telemetry("RF24Radio") {

	this->primary = primary;

	register_elements(new std::string[3] { "AVG ARC", "Packets Out",
			"Packets In" }, 3);

	returnvector = new std::string[3] { std::to_string(0), std::to_string(0),
			std::to_string(0) };

	setup();
}

RF24Radio::~RF24Radio() {
	stop();
}

std::string* RF24Radio::telemetry_collect(const unsigned long delta) {
	returnvector[0] = (std::to_string(sum_arc / count_arc));
	returnvector[1] = (std::to_string(count_arc));
	returnvector[2] = (std::to_string(packets_in));

	sum_arc = count_arc = packets_in = 0;
	return (returnvector);
}

void RF24Radio::check_fault() {
	if (radio->failureDetected || (radio->getDataRate() != Settings::RF24::data_rate)
			|| (!radio->isChipConnected())) {
		radio->failureDetected = 0;
		reset_radio();
		return;
	}
}

void RF24Radio::setup() {
	if (!radio) {
		radio = new RF24(Settings::RF24::ce_pin, Settings::RF24::csn_pin,
				Settings::RF24::spi_speed);
	}

	reset_radio();
}

void RF24Radio::loop(unsigned long delta) {

	check_fault();

	if (primary) {

		fill_buffer_tx();
		send_tx();

	} else {

		if (read()) {
			fill_buffer_ack();
		}

	}

}

void RF24Radio::stop() {

}

void RF24Radio::interrupt_routine() {
	bool tx_ok, tx_fail, rx_ready;
	radio->whatHappened(tx_ok, tx_fail, rx_ready);

	if (rx_ready) {
		if (read())
			fill_buffer_ack();

	}
}

bool RF24Radio::read() {

	bool result = false;

	while (radio->available()) {
		static RadioPacket *rp = new RadioPacket;
		rp->size = radio->getDynamicPayloadSize();
		radio->read(rp, rp->size);

		this->packet_received(rp);
		result = true;
		packets_in++;
	}

	return (result);
}

void RF24Radio::send_tx() {

	if (radio->txStandBy()) {
		// Transmit was successful, everything is okay
		sum_arc += radio->getARC();
		count_arc++;
	} else {
		// Some or all packets failed to transmit. Sorry for the inconvenience
		static bool retry_once = true;
		radio->reUseTX();
		if (retry_once) {
			retry_once = false;
			printf("Retrying to send once .....");
			send_tx();
		} else {
			printf("..... tried\n");
		}
		retry_once = true;
	}

}

bool RF24Radio::fill_buffer_tx() {

	// Check if radio FIFO TX is full, if yes, skip.
	// Also check if the next packet is available
	if (radio->isFifo(true, false) && has_next_packet()) {
		static RadioPacket *rp = next_packet();
		if (radio->writeFast(rp->data, rp->size, false)) {
			// Transfer to radio successful
			return (true);
		} else {
			// Transfer to radio failed
			return (false);
		}
	} else {
		return (false);
	}

}

bool RF24Radio::fill_buffer_ack() {

	// Check if radio FIFO TX is full, if yes, skip.
	// Also check if the next packet is available
	if (radio->isFifo(true, false) && has_next_packet()) {
		static RadioPacket *rp = next_packet();
		if (radio->writeAckPayload(1, rp->data, rp->size)) {
			// Transfer to radio successful
			return (true);
		} else {
			// Transfer to radio failed
			return (false);
		}
	} else {
		return (false);
	}

}

/**
 *  Sweeps all channels to find less disturbance
 */
void RF24Radio::channel_sweep() {

	std::cout
			<< "Channel sweep 0-125\nCCCC -> distrbance, RRRR -> Radio activity (P-Variant only)";

	radio->startListening();
	for (uint8_t i = 0; i < 126; i++) {
		radio->setChannel(i);
		std::cout << ((unsigned int) i) << "\t|\t";
		if (radio->testCarrier())
			std::cout << "CCCC";
		std::cout << "\t";
		if (radio->isPVariant())
			if (radio->testRPD())
				std::cout << "RRRR";
		std::cout << "\n";
	}
	radio->stopListening();

	reset_radio();

}

void RF24Radio::set_speed_pa_retries() {
	radio->setPALevel(Settings::RF24::radio_power);
	radio->setDataRate(Settings::RF24::data_rate);
	radio->setRetries(Settings::RF24::radio_delay,
			Settings::RF24::radio_retries);
}

void RF24Radio::reset_radio() {

	uint8_t t = 1;
	while (!radio->begin()) {
		std::cout << "NRF24 is not responsive" << std::endl;
		usleep(10000);
		// delay(2);
		if (!t++) {
			// handleRadioUnresponsiveTimeout();
		}
	}

	radio->flush_rx();
	radio->flush_tx();

	radio->setAddressWidth(Settings::RF24::address_bytes);

	set_speed_pa_retries();

	// save on transmission time by setting the radio to only transmit the
	// number of bytes we need to transmit
	// radio->setPayloadSize(Settings::payload_size);
	radio->setAutoAck(true);

	// to use ACK payloads, we need to enable dynamic payload lengths (for all nodes)
	radio->enableDynamicPayloads(); // ACK payloads are dynamically sized

	// Acknowledgement packets have no payloads by default. We need to enable
	// this feature for all nodes (TX & RX) to use ACK payloads.
	radio->enableAckPayload();

	radio->setChannel(Settings::RF24::channel);

	radio->setCRCLength(Settings::RF24::crc_length);

	radio->openReadingPipe(1,
			primary ? Settings::RF24::address_2 : Settings::RF24::address_1);
	radio->openWritingPipe(
			primary ? Settings::RF24::address_1 : Settings::RF24::address_2);

	if (primary)
		radio->stopListening();
	else
		radio->startListening();

	radio->printPrettyDetails();
	printf("Radio details! P-Variant: %s\tValid: %s\tCarrier: %s\tRPD: %s\n",
			radio->isPVariant() ? "YES" : "NO",
			radio->isValid() ? "YES" : "NO, WHAT???",
			radio->testCarrier() ? "BUSY" : "FREE",
			radio->testRPD() ? "BUSY" : "FREE");

}
