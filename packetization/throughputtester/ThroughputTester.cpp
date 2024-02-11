/*
 * ThroughputTester.cpp
 *
 *  Created on: 11 Feb 2024
 *      Author: Andrea Roccaccino
 */



#include "ThroughputTester.h"

ThroughputTester::ThroughputTester() : Telemetry("Throughput Tester") {


	returnvector = new std::string[8] { "0" };

		register_elements(new std::string[8] { "Sequential Packets",
				"Unsequential Packets", "Valid Packets", "Invalid Packets",
				"Total Bytes", "Valid Bytes", "Kbps", "Representation" }, 8);


	for (int i = 0; i < 10; i++) {
		test_packets[i].size = 32;
		for (int j = 0; j < 32; j++)
			test_packets[i].data[j] = i;
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

	unsigned int last_valid_packet = 0;

	char symb[10001] = {0};

	for (unsigned int i = 0; i < received_packets_length; i++) {

		if (is_valid(received_packets[i])) {
			packets_valid++;
			bytes_valid_in += received_packets[i].size;

			if(i>0){

				if(received_packets[i].data[0]==((received_packets[last_valid_packet].data[0]+1)%10)){
					packets_sequential++;
					symb[i] = 's';
				}else{
					packets_unsequential++;
					symb[i] = 'u';
				}

			}else{
				symb[i] = 'f';
			}

			last_valid_packet = i;
		} else {
			packets_invalid++;
			symb[i] = 'i';
		}

		bytes_in += received_packets[i].size;

	}


	returnvector[0] = std::to_string(packets_sequential);
	returnvector[1] = std::to_string(packets_unsequential);
	returnvector[2] = std::to_string(packets_valid);
	returnvector[3] = std::to_string(packets_invalid);
	returnvector[4] = std::to_string(bytes_in);
	returnvector[5] = std::to_string(bytes_valid_in);
	returnvector[6] = std::to_string(bytes_in*8/1000.0);
	returnvector[7] = std::string(symb);

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

	memcpy((unsigned char*)(received_packets + received_packets_length++), (unsigned char*)rp, 33);
	//printf("n:%i : %i\n",(received_packets + received_packets_length-1)->data[0], (received_packets + received_packets_length-1)->size);

	return (true);
}
