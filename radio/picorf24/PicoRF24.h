/*
 * PicoRF24.h
 *
 *  Created on: 8 Jun 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../RadioInterface.h"

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

#include <cstring>

#include <thread>

#include <mutex>

#pragma pack(push, 1)
struct PicoSettingsBlock{
		bool auto_ack = false;
		bool dynamic_payloads = false;
		bool ack_payloads = false;
		bool primary = true;

		uint8_t payload_size = 32;
		uint8_t data_rate = 1;
		uint8_t radio_power = 0;
		uint8_t crc_length = 0;
		uint8_t radio_delay = 1;
		uint8_t radio_retries = 15;
		uint8_t channel_0 = 10;
		uint8_t channel_1 = 120;
		uint8_t address_bytes = 3;
		uint8_t address_0_1[6] = "nn1\0\0";
		uint8_t address_0_2[6] = "nn2\0\0";
		uint8_t address_0_3[6] = "nn3\0\0";
		uint8_t address_1_1[6] = "nn4\0\0";
		uint8_t address_1_2[6] = "nn5\0\0";
		uint8_t address_1_3[6] = "nn6\0\0";
};
#pragma pack(pop)

static void apply_settings_to_pico_block(PicoSettingsBlock& psb, const Settings* settings){
	psb.auto_ack = settings->auto_ack;
	psb.dynamic_payloads = settings->dynamic_payloads;
	psb.ack_payloads = settings->ack_payloads;
	psb.primary = settings->primary;
	psb.payload_size = settings->payload_size;
	psb.data_rate = settings->data_rate;
	psb.radio_power = settings->radio_power;
	psb.crc_length = settings->crc_length;
	psb.radio_delay = settings->radio_delay;
	psb.radio_retries = settings->radio_retries;
	psb.channel_0 = settings->channel_0;
	psb.channel_1 = settings->channel_1;
	psb.address_bytes = settings->address_bytes;
	std::memcpy(psb.address_0_1, settings->address_0_1.c_str(), settings->address_bytes);
	std::memcpy(psb.address_0_2, settings->address_0_2.c_str(), settings->address_bytes);
	std::memcpy(psb.address_0_3, settings->address_0_3.c_str(), settings->address_bytes);
	std::memcpy(psb.address_1_1, settings->address_1_1.c_str(), settings->address_bytes);
	std::memcpy(psb.address_1_2, settings->address_1_2.c_str(), settings->address_bytes);
	std::memcpy(psb.address_1_3, settings->address_1_3.c_str(), settings->address_bytes);
}

class PicoRF24 : public RadioInterface {
	protected:
		int uart_file_descriptor;

		std::unique_ptr<std::thread> readthread;

		void initialize_uart();
		void stop_read_thread();
		void restart_pico();
		void close_file();
		void transfer_settings(PicoSettingsBlock *psb);
		std::mutex out_mtx;

	public:
		PicoRF24();
		virtual ~PicoRF24();

		inline void input_finished();
		inline bool input(RFMessage &m);
		inline bool input(std::vector<RFMessage> &ms)override;
		void apply_settings(const Settings &settings) override;
		inline void stop_module();

};

