/*
 * HARQ.cpp
 *
 *  Created on: 7 May 2024
 *      Author: Andrea Roccaccino
 */

#include "HARQ.h"
#include "../../settings/Settings.h"

HARQ::HARQ() :
		Telemetry("HARQ") {
	if (last_packet_byte_pos_offset != 0) {
		std::cout
				<< "ERROR: bit positions aren't set correctly. Please verify the sum of id,segment,last_packet bits is 8.\n";
		exit(1);
	}



	returnvector = new std::string[10] { std::to_string(0), std::to_string(0),
			std::to_string(0), std::to_string(0), std::to_string(0),
			std::to_string(0), std::to_string(0), std::to_string(0),
			std::to_string(0), std::to_string(0) };

	register_elements(new std::string[10] { "Fragments Received",
			"Fragments Sent", "Fragments Resent", "Frames Completed",
			"Control Fragments", "Bytes Discarded", "Dropped (interface)",  "Timed out", "Retired", "NACKs retired" },
			10);

	//rsc = RSCodec();

	id_bits = 1, segment_bits = 6, last_packet_bits = 1; // @suppress("Multiple variable declaration")
	id_max = pow(2, id_bits), segment_max = pow(2, segment_bits); // @suppress("Multiple variable declaration")
	id_byte_pos_offset = 8 - id_bits;
	segment_byte_pos_offset = 8- id_bits - segment_bits;
	last_packet_byte_pos_offset = 8 - id_bits- segment_bits - last_packet_bits; // @suppress("Multiple variable declaration")
	id_mask = bits_to_mask(id_bits);
	segment_mask = bits_to_mask(segment_bits);
	last_packet_mask = bits_to_mask(last_packet_bits); // @suppress("Multiple variable declaration")
	submessage_bytes = 32;
	header_bytes = 1;
	length_last_packet_bytes = 1;
	bytes_per_submessage = submessage_bytes - Settings::ReedSolomon::nsym - header_bytes;
	packet_queue_max_len = id_max - 1;

	resend_wait_time = Settings::minimum_ARQ_wait;
	kill_timeout = Settings::maximum_frame_time > 0;
	timeout = Settings::maximum_frame_time;

	packets_acked=new bool[packet_queue_max_len+2]{ 0 };
	outgoing_frame_sendtime = new uint64_t[packet_queue_max_len+2]{ 0 };
	outgoing_frame_first_sendtime = new uint64_t[packet_queue_max_len+2]{ 0 };
	sendqueue_frames = new Frame<RadioPacket>*[packet_queue_max_len+2]{ nullptr };
	receptions = new message_reception_identikit[packet_queue_max_len+2];
	zeroarray_segment_max=new uint8_t[segment_max]{0};

	for(unsigned int i = 0; i < packet_queue_max_len; i++){
		receptions[i].id=i+1;
		receptions[i].msg_length=0;
		receptions[i].shift=0;
		receptions[i].total_packets=0;
		receptions[i].written_to_buffer=0;
		receptions[i].missing_array_length=0;
		receptions[i].buffer=new uint8_t[3*get_mtu()]{0};
		receptions[i].missing = new uint8_t[segment_max]{0};
		receptions[i].received = new bool[segment_max]{ 0 };
	}

	empty_packet = new RadioPacket; // To be encoded RSC;
		for(int i = 0; i < 32; i++)
			empty_packet->data[i]=0;
		empty_packet->size=32;
		//rsc.efficient_encode(empty_packet->data,32);
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
	returnvector[7] = (std::to_string(frames_timedout));
	returnvector[8] = (std::to_string(fragments_retired));
	returnvector[9] = (std::to_string(nacks_retired));

	//returnvector = { std::to_string(fragments_received), std::to_string( fragments_sent), std::to_string(fragments_resent), std::to_string(frames_completed), std::to_string(fragments_control) };

	fragments_received = fragments_sent = fragments_resent = frames_completed =
			fragments_control = bytes_discarded = frames_dropped = frames_timedout = fragments_retired =
							nacks_retired = 0;

	return (returnvector);

}

RadioPacket* HARQ::next_packet() {
	editing_packages_mutex.lock();
	//memory_test_reception();
	checktimers();
	//memory_test_reception();
	if (delete_queue.packets.size() > id_max) {
		delete delete_queue.packets.front();
		delete_queue.packets.pop_front();
	}
	//memory_test_reception();
	RadioPacket *ret_rp = nullptr;

	if (!send_queue_system.packets.empty()) {
		//std::cout << "-\t-\t-\tExtracting from send_queue_system\n";
		delete_queue.packets.push_back(ret_rp =
				send_queue_system.packets.front());
		send_queue_system.packets.pop_front();
		fragments_sent++;
	} else if (!send_queue.packets.empty()) {
		//std::cout << "-\t-\t-\tExtracting from send_queue\n";
		ret_rp = send_queue.packets.front();
		send_queue.packets.pop_front();
		fragments_sent++;
	} else if (Settings::control_packets) {

		ret_rp = get_empty_packet();
	}
	//memory_test_reception();

#ifdef DEBUG_LOG
	if(ret_rp){
		uint8_t id, seg;
		bool f;
		unpack(ret_rp->data[0],id,seg,f);
		printf("PACKET OUT: ID %d SEG %d F %d TYPE:%s ",id,seg,f,id==0 ? f ? (ret_rp->data[1]==0 ? "ACK" : "NACK") : "WTF" : "DATA");
		if(f && id==0){
			printf("NACKED content (ID-SEG):");
			for(int i = 0; i < bytes_per_submessage-1;i++){
				uint8_t id2, seg2;
				bool f2;
				unpack(ret_rp->data[1+i],id2,seg2,f2);
				printf("%d-%d ",id2,seg2);
			}
		}else if(f){
			printf("approx MSG-LEN (packets): %d", seg);
		}
		printf("\n");
	}
#endif

	editing_packages_mutex.unlock();

	return (ret_rp);

}

void HARQ::checktimers() {
	uint64_t ms = current_millis();
	for (unsigned int i = 0; i < packet_queue_max_len; i++) {
		if ((sendqueue_frames[i]) && (!packets_acked[i])) {
			if (kill_timeout && (ms >= outgoing_frame_first_sendtime[i] + timeout)) {
				frames_timedout++;
				printf("Packet ID %d died\n", i+1);
				received_ok(i+1);
			} else if (ms >= outgoing_frame_sendtime[i] + resend_wait_time) {
				outgoing_frame_sendtime[i] = ms;
				send_queue.packets.push_back(
						sendqueue_frames[i]->packets.back());
			}
		}
	}
}

unsigned int HARQ::get_mtu() {
		return (((pow(2, segment_bits) * bytes_per_submessage)
				- length_last_packet_bytes));
}

