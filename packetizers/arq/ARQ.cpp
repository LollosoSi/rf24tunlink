/*
 * ARQ.cpp
 *
 *  Created on: 27 May 2024
 *      Author: Andrea Roccaccino
 */

#include "ARQ.h"

#include <cstring>

#include <assert.h>
#include "../../crc.h"


static unsigned char bits_to_mask(const int bits) {
	return (pow(2, bits) - 1);
}


ARQ::ARQ() : Packetizer() {
#ifdef USE_PML
	PML = make_unique<ARQPacketIdentityLogger>(this);
#endif
	//apply_settings({});



}

ARQ::~ARQ() {

}
/*
inline void ARQ::add_frame_to_queue(Frame &f, unsigned int queue_id) {
	{
		std::unique_lock<std::mutex> lock(*packet_queues_locks[queue_id]);
		packet_queues[queue_id].get()->push(std::move(f));
	}
	packet_queues_cvs[queue_id]->notify_one();
}

inline ARQ::Frame ARQ::next_frame(unsigned int& queue_id) {
	std::unique_lock<std::mutex> lock(*packet_queues_locks[queue_id]);
	packet_queues_cvs[queue_id]->wait(lock, [&] {
		return (!running_nxfq || !packet_queues[queue_id].get()->empty());
	});
	if (!running_nxfq)
		return {};

	auto f = std::move(packet_queues[queue_id].get()->front());
	packet_queues[queue_id].get()->pop();
	return f;
}

inline bool ARQ::is_next_frame_available(unsigned int& queue_id) {
	return !packet_queues[queue_id].get()->empty();
}
*/

inline void ARQ::substitute_packet(unsigned int queue_id){
}

void ARQ::empty_packets_worker(){


	while(!pmf){
		if(!running_ept)
			return;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	Frame f;
	f.packets.reserve(3);
	for(unsigned int i = 0; i < 3; i++){
		RFMessage empty_message = pmf->make_new_packet();
		memset(empty_message.data.get(), 0, settings->payload_size);
		f.packets.emplace_back(std::move(empty_message));
	}

	etfh.apply_parameters(1000,this->empty_packet_delay);
	etfh.set_fire_call([&]{});
	etfh.set_invalidated_call([&]{});
	etfh.set_resend_call([&]{
		if(!running_ept){
			etfh.invalidate_timer();
			return;
		}

		send_to_radio(f);
	});
	while(running_ept){
		etfh.start();
	}

}

inline void ARQ::worker_packetout(){
	printf("Packetout starting\n");

	packet_outgoing_tfh.set_resend_call([this] {
#ifdef USE_PML
		PML->packet_out(current_packet_outgoing.packets.back());
	#endif
		etfh.reset_resend();
		this->send_to_radio(current_packet_outgoing.packets.back(), true);
	});

	packet_outgoing_tfh.set_fire_call([this] {
		//printf("Fired pack %d\n",queue_id);
		#ifdef USE_PML
		PML->expired(current_packet_outgoing);
		#endif


		printf("Packet EXPIRED\n");

	});
	packet_outgoing_tfh.set_invalidated_call([this] {
		//printf("Invalidated pack %d\n",queue_id);
		#ifdef USE_PML
		PML->acked(current_packet_outgoing);
		#endif

	});


	while (running_nxp) {

		Frame getframe = next_frame();
		if(getframe.packets.size() == 0)
			return;
		packet_outgoing_tfh.apply_parameters(this->timeout, getframe.resend_time);
		{
			std::unique_lock<std::mutex> lk(packet_outgoing_lock);
			current_packet_outgoing = std::move(getframe);
		}

		etfh.reset_resend();
		send_to_radio(current_packet_outgoing);
		packet_outgoing_tfh.start();



	}
	printf("Packetout exiting\n");
}

inline void print_hex(uint8_t* d, int l){
	for (int i = 0; i < l; i++) {
		std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0')
				<< static_cast<int>(d[i]) << " ";
	}
	std::cout << std::endl;
}

inline void ARQ::process_tun(TunMessage &m) {

	if(m.length == 0)
		return;

	static uint8_t id = 0;
	id = (id + 1) & id_mask;
	if (id == 0)
		id++;

	Frame f;

	uint8_t packcrc = gencrc(m.data.get(),m.length);
	f.metadata=packcrc;
	int msglen = m.length;

	int rem = msglen % bytes_per_submessage;
	if(rem==0)
		rem = bytes_per_submessage;
	int packets = std::ceil(static_cast<double>(msglen) / static_cast<double>(bytes_per_submessage));

	f.resend_time = use_estimate ? ceil(estimate_resend_wait_time * packets) : this->resend_wait_time;
	f.packets.reserve(packets);

	//printf("MSGLEN %d, REM %d, PACKETS %d, CRC %d\n", msglen, rem, packets, packcrc);



	for (int i = packets-1; i > 0; i--) {
		RFMessage rpp = pmf->make_new_packet();
		rpp.data.get()[0] = pack(id, i, false);
		rpp.data.get()[1] = packcrc;
		memcpy(rpp.data.get() + 2, m.data.get() + (rem+((i-1)*bytes_per_submessage)), bytes_per_submessage);
		rsc->efficient_encode(rpp.data.get(), current_settings()->payload_size);
		f.packets.emplace_back(std::move(rpp));
	}

	RFMessage rp = pmf->make_new_packet();
	rp.data.get()[0] = pack(id,packets-1,true);
	rp.data.get()[1] = rem;
	memcpy(rp.data.get()+2,m.data.get(), rem);
	rsc->efficient_encode(rp.data.get(),current_settings()->payload_size);
	f.packets.emplace_back(std::move(rp));

#ifdef USE_PML
	PML->tun_in_to_out(m, f);
#endif
	add_frame_to_queue(f);

}

inline void ARQ::push_ack_nack(ARQPacketConsumer* pkc){
	auto out_msg = this->pmf->make_new_packet();
	int missing_size = pkc->missing_segments.size();
	int packet_data_length = missing_size > bytes_per_submessage ? bytes_per_submessage : missing_size;
	bool is_nack = packet_data_length != 0;
	out_msg.data.get()[1] = is_nack ? packet_data_length : pkc->crc;
	out_msg.data.get()[0] = pack(0, is_nack ? 0 : pkc->id, true);
	//printf("Pushing %s for %d\t", is_nack ? "NACK" : "ACK", pkc->id);

	for(int i = 0; i < packet_data_length; i++)
		out_msg.data.get()[2+i] = pkc->missing_segments[i];
#ifdef USE_PML
	PML->packet_out(out_msg);
#endif

	rsc->efficient_encode(out_msg.data.get(), settings->payload_size);

	etfh.reset_resend();
	send_to_radio(out_msg);
}

inline void ARQ::process_packet(RFMessage &m) {
	int e_count = 0;
	if(!rsc->efficient_decode(m.data.get(),settings->payload_size, &e_count)){
		return;
	}
#ifdef USE_PML
	PML->packet_in(m);
#endif


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

				unsigned int p_id = unpack_id(m.data.get()[2]);

				std::unique_lock<std::mutex> lk(packet_outgoing_lock);
				uint8_t u_id, u_seg;
				bool u_lp;
				int current_out_size = current_packet_outgoing.packets.size();
				if(current_out_size == 0){
					printf("No packet is outgoing, quitting\n");
					return;
				}

				if (p_id != unpack_id(current_packet_outgoing.packets.front().data[0])) {
					printf("Requested packet not in consideration\n");
					return;
				}



				for(int i = 0; i < len_or_crc; i++){
					unpack(m.data.get()[2+i],u_id, u_seg, u_lp);
					if(unpack_id(current_packet_outgoing.packets.front().data[0]) != u_id){
						printf("Invalid id in NACK request\n");
						continue;
					}
					if (u_seg >= current_out_size) {
						printf("Invalid segment in NACK request\n");
						continue;
					}
					//printf("Rsnd: id %d, seg %d\t",u_id,u_seg);
#ifdef USE_PML
					PML->recalled(current_packet_outgoing.packets[current_out_size-1-u_seg]);
#endif
					etfh.reset_resend();
					send_to_radio(current_packet_outgoing.packets[current_out_size-1-u_seg]);
				}

				//if (packet_outgoing_tfh[queue_id])
				if (!packet_outgoing_tfh.finished())
					packet_outgoing_tfh.reset_resend();



			}else if(seg < id_max){

				std::unique_lock<std::mutex> lk(packet_outgoing_lock);
				//bool c1 = (current_packet_outgoing.packets.size() == 1 ? true : (len_or_crc == current_packet_outgoing.packets[1].data.get()[1]));
				//c1 = true;
				bool c2 = seg==unpack_id(current_packet_outgoing.packets.front().data.get()[0]);
				if(/*c1 &&*/ c2){
					// ACK ID!
					//printf("Ack! %d\t",seg);
					//if (packet_outgoing_tfh[queue_id])
					if (!packet_outgoing_tfh.finished())
						packet_outgoing_tfh.invalidate_timer();
				}//else{
				//   printf("This is an ACK, but we couldn't verify either CRC: %d, or id: %d, %d -> %d\n",c1,seg,unpack_id(current_packet_outgoing.packets.front().data.get()[0]),c2);
				//}

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
#ifdef USE_PML
					PML->tun_out_to_in(tms);
#endif
					if(this->tun->input(tms)){
						//printf("\tACCEPTED\t");
						this->trigger_led();
					}else
						printf("Packet REJECTED\n");
				}
			}else{
				//printf("Not ready\t");
			}
			push_ack_nack(packet_eaters[idx].get());
		}
		break;
	}

	//printf("\n");

}

inline void ARQ::apply_settings(const Settings &settings) {

	static std::mutex setting_mtx;
	std::unique_lock<std::mutex> settingslk(setting_mtx);

	Packetizer::apply_settings(settings);
#ifdef USE_PML
	PML->register_pmf(pmf);
#endif

	use_estimate = settings.use_tuned_ARQ_wait;
	estimate_resend_wait_time = settings.tuned_ARQ_wait_singlepacket;

	id_bits = settings.bits_id, segment_bits = settings.bits_segment, last_packet_bits = settings.bits_lastpacketmarker; // @suppress("Multiple variable declaration")
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

	empty_packet_delay = settings.empty_packet_delay;

	stop_worker();
	running_nxp = true;
	running_nxfq = true;
	running_ept = true;

	packet_eaters.clear();
	packet_eaters.reserve(packet_queue_max_len);

	for (unsigned int pippo = 0; pippo < packet_queue_max_len; ++pippo) {
		packet_eaters.push_back(std::make_unique<ARQPacketConsumer>(bytes_per_submessage, mtu,segment_max, this));
	}

	frame_thread = std::thread([this]() {
		std::string s = "PackOut";
		prctl(PR_SET_NAME, s.c_str(), 0, 0, 0);
		worker_packetout();
	});

	if(settings.primary)
	empty_packets_thread = std::thread([this]() {
		std::string s = "EmpPackOut";
		prctl(PR_SET_NAME, s.c_str(), 0, 0, 0);
		empty_packets_worker();
	});



}
inline unsigned int ARQ::get_mtu() {
	return (mtu);
}
