/*
 * utils.h
 *
 *  Created on: 28 May 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

static inline uint64_t current_millis() {
	return (std::chrono::duration_cast < std::chrono::milliseconds > (std::chrono::system_clock::now().time_since_epoch()).count());
}
