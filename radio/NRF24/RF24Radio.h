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
	RF24Radio();
	virtual ~RF24Radio();

	void loop(unsigned long delta);
	void set_speed_pa_retries();
	void channel_sweep();

	std::string* telemetry_collect(const unsigned long delta);

protected:
	void send_tx();
	void fill_buffer_tx();
	void fill_buffer_ack();

	void reset_radio();
	virtual void setup() override;
	virtual void stop() override;
	bool primary = false;

	RF24* radio = nullptr;

private:
	std::string* returnvector = nullptr;
	unsigned long sum_arc = 0;
	unsigned int count_arc = 0;

};

