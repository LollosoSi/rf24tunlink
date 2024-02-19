/*
 * Settings.h
 *
 *  Created on: 3 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include <RF24/RF24.h>

namespace Settings {

extern char *address;// Address of the interface		NOTE: Address and destination must be swapped based on the radio role
extern char *destination;		// Destination of the interface
extern char *netmask;			// Network address mask
extern char *interface_name;	// Interface name (arocc)
extern unsigned int mtu;		// MTU, must be determined later
extern bool control_packets;// Whether or not to send empty packets as keepalive

const unsigned int max_pkt_size = 32;

}

namespace Settings::RF24 {

extern bool variable_rate;

extern uint16_t max_radio_silence;

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
extern uint8_t *address_1;
extern uint8_t *address_2;
extern uint8_t *address_3;

extern uint8_t *radio_delay_tuned;
extern uint8_t *radio_retries_tuned;

inline uint8_t select_delay() {
	return (data_rate == RF24_2MBPS ? radio_delay_tuned[2] :
			data_rate == RF24_1MBPS ?
					radio_delay_tuned[1] : radio_delay_tuned[0]);
}
inline uint8_t select_retries() {
	return (data_rate == RF24_2MBPS ? radio_retries_tuned[2] :
			data_rate == RF24_1MBPS ?
					radio_retries_tuned[1] : radio_retries_tuned[0]);
}

}

namespace Settings::DUAL_RF24 {

extern uint16_t max_radio_silence;

extern bool variable_rate;

extern int ce_0_pin;
extern int csn_0_pin;

extern int ce_1_pin;
extern int csn_1_pin;

extern unsigned long spi_speed;

extern rf24_datarate_e data_rate;
extern rf24_pa_dbm_e radio_power;
extern rf24_crclength_e crc_length;

extern uint8_t radio_delay;
extern uint8_t radio_retries;

extern uint8_t channel_0;
extern uint8_t channel_1;
extern uint8_t address_bytes;
extern uint8_t *address_0_1;
extern uint8_t *address_0_2;
extern uint8_t *address_0_3;

extern uint8_t *address_1_1;
extern uint8_t *address_1_2;
extern uint8_t *address_1_3;

extern uint8_t *radio_delay_tuned;
extern uint8_t *radio_retries_tuned;

inline uint8_t select_delay() {
	return (data_rate == RF24_2MBPS ? radio_delay_tuned[2] :
			data_rate == RF24_1MBPS ?
					radio_delay_tuned[1] : radio_delay_tuned[0]);
}
inline uint8_t select_retries() {
	return (data_rate == RF24_2MBPS ? radio_retries_tuned[2] :
			data_rate == RF24_1MBPS ?
					radio_retries_tuned[1] : radio_retries_tuned[0]);
}

}
