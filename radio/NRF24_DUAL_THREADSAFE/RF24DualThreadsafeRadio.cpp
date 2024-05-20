/*
 * RF24DualThreadsafeRadio.cpp
 *
 *  Created on: 13 May 2024
 *      Author: Andrea Roccaccino
 */

#include "RF24DualThreadsafeRadio.h"

RF24DualThreadsafeRadio::RF24DualThreadsafeRadio(bool primary) :
		Telemetry("RF24 DUAL TS") {
	role = primary;

	radio0 = nullptr;
	radio1 = nullptr;

	t0=nullptr;
	t1=nullptr;

	returnvector = new std::string[6] { "", "", "", "", "", "" };
	register_elements(new std::string[6] { "Packets OUT", "Packets IN", "Kbps OUT", "Kbps IN", "Kbps OUT RS" , "Kbps IN RS" }, 6);

	setup();
}

RF24DualThreadsafeRadio::~RF24DualThreadsafeRadio() {
	// TODO Auto-generated destructor stub
}

void RF24DualThreadsafeRadio::setup() {
	if (!radio0)
		radio0 = new RF24(Settings::DUAL_RF24::ce_0_pin,
				Settings::DUAL_RF24::csn_0_pin, Settings::RF24::spi_speed);

	if (!radio1)
		radio1 = new RF24(Settings::DUAL_RF24::ce_1_pin,
				Settings::DUAL_RF24::csn_1_pin, Settings::RF24::spi_speed);

	resetRadio0();
	resetRadio1();
	running = true;
	t0 = new std::thread(
			[&] {
				while (running) {
					if (radio0->failureDetected
							|| radio0->getDataRate()
									!= Settings::RF24::data_rate
							|| !radio0->isChipConnected()) {
						radio0->failureDetected = 0;
						resetRadio0();
						printf("Radio 0 Reset after failure\n");
					}

					read();

					std::this_thread::yield();
				}
				delete radio0;
				radio0=nullptr;
			});
	t0->detach();

	t1 = new std::thread(
			[&] {
				while (running) {
					if (radio1->failureDetected
							|| radio1->getDataRate()
									!= Settings::RF24::data_rate
							|| !radio1->isChipConnected()) {
						radio1->failureDetected = 0;
						resetRadio1();
						printf("Radio 1 Reset after failure\n");
					}

					write();

					std::this_thread::yield();
				}

				delete radio1;
				radio1=nullptr;
			});
	t1->detach();

}
void RF24DualThreadsafeRadio::loop(unsigned long delta) {

}

void RF24DualThreadsafeRadio::resetRadio0() {

	uint8_t t = 1;
	while (!radio0->begin(Settings::DUAL_RF24::ce_0_pin,
			Settings::DUAL_RF24::csn_0_pin)) {
		printf("Radio 0 is not responsive (CE: %i, CSN: %i)\n",
				Settings::DUAL_RF24::ce_0_pin, Settings::DUAL_RF24::csn_0_pin);
		usleep(10000);
		// delay(2);
		if (!t++) {
			// handleRadioUnresponsiveTimeout();
		}
	}

	radio0->flush_rx();
	radio0->flush_tx();

	radio0->setAddressWidth(Settings::DUAL_RF24::address_bytes);

	radio0->setPayloadSize(Settings::RF24::payload_size);

	Settings::DUAL_RF24::radio_delay = Settings::DUAL_RF24::select_delay();
	Settings::DUAL_RF24::radio_retries = Settings::DUAL_RF24::select_retries();

	radio0->setPALevel(Settings::DUAL_RF24::radio_power);
	radio0->setDataRate(Settings::DUAL_RF24::data_rate);
	radio0->setRetries(Settings::DUAL_RF24::radio_delay,
			Settings::DUAL_RF24::radio_retries);

	radio0->setAutoAck(Settings::DUAL_RF24::auto_ack);

	if (Settings::RF24::dynamic_payloads)
		radio0->enableDynamicPayloads();
	else
		radio0->disableDynamicPayloads();

	if (Settings::RF24::ack_payloads)
		radio0->enableAckPayload();
	else
		radio0->disableAckPayload();

	radio0->setCRCLength(Settings::DUAL_RF24::crc_length);

	if (role) {
		radio0->openReadingPipe(1, Settings::DUAL_RF24::address_0_1);
		radio0->setChannel(Settings::DUAL_RF24::channel_0);
		printf("RADIO0 -> ROLE: %d \t read: %s \t write %s \t channel: %d\n",
				role, Settings::DUAL_RF24::address_0_1, "none",
				Settings::DUAL_RF24::channel_0);

	} else {
		radio0->openReadingPipe(1, Settings::DUAL_RF24::address_0_2);
		radio0->setChannel(Settings::DUAL_RF24::channel_1);
		printf("RADIO0 -> ROLE: %d \t read: %s \t write %s \t channel: %d\n",
				role, Settings::DUAL_RF24::address_0_2, "none",
				Settings::DUAL_RF24::channel_1);

	}

	radio0->startListening();

	radio0->printPrettyDetails();

}
void RF24DualThreadsafeRadio::resetRadio1() {

	uint8_t t = 1;
	while (!radio1->begin(Settings::DUAL_RF24::ce_1_pin,
			Settings::DUAL_RF24::csn_1_pin)) {
		printf("Radio 1 is not responsive (CE: %i, CSN: %i)\n",
				Settings::DUAL_RF24::ce_1_pin, Settings::DUAL_RF24::csn_1_pin);
		usleep(10000);
		// delay(2);
		if (!t++) {
			// handleRadioUnresponsiveTimeout();
		}
	}

	radio1->flush_rx();
	radio1->flush_tx();

	radio1->setAddressWidth(Settings::DUAL_RF24::address_bytes);

	radio1->setPayloadSize(Settings::RF24::payload_size);

	Settings::DUAL_RF24::radio_delay = Settings::DUAL_RF24::select_delay();
	Settings::DUAL_RF24::radio_retries = Settings::DUAL_RF24::select_retries();

	radio1->setPALevel(Settings::DUAL_RF24::radio_power);
	radio1->setDataRate(Settings::DUAL_RF24::data_rate);
	radio1->setRetries(Settings::DUAL_RF24::radio_delay,
			Settings::DUAL_RF24::radio_retries);

	radio1->setAutoAck(Settings::DUAL_RF24::auto_ack);

	if (Settings::RF24::dynamic_payloads)
		radio1->enableDynamicPayloads();
	else
		radio1->disableDynamicPayloads();

	if (Settings::RF24::ack_payloads)
		radio1->enableAckPayload();
	else
		radio1->disableAckPayload();

	radio1->setCRCLength(Settings::DUAL_RF24::crc_length);

	if (role) {
		radio1->openReadingPipe(1, Settings::DUAL_RF24::address_1_1);
		radio1->openWritingPipe(Settings::DUAL_RF24::address_0_2);
		radio1->setChannel(Settings::DUAL_RF24::channel_1);
		printf("RADIO1 -> ROLE: %d \t read: %s \t write %s \t channel: %d\n",
				role, Settings::DUAL_RF24::address_1_1,
				Settings::DUAL_RF24::address_0_2,
				Settings::DUAL_RF24::channel_1);
	} else {
		radio1->openReadingPipe(1, Settings::DUAL_RF24::address_1_2);
		radio1->openWritingPipe(Settings::DUAL_RF24::address_0_1);
		radio1->setChannel(Settings::DUAL_RF24::channel_0);
		printf("RADIO1 -> ROLE: %d \t read: %s \t write %s \t channel: %d\n",
				role, Settings::DUAL_RF24::address_1_2,
				Settings::DUAL_RF24::address_0_1,
				Settings::DUAL_RF24::channel_0);

	}

	radio1->stopListening();

	radio1->printPrettyDetails();

}
void RF24DualThreadsafeRadio::write() {
	RadioPacket *rp = nullptr;
	while (has_next_packet() && !radio1->isFifo(true, false)) {
		rp = next_packet();
		if (rp) {
			packets_out++;
			radio1->writeFast(rp->data, 32, !Settings::DUAL_RF24::auto_ack);
		} else
			break;
	}

	if (!radio1->isFifo(true, true))
		radio1->txStandBy();
	//if(radio1->txStandBy()){
//}else{
	//	radio1->flush_tx();
	//}
}
void RF24DualThreadsafeRadio::read() {
	uint8_t pipe;
	RadioPacket rp;
	while (radio0->available(&pipe)) {
		radio0->read(&rp, 32);
		rp.size = 32;
		switch (pipe) {
		default:
			break;
		case 0:
		case 1:
			packets_in++;
			this->packet_received(&rp);
			break;
		}
	}

}

void RF24DualThreadsafeRadio::stop() {

	running = false;
}
