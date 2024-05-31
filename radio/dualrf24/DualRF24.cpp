/*
 * DualRF24.cpp
 *
 *  Created on: 27 May 2024
 *      Author: Andrea Roccaccino
 */

#include "DualRF24.h"

// For printing uint64_t in printf
#include <inttypes.h>

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

	running = true;

	resetRadio0();
	resetRadio1();


	check_radio0_status();
	check_radio1_status();
}

inline void DualRF24::check_radio0_status() {
	bool c1 = radio0->failureDetected,
		 c2 = radio0->getDataRate() != ((rf24_datarate_e)current_settings()->data_rate),
		 c3 = !radio0->isChipConnected();
	if (c1 || c2 || c3) {
		radio0->failureDetected = 0;
		resetRadio0(false);
		printf("Radio 0 Reset after failure. C1 %d, C2 %d, C3 %d\n", c1, c2, c3);
	}
}
inline void DualRF24::check_radio1_status() {
	bool c1 = radio1->failureDetected,
		 c2 = radio1->getDataRate()!= ((rf24_datarate_e)current_settings()->data_rate),
		 c3 = !radio1->isChipConnected();
	if (c1 || c2 || c3) {
		radio1->failureDetected = 0;
		resetRadio1(false);
		printf("Radio 1 Reset after failure. C1 %d, C2 %d, C3 %d\n", c1, c2, c3);
	}
}

void DualRF24::receive_ISR(){
	check_radio0_status();

	std::unique_lock lock(radio0_mtx);
	radio0_cv.wait(lock, [&] {
		return !running || radio0;
	});
	if(!running)return;

	bool tx_ds, tx_df, rx_dr;                // declare variables for IRQ masks
	radio0->whatHappened(tx_ds, tx_df, rx_dr);

	//printf("Radio data in (IRQ)\n");

	if (rx_dr) {
		std::deque<RFMessage> messages;
		uint8_t pipe;
		while (radio0->available(&pipe)) {
			RFMessage rfm = pmf->make_new_packet();
			radio0->read(rfm.data.get(), radio0->getPayloadSize());
			switch(pipe){
			default:
				break;
			case 0:
			case 1:
				messages.push_back(std::move(rfm));
				break;
			}
		}
		//printf("[Radio Read]Collected packets: %ld\n",messages.size());
		packetizer->input(messages);
	}

	lock.unlock();


}

void DualRF24::send_tx(){
	if(!radio1->txStandBy()){
		radio1->flush_tx();
		radio1->flush_rx();
	}
}
void DualRF24::input_finished(){
	//if(!radio1->isFifo(true,true)){
		//radio1->txStandBy();
		//printf("Radio data out (F)\n");
		//radio1->flush_tx();
	//}
}
bool DualRF24::input(RFMessage &m){
	check_radio1_status();

	std::unique_lock lock(radio1_mtx);
	radio1_cv.wait(lock, [&] {
		return !running || radio1;
	});
	if(!running)return (false);


	if(!radio1->writeFast(m.data.get(), current_settings()->payload_size, !current_settings()->auto_ack)){
		send_tx();
	}

	// Get all packets out of the way
	if (!radio1->isFifo(true, true))
		send_tx();


	return (true);
}

bool DualRF24::input(std::deque<RFMessage> &q){
	check_radio1_status();

	std::unique_lock lock(radio1_mtx);
	radio1_cv.wait(lock, [&] {
		return !running || radio1;
	});
	if(!running)return (false);

	for(auto& m : q){
		if(!radio1->writeFast(m.data.get(), current_settings()->payload_size, !current_settings()->auto_ack)){
			send_tx();
		}
	}

	// Get all packets out of the way
	if (!radio1->isFifo(true, true))
		send_tx();

	return (true);
}

inline uint64_t string_to_address(const std::string& str){
	if (str.size() != 3) {
		throw std::invalid_argument("String must be exactly 3 bytes long");
	}

	uint64_t result = 0;
	result |= static_cast<uint64_t>(static_cast<uint8_t>(str[0])) << 16;
	result |= static_cast<uint64_t>(static_cast<uint8_t>(str[1])) << 8;
	result |= static_cast<uint64_t>(static_cast<uint8_t>(str[2]));

	//std::cout << "Byte 0: 0x" << std::hex << static_cast<int>(static_cast<uint8_t>(str[0])) << std::endl;
	//std::cout << "Byte 1: 0x" << std::hex << static_cast<int>(static_cast<uint8_t>(str[1])) << std::endl;
	//std::cout << "Byte 2: 0x" << std::hex << static_cast<int>(static_cast<uint8_t>(str[2])) << std::endl;
	//std::cout << "Result: 0x" << std::hex << result << std::endl;

	return result;
}

void DualRF24::resetRadio0(bool print_info) {

	if(radio0==nullptr){
		printf("Reset radio 0 called without radio 0\n");
		throw std::invalid_argument("Invalid radio 0");
	}
	{
	std::unique_lock<std::mutex> lock(radio0_mtx);

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
		uint64_t ln = string_to_address(current_settings()->address_0_1);
		radio0->openReadingPipe(1, ln);
		radio0->setChannel(current_settings()->channel_0);
		printf("RADIO0 -> ROLE: %d \t read: %s : %" PRIu64 " \t write %s \t channel: %d\n",
				role, current_settings()->address_0_1.c_str(), ln, "none",
				current_settings()->channel_0);

	} else {
		uint64_t ln = string_to_address(current_settings()->address_0_2);
		radio0->openReadingPipe(1, ln);
		radio0->setChannel(current_settings()->channel_1);
		printf("RADIO0 -> ROLE: %d \t read: %s : %" PRIu64 " \t write %s \t channel: %d\n",
				role, current_settings()->address_0_2.c_str(), ln, "none",
				current_settings()->channel_1);

	}

	// Configure radio0 to interrupt for read events
	radio0->maskIRQ(true,true,false);

	if (!attached) {
		attached = true;
		//gpioInitialise();
		pinMode(current_settings()->irq_pin_radio0, INPUT);
		attachInterrupt(current_settings()->irq_pin_radio0, INT_EDGE_FALLING, &ISR_static);
		printf("Attached Interrupt\n");
	}

	radio0->startListening();
	if(print_info)
		radio0->printPrettyDetails();
}
	radio0_cv.notify_all();

}
void DualRF24::resetRadio1(bool print_info) {
	if (radio1 == nullptr) {
		printf("Reset radio 1 called without radio 1\n");
		throw std::invalid_argument("Invalid radio 1");
	}
	{
	std::unique_lock<std::mutex> lock(radio1_mtx);
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
		uint64_t ln1 = string_to_address(current_settings()->address_1_1);
		uint64_t ln2 = string_to_address(current_settings()->address_0_2);

		radio1->openReadingPipe(1, ln1);
		radio1->openWritingPipe(ln2);
		radio1->setChannel(current_settings()->channel_1);
		printf("RADIO1 -> ROLE: %d \t read: %s : %" PRIu64 " \t write %s : %" PRIu64 " \t channel: %d\n",
				role, current_settings()->address_1_1.c_str(), ln1,
				current_settings()->address_0_2.c_str(), ln2,
				current_settings()->channel_1);
	} else {
		uint64_t ln1 = string_to_address(current_settings()->address_1_2);
		uint64_t ln2 = string_to_address(current_settings()->address_0_1);
		radio1->openReadingPipe(1, ln1);
		radio1->openWritingPipe(ln2);
		radio1->setChannel(current_settings()->channel_0);
		printf("RADIO1 -> ROLE: %d \t read: %s : %" PRIu64 " \t write %s : %" PRIu64 " \t channel: %d\n",
				role, current_settings()->address_1_2.c_str(), ln1,
				current_settings()->address_0_1.c_str(), ln2,
				current_settings()->channel_0);

	}

	radio1->stopListening();
	if(print_info)
		radio1->printPrettyDetails();
	}

	radio1_cv.notify_all();
}


void DualRF24::stop_module(){
	radio0->stopListening();
	radio1->stopListening();
	radio0->flush_rx();
	radio0->flush_tx();
	radio1->flush_rx();
	radio1->flush_tx();

	running = false;
	radio0_cv.notify_all();
	radio1_cv.notify_all();


	if(attached){
		attached = false;
		detachInterrupt(settings->irq_pin_radio0);
	}
}

