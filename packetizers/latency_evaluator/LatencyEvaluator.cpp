/*
 * LatencyEvaluator.cpp
 *
 *  Created on: 31 May 2024
 *      Author: Andrea Roccaccino
 */

#include "LatencyEvaluator.h"

#include <inttypes.h>

LatencyEvaluator::LatencyEvaluator() {

	
}

LatencyEvaluator::~LatencyEvaluator() {

}

void LatencyEvaluator::apply_settings(const Settings &settings) {
	Packetizer::apply_settings(settings);
	//sendtime = current_millis();


	static unique_ptr<std::thread> tt, t;

	//this_thread::sleep_for(std::chrono::milliseconds(1500));



	sent = 0;
	received = 0;
	running = false;

	if (tt)
		if(tt->joinable()){
			tt->join();
		}
	tt.reset();
	running = false;
	if (t)
		if(t->joinable()){
			t->join();
		}
	t.reset();

	running = true;

	tt = make_unique<std::thread>([this] {

		uint64_t start = current_millis();
		uint64_t last_recv = 0, last_sent = 0;
		while (running) {
			uint32_t diffr = received - last_recv, diffs = sent - last_sent;
			float rate_r = diffr * 32 * 8 / 100.0f, rate_s = diffs * 32 * 8 /100.0f ;
			float avg_lat = (diffsum / counter);
			printf("Received %" PRIu64 ", sent: %" PRIu64", rate_r: %f, rate_s: %f kbps, avg lat: %f ms\n",
					received, sent, rate_r, rate_s, avg_lat);
			last_sent = sent;
			last_recv = received;
			this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
	});

	if (current_settings()->primary) {
		t = make_unique<std::thread>([this] {

			Frame fg = build_out();
			while(fg.packets.size() == 0){
				fg = build_out();
			}
			uint64_t j = 0;
			while ((j++ < 30000) && running) {
				send_now(fg);
				//this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			printf("Write thread exiting. Reason: %d - %d\n",(j >= 30000), !running);
		});

	}

}

constexpr unsigned int packets_in_frame = 500;

Packetizer<TunMessage, RFMessage>::Frame LatencyEvaluator::build_out() {
	if (!pmf){
			Frame ff;
			ff.packets.clear();
			return ff;
		}
	Frame f;
	f.packets.reserve(packets_in_frame);

	for (int i = 0; i < packets_in_frame; i++) {
		RFMessage rfm = pmf->make_new_packet();
		rfm.data.get()[0] = 0;
		rfm.length = 32;
		f.packets.emplace_back(move(rfm));
	}
	//sent += 5;
	return f;
}

void LatencyEvaluator::send_now(Frame &f) {
	if(f.packets.size() == 0)
		return;
	if(f.packets[0].length != 32)
		return;


	uint64_t sendtime = current_millis();
	uint8_t *split = (uint8_t*) &sendtime;
	for (int i = 0; i < 8; i++) {
		f.packets[0].data.get()[1 + i] = split[i];
	}

	sent+=f.packets.size();

	send_to_radio(f);
}

void LatencyEvaluator::process_tun(TunMessage &m){


}

void LatencyEvaluator::process_packet(RFMessage &m){


	if(current_settings()->primary){
		received++;
		uint64_t now = current_millis();
		uint8_t received_time[8];
		for (int i = 0; i < 8; i++) {
			received_time[i] = m.data.get()[1 + i];
		}
		uint64_t* recv = (uint64_t*)received_time;
		uint64_t diff = (now-*recv);
		if(diff > 5000)
			return;
		sendtime += diff;
		diffsum += diff;
		//++counter;

		//std::cout << "Detected latency: " << diff << "ms\tCount: " << counter << "\tAverage: " << (((*recv)-sendtime)/static_cast<double>(counter)) << std::endl;
		//this_thread::sleep_for(std::chrono::milliseconds(100));
		//send_now();

	}else{
		received++;
		send_to_radio(m);
	}

}
