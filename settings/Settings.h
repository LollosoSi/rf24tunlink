/*
 * Settings.h
 *
 *  Created on: 3 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include <RF24/RF24.h>

namespace Settings{

extern char* address;			// Address of the interface		NOTE: Address and destination must be swapped based on the radio role
extern char* destination;		// Destination of the interface
extern char* netmask;			// Network address mask
extern char* interface_name;	// Interface name (arocc)
extern int mtu;		// MTU, must be determined later
extern bool control_packets;		// Whether or not to send empty packets as keepalive

const unsigned int max_pkt_size = 32;



}

namespace Settings::RF24{

	extern int ce_pin;
	extern int csn_pin;
	extern unsigned long spi_speed;

	extern uint8_t radio_delay;
	extern uint8_t radio_retries;
	extern rf24_datarate_e data_rate;
	extern rf24_pa_dbm_e radio_power;
	extern rf24_crclength_e crc_length;

	extern uint8_t channel;
	extern uint8_t address_bytes;
	extern uint8_t* address_1;
	extern uint8_t* address_2;

}
