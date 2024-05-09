/*
 * HARQ.cpp
 *
 *  Created on: 7 May 2024
 *      Author: Andrea Roccaccino
 */

#include "HARQ.h"

HARQ::HARQ() :
		Telemetry("HARQ") {
	if (last_packet_byte_pos_offset != 0) {
		std::cout
				<< "ERROR: bit positions aren't set correctly. Please verify the sum of id,segment,last_packet bits is 8.\n";
		exit(1);
	}

	returnvector = new std::string[7] { std::to_string(0), std::to_string(0),
			std::to_string(0), std::to_string(0), std::to_string(0),
			std::to_string(0), std::to_string(0) };

	register_elements(new std::string[7] { "Fragments Received",
			"Fragments Sent", "Fragments Resent", "Frames Completed",
			"Control Fragments", "Bytes Discarded", "Dropped (timeout)" }, 7);

}

HARQ::~HARQ() {
	// TODO Auto-generated destructor stub
}

std::string* HARQ::telemetry_collect(const unsigned long delta) {

	returnvector[0] = (std::to_string(fragments_received));
	returnvector[1] = (std::to_string(fragments_sent));
	returnvector[2] = (std::to_string(fragments_resent));
	returnvector[3] = (std::to_string(frames_completed));
	returnvector[4] = (std::to_string(fragments_control));
	returnvector[5] = (std::to_string(bytes_discarded));
	returnvector[6] = (std::to_string(frames_dropped));

	//returnvector = { std::to_string(fragments_received), std::to_string( fragments_sent), std::to_string(fragments_resent), std::to_string(frames_completed), std::to_string(fragments_control) };

	fragments_received = fragments_sent = fragments_resent = frames_completed =
			fragments_control = bytes_discarded = frames_dropped = 0;

	return (returnvector);

}

