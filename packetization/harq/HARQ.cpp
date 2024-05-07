/*
 * HARQ.cpp
 *
 *  Created on: 7 May 2024
 *      Author: Andrea Roccaccino
 */

#include "HARQ.h"

HARQ::HARQ() {
	if(last_packet_byte_pos_offset!=0){
		std::cout << "ERROR: bit positions aren't set correctly. Please verify the sum of id,segment,last_packet bits is 8.\n";
		exit(1);
	}

}

HARQ::~HARQ() {
	// TODO Auto-generated destructor stub
}

