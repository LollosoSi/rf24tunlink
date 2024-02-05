/*
 * RadioPacket.h
 *
 *  Created on: 4 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include <RF24/RF24.h>

struct RadioPacket{
		uint8_t *data = 0;
		uint8_t info = 0;
		uint8_t size = 0;
};
