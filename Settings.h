/*
 * Settings.h
 *
 *  Created on: Oct 11, 2023
 *      Author: andrea
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <RF24/RF24.h>

#include <bitset>
#include <iostream>

#include <unistd.h>
#include <cmath>

#pragma pack(push, 1)
typedef struct {
	uint8_t info = 0;
	uint8_t data[31] = { 0 };
} radiopacket;
#pragma pack(pop)

namespace Settings {

static bool use_empty_packets = 1;
static bool resuscitate_packets = 0;

static const int maxbadpackets = 30;

static bool auto_ack = 1;
static bool dynamic_payloads = 1;
static bool ack_payloads = 1;

static int ce_pin = 25;
static uint8_t channel = (uint8_t)(110);

static uint8_t radio_delay = 3;
static uint8_t radio_retries = 2;


static uint8_t payload_size = 32;
static rf24_datarate_e data_rate = RF24_2MBPS;
static rf24_pa_dbm_e power = RF24_PA_MAX;
static rf24_crclength_e crclen = RF24_CRC_8;



static const uint8_t addressbytes = 3;

static uint8_t address1[addressbytes + 1] = "1No";
static uint8_t address2[addressbytes + 1] = "2No";

static int retrylimit = 2;
static const int buffersize = 3;

static const int rolling_average_size = 100;

static const int segmentbits = 5;
static const int idbits = 8-1-segmentbits;

static const int zeropacketlimit = 3;

static const uint8_t precisionmask = 0x80;
static const uint8_t segmask = pow(2,segmentbits)-1, idmask = pow(2,idbits)-1; // Will be filled at runtime
static const uint8_t segmaskwildcard = (pow(2,8)-1)-idmask, idmaskwildcard = (pow(2,8)-1)-segmask-precisionmask;

static const unsigned int mtu = 31*(pow(2,segmentbits)-1);

inline void apply_settings(int ce_pin, uint8_t channel, uint8_t radio_delay,
		uint8_t radio_retries, rf24_crclength_e crclen, bool auto_ack,
		bool dynamic_payloads, bool ack_payloads, uint8_t payload_size,
		rf24_datarate_e data_rate, rf24_pa_dbm_e power) {

	Settings::ce_pin = ce_pin;
	Settings::channel = channel;
	Settings::radio_delay = radio_delay;
	Settings::radio_retries = radio_retries;
	Settings::crclen = crclen;
	Settings::auto_ack = auto_ack;
	Settings::dynamic_payloads = dynamic_payloads;
	Settings::ack_payloads = ack_payloads;
	Settings::payload_size = payload_size;
	Settings::data_rate = data_rate;
	Settings::power = power;

}

}

static uint8_t extractpacketprecision(uint8_t number) {
	return (number>> 7) & 0x01;
}
static uint8_t extractpacketprecision(radiopacket *rp) {
	return extractpacketprecision(rp->info);
}
static uint8_t extractpacketid(uint8_t number) {
	return (number>> Settings::segmentbits) & Settings::idmask;
}
static uint8_t extractpacketid(radiopacket *rp) {
	return extractpacketid(rp->info);
}
static uint8_t extractpacketsegment(uint8_t number) {
	return number & Settings::segmask;
}
static uint8_t extractpacketsegment(radiopacket *rp) {
	return extractpacketsegment(rp->info);
}

static uint8_t makeidsegment(uint8_t id, uint8_t segment, bool precision){
	return (uint8_t) (((precision<<7)& Settings::precisionmask) | ((id << Settings::segmentbits) & ((uint8_t) Settings::segmaskwildcard))| ((segment) & Settings::segmask));
}

static uint8_t gencrc(uint8_t *data, size_t len) {
	uint8_t crc = 0xff;
	size_t i, j;
	for (i = 0; i < len; i++) {
		crc ^= data[i];
		for (j = 0; j < 8; j++) {
			if ((crc & 0x80) != 0)
				crc = (uint8_t)((crc << 1) ^ 0x31);
			else
				crc <<= 1;
		}
	}
	return crc;
}


// Unused, left for compatibility
static uint8_t genpackcrc(radiopacket *rp) {
	return gencrc((uint8_t*) rp, 31);
}

// Unused, left for compatibility
static bool validatepackcrc(radiopacket *rp) {
	return genpackcrc(rp) == rp->data[30];
}



static char radioppppprintchararray(uint8_t *data, int len) {

	std::cout << "\n";
	for (int i = 0; i < len; i++) {
		std::cout << (int) data[i] << "\t";
		if (((i + 1) % 8 == 0) && (i + 1) != 0)
			std::cout << "\n";
	}
	std::cout << "\n";

	/*
	 std::cout << "\n";
	 for (int i = 0; i < len; i++) {
	 std::bitset<8> x(data[i]);
	 std::cout << x << " ";
	 if (((i + 1) % 8 == 0) && (i + 1) != 0)
	 std::cout << "\n";
	 }
	 std::cout << "\n";
	 */
	return ' ';
}

class rollingaverage {

private:
	double values[Settings::rolling_average_size] = { 0.0 };
	int cursor = 0;

	void incrementCursor() {
		cursor = (cursor + 1) % Settings::rolling_average_size;
	}

public:

	void addValue(double value) {
		values[cursor] = value;
		incrementCursor();
	}

	double getAverage() {
		static double sum;
		sum=0;
		for (int i = 0; i < Settings::rolling_average_size; i++) {
			sum += values[i];
		}
		return (double)sum / (double)Settings::rolling_average_size;
	}

};

#include <chrono>
#include <time.h>      // CLOCK_MONOTONIC_RAW, timespec, clock_gettime()
#include <ctime>       // time()
#include <cmath>
#include <vector>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

using namespace std::chrono;
static uint64_t currentMillis() {
	return duration_cast < milliseconds
			> (system_clock::now().time_since_epoch()).count();
}
using namespace std;

#endif
