/*
 * utils.h
 *
 *  Created on: 7 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include <RF24/RF24.h>
#include <iostream>

static void print_hex(uint8_t* data, unsigned int size){
	for(unsigned int i = 0; i < size; i++){
		std::cout << std::hex << data[i] << std::dec
				<< " ";
	}
	std::cout << "\n";
}

static uint8_t gencrc(uint8_t *data, size_t len) {
	uint8_t crc = 0xff;
	size_t i, j;
	for (i = 0; i < len; i++) {
		crc ^= data[i];
		for (j = 0; j < 8; j++) {
			if ((crc & 0x80) != 0)
				crc = (uint8_t)((crc << 1) ^ 0x31);
			else crc <<= 1;
		}
	}
	return crc;
}

#include <chrono>
#include <time.h>      // CLOCK_MONOTONIC_RAW, timespec, clock_gettime()
#include <ctime>       // time()

static uint64_t current_millis() {
	return (std::chrono::duration_cast < std::chrono::milliseconds
			> (std::chrono::system_clock::now().time_since_epoch()).count());
}

