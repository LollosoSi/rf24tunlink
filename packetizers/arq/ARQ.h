/*
 * ARQ.h
 *
 *  Created on: 27 May 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../Packetizer.h"

#include <math.h>
#include <queue>

//#define USE_PML 1

class ARQPacketConsumer;
class ARQPacketIdentityLogger;

class ARQ : public Packetizer<TunMessage,RFMessage> {

	protected:
#ifdef USE_PML
		unique_ptr<ARQPacketIdentityLogger> PML;
#endif

		bool use_empty_packets = true;
		bool running_ept = false;
		uint16_t empty_packet_delay = 2;
		std::thread empty_packets_thread;
		TimedFrameHandler etfh = TimedFrameHandler(1000,this->empty_packet_delay);

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
		bool use_estimate;
		double estimate_resend_wait_time;

		Frame current_packet_outgoing;
		std::thread frame_thread;
		std::mutex packet_outgoing_lock;
		TimedFrameHandler packet_outgoing_tfh = TimedFrameHandler(1000,1);

		//inline bool is_next_frame_available(unsigned int& queue_id);
		//inline Frame next_frame(unsigned int& queue_id);
		//inline void add_frame_to_queue(Frame &f, unsigned int queue_id);

		std::vector<std::unique_ptr<ARQPacketConsumer>> packet_eaters;

		bool running_nxp = true, running_nxfq = true;

		inline void push_ack_nack(ARQPacketConsumer* pkc);
		inline void substitute_packet(unsigned int queue_id);


	public:
		ARQ();
		virtual ~ARQ();
		inline void stop_worker() {
			running_nxp = false;
			running_nxfq = false;
			running_ept = false;

			if (frame_thread.joinable()) {
				std::cout << "Waiting for packetout to join...";
				frame_thread.join();
				std::cout << "...ok\n";
			}
			if (empty_packets_thread.joinable()) {
				std::cout << "Waiting for empty packetout to join...";
				empty_packets_thread.join();
				std::cout << "...ok\n";
			}
		}

		inline void worker_packetout();
		void empty_packets_worker();
		inline void process_tun(TunMessage &m);
		inline void process_packet(RFMessage &m);
		inline void apply_settings(const Settings &settings) override;
		inline unsigned int get_mtu() override;

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

		inline void stop_module() override {
			Packetizer::stop_module();
			stop_worker();
		}

};

class ARQPacketConsumer{
		int bytes_per_sub;
		int mtu;
		std::unique_ptr<uint8_t[]> buffer;
		unsigned int max_segments;
		std::unique_ptr<bool[]> segments;
		ARQ* ARQ_ref = nullptr;

		uint8_t segments_in_message = 0;
		unsigned int message_length = 0;
		int offset = 0;
		bool consumed = false;

	public:
		uint8_t id = 255;
		int crc = -1;
		std::vector<uint8_t> missing_segments;
		ARQPacketConsumer(unsigned int bytes_per_submessage, unsigned int mtu_p, unsigned int max_seg, ARQ* href) : bytes_per_sub(bytes_per_submessage), mtu(mtu_p), buffer(new uint8_t[mtu_p]), max_segments(max_seg), segments(new bool[max_seg]{false}), ARQ_ref(href) {
			missing_segments.reserve(max_seg);
		}
		~ARQPacketConsumer(){}

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

		inline bool consume_packet(RFMessage& rfm){
			uint8_t unpacked_id, unpacked_segment;
			bool last_packet;
			ARQ_ref->unpack(rfm.data.get()[0],unpacked_id,unpacked_segment,last_packet);

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

		inline bool is_finished(){
			if(segments_in_message == 0)
				return false;

			bool finished = true;
			missing_segments.clear();

			for(uint8_t i = 0; i < segments_in_message; i++){
				if(!segments[i]){
					missing_segments.push_back(ARQ_ref->pack(id,i,false));
					finished = false;
				}
			}
			return finished;
		}

		inline bool is_consumed(){return consumed;}

		inline TunMessage get_tun_object(){
			consumed = true;
			TunMessage tms(message_length);
			std::copy(buffer.get() + offset, buffer.get() + offset + message_length, tms.data.get());
			return tms;
		}
};


class ARQPacketIdentityLogger{

	public:
		enum ARQstate {
					p_acked, p_nacked, p_expired, p_recalled, p_added, p_received
				};

	private:

		PacketMessageFactory *pmf = nullptr;

		string pickstate(ARQstate s){
			switch(s){
			case ARQstate::p_acked:
				return "acked";
				break;
			case ARQstate::p_nacked:
				return "nacked";
				break;
			case ARQstate::p_expired:
				return "expired";
				break;
			case ARQstate::p_recalled:
				return "recalled";
				break;
			case ARQstate::p_added:
				return "added";
				break;
			case ARQstate::p_received:
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
				ARQstate stat;
				uint64_t time;
		};
		arrival make_arrival(RFMessage &packet) {
			return {pmf->make_new_packet(packet), current_millis()};
		}
		event make_event(uint8_t crc, ARQstate stat) {
			return {crc, stat, current_millis()};
		}
		std::vector<arrival> arrivals, outgoing;
		std::vector<event> events;

		inline uint64_t distance(uint64_t a, uint64_t b){return a-b;}

		ARQ* ARQref;

	public:


		ARQPacketIdentityLogger(ARQ* ref) : ARQref(ref) {
		}
		~ARQPacketIdentityLogger() {
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
					ARQref->unpack((*cur_a).packet.data.get()[0], id, seg, lp);
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
					ARQref->unpack((*cur_o).packet.data.get()[0], id, seg, lp);
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
					ARQref->unpack((*cur_e).frame_crc, id, seg, lp);
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
		inline void register_pmf(PacketMessageFactory *pmff) {
			pmf = pmff;
		}
		uint64_t in = 0, out = 0;

		inline void tun_in_to_out(TunMessage &tms, ARQ::Frame &f) {
			events.push_back(make_event(f.metadata,ARQstate::p_added));
		}
		inline void tun_out_to_in(TunMessage &tms) {
			events.push_back(make_event(tms.data[0],ARQstate::p_received));
		}
		inline void acked(ARQ::Frame &f) {
			events.push_back(make_event(f.metadata, ARQstate::p_acked));
		}
		inline void nacked(RFMessage &f) {
			events.push_back(make_event(f.data.get()[0], ARQstate::p_nacked));
		}
		inline void recalled(RFMessage &f) {
			events.push_back(make_event(f.data.get()[0], ARQstate::p_recalled));
		}
		inline void expired(ARQ::Frame &f) {
			events.push_back(make_event(f.metadata, ARQstate::p_expired));
		}
		inline void packet_in(RFMessage &m) {
			arrivals.push_back(make_arrival(m));
		}

		inline void packet_out(RFMessage &m) {
			outgoing.push_back(make_arrival(m));
		}
		inline void packet_out(ARQ::Frame &f) {
			for (unsigned int i = 0; i < f.packets.size(); i++)
				packet_out(f.packets[i]);
		}

};

