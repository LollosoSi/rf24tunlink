#include "RadioAndPacketHandler2.h"

using namespace std;

/**
 * Send and receive packets if available
*/
void RadioHandler2::loop(){

static radiopacket2 rp;
static radiopacketwithsize2 rpwsrec = {&rp,0};

if(primary){

if(this->rpqueue.size() > 0){
m.lock();

if(radio->write(rpqueue.front().rp,rpqueue.front().size)){

}else{

}

delete this->rpqueue.front().rp;
this->rpqueue.pop_front();
m.unlock();
}else{

if(radio->write(emptyrpws.rp,emptyrpws.size)){

}else{

}

}

while(radio->available()){
	radio->read(rpwsrec.rp,rpwsrec.size = radio->getDynamicPayloadSize());
	readRPWS(rpwsrec);
}


}else{

while(radio->available()){
	radio->read(rpwsrec.rp,rpwsrec.size = radio->getDynamicPayloadSize());
	readRPWS(rpwsrec);

if(this->rpqueue.size() > 0){
m.lock();

if(radio->writeAckPayload(1,rpqueue.front().rp,rpqueue.front().size)){

}else{

}

delete this->rpqueue.front().rp;
this->rpqueue.pop_front();
m.unlock();
}else{

if(radio->writeAckPayload(1,emptyrpws.rp,emptyrpws.size)){

}else{

}

}
	
}

}




}

/** Handle packets from TUN
 * to be sent via radio
*/
void RadioHandler2::handleData(uint8_t *data, unsigned int size){

m.lock();

radiopacket2* pointer = new radiopacket2;

int cursor = 0;
int packetcursor = 0;
while(cursor < size){

if(data[cursor] == radio_escape_char){
	if(packetcursor+2 <= 32){
		pointer->data[packetcursor++]=data[cursor];
		pointer->data[packetcursor++]=radio_escape_char;
	}else{
		this->rpqueue.push_back({pointer,packetcursor});
		pointer = new radiopacket2;
		packetcursor=0;
	}
}else{
	pointer->data[packetcursor++]=data[cursor];
	if(packetcursor==32){
		this->rpqueue.push_back({pointer,packetcursor});
		pointer = new radiopacket2;
		packetcursor=0;
	}
}

cursor++;
}

pointer->data[packetcursor++]=radio_escape_char;
		this->rpqueue.push_back({pointer,packetcursor});
		pointer = new radiopacket2;
		packetcursor=0;
	


m.unlock();

}

/**
 * Read packets from the radio and compose messages.
 * Malformed messages will be rejected from the TUN interface
 * Empty messages will just contain an escape sequence
*/
void RadioHandler2::readRPWS(radiopacketwithsize2& rpws){

static uint16_t message_size = 0;
static uint8_t* buffer = new uint8_t[Settings::mtu]{0};

for(int i = 0; i < rpws.size; i++){
	if(rpws.rp->data[i] == radio_escape_char && (i+1 == rpws.size ? true : (rpws.rp->data[i+1]!=radio_escape_char)) ){
		if(message_size!=0){
			// Write message and set stats
			if(c->writeback(buffer,message_size)){
				statistics_packets_ok++;
			}else{
				statistics_packets_corrupted++;
			}
		}else{
			statistics_packets_control++;
		}

		message_size = 0;
	}else{
		buffer[message_size++] = rpws.rp->data[i];

		// Skip the next character because this is an escape
		if(rpws.rp->data[i] == radio_escape_char)
			i++;
		
	}
}

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