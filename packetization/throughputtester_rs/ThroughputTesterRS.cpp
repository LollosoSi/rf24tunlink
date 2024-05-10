/*
 * ThroughputTesterRS.cpp
 *
 *  Created on: 11 Feb 2024
 *      Author: Andrea Roccaccino
 */

#include "ThroughputTesterRS.h"

#include <sstream>

ThroughputTesterRS::ThroughputTesterRS() :
		Telemetry("Throughput Tester") {

	returnvector = new std::string[15] { "0" };

	register_elements(new std::string[15] { "Sequential Packets",
			"Unsequential Packets", "Valid Packets", "Invalid Packets",
			"Total Bytes", "Valid Bytes", "Kbps", "Representation",
			"Valid Bits", "Flipped Bits", "Error bursts", "Packets with flips",
			"Bursts in a single packet", "Error representation", "Valid Kbps" }, 15);

	printf("Initializing RSCodec\n");
	rsc = new RSCodec();

	printf("Preparing send packets\n");
	for (unsigned int i = 0; i < 10; i++) {
		test_packets[i].size = 32;
		for (unsigned int j = 0; j < Settings::max_pkt_size; j++)
			if (Settings::test_bits) {
				test_packets[i].data[j] = 0x55;
			} else {
				test_packets[i].data[j] = i;
			}

	}

}

ThroughputTesterRS::~ThroughputTesterRS() {
	// TODO Auto-generated destructor stub
}

std::string* ThroughputTesterRS::telemetry_collect(const unsigned long delta) {

	std::stringstream bursts_representation;

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
			error_test *ets = new error_test;
			if (received_packets[i].size != 32) {

				printf("Wrong packet size: %i\n", received_packets[i].size);
				packets_invalid++;
			}
			int etest_bit_count = 0;
			int error_burst_count = 0;
			int separate_burst_count = 0;
			bool error_trig = 0;
			for (int j = 0; j < Settings::max_pkt_size; j++) {
				if (received_packets[i].data[j] == test_packets[0].data[j]) {
					bits_valid++;
					ets->representation[etest_bit_count++] = 0;

					if (error_burst_count > 0) {
						error_bursts[error_burst_count - 1]++;
						error_burst_count = 0;
						separate_burst_count++;
					}

				} else {
					error_burst_count++;
					bits_invalid++;
					ets->representation[etest_bit_count++] = 1;
					error_trig = 1;
				}

			}
			if (error_burst_count > 0) {
				error_bursts[error_burst_count - 1]++;
				ets->representation[etest_bit_count++] = 1;
				error_burst_count = 0;
				separate_burst_count++;
			}
			bursts_in_packet[separate_burst_count]++;

			symb[i] = received_packets[i].size != 32 ? 'z' :
						error_trig ? 'i' : 'o';
			bytes_in += received_packets[i].size;
			if (received_packets[i].size == Settings::max_pkt_size
					&& !error_trig)
				packets_valid++;
			else if (error_trig)
				packets_broken++;

			bool found = false;
			std::vector<error_test*>::iterator ite = error_structs.begin();
			while (ite != error_structs.end()) {
				bool equal = true;
				for (int i = 0; i < 32 * 8; i++) {
					if ((*ite)->representation[i] != ets->representation[i]) {
						equal = false;
						break;
					}

				}
				if (equal) {
					found = true;
					(*ite)->popularity++;
					break;
				}
				ite++;
			}
			if (found) {
				delete ets;
			} else {
				error_structs.push_back(ets);
			}

		}

		std::sort(begin(error_structs), end(error_structs),
				[](error_test *a, error_test *b) {
					return (a->popularity > b->popularity);
				});

		std::vector<error_test*>::iterator ite = error_structs.begin();
		bursts_representation << "\"";
		while (ite != error_structs.end()) {

			for (int i = 0; i < 32 * 8; i++) {
				if (i % 8 == 0)
					bursts_representation << " ";
				bursts_representation
						<< (((*ite)->representation[i] == 1) ? '_' : '|');
			}

			bursts_representation << "  hits: " << (*ite)->popularity << "\n";
			delete (*ite);
			ite++;
			//error_structs.pop_back();
		}
		bursts_representation << "\"";
		if (!error_structs.empty())
			error_structs.clear();

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
	error_bursts_output_string << "\"";
	for (int i = 0; i < 31; i++) {

		if (error_bursts[i] > 0) {
			found_one = 1;
			error_bursts_output_string << "Bursts of " << (int) (i + 1) << ": "
					<< error_bursts[i] << "\n";
		}

	}
	if (!found_one)
		error_bursts_output_string
				<< (Settings::test_bits ? "None!" : "Not tested");
	error_bursts_output_string << "\"";

	bool found_one_burst_in_packet = 0;
	bursts_in_packet_output_string << "\"";
	if (bursts_in_packet[0] > 0) {
		found_one_burst_in_packet = 1;
		bursts_in_packet_output_string << "Perfect packets: "
				<< bursts_in_packet[0] << "\n";
	}
	for (int i = 1; i < 32; i++) {

		if (bursts_in_packet[i] > 0) {
			found_one_burst_in_packet = 1;
			bursts_in_packet_output_string << "Had " << (int) (i) << " bursts: "
					<< bursts_in_packet[i] << "\n";
		}

	}
	if (!found_one_burst_in_packet)
		bursts_in_packet_output_string
				<< (Settings::test_bits ? "None!" : "Not tested");
	bursts_in_packet_output_string << "\"";

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
	returnvector[13] = bursts_representation.str();
	returnvector[14] = std::to_string((bursts_in_packet[0]*8*Settings::max_pkt_size)/1000.0);

	received_packets_length = 0;

	return (returnvector);
}

RadioPacket* ThroughputTesterRS::next_packet() {

	static uint8_t iterator = 10;

	if (iterator >= 10)
		iterator = 0;
	static RadioPacket *rp = new RadioPacket;
	rp->size = 32;

	static unsigned char *dt = new unsigned char[32];
	int s = 32;
	rsc->encode(&dt, s, (unsigned char*) (test_packets + (iterator++))->data,
			Settings::max_pkt_size);

	//printf("MSG: ");
	for (int i = 0; i < s; i++) {
		rp->data[i]=dt[i];
	//	printf("%i ", rp->data[i]);
	}
	//printf("\n");

	return rp;
}

bool ThroughputTesterRS::receive_packet(RadioPacket *rp) {

	if (received_packets_length >= 9999) {
		received_packets_length = 0;
		printf("Too many packets received and no request of telemetry\n");
	}

	static unsigned char *out = new unsigned char[32] { 0 };
	int outsize = Settings::max_pkt_size;

	rsc->decode(&out, outsize, rp->data, 32);
	//printf("MSG: ");
	//for (int i = 0; i < rp->size; i++) {
	//	printf("%i ", rp->data[i]);
	//}
	//printf("\n");
	//printf("DECODED: ");
	for (int i = 0; i < outsize; i++) {
		rp->data[i] = out[i];
	//	printf("%i ", out[i]);
	}
	//printf("\n");

	memcpy((unsigned char*) (received_packets + received_packets_length++),
			(unsigned char*) rp, 33);
//printf("n:%i : %i\n",(received_packets + received_packets_length-1)->data[0], (received_packets + received_packets_length-1)->size);

	return (true);
}
