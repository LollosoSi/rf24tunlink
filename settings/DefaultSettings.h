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

bool display_telemetry = true;

char address[18] = { '\0' };
char destination[18] = { '\0' };
char netmask[18] = { '\0' };
char interface_name[6] = { 'a', 'r', 'o', 'c', 'c', '\0' };
unsigned int mtu = 9600;
bool control_packets = true;

uint16_t minimum_ARQ_wait = 10;

uint16_t maximum_frame_time = 30;

char *csv_out_filename = nullptr;	// CSV output, NULLPTR for no output
char csv_divider = ',';

bool test_bits = false;

uint8_t mode = 1; // 0 character stuffing, 1 selective repeat, 2 throughput tester

uint8_t radio_handler = 0; // 0 RF24, 1 Double RF24, 2 DualRF24 special handler, 3 LORA

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
	} else if (name == "display_telemetry") {
		display_telemetry = (value == "yes");
	} else if (name == "test_bits") {
		test_bits = (value == "yes");
	} else if (name == "mode") {
		mode = atoi(value.c_str());
	} else if (name == "maximum_frame_time") {
		maximum_frame_time = atol(value.c_str());
	} else if (name == "radio_handler") {
		radio_handler = atoi(value.c_str());
	} else if (name == "minimum_ARQ_wait") {
		minimum_ARQ_wait = atoi(value.c_str());
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
uint8_t *address_1 = new uint8_t[6] { '1', 'N', 'o', 'o', 'o', 0 };
uint8_t *address_2 = new uint8_t[6] { '2', 'N', 'o', 'o', 'o', 0 };
uint8_t *address_3 = new uint8_t[6] { '3', 'N', 'o', 'o', 'o', 0 };

uint8_t *radio_delay_tuned = new uint8_t[3] { 6, 2, 1 };
uint8_t *radio_retries_tuned = new uint8_t[3] { 15, 15, 15 };

bool ack_payloads = true;
bool dynamic_payloads = true;
int payload_size = 32;

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
	} else if (name == "auto_ack") {
		auto_ack = value == "yes";
	} else if (name == "crc_length") {
		crc_length = atoi(value.c_str()) == 0 ? RF24_CRC_DISABLED :
						atoi(value.c_str()) == 1 ? RF24_CRC_8 : RF24_CRC_16;
	} else if (name == "data_rate") {
		data_rate = atoi(value.c_str()) == 2 ? RF24_250KBPS :
					atoi(value.c_str()) == 1 ? RF24_2MBPS : RF24_1MBPS;
	} else if (name == "radio_power") {
		radio_power = atoi(value.c_str()) == 3 ? RF24_PA_MAX :
						atoi(value.c_str()) == 2 ? RF24_PA_HIGH :
						atoi(value.c_str()) == 1 ? RF24_PA_LOW : RF24_PA_MIN;
	} else if (name == "address_1") {
		for (unsigned int i = 0; i < address_bytes && i < value.length(); i++)
			address_1[i] = value[i];
	} else if (name == "address_2") {
		for (unsigned int i = 0; i < address_bytes && i < value.length(); i++)
			address_2[i] = value[i];
	} else if (name == "address_3") {
		for (unsigned int i = 0; i < address_bytes && i < value.length(); i++)
			address_3[i] = value[i];
	} else if (name == "ack_payloads") {
		ack_payloads = value == "yes";
	} else if (name == "dynamic_payloads") {
		dynamic_payloads = value == "yes";
	} else if (name == "payload_size") {
		int a = atoi(value.c_str());
		payload_size = a > 32 ? 32 : a < 0 ? 1 : a;
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
uint32_t spi_speed = 4000000; //(10000000-4000000);

rf24_datarate_e data_rate = RF24_2MBPS;
rf24_pa_dbm_e radio_power = RF24_PA_LOW;
rf24_crclength_e crc_length = RF24_CRC_8;

uint8_t radio_delay = 0;
uint8_t radio_retries = 15;

uint8_t channel_0 = 2;
uint8_t channel_1 = 124;
uint8_t address_bytes = 3;
uint8_t *address_0_1 = new uint8_t[6] { '4', 'N', 'o', 'o', 'o', 0 };
uint8_t *address_0_2 = new uint8_t[6] { '5', 'N', 'o', 'o', 'o', 0 };
uint8_t *address_0_3 = new uint8_t[6] { '6', 'N', 'o', 'o', 'o', 0 };

uint8_t *address_1_1 = new uint8_t[6] { '7', 'N', 'o', 'o', 'o', 0 };
uint8_t *address_1_2 = new uint8_t[6] { '8', 'N', 'o', 'o', 'o', 0 };
uint8_t *address_1_3 = new uint8_t[6] { '9', 'N', 'o', 'o', 'o', 0 };

uint8_t *radio_delay_tuned = new uint8_t[3] { 6, 2, 1 };
uint8_t *radio_retries_tuned = new uint8_t[3] { 15, 15, 15 };

void apply_settings(std::string &name, std::string &value) {

	if (name == "radio_delay") {
		radio_delay = atoi(value.c_str());
	} else if (name == "radio_retries") {
		radio_retries = atoi(value.c_str());
	} else if (name == "channel_0") {
		channel_0 = atoi(value.c_str());
	} else if (name == "ce_0_pin") {
		ce_0_pin = atoi(value.c_str());
	} else if (name == "csn_0_pin") {
		csn_0_pin = atoi(value.c_str());
	} else if (name == "channel_1") {
		channel_1 = atoi(value.c_str());
	} else if (name == "ce_1_pin") {
		ce_1_pin = atoi(value.c_str());
	} else if (name == "csn_1_pin") {
		csn_1_pin = atoi(value.c_str());
	} else if (name == "spi_speed") {
		spi_speed = atol(value.c_str());
	} else if (name == "variable_rate") {
		variable_rate = value == "yes";
	} else if (name == "max_radio_silence") {
		max_radio_silence = atoi(value.c_str());
	} else if (name == "auto_ack") {
		auto_ack = value == "yes";
	} else if (name == "crc_length") {
		crc_length = atoi(value.c_str()) == 0 ? RF24_CRC_DISABLED :
						atoi(value.c_str()) == 1 ? RF24_CRC_8 : RF24_CRC_16;
	} else if (name == "data_rate") {
		data_rate = atoi(value.c_str()) == 2 ? RF24_250KBPS :
					atoi(value.c_str()) == 1 ? RF24_2MBPS : RF24_1MBPS;
	} else if (name == "radio_power") {
		radio_power = atoi(value.c_str()) == 3 ? RF24_PA_MAX :
						atoi(value.c_str()) == 2 ? RF24_PA_HIGH :
						atoi(value.c_str()) == 1 ? RF24_PA_LOW : RF24_PA_MIN;
	} else if (name == "address_0_1") {
		for (unsigned int i = 0; i < address_bytes && i < value.length(); i++)
			address_0_1[i] = value[i];
	} else if (name == "address_0_2") {
		for (unsigned int i = 0; i < address_bytes && i < value.length(); i++)
			address_0_2[i] = value[i];
	} else if (name == "address_0_3") {
		for (unsigned int i = 0; i < address_bytes && i < value.length(); i++)
			address_0_3[i] = value[i];
	} else if (name == "address_1_1") {
		for (unsigned int i = 0; i < address_bytes && i < value.length(); i++)
			address_1_1[i] = value[i];
	} else if (name == "address_1_2") {
		for (unsigned int i = 0; i < address_bytes && i < value.length(); i++)
			address_1_2[i] = value[i];
	} else if (name == "address_1_3") {
		for (unsigned int i = 0; i < address_bytes && i < value.length(); i++)
			address_1_3[i] = value[i];
	}

}

}

namespace Settings::ReedSolomon {

unsigned int bits = 8;
unsigned int k = 20;
unsigned int nsym = 12;

void apply_settings(std::string &name, std::string &value) {

	if (name == "bits") {
		bits = atoi(value.c_str());
	} else if (name == "k") {
		k = atoi(value.c_str());
	} else if (name == "nsym") {
		nsym = atoi(value.c_str());
	}

}
}

