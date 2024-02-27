/*
 * ThroughputTester.cpp
 *
 *  Created on: 11 Feb 2024
 *      Author: Andrea Roccaccino
 */

#include "ThroughputTester.h"

#include <sstream>

ThroughputTester::ThroughputTester() :
		Telemetry("Throughput Tester") {

	returnvector = new std::string[13] { "0" };

	register_elements(new std::string[13] { "Sequential Packets",
			"Unsequential Packets", "Valid Packets", "Invalid Packets",
			"Total Bytes", "Valid Bytes", "Kbps", "Representation",
			"Valid Bits", "Flipped Bits", "Error bursts", "Packets with flips", "Bursts in a single packet" }, 13);

	for (int i = 0; i < 10; i++) {
		test_packets[i].size = 32;
		for (int j = 0; j < 32; j++)
			if (Settings::test_bits) {
				test_packets[i].data[j] = 0x55;
			} else {
				test_packets[i].data[j] = i;
			}
	}

}

ThroughputTester::~ThroughputTester() {
	// TODO Auto-generated destructor stub
}

std::string* ThroughputTester::telemetry_collect(const unsigned long delta) {
	unsigned long packets_sequential = 0;
	unsigned long packets_unsequential = 0;
	unsigned long packets_valid = 0;
	unsigned long packets_invalid = 0;
	unsigned long bytes_in = 0;
	unsigned long bytes_valid_in = 0;

	unsigned long bits_valid = 0;
	unsigned long bits_invalid = 0;

	unsigned long packets_broken = 0;

	// Each position means m+1 length of error bursts: 1 error in a row, 2 errors in a row, etc
	unsigned long error_bursts[31] = { 0 };
	std::stringstream error_bursts_output_string;

	unsigned long bursts_in_packet[32] = { 0 };
	std::stringstream bursts_in_packet_output_string;

	unsigned int last_valid_packet = 0;

	char symb[10001] = { 0 };

	if (Settings::test_bits) {

		for (unsigned int i = 0; i < received_packets_length; i++) {

			if (received_packets[i].size != 32) {

				printf("Wrong packet size: %i\n", received_packets[i].size);
				packets_invalid++;
			}
			int error_burst_count = 0;
			int separate_burst_count = 0;
			bool error_trig = 0;
			for (int j = 0; j < received_packets[i].size; j++) {
				if (received_packets[i].data[j] == test_packets[0].data[j]) {
					bits_valid++;

					if (error_burst_count > 0) {
						error_bursts[error_burst_count - 1]++;
						error_burst_count = 0;
						separate_burst_count++;
					}

				} else {
					error_burst_count++;
					bits_invalid++;
					error_trig = 1;
				}

			}
			if (error_burst_count > 0) {
				error_bursts[error_burst_count - 1]++;
				error_burst_count = 0;
				separate_burst_count++;
			}
			bursts_in_packet[separate_burst_count]++;


			symb[i] = received_packets[i].size != 32 ? 'z' :
						error_trig ? 'i' : 'o';
			bytes_in += received_packets[i].size;
			if (received_packets[i].size == 32 && !error_trig)
				packets_valid++;
			else if(error_trig)
				packets_broken++;

		}

	} else {

		for (unsigned int i = 0; i < received_packets_length; i++) {

			if (is_valid(received_packets[i])) {
				packets_valid++;
				bytes_valid_in += received_packets[i].size;

				if (i > 0) {

					if (received_packets[i].data[0]
							== ((received_packets[last_valid_packet].data[0] + 1)
									% 10)) {
						packets_sequential++;
						symb[i] = 's';
					} else {
						packets_unsequential++;
						symb[i] = 'u';
					}

				} else {
					symb[i] = 'f';
				}

				last_valid_packet = i;
			} else {
				packets_invalid++;
				symb[i] = 'i';
			}

			bytes_in += received_packets[i].size;

		}
	}

	bool found_one = 0;
	error_bursts_output_string << "";
	for (int i = 0; i < 31; i++) {

		if (error_bursts[i] > 0) {
			found_one = 1;
			error_bursts_output_string << "Bursts of " << (int) (i + 1) << ": "
					<< error_bursts[i] << "  ";
		}

	}
	if (!found_one)
		error_bursts_output_string
				<< (Settings::test_bits ? " none!" : " not checked");

	bool found_one_burst_in_packet = 0;
	bursts_in_packet_output_string << "";
		for (int i = 0; i < 32; i++) {

			if (bursts_in_packet[i] > 0) {
				found_one_burst_in_packet = 1;
				bursts_in_packet_output_string << "Had " << (int) (i) << " separate bursts: "
						<< bursts_in_packet[i] << "  ";
			}

		}
		if (!found_one_burst_in_packet)
			bursts_in_packet_output_string
					<< (Settings::test_bits ? " none!" : " not checked");

	returnvector[0] = std::to_string(packets_sequential);
	returnvector[1] = std::to_string(packets_unsequential);
	returnvector[2] = std::to_string(packets_valid);
	returnvector[3] = std::to_string(packets_invalid);
	returnvector[4] = std::to_string(bytes_in);
	returnvector[5] = std::to_string(bytes_valid_in);
	returnvector[6] = std::to_string(bytes_in * 8 / 1000.0);
	returnvector[7] = std::string(symb);
	returnvector[8] = std::to_string(bits_valid);
	returnvector[9] = std::to_string(bits_invalid);
	returnvector[10] = error_bursts_output_string.str();
	returnvector[11] = std::to_string(packets_broken);
	returnvector[12] = bursts_in_packet_output_string.str();

	received_packets_length = 0;

	return (returnvector);
}

RadioPacket* ThroughputTester::next_packet() {

	static uint8_t iterator = 10;

	if (iterator >= 10)
		iterator = 0;

	return (test_packets + (iterator++));
}

bool ThroughputTester::receive_packet(RadioPacket *rp) {

	if (received_packets_length >= 9999) {
		received_packets_length = 0;
		printf("Too many packets received and no request of telemetry\n");
	}

	memcpy((unsigned char*) (received_packets + received_packets_length++),
			(unsigned char*) rp, 33);
//printf("n:%i : %i\n",(received_packets + received_packets_length-1)->data[0], (received_packets + received_packets_length-1)->size);

	return (true);
}
