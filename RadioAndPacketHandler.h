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

		rpws.rp->info = makeidsegment(0, extractpacketsegment(rpws.rp) + 1, 0);
		setrpwscrc(rpws);

		//	printf("Empty packet contents:\n");
		//	radioppppprintchararray((uint8_t*) rpws.rp, 32);
	}
	radiopacketwithsize nextPacket() {
		if (packetqueue.empty()) {
			makenextemptypacket(emptypacket);
			return emptypacket;
		} else {
			radiopacketwithsize rpws = packetqueue.front();
			packetqueue.pop_front();
			setNextBufferPacket(rpws);
			return rpws;
		}
	}

	std::deque<radiopacketwithsize> packetqueue;

	static const unsigned short maxcircularbuffersize = 900;
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

		resetRadio();
	}

	inline bool sendCustomRadioPacket(radiopacketwithsize rpws) {
		return radio->writeFast(rpws.rp, rpws.total_size);
	}
	inline bool loadCustomRadioPacket(radiopacketwithsize rpws) {
		return radio->writeAckPayload(1, rpws.rp, rpws.total_size);
	}

	unsigned long telemetry_ms = 0, packets_ok = 0, packets_failed = 0,
			packets_control = 0, packets_dropped = 0;

	radiopacketwithsize rpws = { new radiopacket, 0 };
	void loop() {

		if (millis() > telemetry_ms + 1000) {
			telemetry_ms = millis();
			printf(
					"Packets OK: %d, Failed: %d, Control: %d, Dropped: %d, Queue size: %d\n",
					(unsigned int) packets_ok, (unsigned int) packets_failed,
					(unsigned int) packets_control,
					(unsigned int) packets_dropped, (unsigned int)packetqueue.size());
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

		m.lock();

		if (primary) {
			//printf("Sending data\n");
			if (!packetqueue.empty() || Settings::use_empty_packets)
				if (sendCustomRadioPacket(nextPacket())) {
					if (Settings::auto_ack || Settings::ack_payloads)
						while (radio->available()) {
							//printf("Read data 1\n");

							//handle received packet(s)
							radio->read(rpws.rp,
									(rpws.total_size =
											radio->getDynamicPayloadSize()));
							receiveMessage(rpws);
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
					receiveMessage(rpws);
				}
		} else {
			while (radio->available()) {

				// Handle received packet(s)
				//printf("Loading data 1\n");
				if (Settings::ack_payloads)
					if (loadCustomRadioPacket(nextPacket())) {
						// Delete old packet(s) --- handled in nextpacket
					}

				//printf("Read data\n");
				radio->read(rpws.rp,
						(rpws.total_size = radio->getDynamicPayloadSize()));
				receiveMessage(rpws);

			}
			// This line might not be needed
			//printf("Loading data 2\n");
			//while (loadCustomRadioPacket(nextPacket())) {
			//	printf("Loaded more data\n");
			// Do everything again in case TX buffer is not filled
			//}
		}

		m.unlock();

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
			for (int i = 0; i < idnum; i++) {
				for (int j = 0; j < segnum; j++) {
					segments[i][j] = false;
				}
			}
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

	void handleData(uint8_t *data, unsigned int size) {

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

		m.lock();

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
						(int)(precision ? ceil(size / 31.0) : packnum),
						(int)extractpacketsegment(rpws));

			//printf("packet split %d", packnum);
			//radioppppprintchararray((uint8_t*) rpws.rp + 1, 31);

			packetqueue.push_back(rpws);

			//receiveMessage(rpws);

		}
		m.unlock();

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

	virtual ~RadioAndPacketHandler();

protected:

	void resetRadio() {
		if (!radio)
			radio = new RF24(Settings::ce_pin, 0, primary ? 5000000 : 5000000);

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
