/*
 * RF24Radio.cpp
 *
 *  Created on: 6 Feb 2024
 *      Author: Andrea Roccaccino
 */

#include "RF24Radio.h"

#include "../../utils.h"

RF24Radio::RF24Radio(bool primary, uint8_t ce_pin, uint8_t csn_pin,
		uint8_t channel) :
		Telemetry("RF24Radio") {

	this->primary = primary;
	this->ce_pin = ce_pin;
	this->csn_pin = csn_pin;
	this->channel = channel;

	register_elements(new std::string[7] { "AVG ARC", "Packets Out",
			"Packets In", "Radio Bytes Out", "Radio Bytes In", "Data Rate",
			"Kbps" }, 7);

	returnvector = new std::string[7] { std::to_string(0), std::to_string(0),
			std::to_string(0), std::to_string(0), std::to_string(0),
			std::to_string(0), std::to_string(0) };

	setup();
}

RF24Radio::~RF24Radio() {
	stop();
}

std::string* RF24Radio::telemetry_collect(const unsigned long delta) {
	if (count_arc != 0)
		returnvector[0] = (std::to_string(sum_arc / count_arc));
	else
		returnvector[0] = (std::to_string(0));
	returnvector[1] = (std::to_string(count_arc));
	returnvector[2] = (std::to_string(packets_in));
	returnvector[3] = (std::to_string(radio_bytes_out));
	returnvector[4] = (std::to_string(radio_bytes_in));
	returnvector[6] = (std::to_string(
			(radio_bytes_out + radio_bytes_in) * 8 / 1000.0));
	// implemented in check_fault() - returnvector[5] = (std::to_string(radio->getDataRate()));

	sum_arc = count_arc = packets_in = radio_bytes_out = radio_bytes_in = 0;
	return (returnvector);
}

void RF24Radio::check_fault() {
	rf24_datarate_e dt = radio->getDataRate();
	returnvector[5] = std::to_string(dt);
	if (radio->failureDetected || (dt != Settings::RF24::data_rate)
			|| (!radio->isChipConnected())) {
		radio->failureDetected = 0;
		reset_radio();
		printf("Reset after radio failure detected\n");
		return;
	}
}

void RF24Radio::setup() {
	if (!radio) {
		printf("Opening radio in CE:%i CSN:%i\n", ce_pin, csn_pin);
		radio = new RF24(ce_pin, csn_pin, Settings::RF24::spi_speed);
	}

	reset_radio();
	printf("Reset after setup\n");

	/*if (!primary)
	 channel_sweep();
	 else {
	 radio->startConstCarrier(RF24_PA_MAX, Settings::RF24::channel);
	 usleep(5000000);
	 radio->stopConstCarrier();
	 radio->powerUp();
	 reset_radio();
	 }*/
}

inline uint16_t RF24Radio::since_last_packet() {
	return (current_millis() - last_packet);
}

void RF24Radio::loop(unsigned long delta) {

	check_fault();

	if (Settings::RF24::variable_rate)
		if (since_last_packet() > Settings::RF24::max_radio_silence) {
			if (current_millis() % 100 == 0)
				printf("Radio silent for %i ms\n", since_last_packet());

			if (Settings::RF24::variable_rate)
				if (radio->getDataRate() != 2) {
					static RadioPacket *cp = new RadioPacket { { 2 }, 1 };
					process_control_packet(cp);
				}
		}

	if (primary) {

		while (fill_buffer_tx()) {

		}
		if (send_tx()) {

		}
		while (read()) {

		}

		if (Settings::RF24::variable_rate)
			if ((since_last_packet() < 30)
					&& (current_millis() - last_speed_change > 5000)
					&& radio->getDataRate() != 1) {
				static RadioPacket *cp = new RadioPacket { { 1 }, 1 };
				try_change_speed(cp);
			}

	} else {

		//uint8_t cnt = 0;
		while (read()) {
			//if (cnt++ >= 6)
			//	radio->flush_rx();

		}
		while (fill_buffer_ack()) {

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

inline void RF24Radio::process_control_packet(RadioPacket *cp) {

	if (Settings::RF24::variable_rate && cp->data[0] != radio->getDataRate()
			&& cp->data[0] > 3 && cp->data[0] <= 1) {
		printf("-----> Moving to datarate: %i\n", cp->data[0]);
		Settings::RF24::data_rate = (rf24_datarate_e) cp->data[0];

		//this->set_speed_pa_retries();
		reset_radio();
		printf("Reset after changing datarate settings\n");
	} else if (!Settings::RF24::variable_rate) {
		reset_radio();
		printf("Variable rate is not enabled. Radio reset\n");
	} else if (cp->data[0] <= 3 && cp->data[0] >= 0) {
		//reset_radio();
		//printf("Bad data received\n");
	}

}

inline void RF24Radio::try_change_speed(RadioPacket *cp) {
	//radio->flush_rx();
	//radio->flush_tx();
	radio->openWritingPipe(Settings::RF24::address_3[0]);
	//radio->stopListening();
	//usleep(100000);
	if (radio->write(cp->data, 1)) {
		if (read()) {
		}
		printf("-----> Moving to %i\n", cp->data[0]);

		last_speed_change = current_millis();
		process_control_packet(cp);
	} else {
		std::cout << "Failed to send change datarate message\n";
	}
	radio->openWritingPipe(Settings::RF24::address_1[0]);

}

bool RF24Radio::read() {

	bool result = false;
	uint8_t pipe = 0;

	static RadioPacket *rp = new RadioPacket;

	if ((result = (radio->available(&pipe)))) {

		rp->size = radio->getDynamicPayloadSize();
		radio->read(rp, rp->size);

		switch (pipe) {
		default:
			// Radio isn't working properly, try reset
			result = false;
			//reset_radio();
			printf("Bad pipe received\n");

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
			if (Settings::RF24::variable_rate)
				process_control_packet(rp);
			break;
		}
		if (pipe != 0 && pipe != 1)
			printf("Pipe %i\n", pipe);
		packets_in++;
		last_packet = current_millis();

	}

	return (result);
}

bool RF24Radio::send_tx() {

	if (radio->txStandBy()) {
		// Transmit was successful, everything is okay
		sum_arc += radio->getARC();
		count_arc++;
		return (true);
	} else {
		//radio->reUseTX();
		//radio->flush_tx();
		return (false);
	}

}

bool RF24Radio::fill_buffer_tx() {

// Check if radio FIFO TX is full, if yes, skip.
// Also check if the next packet is available
	static uint8_t pk = 0;
	if (radio->isFifo(true, false)) {
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

	if (radio->writeFast(rp->data, rp->size, !Settings::RF24::auto_ack)) {
		radio_bytes_out += rp->size;
		return (true);
	} else {
		return (false);
	}

	if (pk < 3) {
// Transfer to radio successful
//printf("Loaded ack (s %i)\n", rp->size);
		//radio_bytes_out += rp->size;
		return (true);
	} else {
		pk = 0;

// Transfer to radio failed
//printf("Ack load failed\n");
		return (false);
	}

}

bool RF24Radio::fill_buffer_ack() {

// Check if radio FIFO TX is full, if yes, skip.
// Also check if the next packet is available
//if (!radio->isFifo(true, false)) {

	if (radio->isFifo(true, false))
		return (false);

	RadioPacket *rp = nullptr;
	if (!(rp = next_packet()))
		return (false);

//if (rp->size > 1) {
//	printf("Loading packet:\n");
//	print_hex(rp->data + 1, rp->size - 1);
//}

	if (radio->writeAckPayload(1, rp->data, rp->size)) {
// Transfer to radio successful
//printf("Loaded ack (s %i)\n", rp->size);
		radio_bytes_out += rp->size;
		count_arc++;
		return (true);
	} else {
// Transfer to radio failed
//printf("Ack load failed\n");
		return (false);
	}
//} else if (radio->isFifo(true, false)) {
//printf("I want to load ack ... "); printf("But the radio is full\n");
//	return (false);
//} else {
//printf("But I don't have a packet to send\n");
//	return (false);
//}

}

/**
 *  Sweeps all channels to find less disturbance
 */
void RF24Radio::channel_sweep() {

	std::cout
			<< "Channel sweep 0-125\nCCCC -> distrbance, RRRR -> Radio activity (P-Variant only)\n";

	//bool found = false;

	radio->startListening();
//while(!found)
	for (uint8_t i = 0; i < 126; i++) {
		radio->setChannel(i);
		bool cr = radio->testCarrier(), rpd = radio->testRPD();
		if (cr || rpd) {
			printf("Channel %i\t|\t%s\t%s\n", i, (cr ? "CCCC" : ""),
					(rpd ? "RRRR" : ""));
			//found = true;
		}

	}
	radio->stopListening();

	reset_radio();
	printf("Reset after channel sweep\n");

}

void RF24Radio::set_speed_pa_retries() {
	Settings::RF24::radio_delay = Settings::RF24::select_delay();
	Settings::RF24::radio_retries = Settings::RF24::select_retries();

	radio->setPALevel(Settings::RF24::radio_power);
	radio->setDataRate(Settings::RF24::data_rate);
	radio->setRetries(Settings::RF24::radio_delay,
			Settings::RF24::radio_retries);
}

void RF24Radio::reset_radio() {

	uint8_t t = 1;
	while (!radio->begin(this->ce_pin,this->csn_pin)) {
		printf("NRF24 is not responsive (CE: %i, CSN: %i)\n", ce_pin, csn_pin);
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
	radio->setAutoAck(Settings::RF24::auto_ack);

// to use ACK payloads, we need to enable dynamic payload lengths (for all nodes)
	radio->enableDynamicPayloads(); // ACK payloads are dynamically sized

// Acknowledgement packets have no payloads by default. We need to enable
// this feature for all nodes (TX & RX) to use ACK payloads.
	radio->enableAckPayload();

	radio->setChannel(channel);

	radio->setCRCLength(Settings::RF24::crc_length);

	radio->openReadingPipe(1,
			primary ?
					Settings::RF24::address_2[0] :
					Settings::RF24::address_1[0]);

	if (!primary) {
		radio->openReadingPipe(2, Settings::RF24::address_3[0]);
	}

	radio->openWritingPipe(
			primary ?
					Settings::RF24::address_1[0] :
					Settings::RF24::address_2[0]);

	radio->printPrettyDetails();
	/*printf("Radio details! P-Variant: %s\tValid: %s\tCarrier: %s\tRPD: %s\n",
	 radio->isPVariant() ? "YES" : "NO",
	 radio->isValid() ? "YES" : "NO, WHAT???",
	 radio->testCarrier() ? "BUSY" : "FREE",
	 radio->testRPD() ? "BUSY" : "FREE");*/

	if (primary)
		radio->stopListening();
	else
		radio->startListening();

}
