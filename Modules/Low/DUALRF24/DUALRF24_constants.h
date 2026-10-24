#pragma once

#include <assert.h>

#include <stdint.h>

#include "../../Processing/HARQ/HARQ_constants.h"
#include <RF24/RF24.h>

uint8_t radio_0_ce = 24;
uint8_t radio_0_csn = 0;
uint8_t radio_0_irq = 27;

uint8_t radio_1_ce = 26;
uint8_t radio_1_csn = 12;
uint8_t radio_1_irq = 6;

uint8_t radio_speed = RF24_2MBPS;
uint8_t radio_power = 0;

uint8_t channel_0 = 10;
uint8_t channel_1 = 20;

unsigned long spi_speed = 8000000;

uint8_t radio_delay = 1;
uint8_t radio_retries = 15;
bool auto_ack = false;
bool dynamic_payloads = false;
bool ack_payloads = false;

uint8_t CRC_LENGTH = 0; // 0 for disabled

const uint8_t payload_size = bytes_data + bytes_ECC;

static_assert(payload_size <= 32);

constexpr uint8_t address_bytes = 3;

uint8_t address_0[address_bytes] = {'n','d','1'};
uint8_t address_1[address_bytes] = {'n','d','2'};