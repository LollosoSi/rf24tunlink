/*
 * RSSelectiveRepeatPacketizer.cpp
 *
 *  Created on: 4 Feb 2024
 *      Author: Andrea Roccaccino
 */

#include "RSSelectiveRepeatPacketizer.h"

RSSelectiveRepeatPacketizer::RSSelectiveRepeatPacketizer() :
		Telemetry("RSSelectiveRepeatPacketizer") {

	rsc = new RSCodec();

	returnvector = new std::string[7] { std::to_string(fragments_received),
			std::to_string(fragments_sent), std::to_string(fragments_resent),
			std::to_string(frames_completed), std::to_string(fragments_control),
			std::to_string(bytes_discarded), std::to_string(frames_dropped) };

	register_elements(new std::string[7] { "Fragments Received",
			"Fragments Sent", "Fragments Resent", "Frames Completed",
			"Control Fragments", "Bytes Discarded", "Dropped (timeout)" }, 7);

	last_frame_change = current_millis();

}

RSSelectiveRepeatPacketizer::~RSSelectiveRepeatPacketizer() {
	// TODO Auto-generated destructor stub
}

std::string* RSSelectiveRepeatPacketizer::telemetry_collect(
		const unsigned long delta) {

	returnvector[0] = (std::to_string(fragments_received));
	returnvector[1] = (std::to_string(fragments_sent));
	returnvector[2] = (std::to_string(fragments_resent));
	returnvector[3] = (std::to_string(frames_completed));
	returnvector[4] = (std::to_string(fragments_control));
	returnvector[5] = (std::to_string(bytes_discarded));
	returnvector[6] = (std::to_string(frames_dropped));

	//returnvector = { std::to_string(fragments_received), std::to_string( fragments_sent), std::to_string(fragments_resent), std::to_string(frames_completed), std::to_string(fragments_control) };

	fragments_received = fragments_sent = fragments_resent = frames_completed =
			fragments_control = bytes_discarded = frames_dropped = 0;

	return (returnvector);

}

inline bool RSSelectiveRepeatPacketizer::next_packet_ready() {
	return (Settings::control_packets || !frames.empty() || !resend_list.empty());
}

RadioPacket* RSSelectiveRepeatPacketizer::next_packet() {

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

	bool frames_empty = frames.empty(), resend_list_empty = resend_list.empty(),
			static_resend_list_empty = resend_list_static.empty();

	RadioPacket *rs = nullptr; // resend_list.front();
	if (static_resend_list_empty) {
		if (resend_list_empty) {

			if (frames_empty) {
				if (Settings::control_packets) {
					rs = (get_empty_packet());
					printf("Request response: EMPTY PACKET\n");
				} else {
					return (nullptr);
				}
			} else {
				if (current_packet_counter
						== frames.front()->packets.size() - 1) {

					static uint8_t cnt = 0;

					if (reset_cnt) {
						cnt = 0;
						reset_cnt = 0;
					}

					if (cnt++ % 5 == 0 || 1) {
						rs = (frames.front()->packets[current_packet_counter]);
						printf("Request response: LAST DATA PACKET\n");
					} else {
						rs = (get_empty_packet());
						printf(
								"Request response: EMPTY PACKET -- TOO MANY MISSING REQUESTS\n");
					}
				} else {
					fragments_sent++;
					rs = (frames.front()->packets[current_packet_counter++]);
					printf("Request response: DATA PACKET\n");
				}
			}

		} else {
			rs = resend_list.front();
			delete_list.push_back(rs);
			resend_list.pop_front();
			printf("Request response: RESEND LIST PACKET\n");
		}
	} else {
		rs = resend_list_static.front();

		resend_list_static.pop_front();
		printf("Request response: STATIC RESEND LIST PACKET\n");
	}
	return (rs);
}

inline RadioPacket* RSSelectiveRepeatPacketizer::get_empty_packet() {
	static RadioPacket *rp = new RadioPacket { { 0 }, 32 };
	return (rp);
}

bool RSSelectiveRepeatPacketizer::packetize(TUNMessage *tunmsg) {

	static uint8_t id = 0;
	if (++id == 4) {
		id = 1;
	}

	static unsigned int packsize = Settings::ReedSolomon::k - 1;

	Frame<RadioPacket> *frm = new Frame<RadioPacket>;
	RadioPacket *pointer = nullptr;

	int numpacks = floor((tunmsg->size + 1) / packsize);
	printf("Numpacks: %i, size: %i\n", numpacks, tunmsg->size);
	if (tunmsg->size <= (packsize - 1))
		numpacks = 0;

	int ct = numpacks;

	unsigned int dummysize = 0;

	int cursor = tunmsg->size;
	while ((cursor = (cursor - packsize)) > -1) {
		pointer = new RadioPacket;
		pointer->size = 32;
		pointer->data[0] = pack_info(false, id, ct--);

		unsigned char *dt = nullptr;
		dummysize = 0;

		memcpy(pointer->data + 1, tunmsg->data + cursor, packsize);
		rsc->encode(&dt, dummysize, static_cast<unsigned char*>(pointer->data),
				packsize + 1);
		memcpy(pointer->data, dt, dummysize);
		delete[] dt;
		frm->packets.push_back(pointer);
	}

	int _finish = cursor + packsize;

	pointer = new RadioPacket;
	pointer->size = 32;
	pointer->data[0] = pack_info(true, id, numpacks);
	pointer->data[1] = _finish;

	unsigned char *dt = nullptr;
	dummysize = 0;

	memcpy(pointer->data + 2, tunmsg->data, _finish);
	rsc->encode(&dt, dummysize, static_cast<unsigned char*>(pointer->data),
			packsize + 1);
	memcpy(pointer->data, dt, dummysize);
	delete[] dt;

	frm->packets.push_back(pointer);

	/*std::deque<RadioPacket*>::iterator it = frm->packets.begin();
	 while (it != frm->packets.end()) {
	 bool frst;
	 unsigned char seg;
	 unsigned char id;
	 this->unpack_info(((*it)->data)[0], frst, id, seg);
	 std::cout << "Pack " << (int) id << " seg " << (int) seg << ":\t";
	 for (int i = 0; i < 32; i++) {
	 std::cout << ((*it)->data[i]) << " ";
	 }
	 std::cout << "\n";
	 it++;
	 }*/

	if (frames.empty()) {
		last_frame_change = current_millis();
	}

	frames.push_back(frm);

	return (true);
}

RadioPacket* RSSelectiveRepeatPacketizer::request_missing_packets(bool *array,
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

inline void RSSelectiveRepeatPacketizer::response_packet_ok(uint8_t id) {
	printf("OK per id %i\n", id);
	RadioPacket *rp_answer = new RadioPacket { { 0 }, 32 };
	//if (!rp_answer) {
	rp_answer->size = 32;
	rp_answer->data[0] = pack_info(true, 0, id);
	unsigned char *dt = nullptr;
	unsigned int dummysize = 0;
	rsc->encode(&dt, dummysize, static_cast<unsigned char*>(rp_answer->data),
			Settings::ReedSolomon::k);
	memcpy(rp_answer->data, dt, dummysize);
	delete[] dt;
	//}

	resend_list.push_front(rp_answer);
}

inline uint8_t RSSelectiveRepeatPacketizer::get_pack_id(RadioPacket *rp) {
	bool first = 0;
	uint8_t id = 0;
	uint8_t seg = 0;
	unpack_info(rp->data[0], first, id, seg);
	return (id);
}

bool RSSelectiveRepeatPacketizer::receive_packet(RadioPacket *rp) {

	if (!rp)
		return (false);

	static int last_seg = 0;

	static bool *received_packs = new bool[120] { 0 };

	static unsigned char *msg = new unsigned char[32];
	unsigned int dummysize = 32;
	if (!rsc->decode(&msg, dummysize, static_cast<unsigned char*>(rp->data),
			32)) {
		--last_seg;
		printf("Decode failed!");
		if (last_seg > 0) {
			last_seg--;
			printf("Guess: %i\n", last_seg);
			RadioPacket *mrp = new RadioPacket;
			mrp->size = 32;
			mrp->data[0] = pack_info(true, 0, 1);
			mrp->data[1] = pack_info(true, 0, last_seg);
			static unsigned char *dt = new unsigned char[32] { 0 };
			unsigned int dummysize = 0;
			rsc->encode(&dt, dummysize, (unsigned char*) mrp->data,
					Settings::ReedSolomon::k);
			memcpy(mrp->data, dt, dummysize);
			//delete[] dt;

			resend_list.push_front(mrp);

		}
		return (false);
	}

	static int packsize = Settings::ReedSolomon::k - 1;

	if (buffer == nullptr) {
		buffer = new uint8_t[this->get_mtu() * 2] { 0 };
	}

	// Handle system calls first
	bool first = 0;
	uint8_t id = 0;
	uint8_t seg = 0;
	unpack_info(msg[0], first, id, seg);

	//printf("[Packetizer] received f: %d, id: %d, seg: %d (s %i) %s\n", first,
	//		id, seg, rp->size, msg);

	// Is this a control packet?
	if (id == 0) {
		fragments_control++;

		// Find what is the packet carrying
		// NOTE: NAck is ACK if it carries 0 requests
		if (first) {

			// This packetizer is meant to be used with 32 byte packets, let's count the non-zero bytes
			unsigned int size = 0;
			if (!frames.empty()) {
				if (seg != get_pack_id(frames.front()->packets.front())) {
					size = seg + 1;
				} else {
					for (; size < Settings::ReedSolomon::k; ++size) {
						if (msg[size] == 0)
							break;
					}
				}
			} else {
				for (; size < Settings::ReedSolomon::k; ++size) {
					if (msg[size] == 0)
						break;
				}
			}

			if (size > 1) {
				//printf("Received missing packet request: size %i\n", size);
				// We have requests bois.
				// Make sure we have frames queued.
				if (!frames.empty()) {

					// Lets queue all the requested packets.
					for (unsigned int i = 1; i < size; i++) {
						bool pkt_boolean = 0;
						uint8_t pkt_id = 0;
						uint8_t pkt_seg = 0;

						unpack_info(rp->data[i], pkt_boolean, pkt_id, pkt_seg);

						// Pkt seg is the segment number.
						// Remember this scheme:
						// Queued segs: 4 3 2 1 0
						// Queue size: 5
						// request: 4 means size - 1 - request
						unsigned int position = frames.front()->packets.size()
								- 1 - pkt_seg;

						if (position >= frames.front()->packets.size()) {
							printf("Request out of range\n");
						} else {
							fragments_resent++;
							bool dbg_pkt_boolean = 0;
							uint8_t dbg_pkt_id = 0;
							uint8_t dbg_pkt_seg = 0;
							unpack_info(
									frames.front()->packets[position]->data[0],
									dbg_pkt_boolean, dbg_pkt_id, dbg_pkt_seg);

							//printf("| (Requested: %i, loaded: %i) ", pkt_seg, dbg_pkt_seg);
							resend_list_static.push_back(
									frames.front()->packets[position]);
						}
					}
					//	printf("\n");

				}

			} else if (!frames.empty()) {
				if (seg == get_pack_id(frames.front()->packets.front())) {
					// This is a confirmation! Next frame.
					received_ok();
					//printf("Confirmed packet\n");
				} else {
					// Undefined behaviour, wtf
					//received_ok();
					std::cout
							<< "SelectiveRepeatPacketizerRS has hit a WTF checkpoint\n";
				}
			} else {
				//received_ok();
				//std::cout << "Received old OK\n";
			}

		} else {
			// Packet isn't carrying requests
		}

		return (true);
	}

	static uint8_t last_id = 0;
	static uint8_t current_id = 0;
	static int max_seg = 0;

	static int bool_pos = 0;
	bool_pos = first ? 0 : seg;

	static int start_pos = 0;
	start_pos = first ? packsize - 1 - msg[1] : seg * (packsize) - 1;

	static unsigned int message_start = 0;
	static unsigned int message_length = 0;
	static unsigned int message_packs = 0;

	if (id == last_id) {
		//printf("Old packet, responding ok\n");
		response_packet_ok(id);
		return (false);
	}

	if (id != current_id) {
		// We are receiving something that is not what we were set up for
		// Must reset the reception list.
		current_id = id;

		// Erase reception
		// This approach is okay for any iteration
		for (int i = 0; i < max_seg + 1; i++) {
			received_packs[i] = 0;
		}

		max_seg = 0;
		message_start = 0;
		message_length = 0;
		message_packs = 0;
	}

	if (first) {
		message_start = start_pos;
		message_packs = seg;
		message_length = (seg * packsize) + (packsize - start_pos - 1);

		//printf(
		//		"Packet carried info -> message_start: %i, message_packs: %i, message_length: %i\n",
		//		message_start, message_packs, message_length);

		if (message_start < 0) {
			printf("Negative start!\n");
			exit(1);
		}

	}

	if (received_packs[bool_pos]) {
		//printf("Duplicate segment (first: %i)\n", first);

	} else {
		fragments_received++;
		received_packs[bool_pos] = true;
		memcpy(buffer + start_pos, msg + (first ? 2 : 1),
				first ? msg[1] : packsize);

		if (bool_pos > max_seg)
			max_seg = bool_pos;

		last_seg = bool_pos;
	}

	// Find if this was the last pack to be received (see items right to left)
	// Or exit
	for (int i = bool_pos; i >= 0; i--) {
		if (!received_packs[i]) {
			//printf("Packet %i still missing\n", i);
			return (true);
		}
	}

	RadioPacket *missing_rp = new RadioPacket { { 0 }, 32 };
	uint8_t missing_count = 0;
	// Find missing packets left to right
	for (unsigned int i = bool_pos; i < message_packs + 1; i++) {
		if (missing_count >= packsize)
			break;
		if (!received_packs[i]) {
			missing_rp->data[1 + missing_count++] = pack_info(false, id, i);
			//printf(" | Missing packet %i ", i);
		}
	}

	if (missing_count != 0) {
		//printf("\t");

		printf("Missing %i packets!\n", missing_count);
		for (unsigned int i = missing_count; i < Settings::ReedSolomon::k + 1; i++) {
			missing_rp->data[1 + i] = 0;
		}

		missing_rp->size = 32;
		missing_rp->data[0] = pack_info(true, 0, missing_count);
		unsigned char *dt = nullptr;
		unsigned int dummysize = 0;
		rsc->encode(&dt, dummysize,
				static_cast<unsigned char*>(missing_rp->data),
				Settings::ReedSolomon::k);
		memcpy(missing_rp->data, dt, dummysize);
		delete[] dt;

		resend_list.push_front(missing_rp);
		return (true);
	}

	// We're free to go! Send the message to the interface.
	static TUNMessage *tunm = new TUNMessage;
	tunm->size = message_length;
	tunm->data = buffer + message_start;
	response_packet_ok(current_id);
	if (tun_handle->send(tunm)) {
		// Okay, yoohoo
		this->frames_completed++;
	} else {
		// We fucked up somewhere

	}

	// Erase reception
	// Done before
	//for (int i = 0; i < max_seg + 1; i++) {
	//	received_packs[i] = 0;
	//}

	last_id = current_id;
	//max_seg = 0;
	message_start = 0;
	message_length = 0;
	message_packs = 0;

	return (true);

}
