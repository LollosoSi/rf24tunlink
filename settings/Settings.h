/*
 * Settings.h
 *
 *  Created on: 3 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include <RF24/RF24.h>

#include <string>
#include <vector>

namespace Settings {

extern bool display_telemetry;

extern char *address; // Address of the interface		NOTE: Address and destination must be swapped based on the radio role
extern char *destination;		// Destination of the interface
extern char *netmask;			// Network address mask
extern char *interface_name;	// Interface name (arocc)
extern unsigned int mtu;		// MTU, must be determined later
extern bool control_packets; // Whether or not to send empty packets as keepalive
extern uint16_t minimum_ARQ_wait;

extern uint16_t maximum_frame_time;

const unsigned int max_pkt_size = 32;

extern char *csv_out_filename;	// CSV output, NULLPTR for no output
extern char csv_divider;

extern void apply_settings(std::string& name, std::string& value);

}

namespace Settings::RF24 {

extern bool primary;

extern bool variable_rate;

extern uint16_t max_radio_silence;

extern int ce_pin;
extern int csn_pin;
extern uint32_t spi_speed;
extern bool auto_ack;

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

extern void apply_settings(std::string& name, std::string& value);

}

namespace Settings::DUAL_RF24 {

extern uint16_t max_radio_silence;

extern bool variable_rate;

extern bool auto_ack;

extern int ce_0_pin;
extern int csn_0_pin;

extern int ce_1_pin;
extern int csn_1_pin;

extern uint32_t spi_speed;

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
