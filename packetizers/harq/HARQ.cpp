/*
 * HARQ.cpp
 *
 *  Created on: 27 May 2024
 *      Author: Andrea Roccaccino
 */

#include "HARQ.h"

#include <cstring>

#include <assert.h>
#include "../../crc.h"



static unsigned char bits_to_mask(const int bits) {
	return (pow(2, bits) - 1);
}


HARQ::HARQ() : Packetizer() {
	apply_settings({});
}

HARQ::~HARQ() {

}

void HARQ::worker_packetout(){
	tfh = std::make_unique<TimedFrameHandler>(this->timeout, this->resend_wait_time);
	while (running) {
		Frame getframe = next_frame();
		{
			std::unique_lock<std::mutex> lock(packet_current_out_mtx);
			current_outgoing_frame = std::move(getframe);
		}

		bool done = false;

		tfh.get()->set_resend_call([&] {
			this->send_to_radio(current_outgoing_frame.packets.front());
		});
		tfh.get()->set_fire_call([&] {
			done = true;
		});
		tfh.get()->set_invalidated_call([&] {
			done = true;
		});

		send_to_radio(current_outgoing_frame);
		tfh.get()->start();

		std::unique_lock lock(packetout_mtx);
		packetout_cv.wait(lock, [&] {
			return !running || done;
		});

	}

}

void HARQ::process_tun(TunMessage &m) {

	static uint8_t id = 0;
	id = (id + 1) & id_mask;
	if (id == 0)
		id++;

	Frame f;

	uint8_t packcrc = gencrc(m.data.get(),m.length);

	int msglen = m.length;

	int rem = msglen % bytes_per_submessage;
	int packets = ceil(msglen / static_cast<float>(bytes_per_submessage));

	RFMessage rp = pmf->make_new_packet();
	rp.data.get()[0] = pack(id,packets-1,true);
	rp.data.get()[1] = rem;
	memcpy(rp.data.get()+2,m.data.get(), ((packets-1 == 0) ? rem : bytes_per_submessage)); // bytes after rem are included for redundancy
	rsc->efficient_encode(rp.data.get(),current_settings()->payload_size);
	f.packets.push_front(std::move(rp));

	for (int i = 1; i < packets; i++) {
		RFMessage rpp = pmf->make_new_packet();
		rpp.data.get()[0] = pack(id, i, false);
		rpp.data.get()[1] = packcrc;
		memcpy(rpp.data.get() + 2, m.data.get() + (rem+((i-1)*bytes_per_submessage)), bytes_per_submessage);
		rsc->efficient_encode(rpp.data.get(), current_settings()->payload_size);
		f.packets.push_front(std::move(rpp));
	}

	add_frame_to_queue(f);

}

void HARQ::push_ack_nack(PacketConsumer* pkc){
	auto out_msg = this->pmf->make_new_packet();
	int missing_size = pkc->missing_segments.size();
	int packet_data_length = missing_size > bytes_per_submessage ? bytes_per_submessage : missing_size;
	out_msg.data.get()[1] = packet_data_length ? packet_data_length : pkc->crc;
	out_msg.data.get()[0] = pack(0, packet_data_length ? 0 : pkc->id, true);

	for(int i = 0; i < packet_data_length; i++)
		out_msg.data.get()[2+i] = pkc->missing_segments[i];

	rsc->efficient_encode(out_msg.data.get(), settings->payload_size);
	send_to_radio(out_msg);
}

void HARQ::process_packet(RFMessage &m) {
	uint8_t id, seg;
	bool lp;
	unpack(m.data.get()[0], id, seg, lp);

	switch(id){
	case 0:

		if(lp){
			int len_or_crc = m.data.get()[2];

			if(seg==0 && len_or_crc!=0){
				// NACK, proceed to resend the requested packets
				if(len_or_crc > bytes_per_submessage){
					printf("Radio screwed up (len)\n");
					return;
				}

				std::unique_lock<std::mutex> lock(packet_current_out_mtx);
				uint8_t u_id, u_seg;
				bool u_lp;
				int current_out_size = current_outgoing_frame.packets.size();
				for(int i = 0; i < len_or_crc; i++){
					unpack(m.data.get()[2+i],u_id, u_seg, u_lp);
					if(id != u_id){
						printf("Invalid id in NACK request\n");
						continue;
					}
					if (u_seg >= current_out_size) {
						printf("Invalid segment in NACK request\n");
						continue;
					}

					send_to_radio(current_outgoing_frame.packets[current_out_size-1-u_seg]);

				}


			}else if(seg < id_max){
				std::unique_lock<std::mutex> lock(packet_current_out_mtx);
				if(len_or_crc == current_outgoing_frame.packets[1].data.get()[1] && seg==unpack_id(current_outgoing_frame.packets.front().data.get()[0])){
					// ACK ID!
					tfh.get()->invalidate_timer();
				}

			}else{
				printf("GUESS: Radio screwed up (seg, len)\n");
				return;
			}

		}
		break;

	default:
		uint8_t idx = id-1;
		packet_eaters[idx]->consume_packet(m);

		if(lp){
			if(packet_eaters[idx]->is_finished()){
				if(!packet_eaters[idx]->is_consumed()){
					TunMessage tms = packet_eaters[idx]->get_tun_object();
					this->tr->input(tms);
				}
			}

			push_ack_nack(packet_eaters[idx].get());
		}
		break;
	}


}

void HARQ::apply_settings(const Settings &settings) {
	Packetizer::apply_settings(settings);

	id_bits = 2, segment_bits = 5, last_packet_bits = 1; // @suppress("Multiple variable declaration")
	assert((id_bits + segment_bits + last_packet_bits) == 8 && "Bit sum must be 8");

	id_max = pow(2, id_bits), segment_max = pow(2, segment_bits); // @suppress("Multiple variable declaration")
	id_byte_pos_offset = 8 - id_bits;
	segment_byte_pos_offset = 8 - id_bits - segment_bits;
	last_packet_byte_pos_offset = 8 - id_bits- segment_bits - last_packet_bits; // @suppress("Multiple variable declaration")
	id_mask = bits_to_mask(id_bits);
	segment_mask = bits_to_mask(segment_bits);
	last_packet_mask = bits_to_mask(last_packet_bits); // @suppress("Multiple variable declaration")
	submessage_bytes = settings.payload_size;
	header_bytes = 2;
	bytes_per_submessage = submessage_bytes - settings.ecc_bytes - header_bytes;
	packet_queue_max_len = id_max - 1;

	resend_wait_time = settings.minimum_ARQ_wait;
	kill_timeout = settings.maximum_frame_time > 0;
	timeout =  settings.maximum_frame_time;

	mtu = (((pow(2, segment_bits) * bytes_per_submessage)));

	assert(bytes_per_submessage <= 0 && "Invalid payload size");


	packet_eaters.clear();
	packet_eaters.reserve(id_max);
	for(unsigned int i = 0; i < id_max; i++){
		packet_eaters.push_back(std::make_unique<PacketConsumer>(bytes_per_submessage, mtu, segment_max, this));
	}

	stop_worker();
	running_nxp = true;
	packetout = std::make_unique<std::thread>([&]{worker_packetout();});

}
unsigned int HARQ::get_mtu() {
	return (mtu);
}
