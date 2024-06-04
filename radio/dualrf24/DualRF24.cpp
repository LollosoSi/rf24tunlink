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
static void ISR_static_rx(){
	my_instance->receive_ISR_rx();
}
static void ISR_static_tx(){
	my_instance->receive_ISR_tx();
}

DualRF24::DualRF24() {
	
}

DualRF24::~DualRF24() {
	//delete radio0;
	//delete radio1;
}

inline void DualRF24::apply_settings(const Settings &settings){
	RadioInterface::apply_settings(settings);
	role = settings.primary;

	stop_module();

	running = false;

	my_instance = this;

	if(settings.irq_pin_radio0 == 0){
		std::cout << "IRQ PIN for radio 0 can't be unset\n";
		throw std::invalid_argument("IRQ PIN RADIO0 UNSET");
	}

	payload_size = current_settings()->payload_size;
	auto_ack = current_settings()->auto_ack;
	radio_ready_to_use = false;
	//if (!radio0)
		radio0 = RF24(settings.ce_0_pin,
				settings.csn_0_pin, settings.spi_speed);

	//if (!radio1)
		radio1 = RF24(settings.ce_1_pin,
				settings.csn_1_pin, settings.spi_speed);
		radio_ready_to_use = true;

	resetRadio0();
	resetRadio1();
	running = true;

	//radio_read_thread_ptr = std::make_unique<std::thread>([&]{radio_read_thread();});
}

inline bool DualRF24::check_radio_status(RF24* radio) {
	bool c1 = radio->failureDetected;
	bool c2 = radio->getDataRate() != ((rf24_datarate_e)current_settings()->data_rate);
	bool c3 = !radio->isChipConnected();
	if (c1 || c2 || c3) {
		printf("Radio %d has failure. C1 %d, C2 %d, C3 %d\n", radio==&radio0 ? 0 : 1, c1, c2, c3);
		radio->failureDetected = 0;
		return (true);
	}else{
		return (false);
	}
}

inline void DualRF24::receive_ISR_rx(){

	std::unique_lock lock(radio0_mtx);
	radio0_cv.wait(lock, [&] {
		return !running || radio_ready_to_use;
	});
	if(!running || !radio_ready_to_use)return;

	bool tx_ds = 0, tx_df = 0, rx_dr = 0;  // declare variables for IRQ masks
	radio0.whatHappened(tx_ds, tx_df, rx_dr);

	//printf("Radio data in (IRQ)\n");

	if (rx_dr) {
		std::deque<RFMessage> messages;
		uint8_t pipe = 0;
		while (radio0.available(&pipe)) {
			RFMessage rfm = pmf->make_new_packet();
			radio0.read(rfm.data.get(), radio0.getPayloadSize());
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
		// TODO: Check if this is safe
		packetizer->input(messages);
	}

	if (check_radio_status(&radio0))
		resetRadio0(false, false);
}

inline void DualRF24::radio_read_thread() {
	while (running) {
		std::deque<RFMessage> messages;
		uint8_t pipe = 0;
		while (radio0.available(&pipe)) {
			RFMessage rfm = pmf->make_new_packet();
			radio0.read(rfm.data.get(), radio0.getPayloadSize());
			switch (pipe) {
			default:
				break;
			case 0:
			case 1:
				messages.push_back(std::move(rfm));
				break;
			}
		}
		//printf("[Radio Read]Collected packets: %ld\n",messages.size());
		// TODO: Check if this is safe
		packetizer->input(messages);
	}
	printf("Radio read exiting\n");
}

inline void DualRF24::receive_ISR_tx(){
	bool tx_ds = 0, tx_df = 0, rx_dr = 0;  // declare variables for IRQ masks
	{
	std::unique_lock lock(radio1_mtx);
	radio1.whatHappened(tx_ds, tx_df, rx_dr);
	if(tx_ds || tx_df)
		tx_done = true;
	}
	radio1_cv.notify_all();
	//printf("TX ISR %d, %d, %d\n",tx_ds,tx_df,rx_dr);
}

inline void DualRF24::input_finished(){
	//if(!radio1.isFifo(true,true)){
		//radio1.txStandBy();
		//printf("Radio data out (F)\n");
		//radio1.flush_tx();
	//}
}

inline void DualRF24::send_tx(){
	tx_done = false;
	if(!radio1.txStandBy()){
		radio1.flush_tx();
	//	radio1.flush_rx();
	}
	//std::this_thread::sleep_for(std::chrono::milliseconds(2));
}

inline bool DualRF24::input(RFMessage &m){

	std::unique_lock lock(radio1_mtx);

	radio1_cv.wait(lock, [&] {
		return (!running || radio_ready_to_use);
	});
	if(!running || !radio_ready_to_use) return (false);

	if (!radio1.writeFast(m.data.get(), payload_size, !auto_ack))
		send_tx();

	// Get all packets out of the way
	//if (!radio1.isFifo(true, true))
	//	send_tx();

	if (check_radio_status(&radio1))
		resetRadio1(false, false);


	return (true);
}

inline bool DualRF24::input(std::vector<RFMessage> &q) {
	std::unique_lock lock(radio1_mtx);
	radio1_cv.wait(lock, [&] {
		return (!running || radio_ready_to_use);
	});
	if (!running || !radio_ready_to_use)
		return (false);

	if (check_radio_status(&radio1))
		resetRadio1(false, false);

	for (auto& m : q) {
		if (!radio1.writeFast(m.data.get(), payload_size, !auto_ack))
			send_tx();
	}

	// Get all packets out of the way
	if (!radio1.isFifo(true, true))
		send_tx();
	return true;
}

inline bool DualRF24::input(std::deque<RFMessage> &q) {
	std::unique_lock lock(radio1_mtx);
	radio1_cv.wait(lock, [&] {
		return (!running || radio_ready_to_use);
	});
	if (!running || !radio_ready_to_use)
		return (false);

	if (check_radio_status(&radio1))
		resetRadio1(false, false);

	for (auto& m : q) {
		if (!radio1.writeFast(m.data.get(), payload_size, !auto_ack))
			send_tx();
	}

	// Get all packets out of the way
	if (!radio1.isFifo(true, true))
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

inline void DualRF24::resetRadio0(bool print_info, bool acquire_lock) {

	if(!radio_ready_to_use){
		printf("Reset radio 0 called without radio 0\n");
		throw std::invalid_argument("Invalid radio 0");
	}

	if(acquire_lock){
		std::unique_lock<std::mutex> lock(radio0_mtx);

		resetRadio0(print_info, false);
		radio0_cv.notify_all();
		return;
	}

	{
	//std::unique_lock<std::mutex> lock(radio0_mtx);

	uint8_t t = 1;
	while (!radio0.begin(current_settings()->ce_0_pin,
			current_settings()->csn_0_pin)) {
		printf("Radio 0 is not responsive (CE: %i, CSN: %i)\n",
				current_settings()->ce_0_pin, current_settings()->csn_0_pin);
		usleep(10000);
		// delay(2);
		if (!t++) {
			// handleRadioUnresponsiveTimeout();
		}
	}

	radio0.flush_rx();
	radio0.flush_tx();

	radio0.setAddressWidth(current_settings()->address_bytes);

	radio0.setPayloadSize(current_settings()->payload_size);

	radio0.setPALevel(current_settings()->radio_power);
	radio0.setDataRate((rf24_datarate_e)current_settings()->data_rate);
	radio0.setRetries(current_settings()->radio_delay,
			current_settings()->radio_retries);

	radio0.setAutoAck(current_settings()->auto_ack);

	if (current_settings()->dynamic_payloads)
		radio0.enableDynamicPayloads();
	else
		radio0.disableDynamicPayloads();

	if (current_settings()->ack_payloads)
		radio0.enableAckPayload();
	else
		radio0.disableAckPayload();

	radio0.setCRCLength((rf24_crclength_e)current_settings()->crc_length);

	if (role) {
		uint64_t ln = string_to_address(current_settings()->address_0_1);
		radio0.openReadingPipe(1, ln);
		radio0.setChannel(current_settings()->channel_0);
		printf("RADIO0 -> ROLE: %d \t read: %s : %" PRIu64 " \t write %s \t channel: %d\n",
				role, current_settings()->address_0_1.c_str(), ln, "none",
				current_settings()->channel_0);

	} else {
		uint64_t ln = string_to_address(current_settings()->address_0_2);
		radio0.openReadingPipe(1, ln);
		radio0.setChannel(current_settings()->channel_1);
		printf("RADIO0 -> ROLE: %d \t read: %s : %" PRIu64 " \t write %s \t channel: %d\n",
				role, current_settings()->address_0_2.c_str(), ln, "none",
				current_settings()->channel_1);

	}

	// Configure radio0 to interrupt for read events
	radio0.maskIRQ(true,true,false);

	if (!attached_rx) {
		attached_rx = true;
		//gpioInitialise();
		pinMode(current_settings()->irq_pin_radio0, INPUT);
		attachInterrupt(current_settings()->irq_pin_radio0, INT_EDGE_FALLING, &ISR_static_rx);
		printf("Attached Interrupt\n");
	}

	radio0.startListening();
	if(print_info)
		radio0.printPrettyDetails();
}
	if(!acquire_lock)
		radio0_cv.notify_all();

}
inline void DualRF24::resetRadio1(bool print_info, bool acquire_lock) {
	if (!radio_ready_to_use) {
		printf("Reset radio 1 called without radio 1\n");
		throw std::invalid_argument("Invalid radio 1");
	}
	if(acquire_lock){
			std::unique_lock<std::mutex> lock(radio1_mtx);

			resetRadio1(print_info, false);
			radio1_cv.notify_all();
			return;
		}

	{
	//std::unique_lock<std::mutex> lock(radio1_mtx);
	uint8_t t = 1;
	while (!radio1.begin(current_settings()->ce_1_pin,
			current_settings()->csn_1_pin)) {
		printf("Radio 1 is not responsive (CE: %i, CSN: %i)\n",
				current_settings()->ce_1_pin, current_settings()->csn_1_pin);
		usleep(10000);
		// delay(2);
		if (!t++) {
			// handleRadioUnresponsiveTimeout();
		}
	}

	radio1.flush_rx();
	radio1.flush_tx();

	radio1.setAddressWidth(current_settings()->address_bytes);

	radio1.setPayloadSize(current_settings()->payload_size);

	radio1.setPALevel(current_settings()->radio_power);
	radio1.setDataRate((rf24_datarate_e)current_settings()->data_rate);
	radio1.setRetries(current_settings()->radio_delay,
			current_settings()->radio_retries);

	radio1.setAutoAck(current_settings()->auto_ack);

	if (current_settings()->dynamic_payloads)
		radio1.enableDynamicPayloads();
	else
		radio1.disableDynamicPayloads();

	if (current_settings()->ack_payloads)
		radio1.enableAckPayload();
	else
		radio1.disableAckPayload();

	radio1.setCRCLength((rf24_crclength_e)current_settings()->crc_length);

	if (role) {
		uint64_t ln1 = string_to_address(current_settings()->address_1_1);
		uint64_t ln2 = string_to_address(current_settings()->address_0_2);

		//radio1.openReadingPipe(1, ln1);
		radio1.openWritingPipe(ln2);
		radio1.setChannel(current_settings()->channel_1);
		printf("RADIO1 -> ROLE: %d \t read: %s : %" PRIu64 " \t write %s : %" PRIu64 " \t channel: %d. NOTE Reading pipe is disabled\n",
				role, current_settings()->address_1_1.c_str(), ln1,
				current_settings()->address_0_2.c_str(), ln2,
				current_settings()->channel_1);
	} else {
		uint64_t ln1 = string_to_address(current_settings()->address_1_2);
		uint64_t ln2 = string_to_address(current_settings()->address_0_1);
		//radio1.openReadingPipe(1, ln1);
		radio1.openWritingPipe(ln2);
		radio1.setChannel(current_settings()->channel_0);
		printf("RADIO1 -> ROLE: %d \t read: %s : %" PRIu64 " \t write %s : %" PRIu64 " \t channel: %d. NOTE Reading pipe is disabled\n",
				role, current_settings()->address_1_2.c_str(), ln1,
				current_settings()->address_0_1.c_str(), ln2,
				current_settings()->channel_0);

	}

	// Configure radio1 to interrupt for write events
	radio1.maskIRQ(false, false, true);

	if (!attached_tx) {
		//attached_tx = true;

		//pinMode(current_settings()->irq_pin_radio1, INPUT);
		//attachInterrupt(current_settings()->irq_pin_radio1, INT_EDGE_FALLING, &ISR_static_tx);
		//printf("Attached Interrupt radio 1\n");
	}

	radio1.stopListening();
	if(print_info)
		radio1.printPrettyDetails();
	}

	if(!acquire_lock)
		radio1_cv.notify_all();

}


inline void DualRF24::stop_module(){

	running = false;
	radio0_cv.notify_all();
	radio1_cv.notify_all();
	if (radio_read_thread_ptr)
		if (radio_read_thread_ptr->joinable())
			radio_read_thread_ptr->join();
	radio_read_thread_ptr.reset();

	if (radio_ready_to_use) {
		radio0.stopListening();
		radio1.stopListening();
		radio0.flush_rx();
		radio0.flush_tx();
		radio1.flush_rx();
		radio1.flush_tx();
	}

	if (attached_rx) {
		attached_rx = false;
		detachInterrupt(settings->irq_pin_radio0);
	}
	if (attached_tx) {
		attached_tx = false;
		detachInterrupt(settings->irq_pin_radio1);
	}
}

