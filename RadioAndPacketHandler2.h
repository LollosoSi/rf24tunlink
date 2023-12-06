/*
 * RadioAndPacketHandler.h
 *
 *  Created on: Oct 18, 2023
 *      Author: andrea
 */

#ifndef RADIOANDPACKETHANDLER2_H_
#define RADIOANDPACKETHANDLER2_H_

using namespace std;

#include "Settings.h"

#include <cmath>

#include "Callback.h"

#include <RF24/RF24.h>
#include <deque>
#include <mutex>


struct radiopacket2{
    char data[32] = {0};
};

uint8_t extractId(radiopacket2 &rp2){
    return rp2.data[0]&0x7F;
}
uint8_t extractPrecision(radiopacket2 &rp2){
    return (rp2.data[0] >> 7)&0x01;
}
uint8_t extractSegment(radiopacket2 &rp2){
    return rp2.data[1];
}
void setData(radiopacket2 &rp2, bool precision, uint8_t id, uint8_t segment){
    rp2.data[1]=segment;
    rp2.data[0]=((precision<<7) & 0x80) & id;
}


class RadioHandler2{
    private:
    RF24* radio = nullptr;
    bool primary = true;
	uint8_t write_address[Settings::addressbytes + 1] = { 0 };
	uint8_t read_address[Settings::addressbytes + 1] = { 0 };

std::mutex m;
Callback *c = nullptr;

public:


    void setup(Callback *c, bool primary,
			uint8_t write_address[Settings::addressbytes + 1],
			uint8_t read_address[Settings::addressbytes + 1]) {
		this->c = c;
		this->primary = primary;
		memcpy(this->write_address, write_address, Settings::addressbytes + 1);
		memcpy(this->read_address, read_address, Settings::addressbytes + 1);

		resetRadio();
	}

    void loop();
    void handleData(uint8_t *data, unsigned int size);

    protected:
    void resetRadio();
};


#endif