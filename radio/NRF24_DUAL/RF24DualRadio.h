/*
 * RF24DualRadio.h
 *
 *  Created on: 19 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include <RF24/RF24.h>

#include <iostream>
#include <unistd.h>

#include "../../telemetry/Telemetry.h"

#include "../../interfaces/RadioHandler.h"

class RF24DualRadio: public RadioHandler<RadioPacket>, public Telemetry {
public:
	RF24DualRadio(bool radio_role);
	virtual ~RF24DualRadio();

	void setup();
	void loop(unsigned long delta);

	bool role = false;
	std::string* telemetry_collect(const unsigned long delta);

protected:

	RF24 *radio_0 = nullptr;
	RF24 *radio_1 = nullptr;

	void check_fault();
	void reset_radio();

	void stop();

	void set_speed_pa_retries();

	bool read();
	bool send_tx();
	bool fill_buffer_tx();


private:
	std::string *returnvector = nullptr;
	unsigned long sum_arc = 0;
	unsigned int count_arc = 0;
	unsigned int packets_in = 0;
	unsigned long radio_bytes_out = 0;
	unsigned long radio_bytes_in = 0;

};

