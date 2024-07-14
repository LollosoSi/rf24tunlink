/*
 * crc.h
 *
 *  Created on: 28 May 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

static uint8_t gencrc(uint8_t *data, size_t len) {
	uint8_t crc = 0xff;
	size_t i, j;
	for (i = 0; i < len; i++) {
		crc ^= data[i];
		for (j = 0; j < 8; j++) {
			if ((crc & 0x80) != 0)
				crc = static_cast<uint8_t>(((crc << 1) ^ 0x31));
			else crc <<= 1;
		}
	}
	return (crc);
}
