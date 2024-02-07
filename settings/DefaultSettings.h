/*
 * DefaultSettings.h
 *
 *  Created on: 3 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "Settings.h"
#include <string>

namespace Settings {

char *address = new char[18] { '\0' };
char *destination = new char[18] { '\0' };
char *netmask = new char[18] { '\0' };
char *interface_name = new char[6] { 'a', 'r', 'o', 'c', 'c', '\0' };
int mtu = 9600;
bool control_packets = true;

}

namespace Settings::RF24{

	int ce_pin = 25;
	int csn_pin = 0;
	unsigned long spi_speed = 5000000;

	uint8_t radio_delay = 3;
	uint8_t radio_retries = 6;
	rf24_datarate_e data_rate = RF24_2MBPS;
	rf24_pa_dbm_e radio_power = RF24_PA_MAX;
	rf24_crclength_e crc_length = RF24_CRC_8;

	uint8_t channel = 90;
	uint8_t address_bytes = 3;
	uint8_t* address_1 = new uint8_t[3]{'1','N','o'};
	uint8_t* address_2 = new uint8_t[3]{'2','N','o'};



}
