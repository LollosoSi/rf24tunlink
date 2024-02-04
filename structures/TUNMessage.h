/*
 * TUNMessage.h
 *
 *  Created on: 4 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include <RF24/RF24.h>

struct TUNMessage{
		uint8_t *data = 0;
		unsigned int size = 0;
};
