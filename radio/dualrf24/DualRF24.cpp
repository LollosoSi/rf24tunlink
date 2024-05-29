/*
 * DualRF24.cpp
 *
 *  Created on: 27 May 2024
 *      Author: Andrea Roccaccino
 */

#include "DualRF24.h"

static DualRF24* my_instance = nullptr;
static void ISR_static(){
	my_instance->receive_ISR();
}

DualRF24::DualRF24() {
	
}

DualRF24::~DualRF24() {
	delete radio0;
	delete radio1;
}

void DualRF24::apply_settings(const Settings &settings){
	RadioInterface::apply_settings(settings);
	role = settings.primary;

	my_instance = this;

	if(settings.irq_pin_radio0 == 0){
		std::cout << "IRQ PIN for radio 0 can't be unset\n";
		throw std::invalid_argument("IRQ PIN RADIO0 UNSET");
	}

	if (!radio0)
		radio0 = new RF24(settings.ce_0_pin,
				settings.csn_0_pin, settings.spi_speed);

	if (!radio1)
		radio1 = new RF24(settings.ce_1_pin,
				settings.csn_1_pin, settings.spi_speed);

}

inline void DualRF24::check_radio0_status(){
	if (radio0->failureDetected || radio0->getDataRate() != current_settings()->data_rate
		|| !radio0->isChipConnected()) {
			radio0->failureDetected = 0;
			resetRadio0();
			printf("Radio 0 Reset after failure\n");
		}
}
inline void DualRF24::check_radio1_status(){
	if (radio1->failureDetected || radio1->getDataRate() != current_settings()->data_rate
		|| !radio1->isChipConnected()) {
			radio1->failureDetected = 0;
			resetRadio0();
			printf("Radio 1 Reset after failure\n");
		}
}

void DualRF24::receive_ISR(){

	check_radio0_status();

	bool tx_ds, tx_df, rx_dr;                // declare variables for IRQ masks
	radio0->whatHappened(tx_ds, tx_df, rx_dr);

	if (rx_dr) {
		uint8_t pipe;
		while (radio0->available(&pipe)) {
			RFMessage rfm = pmf->make_new_packet();
			radio0->read(rfm.data.get(), radio0->getPayloadSize());
			switch(pipe){
			default:
				break;
			case 0:
			case 1:
				packetizer->input(rfm);
				break;
			}
		}
	}

}


void DualRF24::input_finished(){
	if(!radio1->isFifo(true,true))
		radio1->txStandBy();
}
bool DualRF24::input(RFMessage &m){
	check_radio1_status();
	radio1->writeFast(m.data.get(), current_settings()->payload_size, !current_settings()->auto_ack);

	if (radio1->isFifo(true, false))
		radio1->txStandBy();

	return (true);
}


void DualRF24::resetRadio0() {

	if(radio0==nullptr)
		radio0=new RF24();

	uint8_t t = 1;
	while (!radio0->begin(current_settings()->ce_0_pin,
			current_settings()->csn_0_pin)) {
		printf("Radio 0 is not responsive (CE: %i, CSN: %i)\n",
				current_settings()->ce_0_pin, current_settings()->csn_0_pin);
		usleep(10000);
		// delay(2);
		if (!t++) {
			// handleRadioUnresponsiveTimeout();
		}
	}

	radio0->flush_rx();
	radio0->flush_tx();

	radio0->setAddressWidth(current_settings()->address_bytes);

	radio0->setPayloadSize(current_settings()->payload_size);

	radio0->setPALevel(current_settings()->radio_power);
	radio0->setDataRate((rf24_datarate_e)current_settings()->data_rate);
	radio0->setRetries(current_settings()->radio_delay,
			current_settings()->radio_retries);

	radio0->setAutoAck(current_settings()->auto_ack);

	if (current_settings()->dynamic_payloads)
		radio0->enableDynamicPayloads();
	else
		radio0->disableDynamicPayloads();

	if (current_settings()->ack_payloads)
		radio0->enableAckPayload();
	else
		radio0->disableAckPayload();

	radio0->setCRCLength((rf24_crclength_e)current_settings()->crc_length);

	if (role) {
		radio0->openReadingPipe(1, atol(current_settings()->address_0_1.c_str()));
		radio0->setChannel(current_settings()->channel_0);
		printf("RADIO0 -> ROLE: %d \t read: %s \t write %s \t channel: %d\n",
				role, current_settings()->address_0_1.c_str(), "none",
				current_settings()->channel_0);

	} else {
		radio0->openReadingPipe(1, atol(current_settings()->address_0_2.c_str()));
		radio0->setChannel(current_settings()->channel_1);
		printf("RADIO0 -> ROLE: %d \t read: %s \t write %s \t channel: %d\n",
				role, current_settings()->address_0_2.c_str(), "none",
				current_settings()->channel_1);

	}

	// Configure radio0 to interrupt for read events
	radio0->maskIRQ(true,true,false);

	static bool attached = false;
	if (!attached) {
		attached = true;
		pinMode(current_settings()->irq_pin_radio0, INPUT);
		attachInterrupt(current_settings()->irq_pin_radio0, INT_EDGE_FALLING, &ISR_static);

	}

	radio0->startListening();

	radio0->printPrettyDetails();

}
void DualRF24::resetRadio1() {

	uint8_t t = 1;
	while (!radio1->begin(current_settings()->ce_1_pin,
			current_settings()->csn_1_pin)) {
		printf("Radio 1 is not responsive (CE: %i, CSN: %i)\n",
				current_settings()->ce_1_pin, current_settings()->csn_1_pin);
		usleep(10000);
		// delay(2);
		if (!t++) {
			// handleRadioUnresponsiveTimeout();
		}
	}

	radio1->flush_rx();
	radio1->flush_tx();

	radio1->setAddressWidth(current_settings()->address_bytes);

	radio1->setPayloadSize(current_settings()->payload_size);

	radio1->setPALevel(current_settings()->radio_power);
	radio1->setDataRate((rf24_datarate_e)current_settings()->data_rate);
	radio1->setRetries(current_settings()->radio_delay,
			current_settings()->radio_retries);

	radio1->setAutoAck(current_settings()->auto_ack);

	if (current_settings()->dynamic_payloads)
		radio1->enableDynamicPayloads();
	else
		radio1->disableDynamicPayloads();

	if (current_settings()->ack_payloads)
		radio1->enableAckPayload();
	else
		radio1->disableAckPayload();

	radio1->setCRCLength((rf24_crclength_e)current_settings()->crc_length);

	if (role) {
		radio1->openReadingPipe(1, atol(current_settings()->address_1_1.c_str()));
		radio1->openWritingPipe(atol(current_settings()->address_0_2.c_str()));
		radio1->setChannel(current_settings()->channel_1);
		printf("RADIO1 -> ROLE: %d \t read: %s \t write %s \t channel: %d\n",
				role, current_settings()->address_1_1.c_str(),
				current_settings()->address_0_2.c_str(),
				current_settings()->channel_1);
	} else {
		radio1->openReadingPipe(1, atol(current_settings()->address_1_2.c_str()));
		radio1->openWritingPipe(atol(current_settings()->address_0_1.c_str()));
		radio1->setChannel(current_settings()->channel_0);
		printf("RADIO1 -> ROLE: %d \t read: %s \t write %s \t channel: %d\n",
				role, current_settings()->address_1_2.c_str(),
				current_settings()->address_0_1.c_str(),
				current_settings()->channel_0);

	}

	radio1->stopListening();

	radio1->printPrettyDetails();

}


void DualRF24::stop_module(){
	radio0->stopListening();
	radio1->stopListening();
	radio0->flush_rx();
	radio0->flush_tx();
	radio1->flush_rx();
	radio1->flush_tx();
}

