/*
 * DualRF24.h
 *
 *  Created on: 27 May 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../RadioInterface.h"

#include "RF24/RF24.h"

#include <unistd.h>
#include <iostream>
#include <thread>

#include <mutex>
#include <condition_variable>

class DualRF24 : public RadioInterface {

		std::unique_ptr<std::thread> radio_read_thread_ptr;

		unsigned int payload_size = 32;

		bool role = true;

		RF24 radio0;
		RF24 radio1;
		bool radio_ready_to_use = false;

		inline void resetRadio0(bool print_info = true, bool acquire_lock = true);
		inline void resetRadio1(bool print_info = true, bool acquire_lock = true);

		inline bool check_radio_status(RF24* radio);
		inline void radio_read_thread();

		bool auto_ack = false;

		std::mutex radio0_mtx, radio1_mtx;
		std::condition_variable radio0_cv, radio1_cv;
		inline void send_tx();

		unsigned int buffer_counter = 0;

		bool attached_rx = false, attached_tx = false, tx_done = true;;
	public:
		DualRF24();
		virtual ~DualRF24();

		inline void receive_ISR_rx();
		inline void receive_ISR_tx();

		inline void input_finished();
		inline bool input(RFMessage &m);
		inline bool input(std::deque<RFMessage> &q) override;
		inline bool input(std::vector<RFMessage> &q) override;

		inline void apply_settings(const Settings &settings);
		inline void stop_module();

};


