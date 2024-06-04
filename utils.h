/*
 * utils.h
 *
 *  Created on: 7 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include <RF24/RF24.h>
#include <iostream>

/*
static void print_hex(uint8_t* data, unsigned int size){
	for(unsigned int i = 0; i < size; i++){
		std::cout << std::hex << data[i] << std::dec
				<< " ";
	}
	std::cout << "\n";
}*/


#include <chrono>
#include <time.h>      // CLOCK_MONOTONIC_RAW, timespec, clock_gettime()
#include <ctime>       // time()

static inline uint64_t current_millis() {
	return (std::chrono::duration_cast < std::chrono::milliseconds > (std::chrono::system_clock::now().time_since_epoch()).count());
}

