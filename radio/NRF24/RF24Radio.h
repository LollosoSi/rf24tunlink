/*
 * RF24Radio.h
 *
 *  Created on: 6 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../../interfaces/RadioHandler.h"
#include "../../structures/RadioPacket.h"
#include "../../telemetry/Telemetry.h"

#include <RF24/RF24.h>

#include <iostream>
#include <unistd.h>

class RF24Radio: public RadioHandler<RadioPacket>, public Telemetry {
public:
	RF24Radio(bool primary);
	virtual ~RF24Radio();

	void loop(unsigned long delta);
	void set_speed_pa_retries();
	void channel_sweep();

	void interrupt_routine();

	std::string* telemetry_collect(const unsigned long delta);

protected:
	bool send_tx();
	bool fill_buffer_tx();
	bool fill_buffer_ack();
	bool read();

	void check_fault();



	void reset_radio();
	virtual void setup() override;
	virtual void stop() override;
	bool primary = false;

	RF24* radio = nullptr;

private:
	std::string* returnvector = nullptr;
	unsigned long sum_arc = 0;
	unsigned int count_arc = 0;
	unsigned int packets_in = 0;
	unsigned long radio_bytes_out = 0;
	unsigned long radio_bytes_in = 0;

	inline uint16_t since_last_packet();
	uint64_t last_packet = 0;

};

