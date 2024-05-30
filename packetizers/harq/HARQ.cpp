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
	printf("Packetout starting\n");
	while (running_nxp) {
		Frame getframe = next_frame();
		if(getframe.packets.size() == 0)
			continue;

		{
			std::unique_lock<std::mutex> lock(packet_current_out_mtx);
			current_outgoing_frame = std::move(getframe);
		}
		tfh = std::make_unique<TimedFrameHandler>(this->timeout, this->resend_wait_time);


		bool done = false;

		tfh.get()->set_resend_call([&] {
			this->send_to_radio(current_outgoing_frame.packets.back(), false);
		});
		tfh.get()->set_fire_call([&] {
			{
			std::unique_lock<std::mutex> lock(packet_current_out_mtx);
			done = true;
			printf("Packet EXPIRED\n");
			}
			packetout_cv.notify_one();

		});
		tfh.get()->set_invalidated_call([&] {
			{
			std::unique_lock<std::mutex> lock(packet_current_out_mtx);
			done = true;
			//printf("Packet ACKED\n");
			}
			packetout_cv.notify_one();

		});

		tfh.get()->start();
		send_to_radio(current_outgoing_frame);

		std::unique_lock lock(packet_current_out_mtx);
		packetout_cv.wait(lock, [&] {
			return !running_nxp || done;
		});
		tfh.reset();
	}
	printf("Packetout exiting\n");
}

void print_hex(uint8_t* d, int l){
	for (int i = 0; i < l; i++) {
		std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0')
				<< static_cast<int>(d[i]) << " ";
	}
	std::cout << std::endl;
}

void HARQ::process_tun(TunMessage &m) {

	if(m.length == 0)
		return;

	static uint8_t id = 0;
	id = (id + 1) & id_mask;
	if (id == 0)
		id++;

	Frame f;

	uint8_t packcrc = gencrc(m.data.get(),m.length);

	int msglen = m.length;

	int rem = msglen % bytes_per_submessage;
	if(rem==0)
		rem = bytes_per_submessage;
	int packets = std::ceil(static_cast<double>(msglen) / static_cast<double>(bytes_per_submessage));

	printf("MSGLEN %d, REM %d, PACKETS %d, CRC %d\n", msglen, rem, packets, packcrc);

	RFMessage rp = pmf->make_new_packet();
	rp.data.get()[0] = pack(id,packets-1,true);
	rp.data.get()[1] = rem;
	memcpy(rp.data.get()+2,m.data.get(), rem);
	printf("Seg 0 printout: ");
	print_hex(rp.data.get()+2,rem);
	printf("\n");
	rsc->efficient_encode(rp.data.get(),current_settings()->payload_size);
	f.packets.push_front(std::move(rp));

	for (int i = 1; i < packets; i++) {
		RFMessage rpp = pmf->make_new_packet();
		rpp.data.get()[0] = pack(id, i, false);
		rpp.data.get()[1] = packcrc;
		memcpy(rpp.data.get() + 2, m.data.get() + (rem+((i-1)*bytes_per_submessage)), bytes_per_submessage);
		printf("Seg %d printout: ", i);
		print_hex(rpp.data.get()+2, bytes_per_submessage);
		printf("\n");
		rsc->efficient_encode(rpp.data.get(), current_settings()->payload_size);
		f.packets.push_front(std::move(rpp));
	}

	add_frame_to_queue(f);

}

void HARQ::push_ack_nack(PacketConsumer* pkc){
	auto out_msg = this->pmf->make_new_packet();
	int missing_size = pkc->missing_segments.size();
	int packet_data_length = missing_size > bytes_per_submessage ? bytes_per_submessage : missing_size;
	bool is_nack = packet_data_length != 0;
	out_msg.data.get()[1] = is_nack ? packet_data_length : pkc->crc;
	out_msg.data.get()[0] = pack(0, is_nack ? 0 : pkc->id, true);
	printf("Pushing %s for %d\t", is_nack ? "NACK" : "ACK", pkc->id);

	for(int i = 0; i < packet_data_length; i++)
		out_msg.data.get()[2+i] = pkc->missing_segments[i];


	rsc->efficient_encode(out_msg.data.get(), settings->payload_size);
	send_to_radio(out_msg);
}

void HARQ::process_packet(RFMessage &m) {
	int e_count = 0;
	if(!rsc->efficient_decode(m.data.get(),settings->payload_size, &e_count)){
		return;
	}


	uint8_t id, seg;
	bool lp;
	unpack(m.data.get()[0], id, seg, lp);

	//printf("Decoded; id %d, seg %d, lp %d\t",id,seg,lp);

	switch(id){
	case 0:

		if(lp){
			int len_or_crc = m.data.get()[1];

			if(seg==0 && len_or_crc!=0){
				// NACK, proceed to resend the requested packets
				if(len_or_crc > bytes_per_submessage){
					printf("Radio screwed up (len %d)\n",len_or_crc);
					return;
				}

				std::unique_lock<std::mutex> lock(packet_current_out_mtx);
				uint8_t u_id, u_seg;
				bool u_lp;
				int current_out_size = current_outgoing_frame.packets.size();
				if(current_out_size == 0){
					printf("No packet is outgoing, quitting\n");
					return;
				}
				for(int i = 0; i < len_or_crc; i++){
					unpack(m.data.get()[2+i],u_id, u_seg, u_lp);
					if(unpack_id(current_outgoing_frame.packets.front().data.get()[0]) != u_id){
						printf("Invalid id in NACK request\n");
						continue;
					}
					if (u_seg >= current_out_size) {
						printf("Invalid segment in NACK request\n");
						continue;
					}
					printf("Rsnd: id %d, seg %d\t",u_id,u_seg);
					send_to_radio(current_outgoing_frame.packets[current_out_size-1-u_seg]);

				}


			}else if(seg < id_max){
				std::unique_lock<std::mutex> lock(packet_current_out_mtx);
				bool c1 = (current_outgoing_frame.packets.size() == 1 ? true : (len_or_crc == current_outgoing_frame.packets[1].data.get()[1]));
				c1 = 1;
				bool c2 = seg==unpack_id(current_outgoing_frame.packets.front().data.get()[0]);
				if(c1 && c2){
					// ACK ID!
					//printf("Ack! %d\t",seg);
					if (tfh)
						if (!tfh.get()->finished())
							tfh.get()->invalidate_timer();
				}else{
					//printf("This is an ACK, but we couldn't verify either CRC: %d, or id: %d, %d -> %d\n",c1,seg,unpack_id(current_outgoing_frame.packets.front().data.get()[0]),c2);
				}

			}else{
				printf("GUESS: Radio screwed up (seg, len %d)\n", len_or_crc);
				return;
			}

		}
		break;

	default:
		uint8_t idx = id-1;
		packet_eaters[idx]->consume_packet(m);

		if(lp){
			if(packet_eaters[idx]->is_finished()){
				//printf("Packet finished\t");
				if(!packet_eaters[idx]->is_consumed()){
					//printf("Writing to TUN\t");
					TunMessage tms = packet_eaters[idx]->get_tun_object();
					//printf("Message received: ");
					//print_hex(tms.data.get(), tms.length);
					//printf("Verify CRC: %d : %d -> %d",gencrc(tms.data.get(),tms.length),packet_eaters[idx]->crc,packet_eaters[idx]->crc==gencrc(tms.data.get(),tms.length));
					if(this->tun->input(tms))
						printf("\tACCEPTED\t");
					else
						printf("\tREJECTED\t");
				}
			}else{
				//printf("Not ready\t");
			}
			push_ack_nack(packet_eaters[idx].get());
		}
		break;
	}

	printf("\n");

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

	assert(bytes_per_submessage > 0 && "Invalid payload size");


	packet_eaters.clear();
	packet_eaters.reserve(id_max);
	for(unsigned int i = 0; i < id_max; i++){
		packet_eaters.push_back(std::make_unique<PacketConsumer>(bytes_per_submessage, mtu, segment_max, this));
	}

	stop_worker();
	running_nxp = true;
	packetout = std::make_unique<std::thread>([&]{prctl(PR_SET_NAME, "Packetout", 0, 0, 0);worker_packetout();});

}
unsigned int HARQ::get_mtu() {
	return (mtu);
}
