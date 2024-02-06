/*
 * RF24Radio.h
 *
 *  Created on: 6 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../../interfaces/RadioHandler.h"
#include "../../structures/RadioPacket.h"

#include <RF24/RF24.h>

#include <iostream>
#include <unistd.h>

class RF24Radio: public RadioHandler<RadioPacket> {
public:
	RF24Radio();
	virtual ~RF24Radio();

	void loop(unsigned long delta);
	void set_speed_pa_retries();
	void channel_sweep();

protected:

	void reset_radio();
	virtual void setup() override;
	virtual void stop() override;
	bool primary = false;

	RF24* radio = nullptr;

};

