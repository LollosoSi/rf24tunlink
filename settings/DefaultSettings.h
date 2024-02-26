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
unsigned int mtu = 9600;
bool control_packets = true;

uint16_t minimum_ARQ_wait = 3;

uint16_t maximum_frame_time = 30;

char *csv_out_filename = nullptr;	// CSV output, NULLPTR for no output
char csv_divider = ',';

void apply_settings(std::string &name, std::string &value) {

	if (name == "addr") {
		strcpy(address, value.c_str());
	} else if (name == "dest_addr") {
		strcpy(destination, value.c_str());
	} else if (name == "netmask") {
		strcpy(netmask, value.c_str());
	} else if (name == "iname") {
		strcpy(interface_name, value.c_str());
	} else if (name == "control_packets") {
		control_packets = (value == "yes");
	} else if (name == "csv_filename") {
		csv_out_filename = new char[value.length() + 1];
		strcpy(csv_out_filename, value.c_str());
	}

}

}

namespace Settings::RF24 {

bool primary = true;

uint16_t max_radio_silence = 600;

bool variable_rate = true;

int ce_pin = 24;
int csn_pin = 0; // Possible values: 0 1 for spi1: 10 11 12
//unsigned long spi_speed = 6000000; //(10000000-4000000);
//unsigned long spi_speed = 5300000; //(10000000-4000000);
uint32_t spi_speed = 6000000; //(10000000-4000000);
bool auto_ack = true;

rf24_datarate_e data_rate = RF24_2MBPS;
rf24_pa_dbm_e radio_power = RF24_PA_MAX;
rf24_crclength_e crc_length = RF24_CRC_8;

uint8_t radio_delay = 1;
uint8_t radio_retries = 15;

uint8_t channel = 124;
uint8_t address_bytes = 3;
uint8_t *address_1 = new uint8_t[3] { '1', 'N', 'o' };
uint8_t *address_2 = new uint8_t[3] { '2', 'N', 'o' };
uint8_t *address_3 = new uint8_t[3] { '3', 'N', 'o' };

uint8_t *radio_delay_tuned = new uint8_t[3] { 6, 2, 1 };
uint8_t *radio_retries_tuned = new uint8_t[3] { 15, 15, 15 };

void apply_settings(std::string &name, std::string &value) {

	if (name == "radio_delay") {
		radio_delay = atoi(value.c_str());
	} else if (name == "radio_retries") {
		radio_retries = atoi(value.c_str());
	} else if (name == "channel") {
		channel = atoi(value.c_str());
	} else if (name == "ce_pin") {
		ce_pin = atoi(value.c_str());
	} else if (name == "csn_pin") {
		csn_pin = atoi(value.c_str());
	} else if (name == "spi_speed") {
		spi_speed = atol(value.c_str());
	} else if (name == "variable_rate") {
		variable_rate = value == "yes";
	} else if (name == "max_radio_silence") {
		max_radio_silence = atoi(value.c_str());
	} else if (name == "primary") {
		primary = value == "yes";
	}else if (name == "auto_ack") {
		auto_ack = value == "yes";
	}

}

}

namespace Settings::DUAL_RF24 {

uint16_t max_radio_silence = 600;

bool variable_rate = true;

bool auto_ack = true;

int ce_0_pin = 24;
int csn_0_pin = 0;

int ce_1_pin = 25;
int csn_1_pin = 1;
//unsigned long spi_speed = 6000000; //(10000000-4000000);
//unsigned long spi_speed = 5300000; //(10000000-4000000);
uint32_t spi_speed = 1000000; //(10000000-4000000);

rf24_datarate_e data_rate = RF24_2MBPS;
rf24_pa_dbm_e radio_power = RF24_PA_LOW;
rf24_crclength_e crc_length = RF24_CRC_8;

uint8_t radio_delay = 0;
uint8_t radio_retries = 15;

uint8_t channel_0 = 2;
uint8_t channel_1 = 124;
uint8_t address_bytes = 3;
uint8_t *address_0_1 = new uint8_t[3] { '4', 'N', 'o' };
uint8_t *address_0_2 = new uint8_t[3] { '5', 'N', 'o' };
uint8_t *address_0_3 = new uint8_t[3] { '6', 'N', 'o' };

uint8_t *address_1_1 = new uint8_t[3] { '7', 'N', 'o' };
uint8_t *address_1_2 = new uint8_t[3] { '8', 'N', 'o' };
uint8_t *address_1_3 = new uint8_t[3] { '9', 'N', 'o' };

uint8_t *radio_delay_tuned = new uint8_t[3] { 6, 2, 1 };
uint8_t *radio_retries_tuned = new uint8_t[3] { 15, 15, 15 };

}

