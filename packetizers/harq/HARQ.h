/*
 * HARQ.h
 *
 *  Created on: 27 May 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../Packetizer.h"

#include <math.h>

//#define USE_PML 1

class PacketConsumer;
class PacketIdentityLogger;

class HARQ : public Packetizer<TunMessage,RFMessage> {

	protected:
#ifdef USE_PML
		unique_ptr<PacketIdentityLogger> PML;
#endif

		std::mutex packetout_mtx;
		std::condition_variable packetout_cv;
		std::mutex packet_current_out_mtx;
		std::condition_variable packet_current_out_cv;

		unsigned int mtu = 100;

		unsigned int id_bits, segment_bits, last_packet_bits; // @suppress("Multiple variable declaration")
		unsigned int id_max, segment_max; // @suppress("Multiple variable declaration")
		int id_byte_pos_offset, segment_byte_pos_offset, last_packet_byte_pos_offset; // @suppress("Multiple variable declaration")
		unsigned char id_mask, segment_mask, last_packet_mask; // @suppress("Multiple variable declaration")

		int submessage_bytes;
		int header_bytes;
		int length_last_packet_bytes;
		int bytes_per_submessage;
		unsigned int packet_queue_max_len;

		uint64_t resend_wait_time;
		bool kill_timeout;
		uint64_t timeout;

		std::unique_ptr<std::thread> packetout;

		std::vector<std::unique_ptr<PacketConsumer>> packet_eaters;

		std::unique_ptr<TimedFrameHandler> tfh; // Handles packets one by one
		Frame current_outgoing_frame;

		bool running_nxp = true;

		void push_ack_nack(PacketConsumer* pkc);


	public:
		HARQ();
		virtual ~HARQ();
		void stop_worker(){
			running_nxp = false;
			if(packetout){
				if(packetout.get()->joinable()){
					packetout_cv.notify_all();
					packetout.get()->join();
				}
			}
		}
		void worker_packetout();
		void process_tun(TunMessage &m);
		void process_packet(RFMessage &m);
		void apply_settings(const Settings &settings) override;
		unsigned int get_mtu() override;

		inline unsigned char pack(unsigned char id, unsigned char segment,
				bool last_packet) {
			if (id >= id_max)
				printf("Packing id greater than max. ID:%d, MAX:%d\n", id, id_max);

			if (segment >= segment_max)
				printf("Packing segment greater than max. S:%d, MAX:%d\n", segment, segment_max);

			return (0x00 | ((id & id_mask) << id_byte_pos_offset) | ((segment & segment_mask) << segment_byte_pos_offset) | (last_packet & last_packet_mask));
		}
		inline void unpack(unsigned char data, unsigned char &id, unsigned char &segment, bool &last_packet) {
			id = id_mask & (data >> id_byte_pos_offset);
			segment = segment_mask & (data >> segment_byte_pos_offset);
			last_packet = last_packet_mask & (data >> last_packet_byte_pos_offset);
		}
		inline uint8_t unpack_id(unsigned char data) {
			return (id_mask & (data >> id_byte_pos_offset));
		}

		void stop_module() override {
			Packetizer::stop_module();
			stop_worker();
		}

};

class PacketConsumer{
		int bytes_per_sub;
		int mtu;
		std::unique_ptr<uint8_t[]> buffer;
		unsigned int max_segments;
		std::unique_ptr<bool[]> segments;
		HARQ* harq_ref = nullptr;

		uint8_t segments_in_message = 0;
		unsigned int message_length = 0;
		int offset = 0;
		bool consumed = false;

	public:
		uint8_t id = 255;
		int crc = -1;
		std::vector<uint8_t> missing_segments;
		PacketConsumer(unsigned int bytes_per_submessage, unsigned int mtu_p, unsigned int max_seg, HARQ* href) : bytes_per_sub(bytes_per_submessage), mtu(mtu_p), buffer(new uint8_t[mtu_p]), max_segments(max_seg), segments(new bool[max_seg]{false}), harq_ref(href) {
			missing_segments.reserve(max_seg);
		}
		~PacketConsumer(){}

		inline void reset(){
			for(unsigned int i = 0; i < max_segments; i++)
				segments.get()[i] = false;
			missing_segments.clear();
			id = 255;
			segments_in_message = 0;
			message_length = 0;
			offset = 0;
			crc = -1;
			consumed = false;
		}

		bool consume_packet(RFMessage& rfm){
			uint8_t unpacked_id, unpacked_segment;
			bool last_packet;
			harq_ref->unpack(rfm.data.get()[0],unpacked_id,unpacked_segment,last_packet);

			if(id != 255 && id != unpacked_id){
				//printf("New id, resetting\n");
				reset();
			}

			if (last_packet) {
				if(consumed && ( ((unpacked_segment + 1) != segments_in_message) || (message_length != ((bytes_per_sub * unpacked_segment) + rfm.data.get()[1])))){
				//	printf("New packet, resetting\n");
					reset();
				}
				segments_in_message = unpacked_segment + 1;
				message_length = (bytes_per_sub * unpacked_segment) + rfm.data.get()[1];
				//printf("Seg length: %d\t", rfm.data.get()[1]);
				//printf("Total length: %d\t", message_length);
			} else if (crc == -1){
				crc = rfm.data.get()[1];
			} else if(rfm.data.get()[1] != crc){
				if(!consumed)
					printf("Unfinished packets have different CRC, guess corruption or packet was killed, resetting\n");
				//else
				//	printf("Moving to new packet\n");
				reset();
				crc = rfm.data.get()[1];
			}

			if (id == 255) {
				id = unpacked_id;
			}

			//printf("Message crc: %d\t", crc);

			segments.get()[last_packet ? 0 : unpacked_segment] = 1;

			int copylength = last_packet ? rfm.data.get()[1] : bytes_per_sub;
			int copystart = last_packet ? (offset=(bytes_per_sub-rfm.data.get()[1])) : bytes_per_sub*unpacked_segment;

			if (copystart < 0 || copystart > mtu) {
				printf("Copystart is incorrect, %d\n", copystart);
				throw std::range_error("Out of bounds");
			}
			if (copylength < 0 || copylength > bytes_per_sub) {
				printf("Copylength is incorrect, %d\n", copylength);
				throw std::range_error("Out of bounds");
			}
			if(offset < 0 || offset > mtu){
				printf("Offset is incorrect, %d\n",offset);
				throw std::range_error("Out of bounds");
			}
			if(copystart+copylength > mtu){
				printf("Copystart+Copylength is out of bounds, %d\n", copystart+copylength);
				throw std::range_error("Out of bounds");
			}

			std::copy(rfm.data.get()+2, rfm.data.get()+2+copylength, buffer.get()+copystart);

			return (true);
		}

		bool is_finished(){
			if(segments_in_message == 0)
				return false;

			bool finished = true;
			missing_segments.clear();

			for(uint8_t i = 0; i < segments_in_message; i++){
				if(!segments[i]){
					missing_segments.push_back(harq_ref->pack(id,i,false));
					finished = false;
				}
			}
			return finished;
		}

		bool is_consumed(){return consumed;}

		TunMessage get_tun_object(){
			consumed = true;
			TunMessage tms(message_length);
			std::copy(buffer.get() + offset, buffer.get() + offset + message_length, tms.data.get());
			return tms;
		}
};

enum state {
	acked, nacked, expired, recalled, added, received
};
class PacketIdentityLogger{
		PacketMessageFactory *pmf = nullptr;

		string pickstate(state s){
			switch(s){
			case state::acked:
				return "acked";
				break;
			case state::nacked:
				return "nacked";
				break;
			case state::expired:
				return "expired";
				break;
			case state::recalled:
				return "recalled";
				break;
			case state::added:
				return "added";
				break;
			case state::received:
				return "received";
				break;
			default:
				return "undefined";
				break;
			}
		}

		struct arrival {
				RFMessage packet;
				uint64_t time;
		};
		struct event {
				uint8_t frame_crc;
				state stat;
				uint64_t time;
		};
		arrival make_arrival(RFMessage &packet) {
			return {pmf->make_new_packet(packet), current_millis()};
		}
		event make_event(uint8_t crc, state stat) {
			return {crc, stat, current_millis()};
		}
		std::vector<arrival> arrivals, outgoing;
		std::vector<event> events;

		inline uint64_t distance(uint64_t a, uint64_t b){return a-b;}

		HARQ* harqref;

	public:
		PacketIdentityLogger(HARQ* ref) : harqref(ref) {
		}
		~PacketIdentityLogger() {
			std::ofstream file;
			file.open("events.csv");
			if (!file.good()) {
				printf("Problem opening file\n");

				if (file.fail()) {
					// Get the error code
					std::ios_base::iostate state = file.rdstate();

					// Check for specific error bits
					if (state & std::ios_base::eofbit) {
						std::cout << "End of file reached." << std::endl;
					}
					if (state & std::ios_base::failbit) {
						std::cout << "Non-fatal I/O error occurred."
								<< std::endl;
					}
					if (state & std::ios_base::badbit) {
						std::cout << "Fatal I/O error occurred." << std::endl;
					}

					// Print system error message
					std::perror("Error: ");
				}

				file.close();
				return;
			}
			uint64_t baseline = 0;
			file<<"Arrival Packet ID,Segment,LP,Arrival time,Outgoing Packet ID,Segment,LP,Outgoing Time,ID,Segment,LP,Event CRC, Event, Time\n";
			auto cur_a = arrivals.begin(), cur_o = outgoing.begin();
			auto cur_e = events.begin();
			while(cur_a != arrivals.end() || cur_o != outgoing.end() || cur_e != events.end()){
				uint64_t best_time = 0;
				bool print_a = false, print_b = false, print_c = false;
				if(cur_a != arrivals.end()){
					if((*cur_a).time <= best_time || best_time==0){
						best_time = (*cur_a).time;
					}
				}
				if (cur_o != outgoing.end()) {
					if ((*cur_o).time <= best_time|| best_time==0) {
						best_time = (*cur_o).time;
					}
				}
				if (cur_e != events.end()) {
					if ((*cur_e).time <= best_time|| best_time==0) {
						best_time = (*cur_e).time;
					}
				}
				if (cur_o != outgoing.end()) {
					if (distance((*cur_o).time, best_time) < 3) {
						print_b = true;
					}
				}
				if (cur_a != arrivals.end()) {
					if (distance((*cur_a).time, best_time) < 3) {
						print_a = true;
					}
				}
				if (cur_e != events.end()) {
					if (distance((*cur_e).time, best_time) < 3) {
						print_c = true;
					}
				}
				if(baseline == 0)
					baseline = best_time;
				uint8_t id, seg;
				bool lp;
				if (print_a) {
					harqref->unpack((*cur_a).packet.data.get()[0], id, seg, lp);
					file << std::to_string(id);
					file << ",";
					file << std::to_string(seg);
					file << ",";
					file << std::to_string(lp);
					file << ",";
					file << std::to_string(distance((*cur_a).time, baseline));
					file << ",";
					cur_a++;
				}else{
					file << ",,,,";
				}

				if (print_b) {
					harqref->unpack((*cur_o).packet.data.get()[0], id, seg, lp);
					file << std::to_string(id);
					file << ",";
					file << std::to_string(seg);
					file << ",";
					file << std::to_string(lp);
					file << ",";
					file << std::to_string(distance((*cur_o).time, baseline));
					file << ",";
					cur_o++;
				} else {
					file << ",,,,";
				}

				if(print_c){
					harqref->unpack((*cur_e).frame_crc, id, seg, lp);
					file << std::to_string(id);
					file << ",";
					file << std::to_string(seg);
					file << ",";
					file << std::to_string(lp);
					file << ",";
					file << std::to_string((*cur_e).frame_crc);
					file << ",";
					file << pickstate((*cur_e).stat);
					file << ",";
					file << std::to_string(distance((*cur_e).time, baseline));
					file << "\n";
					cur_e++;
				}else{
					file << ",,,,,\n";
				}
				baseline = best_time;
				if(!print_a && !print_b && !print_c)break;
			}

			file.close();
		}
		void register_pmf(PacketMessageFactory *pmff) {
			pmf = pmff;
		}
		uint64_t in = 0, out = 0;

		void tun_in_to_out(TunMessage &tms, HARQ::Frame &f) {
			events.push_back(make_event(f.metadata,state::added));
		}
		void tun_out_to_in(TunMessage &tms) {
			events.push_back(make_event(tms.data[0],state::received));
		}
		void acked(HARQ::Frame &f) {
			events.push_back(make_event(f.metadata, state::acked));
		}
		void nacked(RFMessage &f) {
			events.push_back(make_event(f.data.get()[0], state::nacked));
		}
		void recalled(RFMessage &f) {
			events.push_back(make_event(f.data.get()[0], state::recalled));
		}
		void expired(HARQ::Frame &f) {
			events.push_back(make_event(f.metadata, state::expired));
		}
		void packet_in(RFMessage &m) {
			arrivals.push_back(make_arrival(m));
		}

		void packet_out(RFMessage &m) {
			outgoing.push_back(make_arrival(m));
		}
		void packet_out(HARQ::Frame &f) {
			for (unsigned int i = 0; i < f.packets.size(); i++)
				packet_out(f.packets[i]);
		}

};

