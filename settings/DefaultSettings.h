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

}

namespace Settings::RF24 {

uint16_t max_radio_silence = 600;

bool variable_rate = true;

int ce_pin = 25;
int csn_pin = 0;
//unsigned long spi_speed = 6000000; //(10000000-4000000);
//unsigned long spi_speed = 5300000; //(10000000-4000000);
unsigned long spi_speed = 5200000; //(10000000-4000000);


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

uint8_t *radio_delay_tuned = new uint8_t[3]{ 6, 2, 1 };
uint8_t *radio_retries_tuned = new uint8_t[3]{ 15, 15, 15 };



}
