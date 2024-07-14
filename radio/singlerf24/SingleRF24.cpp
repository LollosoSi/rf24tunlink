/*
 * SingleRF24.cpp
 *
 *  Created on: 5 Jun 2024
 *      Author: Andrea Roccaccino
 */

#include "SingleRF24.h"

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

SingleRF24* rf24reference;
static void ISR_static_rx(){
	rf24reference->ISR_RX();
}

inline void SingleRF24::stop_module(){

	running = false;
	ack_done=true;
	radio_cv.notify_all();
	radio_ackwait_cv.notify_all();

	if (radio_ready_to_use) {
		radio.stopListening();
		radio.flush_rx();
		radio.flush_tx();
	}

	if (attached_rx) {
		attached_rx = false;
		detachInterrupt(settings->irq_pin_radio0);
	}

}

SingleRF24::SingleRF24() {
	rf24reference = this;
}

SingleRF24::~SingleRF24() {
}

inline void SingleRF24::apply_settings(const Settings &settings) {
	RadioInterface::apply_settings(settings);

	stop_module();

	role = settings.primary;
	payload_size = current_settings()->payload_size;
	auto_ack = current_settings()->auto_ack;
	radio_ready_to_use = false;

	radio = RF24(settings.ce_0_pin, settings.csn_0_pin,settings.spi_speed);

	radio_ready_to_use = true;

	reset_radio();
	check_radio();

	running = true;

}

void SingleRF24::input_finished(){}
inline bool SingleRF24::input(RFMessage &m) {
	std::unique_lock lock(radio_mtx);

	radio_cv.wait(lock, [&] {
		return (!running || radio_ready_to_use);
	});
	if (!running || !radio_ready_to_use)
		return (false);
	if (role) {
		if(!radio.writeFast(m.data.get(), payload_size, !auto_ack))
			radio.txStandBy();
		//else if(!radio.txStandBy())
		//	radio.reUseTX();

	} else {

		if (radio.isFifo(false, false)) {
			ack_done = false;
			radio_ackwait_cv.wait(lock, [&] {
				return (!running || radio_ready_to_use) || ack_done;
			});
		}
		radio.writeAckPayload(1, m.data.get(), payload_size);

	}
	// Get all packets out of the way
	//if (!radio1.isFifo(true, true))
	//	send_tx();

	if (check_radio())
		reset_radio(false, false);

	return true;
}

inline void SingleRF24::ISR_RX() {
	{
		std::unique_lock lock(radio_mtx);
		radio_cv.wait(lock, [&] {
			return !running || radio_ready_to_use;
		});
		if (!running || !radio_ready_to_use)
			return;

		bool tx_ds = 0, tx_df = 0, rx_dr = 0; // declare variables for IRQ masks
		radio.whatHappened(tx_ds, tx_df, rx_dr);

		//printf("Radio data in (IRQ)\n");

		if (rx_dr) {
			std::deque<RFMessage> messages;
			uint8_t pipe = 0;
			while (radio.available(&pipe)) {
				RFMessage rfm = pmf->make_new_packet();
				radio.read(rfm.data.get(), radio.getPayloadSize());
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

		if (check_radio())
			reset_radio(false, false);

		ack_done = true;
	}
	radio_ackwait_cv.notify_all();
}

void SingleRF24::reset_radio(bool print_info, bool acquire_lock){
	if(!radio_ready_to_use){
			printf("Reset radio called without radio\n");
			throw std::invalid_argument("Invalid radio");
		}

		if(acquire_lock){
			std::unique_lock<std::mutex> lock(radio_mtx);

			reset_radio(print_info, false);
			radio_cv.notify_all();
			return;
		}

		{
		//std::unique_lock<std::mutex> lock(radio_mtx);

		uint8_t t = 1;
		while (!radio.begin(current_settings()->ce_0_pin,
				current_settings()->csn_0_pin)) {
			printf("Radio 0 is not responsive (CE: %i, CSN: %i)\n",
					current_settings()->ce_0_pin, current_settings()->csn_0_pin);
			usleep(10000);
			// delay(2);
			if (!t++) {
				// handleRadioUnresponsiveTimeout();
			}
		}

		radio.flush_rx();
		radio.flush_tx();

		radio.setAddressWidth(current_settings()->address_bytes);

		radio.setPayloadSize(current_settings()->payload_size);

		radio.setPALevel(current_settings()->radio_power);
		radio.setDataRate((rf24_datarate_e)current_settings()->data_rate);
		radio.setRetries(current_settings()->radio_delay,
				current_settings()->radio_retries);

		radio.setAutoAck(current_settings()->auto_ack);

		if (current_settings()->dynamic_payloads)
			radio.enableDynamicPayloads();
		else
			radio.disableDynamicPayloads();

		if (current_settings()->ack_payloads)
			radio.enableAckPayload();
		else
			radio.disableAckPayload();

		radio.setCRCLength((rf24_crclength_e)current_settings()->crc_length);

		if (role) {
			uint64_t ln = string_to_address(current_settings()->address_0_1);
			uint64_t ln2 = string_to_address(current_settings()->address_0_2);
			radio.openReadingPipe(1, ln);
			radio.openWritingPipe(ln2);
			radio.setChannel(current_settings()->channel_0);
			printf("radio -> ROLE: %d \t read: %s : %" PRIu64 " \t write %s \t channel: %d\n",
					role, current_settings()->address_0_1.c_str(), ln, current_settings()->address_0_2.c_str(),
					current_settings()->channel_0);

		} else {
			uint64_t ln = string_to_address(current_settings()->address_0_2);
			uint64_t ln2 = string_to_address(current_settings()->address_0_1);
			radio.openReadingPipe(1, ln);
			radio.openWritingPipe(ln2);
			radio.setChannel(current_settings()->channel_0);
			printf("radio -> ROLE: %d \t read: %s : %" PRIu64 " \t write %s \t channel: %d\n",
					role, current_settings()->address_0_2.c_str(), ln, current_settings()->address_0_1.c_str(),
					current_settings()->channel_1);
			RFMessage rfm = RFMessage(settings->payload_size);
			memset(rfm.data.get(),0,settings->payload_size);
			radio.writeAckPayload(1, rfm.data.get(), settings->payload_size);
		}

		// Configure radio to interrupt for read events
		radio.maskIRQ(true,true,false);

		if (!attached_rx) {
			attached_rx = true;
			//gpioInitialise();
			pinMode(current_settings()->irq_pin_radio0, INPUT);
			attachInterrupt(current_settings()->irq_pin_radio0, INT_EDGE_FALLING, &ISR_static_rx);
			printf("Attached Interrupt\n");
		}
		if(role)
			radio.stopListening();
		else
			radio.startListening();

		if(print_info)
			radio.printPrettyDetails();
	}
		if(!acquire_lock)
			radio_cv.notify_all();
}
