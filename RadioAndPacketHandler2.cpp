#include "RadioAndPacketHandler2.h"

using namespace std;


void RadioHandler2::loop(){

}

void RadioHandler2::handleData(uint8_t *data, unsigned int size){
    
}

void RadioHandler2::resetRadio() {
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
		//radio->setPayloadSize(Settings::payload_size);
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