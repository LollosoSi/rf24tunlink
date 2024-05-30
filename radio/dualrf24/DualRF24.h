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

		unsigned int payload_size = 32;

		bool role = true;

		RF24* radio0 = nullptr;
		RF24* radio1 = nullptr;

		void resetRadio0();
		void resetRadio1();

		inline void check_radio0_status();
		inline void check_radio1_status();

		std::mutex radio0_mtx, radio1_mtx;
		std::condition_variable radio0_cv, radio1_cv;


		bool attached = false;
	public:
		DualRF24();
		virtual ~DualRF24();

		void receive_ISR();

		void input_finished();
		bool input(RFMessage &m);
		void apply_settings(const Settings &settings);
		void stop_module();

};


