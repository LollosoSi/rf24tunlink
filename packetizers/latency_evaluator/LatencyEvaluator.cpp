/*
 * LatencyEvaluator.cpp
 *
 *  Created on: 31 May 2024
 *      Author: Andrea Roccaccino
 */

#include "LatencyEvaluator.h"

LatencyEvaluator::LatencyEvaluator() {

	
}

LatencyEvaluator::~LatencyEvaluator() {

}

void LatencyEvaluator::apply_settings(const Settings &settings){
	Packetizer::apply_settings(settings);
	//sendtime = current_millis();
	send_now();
}

void LatencyEvaluator::send_now() {
	if(!pmf)return;
	Frame f;

	for(int i = 0; i < 32; i++){
	RFMessage rfm = pmf->make_new_packet();
	rfm.data.get()[0] = 0;
	uint64_t sendtime = current_millis();
	uint8_t *split = (uint8_t*) &sendtime;
	for (int i = 0; i < 8; i++) {
		rfm.data.get()[1 + i] = split[i];
	}
		f.packets.emplace_back(move(rfm));
	}

	send_to_radio(f);
}

void LatencyEvaluator::process_tun(TunMessage &m){


}

void LatencyEvaluator::process_packet(RFMessage &m){

	if(current_settings()->primary){
		uint64_t now = current_millis();
		uint8_t received_time[8];
		for (int i = 0; i < 8; i++) {
			received_time[i] = m.data.get()[1 + i];
		}
		uint64_t* recv = (uint64_t*)received_time;
		uint64_t diff = (now-*recv);
		sendtime += diff;
		++counter;
		std::cout << "Detected latency: " << diff << "ms\tCount: " << counter << "\tAverage: " << (((*recv)-sendtime)/static_cast<double>(counter)) << std::endl;
		//this_thread::sleep_for(std::chrono::milliseconds(100));
		//send_now();

	}else{
		send_to_radio(m);
	}

}
