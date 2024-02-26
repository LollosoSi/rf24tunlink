/*
 * SelectiveRepeatPacketizer.cpp
 *
 *  Created on: 4 Feb 2024
 *      Author: Andrea Roccaccino
 */

#include "SelectiveRepeatPacketizer.h"

SelectiveRepeatPacketizer::SelectiveRepeatPacketizer() :
		Telemetry("SelectiveRepeatPacketizer") {

	returnvector = new std::string[7] { std::to_string(fragments_received),
			std::to_string(fragments_sent), std::to_string(fragments_resent),
			std::to_string(frames_completed), std::to_string(fragments_control),
			std::to_string(bytes_discarded), std::to_string(frames_dropped) };

	register_elements(new std::string[7] { "Fragments Received",
			"Fragments Sent", "Fragments Resent", "Frames Completed",
			"Control Fragments", "Bytes Discarded", "Dropped (timeout)" }, 7);

	last_frame_change = current_millis();

}

SelectiveRepeatPacketizer::~SelectiveRepeatPacketizer() {
	// TODO Auto-generated destructor stub
}

std::string* SelectiveRepeatPacketizer::telemetry_collect(
		const unsigned long delta) {

	returnvector[0] = (std::to_string(fragments_received));
	returnvector[1] = (std::to_string(fragments_sent));
	returnvector[2] = (std::to_string(fragments_resent));
	returnvector[3] = (std::to_string(frames_completed));
	returnvector[4] = (std::to_string(fragments_control));
	returnvector[5] = (std::to_string(bytes_discarded));
	returnvector[6] = (std::to_string(frames_dropped));

	//returnvector = { std::to_string(fragments_received), std::to_string( fragments_sent), std::to_string(fragments_resent), std::to_string(frames_completed), std::to_string(fragments_control) };

	fragments_received = fragments_sent = fragments_resent = frames_completed =	fragments_control = bytes_discarded = frames_dropped = 0;

	return (returnvector);

}

inline bool SelectiveRepeatPacketizer::next_packet_ready() {
	return (Settings::control_packets || !frames.empty() || !resend_list.empty());
}

RadioPacket* SelectiveRepeatPacketizer::next_packet() {

	if (!frames.empty()) {
		uint64_t cm;
		if ((cm = current_millis()) - last_frame_change
				> Settings::maximum_frame_time) {
			printf("Dropping packet (timeout)\n");
			//free_frame(frames.front());
			//frames.pop_front();
			received_ok();
			frames_dropped++;
		}
	}

	bool frames_empty = frames.empty(), resend_list_empty = resend_list.empty();

	RadioPacket *rs = nullptr; // resend_list.front();
	if (resend_list_empty) {

		if (frames_empty) {
			if (Settings::control_packets) {
				rs = (get_empty_packet());
			} else {
				return (nullptr);
			}
		} else {
			if (current_packet_counter == frames.front()->packets.size() - 1) {
				static uint8_t cnt = 0;
				if (cnt++ % 5 == 0)
					rs = (frames.front()->packets[current_packet_counter]);
				else
					rs = (get_empty_packet());
			} else {
				fragments_sent++;
				rs = (frames.front()->packets[current_packet_counter++]);
			}
		}

	} else {
		rs = resend_list.front();
		resend_list.pop_front();
	}
	return (rs);
}

inline RadioPacket* SelectiveRepeatPacketizer::get_empty_packet() {
	static RadioPacket *rp = new RadioPacket { { 0 }, 1 };
	return (rp);
}

bool SelectiveRepeatPacketizer::packetize(TUNMessage *tunmsg) {

	static uint8_t id = 0;
	if (++id == 4) {
		id = 1;
	}

	Frame<RadioPacket> *frm = new Frame<RadioPacket>;
	RadioPacket *pointer = nullptr;

	int packetcounter = floor(tunmsg->size / 31.0);
	int totalpackets = packetcounter;

	int leftover = tunmsg->size - (totalpackets * 31);
	int first_pack_missing_bytes = 31 - leftover;

	for (int i = 0; i <= totalpackets; i++) {
		pointer = new RadioPacket;
		pointer->size = (i == 0 ? leftover : 31) + 1;
		pointer->data[0] = pack_info(i == 0, id, i == 0 ? totalpackets : i);

		int start = i == 0 ? 0 : (31 * i) - first_pack_missing_bytes;
		int finish =
				i == 0 ? leftover : (31 * (i + 1)) - first_pack_missing_bytes;
		int count = 0;

		for (int j = start; j < finish; j++) {
			pointer->data[1 + count++] = tunmsg->data[j];
		}
		frm->packets.push_front(pointer);
	}

	//printf("Packetizing message (size %i), CRC: %i\n", tunmsg->size,
	//		(int) gencrc(tunmsg->data, tunmsg->size));

	if (frames.empty()) {
		last_frame_change = current_millis();
	}

	frames.push_back(frm);

	return (true);
}

RadioPacket* SelectiveRepeatPacketizer::request_missing_packets(bool *array,
		unsigned int size) {

	unsigned int findings = 0;

	static RadioPacket *rp_answer = new RadioPacket;

	unsigned int rp_cursor = 0;
	for (unsigned int i = 0; i <= size; i++) {
		if (!array[i]) {
			findings++;

			rp_answer->data[1 + rp_cursor++] = pack_info(0, 0, i);
			printf("%i ", i);
		}
	}
	if (findings > 0) {
		printf("are missing packets\n");
		fragments_resent += findings;
		rp_answer->data[0] = pack_info(true, 0, findings);
		rp_answer->size = 1 + findings;
		//resend_list.push_front(rp_answer);
		return (rp_answer);
	} else
		return (nullptr);

}

inline void SelectiveRepeatPacketizer::response_packet_ok(uint8_t id) {
	//printf("OK per id %i\n", id);
	static RadioPacket *rp_answer = new RadioPacket;
	rp_answer->size = 1;
	rp_answer->data[0] = pack_info(true, 0, id);
	resend_list.push_front(rp_answer);
}

inline uint8_t SelectiveRepeatPacketizer::get_pack_id(RadioPacket *rp) {
	bool first = 0;
	uint8_t id = 0;
	uint8_t seg = 0;
	unpack_info(rp->data[0], first, id, seg);
	return (id);
}

bool SelectiveRepeatPacketizer::receive_packet(RadioPacket *rp) {

	//printf("[Packetizer] received (s %i) %s\n", rp->size, rp->data);

	// Handle system calls first
	bool first = 0;
	uint8_t id = 0;
	uint8_t seg = 0;
	unpack_info(rp->data[0], first, id, seg);

	// Is this a control packet?
	if (id == 0) {

		fragments_control++;
		// Does this packet carry a NAck or something else?
		// NOTE: NAck could be a confirmation if it carries 0 requests
		if (first) {
			//printf("Ricevuto pack: %i %s con valori %i %i %i\n", (int) rp->size,
			//		rp->data, (int) first, (int) id, (int) seg);

			// If it carries more than the info byte, assume these are all requests
			if (rp->size > 1) {
				int counter = 1;
				if (!frames.empty()) {
					static uint64_t last_request = 0;
					uint64_t cm;
					if ((cm = current_millis()) - last_request
							< Settings::minimum_ARQ_wait) {
						printf("Too many packet requests, denying\n");
						return (true);
					}
					printf("Requested packets: ");
					while (counter < rp->size) {

						bool pkt_boolean = 0;
						uint8_t pkt_id = 0;
						uint8_t pkt_seg = 0;

						unpack_info(rp->data[counter], pkt_boolean, pkt_id,
								pkt_seg);

						bool dbg2_pkt_boolean = 0;
						uint8_t dbg2_pkt_id = 0;
						uint8_t dbg2_pkt_seg = 0;
						unpack_info(frames.back()->packets[0]->data[0],
								dbg2_pkt_boolean, dbg2_pkt_id, dbg2_pkt_seg);

						unsigned int position = frames.front()->packets.size()
								- 1 - pkt_seg;

						if (position >= frames.front()->packets.size()) {
							printf("Request out of range\n");
							counter++;
							continue;
						}

						bool dbg_pkt_boolean = 0;
						uint8_t dbg_pkt_id = 0;
						uint8_t dbg_pkt_seg = 0;
						unpack_info(frames.front()->packets[position]->data[0],
								dbg_pkt_boolean, dbg_pkt_id, dbg_pkt_seg);

						printf("| (SEG: %i, carried: %i) ", pkt_seg,
								dbg_pkt_seg);
						//frames.front()->packets[position]->data);
						resend_list.push_back(
								frames.front()->packets[position]);

						counter++;
					}
					printf("\n");
				} else {
					printf("Frames requested but request can't be fulfilled\n");
				}

			} else if (!frames.empty()) {
				if (seg == get_pack_id(frames.front()->packets.front())) {
					// This is a confirmation! Next frame.
					received_ok();
					//printf("Confirmed packet\n");
				} else {
					// Undefined behaviour, wtf
					//received_ok();
					//std::cout
					//		<< "SelectiveRepeatPacketizer has hit a WTF checkpoint\n";
				}
			} else {
				//received_ok();
				//std::cout << "Received old OK\n";
			}
		} else {
			// This packet isn't carrying requests
			//printf("This packet isn't carrying requests\n");
		}

		return (true);
	}

	fragments_received++;

	static bool *received_packets_boolean = nullptr;
	static unsigned int packet_bool_length = ceil(this->get_mtu() / 31);
	if (buffer == nullptr) {
		buffer = new uint8_t[this->get_mtu() * 2] { 0 };
		received_packets_boolean = new bool[packet_bool_length] { 0 };
	}

	static uint8_t last_id = 0;
	static uint8_t current_id = 0;
	static uint8_t leftover = 0;

	//if (id != last_id) {

	/*if (current_id != id) {
	 printf("Packet %i was discarded\n", current_id);
	 current_id = id;
	 for (unsigned int b = 0; b <= packet_bool_length; b++) {
	 if (received_packets_boolean[b])
	 bytes_discarded += 31;
	 received_packets_boolean[b] = false;
	 }

	 }*/

	unsigned int data_size = ((char) (rp->size - 1));
	static unsigned int count_size = 0;

	if (first)
		leftover = 31 - data_size;

	//unsigned int copyposition = (unsigned int) (first ? leftover : 0)
	//		+ ((unsigned int) 31 * (first ? 0 : (unsigned int) seg));

	unsigned int copyposition = first ? leftover : 31 * seg;

	for (unsigned int i = 0; i < data_size; i++) {
		buffer[copyposition + i] = rp->data[i + 1];
		count_size++;
	}

	//strncpy((char*) (buffer + copyposition), (const char*) (rp->data + 1),
	//		copylength);
	/*if (first) {
	 printf(
	 "Copied in position (%i, +%i), first packet, expected total size (%i packets, %i chars, %i counted) \n\n",
	 copyposition, data_size, seg,
	 (31 * seg) + 31 - leftover, count_size);
	 count_size = 0;
	 } else
	 printf("Copied in position (%i, +%i), pack index %i \n\n",
	 copyposition, data_size, seg);
	 print_hex((rp->data + 1), data_size);*/

	unsigned int pos = first ? 0 : seg;
	received_packets_boolean[pos] = 1;
	//}

// Was this the last packet
	if (first) {

		if (id == last_id) {
			//printf("Double packet\n");
			response_packet_ok(id);
		} else {

			static RadioPacket *rp_answer = nullptr;
			if ((rp_answer = request_missing_packets(received_packets_boolean,
					seg))) {
				static uint64_t last_request = 0;
				uint64_t cm;
				if ((cm = current_millis()) - last_request
						> Settings::minimum_ARQ_wait) {
					last_request = cm;
					resend_list.push_front(rp_answer);
					printf("Reception is not okay, trying packet request\n");
				}

			} else {

				//printf("Reception is okay\n");
				frames_completed++;
				// Respond OK
				response_packet_ok(id);

				unsigned int packets_full = (seg);
				unsigned int first_packet_size = 31 - leftover;
				unsigned int message_size = (31 * packets_full)
						+ first_packet_size;

				static TUNMessage *tm = new TUNMessage { nullptr, 0 };
				tm->data = (buffer + leftover);
				tm->size = message_size;

				if (this->tun_handle->send(tm)) {
					//printf("TUN Handler accepted the data\n");
				} else {
					//printf("TUN Handler REJECTED the data\n");
				}
				//printf("Received message (size %i), CRC: %i\n", tm->size,
				//		(int) gencrc(tm->data, tm->size));
				//print_hex(tm->data, tm->size);

				for (unsigned int b = 0; b < packet_bool_length; b++)
					received_packets_boolean[b] = false;

				if (!((last_id == id - 1) || ((id - 1 == 0) && last_id == 3))) {
					printf("Last packet was lost! %i %i\n", last_id, id);
				}

				last_id = id;
				current_id = id + 1;
				if (current_id == 4)
					current_id = 1;

				//printf("Packet %i\n", id);

			}
		}
	}

	return (true);

}
