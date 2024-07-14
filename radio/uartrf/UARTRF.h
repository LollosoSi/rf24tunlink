/*
 * UARTRF.h
 *
 *  Created on: 12 Jun 2024
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

class UARTRF : public RadioInterface {

		int uart_file_descriptor = 0;
		std::unique_ptr<std::thread> readthread;
		std::mutex out_mtx;

		static const uint8_t byte_escape = 250, byte_SOF = 1, byte_EOF = 61;

		uint8_t receive_buffer_max_size = 0, receive_buffer_cursor = 0;
		uint8_t* receive_buffer = nullptr;
		uint8_t payload_size = 32;
		bool is_waiting_next_escape = false;

		void initialize_uart();
		void stop_read_thread();
		void close_file();
		void write_stuff(uint8_t *data, unsigned int length);
		void receive_bytes(uint8_t *data, unsigned int length);
		void message_reset();
		void message_read_completed();
	public:
		UARTRF();
		virtual ~UARTRF();

		inline void input_finished();
		inline bool input(RFMessage &m);
		inline bool input(std::vector<RFMessage> &ms);
		inline void apply_settings(const Settings &settings) override;
		inline void stop_module();
};

