/*
 * SelectiveRepeatPacketizer.cpp
 *
 *  Created on: 4 Feb 2024
 *      Author: Andrea Roccaccino
 */

#include "SelectiveRepeatPacketizer.h"

SelectiveRepeatPacketizer::SelectiveRepeatPacketizer() :
		Telemetry("SelectiveRepeatPacketizer") {

	returnvector =
			new std::string[5] { std::to_string(fragments_received),
					std::to_string(fragments_sent), std::to_string(
							fragments_resent), std::to_string(frames_completed),
					std::to_string(fragments_control) };

	register_elements(new std::string[5] { "Fragments Received",
			"Fragments Sent", "Fragments Resent", "Frames completed",
			"Control Fragments" }, 5);

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
	//returnvector = { std::to_string(fragments_received), std::to_string( fragments_sent), std::to_string(fragments_resent), std::to_string(frames_completed), std::to_string(fragments_control) };
	fragments_received = fragments_sent = fragments_resent = frames_completed =
			fragments_control = 0;
	return (returnvector);

}

bool SelectiveRepeatPacketizer::next_packet_ready() {
	return (Settings::control_packets || !frames.empty() || !resend_list.empty());
}

RadioPacket* SelectiveRepeatPacketizer::next_packet() {
	bool frames_empty = frames.empty(), resend_list_empty = resend_list.empty();

	RadioPacket *rs = resend_list.front();
	if (resend_list_empty) {

		if (frames_empty) {
			if (Settings::control_packets) {
				rs = (get_empty_packet());
			} else {
				std::cout << "Requested frame when no one is available\n";
				exit(2);
			}
		} else {
			if (current_packet_counter == frames.front()->packets.size() - 1) {
				rs = (frames.front()->packets[current_packet_counter]);
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
	/*
	 if (Settings::control_packets) {
	 if (frames_empty && resend_list_empty) {
	 return (get_empty_packet());
	 } else {
	 if (current_packet_counter == frames.front()->packets.size() - 1) {
	 if (resend_list.empty())
	 return (frames.front()->packets[current_packet_counter]);
	 else {
	 RadioPacket *rs = resend_list.front();
	 resend_list.pop_front();
	 return (rs);
	 }
	 } else {
	 fragments_sent++;
	 return (frames.front()->packets[current_packet_counter++]);
	 }
	 }
	 } else {
	 if (frames.empty() && resend_list.empty()) {
	 // Undefined behaviour
	 std::cout << "Requested frame when no one is available\n";
	 exit(2);
	 } else {
	 if (current_packet_counter == frames.front()->packets.size() - 1) {
	 if (resend_list.empty())
	 return (frames.front()->packets[current_packet_counter]);
	 else {
	 RadioPacket *rs = resend_list.front();
	 resend_list.pop_front();
	 return (rs);
	 }
	 } else {
	 fragments_sent++;
	 return (frames.front()->packets[current_packet_counter++]);
	 }
	 }
	 }
	 */
}

void SelectiveRepeatPacketizer::free_frame(Frame<RadioPacket> *frame) {

	while (!frame->packets.empty()) {
		frame->packets.pop_front();
	}

}

RadioPacket* SelectiveRepeatPacketizer::get_empty_packet() {
	static RadioPacket *rp = new RadioPacket { { 0 }, 1 };
	return (rp);
}

bool SelectiveRepeatPacketizer::packetize(TUNMessage *tunmsg) {

	//printf("Packetizing message (size %i) %s\n", tunmsg->size, tunmsg->data);

	static uint8_t id = 0;
	if (++id == 4) {
		id = 1;
	}

	Frame<RadioPacket> *frm = new Frame<RadioPacket>;
	RadioPacket *pointer = nullptr;

	int tracker = tunmsg->size - 31;

	int packetcounter = floor(tunmsg->size / 31.0);
	int totalpackets = packetcounter;

	int leftover = tunmsg->size - (totalpackets * 31);
	int first_pack_missing_bytes = 31 - leftover;

	for (int i = 0; i <= totalpackets; i++) {
		pointer = new RadioPacket;
		pointer->size = (i == 0 ? leftover : 31) + 1;
		pointer->data[0] = pack_info(i == 0, id, i == 0 ? totalpackets : i);

		int start = i==0 ? 0 : (31 * i)-first_pack_missing_bytes, finish = i == 0 ? leftover : (31 * (i + 1))-first_pack_missing_bytes, count = 0;

		for (int j = start; j < finish; j++) {
			pointer->data[1 + count++] = tunmsg->data[j];
		}
		frm->packets.push_front(pointer);
		//printf("Pacchetto creato: %i, pack: %i posizione: %i\n",
		//		(int) pointer->size-1, i, start);
		//print_hex(pointer->data + 1, pointer->size - 1);
	}

	/*
	 for (i = totalpackets; i > 0; i--) {
	 pointer = new RadioPacket;
	 pointer->size = 32;
	 pointer->data[0] = pack_info(false, id, i);
	 for (int i = 0; i < pointer->size - 1; i++) {
	 ((pointer->data) + 1)[i] = tunmsg->data[i + tracker];
	 }
	 //strncpy((char*) ((pointer->data) + 1),
	 //		(const char*) (tunmsg->data + tracker), 31);

	 frm->packets.push_back(pointer);
	 printf("Pacchetto creato: %i, pack: %i posizione: %i\n",
	 (int) pointer->size, i, tracker);
	 print_hex(pointer->data + 1, pointer->size - 1);

	 tracker -= 31;
	 }

	 pointer = new RadioPacket;
	 pointer->data[0] = pack_info(true, id, totalpackets);
	 pointer->size = 1 + (31 + tracker);
	 for (int i = 0; i < pointer->size - 1; i++) {
	 ((pointer->data) + 1)[i] = tunmsg->data[i];
	 }
	 //strncpy((char*) ((pointer->data) + 1), (const char*) (tunmsg->data),
	 //		pointer->size-1);
	 frm->packets.push_back(pointer);
	 printf("Pacchetto creato: %i\n", (int) pointer->size);
	 print_hex(pointer->data + 1, pointer->size - 1);
 */


	 frames.push_back(frm);


	return (true);
}

bool SelectiveRepeatPacketizer::request_missing_packets(bool *array,
		unsigned int size) {

	unsigned int findings = 0;

	static RadioPacket *rp_answer = new RadioPacket;

	unsigned int rp_cursor = 0;
	for (unsigned int i = 0; i <= size; i++) {
		if (!array[i]) {
			findings++;

			rp_answer->data[1 + rp_cursor++] = pack_info(0, 0, i);
		}
	}
	if (findings > 0) {
		fragments_resent += findings;
		rp_answer->data[0] = pack_info(true, 0, findings);
		rp_answer->size = 1 + findings;
		resend_list.push_front(rp_answer);
		return (true);
	} else
		return (false);

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
							continue;
						}

						bool dbg_pkt_boolean = 0;
						uint8_t dbg_pkt_id = 0;
						uint8_t dbg_pkt_seg = 0;
						unpack_info(frames.front()->packets[position]->data[0],
								dbg_pkt_boolean, dbg_pkt_id, dbg_pkt_seg);

						printf("| (SEG: %i, carried: %i) %s ", pkt_seg,
								dbg_pkt_seg,
								frames.front()->packets[position]->data);
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

	static uint8_t *buffer = nullptr;
	static bool *received_packets_boolean = nullptr;
	if (buffer == nullptr) {
		buffer = new uint8_t[this->get_mtu()] { 0 };
		received_packets_boolean = new bool[this->get_mtu() / 31] { 0 };
	}

	static uint8_t last_id = 0;
	static uint8_t leftover = 0;

	if (id != last_id) {

		if (first)
			leftover = 31 - (rp->size - 1);

		int copyposition = (int)(first ? leftover : 0)
				+ ((int)31 * (first ? 0 : (int)seg));
		uint8_t copylength = (rp->size - 1);

		for (int i = 0; i < rp->size - 1; i++) {
			buffer[copyposition + i] = rp->data[i + 1];
		}

		//strncpy((char*) (buffer + copyposition), (const char*) (rp->data + 1),
		//		copylength);

		//printf("Copied in position (%i, +%i), pack index %i \n", copyposition, copylength, seg);
		//print_hex((rp->data + 1), copylength);

		unsigned int pos = first ? 0 : seg;
		received_packets_boolean[pos] = 1;
	}

// Was this the last packet
	if (first) {

		if (id == last_id) {
			//printf("Double packet\n");
			response_packet_ok(id);
		} else {

			if (request_missing_packets(received_packets_boolean, seg)) {
				printf("Reception is not okay, trying packet request\n");

			} else {
				//printf("Reception is okay\n");
				frames_completed++;
				// Respond OK
				response_packet_ok(id);
				unsigned int packets_full = (seg);
				unsigned int first_packet_size = 31 - leftover;
				unsigned int message_size = (31 * packets_full)
						+ first_packet_size;
				//printf("Packets full: %i\tFirst packet size: %i\t Message size: %i\n",packets_full,first_packet_size,message_size);
				static TUNMessage *tm = new TUNMessage { nullptr, 0 };
				tm->data = (buffer + leftover);
				tm->size = message_size;
				//std::cout << "Message length: " << (unsigned int)message_size << std::endl;
				if (this->tun_handle->send(tm)) {
					//printf("TUN Handler accepted the data\n");
				} else {
					//printf("TUN Handler REJECTED the data\n");
				}

				for (unsigned int b = 0; b <= seg; b++)
					received_packets_boolean[b] = false;

				last_id = id;
			}
		}
	}

	return (true);

}
