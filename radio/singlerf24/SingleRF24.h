/*
 * SingleRF24.h
 *
 *  Created on: 5 Jun 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../RadioInterface.h"
#include "RF24/RF24.h"

#include <mutex>
#include <condition_variable>
#include <deque>
#include <string>
#include <unistd.h>
#include <iostream>
#include <cinttypes>

class SingleRF24 : public RadioInterface {

		unsigned int payload_size = 32;
		bool auto_ack = false;
		bool radio_ready_to_use = false;
		bool attached_rx = false;

		std::mutex radio_mtx;
		std::condition_variable radio_cv;
		std::condition_variable radio_ackwait_cv;

		bool role = true;
		bool ack_done = true;

	public:
		SingleRF24();
		virtual ~SingleRF24();

		RF24 radio;

		virtual inline void input_finished();
		virtual inline bool input(RFMessage &m);
		inline void stop_module();
		inline void apply_settings(const Settings &settings) override;
		inline bool check_radio(){
			bool c1 = radio.failureDetected;
			bool c2 = radio.getDataRate()
					!= ((rf24_datarate_e) current_settings()->data_rate);
			bool c3 = !radio.isChipConnected();
			if (c1 || c2 || c3) {
				printf("Radio has failure. C1 %d, C2 %d, C3 %d\n", c1, c2, c3);
				radio.failureDetected = 0;
				return (true);
			} else {
				return (false);
			}
		}
		void reset_radio(bool print_info = true, bool acquire_lock = true);

		inline void ISR_RX();

};

