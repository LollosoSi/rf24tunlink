/*
 * RadioAndPacketHandler.h
 *
 *  Created on: Oct 18, 2023
 *      Author: andrea
 */

#ifndef RADIOANDPACKETHANDLER_H_
#define RADIOANDPACKETHANDLER_H_

using namespace std;

#include "Settings.h"

#include <cmath>

#include "Callback.h"

#include <RF24/RF24.h>
#include <deque>
#include <mutex>

struct radiopacketwithsize {
	radiopacket *rp = nullptr;
	uint8_t total_size = 0;
};

static uint8_t genrpwscrc(radiopacketwithsize &rpws) {
	return gencrc((uint8_t*) rpws.rp, (int) (rpws.total_size - 2));
}

static void setrpwscrc(radiopacketwithsize &rpws) {
	rpws.rp->data[rpws.total_size - 2] = genrpwscrc(rpws);
	//printf("Set packet CRC %d\n", rpws.rp->data[rpws.total_size - 2]);
}

static bool validaterpwscrc(radiopacketwithsize &rpws) {
	//printf("Received packet CRC: %d, should be %d\n",
	//		rpws.rp->data[(int) (rpws.total_size - 2)], genrpwscrc(rpws));
	return genrpwscrc(rpws) == rpws.rp->data[(int) (rpws.total_size - 2)];
}

static uint8_t extractpacketid(radiopacketwithsize &rpws) {
	return extractpacketid((uint8_t) rpws.rp->info);
}
static uint8_t extractpacketsegment(radiopacketwithsize &rpws) {
	return extractpacketsegment((uint8_t) rpws.rp->info);
}

class RadioAndPacketHandler {

private:

	std::mutex m;

	bool primary = true;
	uint8_t write_address[Settings::addressbytes + 1] = { 0 };
	uint8_t read_address[Settings::addressbytes + 1] = { 0 };

	RF24 *radio = nullptr;

protected:
	radiopacket* getNewEmptyRadioPacket() {
		return new radiopacket { 0, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } };
	}
	radiopacketwithsize emptypacket { getNewEmptyRadioPacket(), 2 };
	void makenextemptypacket(radiopacketwithsize &rpws) {

		rpws.total_size = 2;

		rpws.rp->info = makeidsegment(0,
				((int) extractpacketsegment(rpws.rp) + (int) 1) % 16, 0);
		setrpwscrc(rpws);

		//	printf("Empty packet contents:\n");
		//	radioppppprintchararray((uint8_t*) rpws.rp, 32);
	}
	radiopacketwithsize nextPacket() {
		if (packetqueue.empty()) {

			static int c = 0;

			radiopacketwithsize rpwss;
			rpwss.rp = new radiopacket{makeidsegment(0,((int) c + (int) 1) % 16,0)};
			rpwss.total_size = 2;
			setrpwscrc(rpwss);

			setNextBufferPacket(rpwss);
			//makenextemptypacket(emptypacket);
			//printf("Loading EMPTY Packet: ID %d, seg %d, prec %d, size %d",
			//		extractpacketid(emptypacket.rp),
			//		extractpacketsegment(emptypacket.rp),
			//		extractpacketprecision(emptypacket.rp),
			//		emptypacket.total_size);
			//radioppppprintchararray((uint8_t*) emptypacket.rp,
			//		emptypacket.total_size);
			return emptypacket;
		} else {

			radiopacketwithsize rpws = packetqueue.front();
			printf("Loading Packet: ID %d, seg %d, prec %d, size %d",
					extractpacketid(rpws.rp), extractpacketsegment(rpws.rp),
					extractpacketprecision(rpws.rp), rpws.total_size);
			radioppppprintchararray((uint8_t*) rpws.rp, rpws.total_size);
			packetqueue.pop_front();
			setNextBufferPacket(rpws);
			return rpws;
		}
	}

	std::deque<radiopacketwithsize> packetqueue;

	static const unsigned short maxcircularbuffersize = 10;
	unsigned short buffercursor = 0;
	radiopacketwithsize deletionbuffer[maxcircularbuffersize] = { nullptr };

	void setNextBufferPacket(radiopacketwithsize rpws) {
		//printf("Writing in circular buffer\n");
		if (deletionbuffer[buffercursor].rp != nullptr)
			delete deletionbuffer[buffercursor].rp;
		deletionbuffer[buffercursor] = rpws;
		buffercursor = (buffercursor + 1) % maxcircularbuffersize;
	}

	void findandreloadfrombuffer(uint8_t *elements, uint8_t size) {

		std::deque<radiopacketwithsize> tempdeque;

		uint8_t found_elements = 0;
		int cursor = buffercursor;
		do {
			if (--cursor == -1) {
				cursor = maxcircularbuffersize - 1;
			}
			radiopacketwithsize rpws = deletionbuffer[cursor];
			for (int i = 0; i < size; i++)
				if (rpws.rp != nullptr) {
					if ((extractpacketid(rpws.rp)
							== extractpacketid(elements[i]))
							&& (extractpacketprecision(elements[i]) ?
									extractpacketprecision(rpws.rp->info) :
									extractpacketsegment(rpws.rp)
											== extractpacketsegment(elements[i]))) {
						tempdeque.push_front(rpws);
						deletionbuffer[cursor].rp = nullptr;
						found_elements++;
						//printf("Found element id %d seg %d",
						//		extractpacketid(elements[i]),
						//		extractpacketsegment(elements[i]));
						break;
					}
				}

		} while ((cursor != buffercursor) && (found_elements != size));

		printf("Resuscitated packets in buffer. Asked: %d, found: %d\n", size,
				found_elements);

		while (!tempdeque.empty()) {
			packetqueue.push_front(tempdeque.front());
			tempdeque.pop_front();
		}

	}

	bool processEmptyPacket(radiopacketwithsize rpws) {
		if (rpws.total_size >= 2) {
			//printf("Received empty packet contents:\n");
			//radioppppprintchararray((uint8_t*) rpws.rp, rpws.total_size);

			if (validaterpwscrc(rpws)) {
				//printf("Valid rpws crc\n");
				if (rpws.total_size > 2)
					findandreloadfrombuffer(rpws.rp->data, rpws.total_size - 2);
				return true;
			} else {
				printf("Invalid rpws crc\n");
				return false;
			}
		} else {
			printf("Invalid size rpws\n");
			return false;
		}
	}

public:
	Callback *c = nullptr;
	RadioAndPacketHandler() {

	}

	void setup(Callback *c, bool primary,
			uint8_t write_address[Settings::addressbytes + 1],
			uint8_t read_address[Settings::addressbytes + 1]) {
		this->c = c;
		this->primary = primary;
		memcpy(this->write_address, write_address, Settings::addressbytes + 1);
		memcpy(this->read_address, read_address, Settings::addressbytes + 1);

		for (int i = 0; i < idnum; i++) {
			for (int j = 0; j < segnum; j++) {
				segments[i][j] = false;
			}
		}

		resetRadio();
	}

	bool sendCustomRadioPacket(radiopacketwithsize &srpws) {
		return radio->writeFast(srpws.rp, srpws.total_size);
	}
	bool loadCustomRadioPacket(radiopacketwithsize &srpws) {
		return radio->writeAckPayload(1, srpws.rp, srpws.total_size);
	}

	unsigned long telemetry_ms = 0, packets_ok = 0, packets_failed = 0,
			packets_control = 0, packets_dropped = 0;

	radiopacketwithsize rpws = { new radiopacket, 0 };
	radiopacketwithsize sendrpws;

	void loop() {

		if(sendrpws.rp == nullptr){
			sendrpws = nextPacket();
		}

		if (millis() > telemetry_ms + 1000) {
			telemetry_ms = millis();
			printf(
					"Packets OK: %d, Failed: %d, Control: %d, Dropped: %d, Queue size: %d\n",
					(unsigned int) packets_ok, (unsigned int) packets_failed,
					(unsigned int) packets_control,
					(unsigned int) packets_dropped,
					(unsigned int) packetqueue.size());
			packets_ok = packets_failed = packets_control = packets_dropped = 0;

			//snapshot();

		}

		if (radio->failureDetected
				|| (radio->getDataRate() != Settings::data_rate)
				|| (!radio->isChipConnected())) {
			radio->failureDetected = 0;
			resetRadio();
			return;
		}

		if (primary) {
			while (radio->available()) {
				radio->read(rpws.rp,
						(rpws.total_size = radio->getDynamicPayloadSize()));
				receiveMessage2(rpws);
			}
			m.lock();
			while ((sendCustomRadioPacket(sendrpws) && radio->txStandBy())) {
				sendrpws = nextPacket();
			}
			m.unlock();
		} else {
			//if (radio->rxFifoFull()) {
				while (radio->available()) {
					radio->read(rpws.rp,
							(rpws.total_size = radio->getDynamicPayloadSize()));
					receiveMessage2(rpws);
				}
				//radio->flush_tx();
				m.lock();
				while (loadCustomRadioPacket(sendrpws)) {
					sendrpws = nextPacket();
				}
				m.unlock();
			//}

		}

		//if (radio->rxFifoFull()) {

		//}

		/*if (primary) {
		 //printf("Sending data\n");
		 if (!packetqueue.empty() || Settings::use_empty_packets)
		 if (sendCustomRadioPacket(sendrpws)) {
		 sendrpws = nextPacket();
		 if (Settings::auto_ack || Settings::ack_payloads)
		 while (radio->available()) {
		 //printf("Read data 1\n");

		 //handle received packet(s)
		 radio->read(rpws.rp,
		 (rpws.total_size =
		 radio->getDynamicPayloadSize()));
		 receiveMessage2(rpws);
		 }
		 // Delete old packet(s) from memory
		 }
		 // Not required due to auto-ack
		 if (!radio->txStandBy()) {
		 //	radio->reUseTX();
		 }
		 if (Settings::auto_ack || Settings::ack_payloads)
		 while (radio->available()) {
		 //printf("Read data 2\n");
		 // Handle rest of the packets
		 radio->read(rpws.rp,
		 (rpws.total_size = radio->getDynamicPayloadSize()));
		 receiveMessage2(rpws);
		 }
		 } else {
		 while (radio->available()) {

		 // Handle received packet(s)
		 //printf("Loading data 1\n");
		 if (Settings::ack_payloads)
		 while (loadCustomRadioPacket(sendrpws)) {
		 sendrpws = nextPacket();
		 // Delete old packet(s) --- handled in nextpacket
		 }

		 //printf("Read data\n");
		 radio->read(rpws.rp,
		 (rpws.total_size = radio->getDynamicPayloadSize()));
		 receiveMessage2(rpws);

		 }
		 // This line might not be needed
		 //printf("Loading data 2\n");
		 //if (loadCustomRadioPacket(sendrpws)) {
		 sendrpws = nextPacket();
		 //	printf("Loaded more data\n");
		 // Do everything again in case TX buffer is not filled
		 //}
		 }*/

	}

	void addControlRecoveryPacket(uint8_t *pidseg, int size) {

		if (size >= 30) {
			printf("Requested too many packets\n");
			return;
		}

		radiopacketwithsize rpws { getNewEmptyRadioPacket(),
				(uint8_t) (2 + size) };

		for (int i = 0; i < size; i++) {
			rpws.rp->data[i] = pidseg[i];
		}
		makenextemptypacket(rpws);
		packetqueue.push_front(rpws);
		//printf("Written control packets\n");
	}

	static const int segnum = (int) pow(2, Settings::segmentbits), idnum =
			(int) pow(2, Settings::idbits);

	uint8_t last_id = 0;
	uint8_t buffers[idnum][Settings::mtu];
	uint8_t max_seg[idnum] { 0 };
	uint8_t min_seg[idnum] { segnum };
	uint8_t len[idnum] { 0 };
	uint8_t firstpacksize[idnum] { 0 };
	uint8_t last_seg[idnum] { 0 };
	bool segments[idnum][segnum];

	unsigned long bad_packets_counter = 0;

	void handleData2(uint8_t *data, unsigned int size) {

		std::deque<radiopacketwithsize> tempdeque;

		//if (packetqueue.size() > 150) {
		//	packetqueue.clear();
		//}

		static uint8_t id = 1;
		int packnum = ceil(size / 31.0);

		int cursor = size;

		int firstpacksize = size - (31 * ((int) floor(size / 31.0)));
		if (firstpacksize == 0)
			firstpacksize = 31;
		//printf("First pack size: %d", );

		while (packnum > 0) {
			packnum--;
			bool precision = packnum == 0;
			radiopacketwithsize rpws = { new radiopacket { makeidsegment(id,
					precision ? ceil(size / 31.0) : packnum, precision) },
					(uint8_t) (1
							+ ((uint8_t) ((precision ? firstpacksize : 31)))) };

			for (int i = 0; i < rpws.total_size - 1; i++) {
				rpws.rp->data[(rpws.total_size - 2) - i] = data[--cursor];
			}

			if (extractpacketid(rpws) != id)
				printf("Wrong id inside packet\n");

			if (extractpacketsegment(rpws)
					!= (precision ? ceil(size / 31.0) : packnum))
				printf("Wrong packnum inside packet, expected: %d, value: %d\n",
						(int) (precision ? ceil(size / 31.0) : packnum),
						(int) extractpacketsegment(rpws));

			//printf("packet split %d", packnum);
			//radioppppprintchararray((uint8_t*) rpws.rp + 1, 31);

			tempdeque.push_front(rpws);

			//receiveMessage(rpws);

		}
		if (!tempdeque.empty()) {
			m.lock();
			while (!tempdeque.empty()) {
				packetqueue.push_back(tempdeque.front());
				tempdeque.pop_front();
			}
			m.unlock();
		}

		//printf("Pack Written: id %d, len %d, crc %d\n", id, size,
		//		gencrc(data, size));

		//printf(
		//		"-----------------------------------------------------------\nSend message. Id: %d\tParts: %d\Length: %d\tcrc: %d",
		//		id, (int) ceil(size / 31.0), size, gencrc(data, size));
		//radioppppprintchararray((uint8_t*) data, size);
		//printf(
		//		"------------------------------------------------------------------------------\n");

		id = (id + 1) % ((int) pow(2, Settings::idbits));
		if (id == 0)
			++id;

	}

	// Tracks how many packets are left.
	// 0 : message complete(d). Write to buffer and clear
	int packetsleft[idnum] { 0 };
	int packtot[idnum] { 0 };

	// Offsets from zero. Where to start reading the buffer. Remember that the first message is the smaller one
	int offsets[idnum] { 0 };

	// Total packet length. Calculated when resetting with a new packet
	int packlen[idnum] { 0 };

	// Maximum number of segment we see. Useful for detecting lost packets
	uint8_t max_psgn[idnum] { 0 };

	// Keeps track of the segments we already checked for missing packets
	uint8_t max_packetwindow[idnum] { 0 };

	// Keep track of the bad packets (bad packet is a control packet with invalid crc or a packet with incorrect length)
	unsigned int badpackets = 0;

	// Meant to be used in pair with handleData2
	void receiveMessage2(radiopacketwithsize rpws) {

		// Packet size > 32 is unlikely, while a functioning system won't transmit payloads smaller than 2 bytes (at least info + crc)
		if (rpws.total_size < 2 || rpws.total_size > 32) {
			// Invalid packet! Something is up with one or both radios.
			printf("Invalid packet. Size: %d\n", rpws.total_size);
			return;
		}

		// Initial data
		// pid: packet id
		// psgn: packet segment number (OR number of segments if precision == 1)
		// precision: indicates whether psgn means segment number start or if precision is 0 psgn means the total number of segments of the packet
		static uint8_t pid, psgn;
		static bool precision = 0;
		pid = extractpacketid(rpws);

		if (rpws.total_size == 2 && pid == 3) {
			printf("Possible buggy packet detected, old pid: %d, new: %d.", pid,
					extractpacketid(rpws.rp->data[0]));
			if (extractpacketid(rpws.rp->data[0]) == 0) {
				printf("Should be safe to ignore\n");
				return;
			}
			printf("\n");
		}

		// Check for a control packet. ID == 0, psgn is incremental and is useful to find lost packets (even if only control)
		// Payload will always have last byte CRC, while bytes of data before crc are information about the lost packets (precision + id + segment)
		if (pid == 0 /*|| rpws.total_size == 2*/) {
			//printf("Reading EMPTY Packet: ID %d, seg %d, prec %d, size %d",
			//		extractpacketid(rpws.rp), extractpacketsegment(rpws.rp),
			//		extractpacketprecision(rpws.rp), rpws.total_size);
			//radioppppprintchararray((uint8_t*) rpws.rp, rpws.total_size);
			if (validaterpwscrc(rpws)) {
				badpackets = 0;
				packets_control++;
				if (rpws.total_size > 2)
					findandreloadfrombuffer(rpws.rp->data, rpws.total_size - 2);

			} else {
				if (badpackets++ > Settings::maxbadpackets) {
					badpackets = 0;
					printf("Reached bad packets limit \n");
				}

				printf("Packet wasn't validated. Contents follow:");
				radioppppprintchararray((uint8_t*) rpws.rp, rpws.total_size);
			}
			return;

		}

		printf("Reading Packet: ID %d, seg %d, prec %d, size %d",
				extractpacketid(rpws.rp), extractpacketsegment(rpws.rp),
				extractpacketprecision(rpws.rp), rpws.total_size);
		radioppppprintchararray((uint8_t*) rpws.rp, rpws.total_size);

		// Once the packet is ensured to be correct, calculate the remaining info
		precision = extractpacketprecision(rpws.rp->info);
		psgn = precision ? 0 : extractpacketsegment(rpws);

		// What id was the last packet. ID 0 is not tracked
		// This is useful for a specific case: last packet of the previous id failed to complete a whole message
		static uint8_t last_id = 0;

		// Track info of missing packets
		static uint8_t missinginfos[segnum] { 0 };
		static int missingpackets = 0;

		// Is the packet sequential? (Poor try of filtering out older packets and avoids double requests)
		if (last_id == (pid - 1 == 0 ? idnum - 1 : pid - 1)) {
			// TODO: find a good algorithm for this task
		}
		last_id = pid;

		// Logic:
		// First packet sent is the one with precision == 1. Marks the total number of packets to wait for and the total size of the packet
		// Each packet received can help identifying whether some packets have been lost between received and first
		// Each packet will be validated then written to buffer[pid]

		// This is the first packet. Initialize buffers for reception if required.
		// Packets sent earlier with this same id are no longer recoverable / requesting them is nonsensical
		if (precision) {

			// Reset the maximum segment received
			max_psgn[pid] = max_packetwindow[pid] = psgn;

			if (packetsleft[pid] != 0) {
				// Previous packet was incomplete!
				// The entire data unit is lost unfortunately
				// This can happen in case of unstable configuration, unstable signal or cabling
				// Not much left to do here.
				packets_dropped++;
			}

			// Reset the received packets. This is new data
			for (int i = 0; i < segnum; i++) {
				segments[pid][i] = 0;
			}

			// In case of precision == 1, psgn is set to 0, the number of segments must be extracted again.
			packetsleft[pid] = packtot[pid] = extractpacketsegment(rpws);

			// Offset is when to start reading buffer later than zero. the smaller packet is the first
			// Prototype message in buffer:
			//  0  1  2  3  4  5  6  7  8  9
			//  x  x  c  i  a  b  b  a  l  e
			//  This is a message of length 8 with offset 2
			offsets[pid] = 31 - (rpws.total_size - 1);

			// Total message length
			// Calculated with multiples of 31 + a bit of the first packet
			packlen[pid] = ((packetsleft[pid] - 1) * 31)
					+ (rpws.total_size - 1);
		}

		printf(
				"Packet information: PID %d\tPSGN %d\tLeft %d\tMissing %d\tMPKW %d\tMPSGN %d\tPacklen %d\n",
				pid, psgn, packetsleft[pid], 0/* TODO: Edit */,
				max_packetwindow[pid], max_psgn[pid], packlen[pid]);
		if (packlen[pid] == 0) {
			printf(
					"Received a packet, but packet length is undetermined so we can't do much with it\n");
			return;
		}

		if (packlen[pid] > Settings::mtu || packlen[pid] <= 0) {
			printf(
					"Pack length resulted bigger than mtu or <= 0. This IS a problem. Value: %d, MTU: %d. Sorry.\n",
					packlen[pid], Settings::mtu);
			packlen[pid] = packtot[pid] = max_packetwindow[pid] =
					max_psgn[pid] = offsets[pid] = 0;
			return;
		}

		if (max_packetwindow[pid] > Settings::mtu
				|| max_packetwindow[pid] < 0) {
			printf(
					"MaxPacketWindow resulted bigger than mtu or < 0. This IS a problem. Value: %d, MTU: %d. Sorry.\n",
					max_packetwindow[pid], Settings::mtu);
			max_packetwindow[pid] = 0;
			return;
		}

		if (max_psgn[pid] > Settings::mtu || max_psgn[pid] < 0) {
			printf(
					"MaxPSGN resulted bigger than mtu or < 0. This IS a problem. Value: %d, MTU: %d. Sorry.\n",
					max_psgn[pid], Settings::mtu);
			max_psgn[pid] = 0;
			return;
		}

		if (packtot[pid] > Settings::mtu || packtot[pid] <= 0) {
			printf(
					"packtot resulted bigger than mtu or <= 0. This IS a problem. Value: %d, MTU: %d. Sorry.\n",
					packtot[pid], Settings::mtu);
			packtot[pid] = 0;
			return;
		}

		if (segments[pid][psgn]) {
			// This packet is a duplicate, should check if segment data in buffer is also duplicate, otherwise we lost something here
		} else {
			// Mark segment as received
			segments[pid][psgn] = 1;
			packetsleft[pid]--;
		}

		if (max_psgn[pid] < psgn)
			max_psgn[pid] = psgn;

		// Copy to buffer
		// Copy is from right to left, while keeping in mind that
		// 	the segment position starts from the left and in multiples of 31.
		// This allows copy of all packets including the smaller one in a compatible manner
		for (int i = 0; i < (rpws.total_size - 1); i++) {
			buffers[pid][(31 * psgn) + (30 - i)] =
					rpws.rp->data[(rpws.total_size - 2) - i];
		}

		// missingpackets accounts for the last_id missed packets, we must count packets for this instance too
		int instancemissingpackets = 0;

		// Find if there are any missing packets and ask for them before it's too late
		// Packet might be asked twice, we don't want this behaviour,
		// 	for this reason i doesn't start at 0 but uses the last maximum packet we've already seen
		for (int i = max_packetwindow[pid]; i < max_psgn[pid]; i++) {
			if (!segments[pid][i]) {
				missinginfos[missingpackets++] = makeidsegment(pid, psgn,
						precision);
				instancemissingpackets++;
			}
		}

		int packtotcopy = packtot[pid];
		printf("Iterating with: i from 0 to %d \nSegments:\t(%d)\t",
				packtot[pid], packtot[pid]);

		for (int i = 0; i < packtotcopy; i++) {
			cout << (segments[pid][i] ? '1' : '0') << " ";
		}
		printf("\n         \t\t");
		for (int i = 0; i < packtotcopy; i++) {
			cout
					<< (i == psgn ?
							'^' :
							((max_packetwindow[pid] == i) ?
									'[' :
									((i == max_psgn[pid]) ?
											']' :
											((max_packetwindow[pid] < i)
													&& (i < max_psgn[pid]) ?
													'*' : ' ')))) << " ";
		}
		cout << endl;

		cout << "Setting new window" << endl;

		// Next time don't ask for packets you've already asked for
		max_packetwindow[pid] = max_psgn[pid];

		// Finally write packet to buffer.
		// When to do it? We reached end of packlen with no missing packets.
		// This result can be repeated in case of missing packets inside the message
		cout << "Is packet ready?" << endl;
		if ((packetsleft[pid] == 0) && (instancemissingpackets == 0)) {
			cout << "Writing message" << endl;
			if (c->writeback(buffers[pid] + offsets[pid], packlen[pid])) {
				packets_ok++;
			} else {
				packets_failed++;
			}
		} else {
			cout << "NO" << endl;
		}

		// Enqueue the next missing packets list
		if (Settings::resuscitate_packets && Settings::ack_payloads) {
			if (missingpackets > 0) {
				//printf("Writing control packets for recovery. Total: %d\n", total_missing_packets);
				addControlRecoveryPacket(missinginfos, missingpackets);
			}
		}

	}

	void snapshot() {
		printf("State of messages:\n");
		for (int i = 1; i < idnum; i++) {
			int m = 0, r = 0;
			for (int j = min_seg[i]; j < max_seg[i]; j++) {
				if (!segments[i][j]) {
					m++;
				}
			}
			for (int j = 0; j < min_seg[i]; j++) {
				if (!segments[i][j]) {
					r++;
				}
			}

			printf(
					"id %d, max_seg: %d, min seg: %d, len: %d, calclen: %d, missing: %d, remaining: %d",
					i, max_seg[i], min_seg[i], len[i],
					((max_seg[i] - 1) * 31) + firstpacksize[i], m, r);
			printf("\nSegboolcontents: ");
			for (int j = 0; j < max_seg[i]; j++) {
				cout << segments[i][j] << " ";
			}
			printf("\n");
		}
	}

	void receiveMessage(radiopacketwithsize rpws) {

		static uint8_t pid, psgn;
		static bool precision = 0;
		precision = extractpacketprecision(rpws.rp->info);
		pid = extractpacketid(rpws);
		psgn = precision ? 0 : extractpacketsegment(rpws);

		int total_missing_packets = 0;
		static uint8_t packets[segnum] { 0 };

		if (last_id == 0) {

		}

		//printf("Is pid 0 ? %d\n", pid == 0);
		if (pid == 0) {
			if (rpws.total_size > 1)
				if (processEmptyPacket(rpws)) {
					packets_control++;
					bad_packets_counter = 0;
					return;
				}

			if (bad_packets_counter++ > Settings::maxbadpackets) {
				resetRadio();
				printf("Reached max bad packets limit\n");
				bad_packets_counter = 0;
			}
			return;
		}

		//printf("Received message of id %d segment %d size %d", pid, psgn,
		//		rpws.total_size);
		//radioppppprintchararray(rpws.rp->data, rpws.total_size - 1);

		//printf("Is pid changed ? %d. Last id: %d, now: %d\n", (last_id != pid),
		//		last_id, pid);
		if (Settings::ack_payloads && Settings::resuscitate_packets) {
			if (last_id != pid && (last_id != 0)) {
				//printf("Is last_id the previous in respect to pid? -> %d \n",
				//		(pid == 1) ? (last_id == idnum - 1) : (pid - 1 == last_id));
				if ((pid == 1) ? (last_id == idnum - 1) : (pid - 1 == last_id))
					if (len[last_id] != 0) {
						if (!segments[last_id][0]) {
							packets[total_missing_packets++] = makeidsegment(
									last_id, 0, 0);
						}
					}
			} else if (last_id == 0) {
				printf("Ignoring check cause last_id is 0\n");
			}
		}

		last_id = pid;

		//printf("Is segment already written? %d\n", segments[pid][psgn]);
		bool seen = false;
		/*if (segments[pid][psgn]) {
		 //printf("Segment already seen. Does it bring new data?\n");
		 //for(int i = 0; i < (rpws.total_size - 1); i++){}
		 if ((strncmp((char*) (rpws.rp->data),
		 (char*) (buffers[pid] + (31 * psgn)), rpws.total_size - 1)
		 != 0)) {

		 //printf("Segment brings new data. reset\n");
		 packets_dropped++;
		 for (int j = 0; j < segnum; j++)
		 segments[pid][j] = false;

		 min_seg[pid] = psgn;
		 max_seg[pid] = psgn;
		 //firstpacksize[pid] = rpws.total_size - 1;
		 len[pid] = 0;
		 } else {
		 seen = true;
		 //printf("Segment does not bring new data. okay\n");
		 }
		 }*/

		//printf("Confronting min max\n");
		uint8_t missing_packets = 0, remaining = 0;
		//bool triggermissingresearch = (len[pid] != 0)
		//		&& (last_seg[pid] - psgn > 1) && !segments[pid][0];

		int max_findout;

		segments[pid][psgn] = 1;
		if (!seen)
			last_seg[pid] = psgn;

		if (max_seg[pid] < psgn) {
			max_seg[pid] = psgn;

		}
		max_findout = max_seg[pid];
		if (min_seg[pid] > psgn) {
			min_seg[pid] = psgn;

		}

		if (psgn == 0) {
			firstpacksize[pid] = rpws.total_size - 1;
			//printf("Packet revealed total packets: %d, we had: %d\n",
			//		extractpacketsegment(rpws.rp->info), max_findout + 1);
			max_findout = extractpacketsegment(rpws.rp->info) - 1;
			if (max_findout < max_seg[pid]) {
				max_seg[pid] = max_findout;
			}
		}

		len[pid] += rpws.total_size - 1;

		// Copia in buffer
		for (int i = 0; i < rpws.total_size - 1; i++) {
			buffers[pid][(31 * psgn) + (30 - i)] =
					rpws.rp->data[(rpws.total_size - 2) - i];
		}

		// Conteggio pacchetti rimasti e mancanti
		if (Settings::ack_payloads && Settings::resuscitate_packets) {
			for (int i = 0; i < max_findout; i++) {
				if (!segments[pid][i]) {
					if (i < min_seg[pid]) {
						remaining++;
					} else {
						packets[total_missing_packets++] = makeidsegment(pid, i,
								0);
						missing_packets++;
					}
				}
			}
		}

		//printf(
		//		"Missing packets final confrontation. missing: %d, remaining: %d\n",
		//		missing_packets, remaining);
		if (missing_packets == 0) {
			// Yay! message is ok
			if (segments[pid][0] && remaining == 0) {
				int calclen = (31 * max_seg[pid]) + firstpacksize[pid];
				int flen = calclen > len[pid] ? calclen : len[pid];
				int offset = 31 - firstpacksize[pid];
				//printf(
				//		"-----------------------------------------------------------\nFinal message. Id: %d\tParts: %d \tcrc: %d\t Length: %d \t Calculated length: %d",
				//		pid, (int) max_seg[pid] + 1,
				//		gencrc(buffers[pid] + offset, flen), len[pid], calclen);
				//radioppppprintchararray((uint8_t*) buffers[pid] + offset, flen);

				if (c->writeback(buffers[pid] + offset, flen)) {
					//printf("Pack Received: id %d, len %d, crc %d\n", pid, flen,
					//		gencrc(buffers[pid] + offset, flen));
					packets_ok++;
				} else {
					packets_failed++;
				}
				len[pid] = 0;
				for (int j = 0; j < max_seg[pid]; j++)
					segments[pid][j] = false;
				min_seg[pid] = psgn;
				max_seg[pid] = psgn;
				firstpacksize[pid] = 0;
			}

		} else if (Settings::ack_payloads && Settings::resuscitate_packets) {
			// Packet is not okay...
			min_seg[pid] = max_seg[pid];

		}

		if (total_missing_packets > 0
				&& (Settings::ack_payloads && Settings::resuscitate_packets)) {
			//printf("Writing control packets for recovery. Total: %d\n",
			//		total_missing_packets);
			addControlRecoveryPacket(packets, total_missing_packets);
		}

		//snapshot();
	}

	virtual ~RadioAndPacketHandler();

protected:

	void resetRadio() {
		if (!radio)
			radio = new RF24(Settings::ce_pin, 0, primary ? 500000 : 500000);

		char t = 1;
		while (!radio->begin()) {
			cout << "radio hardware is not responding!!" << endl;
			usleep(10000);
//delay(2);
			if (!t++) {
				//handleRadioUnresponsiveTimeout();
			}
		}

		radio->flush_rx();
		radio->flush_tx();

		radio->setAddressWidth(Settings::addressbytes);

		radio->setPALevel(Settings::power);
		radio->setDataRate(Settings::data_rate);

		// save on transmission time by setting the radio to only transmit the
		// number of bytes we need to transmit
		radio->setPayloadSize(Settings::payload_size);
		radio->setAutoAck(Settings::auto_ack);

		// to use ACK payloads, we need to enable dynamic payload lengths (for all nodes)
		if (Settings::dynamic_payloads)
			radio->enableDynamicPayloads();	// ACK payloads are dynamically sized
		else
			radio->disableDynamicPayloads();

		// Acknowledgement packets have no payloads by default. We need to enable
		// this feature for all nodes (TX & RX) to use ACK payloads.
		if (Settings::ack_payloads)
			radio->enableAckPayload();
		else
			radio->enableAckPayload();

		radio->setRetries(Settings::radio_delay, Settings::radio_retries);
		radio->setChannel(Settings::channel);

		radio->setCRCLength(Settings::crclen);

		radio->openReadingPipe(1, read_address[0]);
		radio->openWritingPipe(write_address[0]);

		if (!primary) {
			radio->startListening();
		} else {
			radio->stopListening();
		}

		radio->printPrettyDetails();
		if (radio->isPVariant())
			cout << "    #####     [RadioInfo] This radio is a P Variant"
					<< endl;

		if (!radio->isValid())
			cout
					<< "    #####     OH OH OH Looks like this radio is not valid! What?"
					<< endl;

		if (radio->testCarrier())
			cout
					<< "    #####     [RadioCarrier] Looks like this channel is busy"
					<< endl;

		if (radio->testRPD())
			cout
					<< "    #####     [RadioCarrier] Looks like this channel is very busy!"
					<< endl;

	}

};

#endif /* RADIOANDPACKETHANDLER_H_ */
