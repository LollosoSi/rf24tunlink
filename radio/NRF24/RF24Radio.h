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


class RF24Radio: public RadioHandler<RadioPacket> {
public:
	RF24Radio();
	virtual ~RF24Radio();

	void setup();
	void loop(unsigned long delta);

protected:
	void stop();

};

