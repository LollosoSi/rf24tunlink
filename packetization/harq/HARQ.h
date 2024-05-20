/*
 * HARQ.h
 *
 *  Created on: 7 May 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../../interfaces/PacketHandler.h"
#include "../../telemetry/Telemetry.h"
#include "../../rs_codec/RSCodec.h"
#include "../../utils.h"
#include <cmath>
#include <algorithm>
#include <mutex>


#include <stdexcept>

//#define DEBUG_LOG

#ifdef DEBUG_LOG
#endif

static unsigned char bits_to_mask(const int bits) {
	return (pow(2, bits) - 1);
}

struct message_reception_identikit {
	uint8_t id = 0;
	bool written_to_buffer = false;
	uint8_t *buffer = nullptr;
	int shift = 0;
	int msg_length = 0;
	int total_packets = 0;
	bool *received = nullptr;
	uint8_t *missing = nullptr;
	int missing_array_length = 0;
};

class HARQ: public PacketHandler<RadioPacket>, public Telemetry {
public:
	HARQ();
	virtual ~HARQ();

	int id_bits = 2, segment_bits = 5, last_packet_bits = 1; // @suppress("Multiple variable declaration")
	int id_max = pow(2, id_bits), segment_max = pow(2, segment_bits); // @suppress("Multiple variable declaration")
	int id_byte_pos_offset = 8 - id_bits, segment_byte_pos_offset = 8 - id_bits - segment_bits, last_packet_byte_pos_offset = 8 - id_bits - segment_bits - last_packet_bits; // @suppress("Multiple variable declaration")
	unsigned char id_mask = bits_to_mask(id_bits), segment_mask = bits_to_mask(segment_bits), last_packet_mask = bits_to_mask(last_packet_bits); // @suppress("Multiple variable declaration")

	unsigned char pack(unsigned char id, unsigned char segment,
			bool last_packet) {
		return (0x00 | ((id & id_mask) << id_byte_pos_offset)
				| ((segment & segment_mask) << segment_byte_pos_offset)
				| (last_packet & last_packet_mask));
	}
	void unpack(unsigned char data, unsigned char &id,
			unsigned char &segment, bool &last_packet) {
		id = id_mask & (data >> id_byte_pos_offset);
		segment = segment_mask & (data >> segment_byte_pos_offset);
		last_packet = last_packet_mask & (data >> last_packet_byte_pos_offset);
	}
	inline uint8_t unpack_id(unsigned char data) {
		return (id_mask & (data >> id_byte_pos_offset));
	}

	int submessage_bytes = 32;
	int header_bytes = 1;
	int length_last_packet_bytes = 1;
	int bytes_per_submessage = submessage_bytes - Settings::ReedSolomon::nsym
			- header_bytes;

	int packet_queue_max_len = id_max - 1;

	RSCodec rsc;

	uint64_t resend_wait_time = Settings::minimum_ARQ_wait;
	bool kill_timeout = Settings::maximum_frame_time > 0;
	uint64_t timeout = Settings::maximum_frame_time;

	std::string* telemetry_collect(const unsigned long delta);

	void memory_test_reception() {

		for (int i = 0; i < packet_queue_max_len; i++) {
			if (!(receptions[i].id > 0
					&& receptions[i].id <= packet_queue_max_len)
					|| !(receptions[i].total_packets >= 0
							&& receptions[i].total_packets <= 32)
					|| !(receptions[i].msg_length >= 0
							&& receptions[i].msg_length <= get_mtu())) {
				printf("Trigger A:%d B:%d C:%d\n",!(receptions[i].id > 0
						&& receptions[i].id <= packet_queue_max_len),!(receptions[i].total_packets >= 0
								&& receptions[i].total_packets <= 32),!(receptions[i].msg_length >= 0
										&& receptions[i].msg_length <= get_mtu()));
				throw std::invalid_argument("test failed");
			}
		}

	}

	void write_msri_data(message_reception_identikit *msri, uint8_t *data,
			int size, int segment) {
		if(msri == nullptr){
			printf("MSRI is NULL\n");
			throw std::invalid_argument("NULL MSRI");
		}

		bool found = false;
		for(int i = 0; i < this->packet_queue_max_len; i++){
			if(msri == (receptions+i)){
				found = true;
				break;
			}
		}
		if(!found){
			printf("MSRI is not in receptions\n");
			throw std::invalid_argument("invalid MSRI");
		}
		if(msri->shift < 0){
			printf("MSRI has negative shift\n");
			throw std::invalid_argument("negative shift MSRI");
		}

		if(((bytes_per_submessage * segment) + (segment == 0 ? msri->shift : 0)) + size > get_mtu()+1){
			printf("MSRI tries to write out of memory data. Shift: %d, MSGLEN: %d, Writestart: %d, Writesize: %d, MTU:%d\n", msri->shift, msri->msg_length, ((bytes_per_submessage * segment) + (segment == 0 ? msri->shift : 0)), size, get_mtu());
			throw std::invalid_argument("out of memory MSRI");
		}

		if(msri->msg_length > get_mtu()+1){
				printf("MSRI is bigger than MTU. MSGLEN: %d, MTU:%d\n", msri->msg_length, get_mtu());
				throw std::invalid_argument("MSRI too big");
			}
#ifdef DEBUG_LOG
		printf("Writing %d bytes at MSRI b+%d, MSGLEN: %d", size, (bytes_per_submessage * segment)
				+ (segment == 0 ? msri->shift : 0), msri->msg_length);
#endif

		for(int i = 0; i < size; i++){
			msri->buffer[(bytes_per_submessage * segment) + (segment == 0 ? msri->shift : 0) + i] = data[i];
		}
		msri->received[segment] = true;
	}

	bool is_msri_completed(message_reception_identikit *msri) {
		if (msri->total_packets == 0)
			return (false);
		if (msri->id == 0)
			std::cout << "Note: message_reception_identikit has no id. Messages can't be recognised later on.\n";
		if(msri->id > id_max)
			std::cout << "Note: message_reception_identikit invalid id. This is likely due to corruption.\n";

		msri->missing_array_length = 0;
		int i = 0;
		for (; i <= msri->total_packets && i < segment_max; i++)
			if (!msri->received[i])
				msri->missing[msri->missing_array_length++] = pack(msri->id, i,
						false);

		if(msri->total_packets > segment_max){
			printf("Total packets %d is bigger than segment_max %d", msri->total_packets, segment_max);
			throw std::invalid_argument("total packets bigger than segment_max");
		}

		if (i > segment_max) {
			throw std::invalid_argument("i bigger than segment_max");
		}

		return (msri->missing_array_length == 0);
	}

	unsigned int get_mtu() {
		return (((pow(2, segment_bits) * bytes_per_submessage)
				- length_last_packet_bytes));
		//return 300;
	}

	inline bool next_packet_ready() {
		return (true);
	}

	RadioPacket *empty_packet = nullptr;
	inline RadioPacket* get_empty_packet() {
		return (empty_packet);
	}

	RadioPacket* next_packet();

protected:

	std::mutex editing_packages_mutex;

	bool *packets_acked = nullptr;
	uint64_t *outgoing_frame_sendtime = nullptr;
	uint64_t *outgoing_frame_first_sendtime = nullptr;
	Frame<RadioPacket> **sendqueue_frames = nullptr;
	message_reception_identikit *receptions = nullptr;

	void findout_add_sendqueue() {

		if (frames.empty()) {
			return;
		}

		uint8_t id = unpack_id(frames.front()->packets[0]->data[0]);

		if (!sendqueue_frames[id - 1]) {

			sendqueue_frames[id - 1] = frames.front();
			frames.pop_front();
			outgoing_frame_first_sendtime[id - 1] = outgoing_frame_sendtime[id
					- 1] = current_millis();
			packets_acked[id - 1] = false;
			add_sendqueue(sendqueue_frames[id - 1]);
#ifdef DEBUG_LOG
			std::cout << "\tENQUEUED ID " << static_cast<int>(id) << "\n";
#endif

		}

	}

	bool check_pack_id(const RadioPacket *rp, uint8_t id) {
		return (id == unpack_id(rp->data[0]));
	}

	void delete_id_from_sendqueue(uint8_t id) {
		for (auto cur = send_queue.packets.begin();
				cur != send_queue.packets.end();)
			if (check_pack_id(*cur, id)) {
				cur = send_queue.packets.erase(cur);
				fragments_retired++;
			} else
				cur++;

	}
	void delete_id_from_NACK(uint8_t r_id) {

		auto cur = send_queue_system.packets.begin();
		while (cur != send_queue_system.packets.end()) {
			int removed = 0;
			uint8_t id, seg;
			bool lp;
			unpack((*cur)->data[0], id, seg, lp);
			int len = 0;
			while ((*cur)->data[1 + len] != 0 && len < bytes_per_submessage)
				len++;

			if (id == 0 && lp && len != 0) {
				for (int i = 0; i < len - removed; i++) {
					if (unpack_id((*cur)->data[1 + i]) == r_id) {
						removed++;
						for (int j = i; j < len - removed; j++)
							(*cur)->data[1 + j] = (*cur)->data[2 + j];

						i--;
					}
				}
			}
			if (removed > 0) {
				nacks_retired += removed;
				if (removed == len) {
					delete (*cur);
					cur = send_queue_system.packets.erase(cur);
					fragments_retired++;
				} else
					cur++;
				std::cout << "Removed some NACKs\n";
			} else
				cur++;
		}
	}

	inline void add_sendqueue(Frame<RadioPacket> *frp) {
		for (unsigned int i = 0; i < frp->packets.size(); i++)
			send_queue.packets.push_back(frp->packets[i]);
	}

	void flush_acked() {

		for (int i = 0; i < packet_queue_max_len; i++) {
			if (sendqueue_frames[i] && packets_acked[i]) {
				packets_acked[i] = false;
				free_frame(sendqueue_frames[i]);
				sendqueue_frames[i] = nullptr;
			}
		}

		findout_add_sendqueue();

	}

	void received_ok(uint8_t id) {

		if (id > 0 && id <= packet_queue_max_len) {
			//for (unsigned int i = 0; i < packet_queue_max_len; i++) {
			if (sendqueue_frames[id - 1]) {
				packets_acked[id - 1] = true;
				delete_id_from_sendqueue(id);
			}
			//}
			flush_acked();
		} else {
			printf("ACKED non existent frame\n");
			throw std::invalid_argument("illegal id");
		}
	}

	void catch_lost(uint8_t *lostpackets, int size) {
		for (int i = 0; i < size; i++) {
			uint8_t p_id, p_seg;
			bool p_b;
			unpack(lostpackets[i], p_id, p_seg, p_b);
#ifdef DEBUG_LOG
			printf("\nMissing: ID - %d SEG - %d \t", static_cast<int>(p_id),
					static_cast<int>(p_seg));
#endif
			if (p_id < 1 || p_id > packet_queue_max_len) {
				std::cout << "P_ID out of range, I quit\n";
				return;
			}
			if (!sendqueue_frames[p_id - 1]) {

				std::cout << "Requested a finished packet! Holy crap! I'm sorry, I refuse\n";

				return;
			}
			if (sendqueue_frames[p_id - 1]->packets.size() <= p_seg) {
				std::cout << "Segment is greater than packets length. L: "
						<< sendqueue_frames[p_id - 1]->packets.size() << " S:"
						<< p_seg << "\n";
				return;
			}
#ifdef DEBUG_LOG
			std::cout << "Reloading packet: ";
			print_rp_data(
					sendqueue_frames[p_id - 1]->packets[sendqueue_frames[p_id
							- 1]->packets.size() - 1 - p_seg]->data);
			std::cout << "\n";
#endif
			send_queue.packets.push_back(
					sendqueue_frames[p_id - 1]->packets[sendqueue_frames[p_id
							- 1]->packets.size() - 1 - p_seg]);
			fragments_resent++;

		}
#ifdef DEBUG_LOG
		std::cout << "\n";
#endif

	}

	void checktimers();

	bool packetize(TUNMessage *tunmsg) {

		Frame<RadioPacket> *f = new Frame<RadioPacket>;

		static uint8_t id = 0;
		id = (id + 1) & id_mask;
		if (id == 0)
			id++;

		int msglen = tunmsg->size;

		int rem = msglen % bytes_per_submessage;
		int packets = ceil(msglen / static_cast<float>(bytes_per_submessage));
		int arrayshift = bytes_per_submessage - rem;
		if (rem == 0)
			packets++;

		uint8_t (*nums)[bytes_per_submessage] =
				reinterpret_cast<uint8_t (*)[bytes_per_submessage]>(tunmsg->data
						- arrayshift);

		RadioPacket *rp = new RadioPacket { { 0 }, 32 };
		rp->data[0] = pack(id, packets - 1, true);
		rp->data[1] = rem;
		memcpy(rp->data + 2, nums[0] + (bytes_per_submessage - rem), rem);
		rsc.efficient_encode(rp->data, rp->size);
		f->packets.push_front(rp);

		for (int i = 1; i < packets; i++) {
			rp = new RadioPacket { { 0 }, 32 };
			rp->data[0] = pack(id, i, false);
			memcpy(rp->data + 1, nums[i], bytes_per_submessage);
			rsc.efficient_encode(rp->data, rp->size);
			f->packets.push_front(rp);
		}

		frames.push_back(f);
		editing_packages_mutex.lock();
		findout_add_sendqueue();
		editing_packages_mutex.unlock();

		return (true);
	}

	void print_rp_data(uint8_t *data) {
		for (int i = 1; i < bytes_per_submessage + 1; i++) {
			std::cout << " ";
			if (((data[i] > 31) && (data[i] < 129))) {
				std::cout << static_cast<unsigned char>(data[i]);
			} else {
				std::cout << static_cast<int>(data[i]);
			}

		}
		std::cout << "\t";
	}

	bool receive_packet(RadioPacket *rp) {

		editing_packages_mutex.lock();
		//memory_test_reception();

		int error_count = 0;

		if (!rsc.efficient_decode(rp->data, 32, &error_count)) {
#ifdef DEBUG_LOG
			std::cout << "\tPacket broken\n";
#endif
			quality_count++;
			//memory_test_reception();
			editing_packages_mutex.unlock();
			return (false);
		}
		quality_sum += 1.0 - (error_count / Settings::ReedSolomon::k);
		quality_count++;

		static uint8_t waiting_id = 1;

		uint8_t id, segment;
		bool last_packet;
		unpack(rp->data[0], id, segment, last_packet);

		if (id > packet_queue_max_len) {
			printf("This packet has a problem\n");
			//memory_test_reception();
			editing_packages_mutex.unlock();
			return (true);
		}

#ifdef DEBUG_LOG
		std::cout << "Message content: ID - " << static_cast<int>(id)
				<< " SEG - "
				<< static_cast<int>(segment) << " last - "
				<< static_cast<int>(last_packet) << " : ";
		//print_rp_data(rp->data);
#endif

#ifdef DEBUG_LOG
		std::cout << "\tThis packet is:\t";
#endif
		//memory_test_reception();
		if (id == 0) {
			fragments_control++;

			if (last_packet) {

				// Is this ACK or NACK or BOTH??
				int len = 0;
				while (rp->data[1 + len] != 0 && len < bytes_per_submessage) {
					len++;
				}
				if (len != 0) {
					// NACK
#ifdef DEBUG_LOG
					std::cout << " NACK with missing " << len;
					uint8_t a, b;
					bool c;
					for (int i = 0; i < len; i++) {
						unpack(rp->data[1 + i], a, b, c);
						printf("ID %d SEG %d ", a, b);
					}
#endif
					catch_lost(rp->data + 1, len);
				} else if (segment > 0 && segment < id_max) {
					// ACK!
#ifdef DEBUG_LOG
					std::cout << " ACK " << static_cast<int>(segment);
#endif
					received_ok(segment);
				} else {
					printf("Seg & len > 0\n");
				}

				if (len == 0 && segment == 0) {
#ifdef DEBUG_LOG
					std::cout << " Nothing. What? ";
#endif
				}
#ifdef DEBUG_LOG
				std::cout << "\n";
#endif
				//memory_test_reception();
			} else {
#ifdef DEBUG_LOG
				std::cout << " Empty. What? ";
#endif
			}

		} else {
#ifdef DEBUG_LOG
			std::cout << " DATA : ACTION -> ";
#endif

			fragments_received++;

			message_reception_identikit *work_msri = receptions + id - 1;
			//memory_test_reception();
			if ((work_msri->id <= 0) && (work_msri->id >= 4)) {
				printf("Unexpected ID in MSRI\n");
				throw std::invalid_argument("What the fuck");
			}

			//memory_test_reception();
			if (work_msri->written_to_buffer) {
				if (last_packet) {

					send_ACK_NACK(id);
#ifdef DEBUG_LOG
					std::cout << "\tOLD PACKET; ACKED ";
#endif
					//memory_test_reception();
					editing_packages_mutex.unlock();
					return (true);
				} else {

					//printf("Clearing msri id %d len %d, addr %d. Call had size: %d, buffer content: %s\n",id,work_msri->msg_length,work_msri,receptions.size(), work_msri->buffer);
					//memory_test_reception();
					work_msri->msg_length = 0;
					work_msri->shift = 0;
					work_msri->total_packets = 0;
					work_msri->written_to_buffer = 0;
					work_msri->missing_array_length = 0;
					//memory_test_reception();
					for (int i = 0; i < segment_max; i++) {
						work_msri->received[i] = 0;
						work_msri->missing[i] = 0;
					}
					delete_id_from_NACK(id);
					//memory_test_reception();
#ifdef DEBUG_LOG
					std::cout << "\tDELETE OLD MSRI ";
#endif

				}
			}
			//memory_test_reception();
			/*
			 if ((last_packet&&(work_msri->msg_length
			 != ((segment * bytes_per_submessage) + rp->data[1])))
			 || (!last_packet && work_msri->written_to_buffer)) {
			 delete[] work_msri->buffer;
			 receptions.erase(
			 std::find(receptions.begin(), receptions.end(),
			 work_msri));
			 work_msri = nullptr;
			 std::cout << "\tDELETE OLD MSRI ";
			 } else if(last_packet) {
			 send_ACK_NACK(id);
			 std::cout << "\tOLD PACKET; ACKED ";
			 return(true);
			 }*/

			//if (work_msri == nullptr)
			//	receptions.push_back(work_msri = make_MSRI(id));
			if (last_packet) {
				work_msri->shift = bytes_per_submessage - rp->data[1];
				if(work_msri->shift < 0){
					work_msri->shift = 0;
					printf("DETECTED NEGATIVE SHIFT: bytes_per_submessage: %d, rp->data[1]: %d. MSGLEN: %d", bytes_per_submessage, static_cast<int>(rp->data[1]), work_msri->msg_length);
					editing_packages_mutex.unlock();
					return (false);
					//throw std::invalid_argument("NEGATIVE SHIFT DETECTED");
				}
				work_msri->total_packets = segment;
				work_msri->msg_length = rp->data[1]
						+ (bytes_per_submessage * segment);
				write_msri_data(work_msri, rp->data + (2), rp->data[1], 0);
#ifdef DEBUG_LOG
				std::cout << "\tPACKLEN: " << work_msri->total_packets;
				std::cout << "\tDONE? "
						<< (is_msri_completed(work_msri) ? "YES" : "NO");
				std::cout << "\tMISSING: " << work_msri->missing_array_length;
#endif
				//memory_test_reception();
			} else{
				write_msri_data(work_msri, rp->data + (1), bytes_per_submessage,
						segment);
				//memory_test_reception();
			}

			//memory_test_reception();
			if (is_msri_completed(work_msri)) {
				if (waiting_id == work_msri->id) {
					waiting_id = (waiting_id + 1) & id_mask;
					if (waiting_id == 0)
						waiting_id++;
					//memory_test_reception();
					send_ACK_NACK(id);
					//memory_test_reception();
#ifdef DEBUG_LOG
					std::cout << "\tCOMPLETED ";
#endif
				} else {
#ifdef DEBUG_LOG
					std::cout << "\tWRONG TURN ";
#endif
				}
				//memory_test_reception();
				if (!work_msri->written_to_buffer) {
					work_msri->written_to_buffer = true;
					static TUNMessage *tm = new TUNMessage;
					tm->data = work_msri->buffer + work_msri->shift;
					tm->size = work_msri->msg_length;
					//std::cout << "Final message:\t"
					//		<< work_msri->buffer + work_msri->shift << "\n";
#ifdef DEBUG_LOG
					std::cout << "\tWRITTEN ";
#endif
					//memory_test_reception();
					if (this->tun_handle->send(tm)) {
						frames_completed++;
					} else {
						bytes_discarded += work_msri->msg_length;
						frames_dropped++;
						std::cout << "\tHANDLER REFUSED ";
					}
					//memory_test_reception();
				}
			}
			//memory_test_reception();
			if (last_packet && work_msri->missing_array_length > 0) {

				send_ACK_NACK(0, work_msri->missing,
						work_msri->missing_array_length);
#ifdef DEBUG_LOG
				std::cout << "\tMISSING PIECES ";
#endif
			}

#ifdef DEBUG_LOG
			std::cout << "\n";
#endif
		}
		//memory_test_reception();
		editing_packages_mutex.unlock();
		return (true);
	}

	// Send ACK : id != 0, Send NACK lost_list is array of pack(...) with size. or nullptr
	// NOTE:
	void send_ACK_NACK(uint8_t id = 0, uint8_t *lost_list = nullptr, int size =
			0) {
		RadioPacket *rp_ok = new RadioPacket;
		rp_ok->data[0] = pack(0, id, true);
		rp_ok->size = 32;

		int i = 0;
		if (size > bytes_per_submessage-1)
			size = bytes_per_submessage-1;
		if (lost_list){
			printf("Sending NACK. Data: id %d, segments id: ",
					static_cast<int>(id));
			for (; i < size; i++){
				rp_ok->data[1 + i] = lost_list[i];
				printf(" %d",
						static_cast<int>((unpack_id(rp_ok->data[1 + i]))));
			}
		}
		for(;i<bytes_per_submessage-1;i++){
			rp_ok->data[1+i]=0;
		}
		rsc.efficient_encode(rp_ok->data, rp_ok->size);
		send_queue_system.packets.push_back(rp_ok);
	}

	inline void free_frame(Frame<RadioPacket> *frame) {
		while (!frame->packets.empty()) {
			delete frame->packets.front();
			frame->packets.pop_front();
		}
		delete frame;
	}

	Frame<RadioPacket> send_queue;
	Frame<RadioPacket> send_queue_system;

	Frame<RadioPacket> delete_queue;

	std::string *returnvector = nullptr;
	unsigned long fragments_received = 0, fragments_sent = 0, fragments_resent =
			0, frames_completed = 0, fragments_control = 0, bytes_discarded = 0,
			frames_dropped = 0, frames_timedout = 0, fragments_retired = 0,
			nacks_retired = 0; // @suppress("Multiple variable declaration")

	float quality_sum = 0;
	unsigned long quality_count = 0;

};

