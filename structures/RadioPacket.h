/*
 * RadioPacket.h
 *
 *  Created on: 4 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include <RF24/RF24.h>

#include "../settings/Settings.h"

struct RadioPacket{
		uint8_t data[Settings::max_pkt_size] = {0};
		uint8_t size = 0;
};

