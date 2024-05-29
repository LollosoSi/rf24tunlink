/*
 * HARQ.h
 *
 *  Created on: 27 May 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../Packetizer.h"

#include <math.h>

class PacketConsumer;

class HARQ : public Packetizer<TunMessage,RFMessage> {

	protected:
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
			if(packetout){
				if(packetout.get()->joinable()){
					running_nxp = false;
					packetout.get()->join();
				}
			}
		}
		void worker_packetout();
		void process_tun(TunMessage &m);
		void process_packet(RFMessage &m);
		void apply_settings(const Settings &settings) override;
		unsigned int get_mtu();

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


			if (last_packet) {
				segments_in_message = unpacked_segment + 1;
				message_length = (bytes_per_sub * unpacked_segment) + rfm.data.get()[1];
			} else if (crc == -1){
				crc = rfm.data.get()[1];
			} else if(rfm.data.get()[1] != crc){
				printf("Unfinished packets have different CRC, guess corruption or packet was killed, resetting\n");
				reset();
				crc = rfm.data.get()[1];
			}

			if (id == 255) {
				id = unpacked_id;
			}

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

