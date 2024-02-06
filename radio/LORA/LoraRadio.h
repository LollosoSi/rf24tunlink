/*
 * LoraRadio.h
 *
 *  Created on: 6 Feb 2024
 *      Author: Andrea Roccaccino
 */
#pragma once

#include "../../interfaces/RadioHandler.h"
#include "../../structures/RadioPacket.h"

class LoraRadio: public RadioHandler<RadioPacket> {
public:
	LoraRadio();
	virtual ~LoraRadio();

	void setup();
	void loop(unsigned long delta);

protected:
	void stop();
};

