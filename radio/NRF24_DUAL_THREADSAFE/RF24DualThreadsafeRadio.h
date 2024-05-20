/*
 * RF24DualThreadsafeRadio.h
 *
 *  Created on: 13 May 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../../telemetry/Telemetry.h"
#include "../../interfaces/RadioHandler.h"

#include <RF24/RF24.h>

#include <iostream>
#include <unistd.h>
#include <string>

#include <thread>

class RF24DualThreadsafeRadio: public RadioHandler<RadioPacket>, public Telemetry {
public:
	RF24DualThreadsafeRadio(bool primary);
	virtual ~RF24DualThreadsafeRadio();


	bool role = false;
	bool running = false;

	RF24* radio0 = nullptr;
	RF24* radio1 = nullptr;

	std::thread* t0=nullptr;
	std::thread* t1=nullptr;

	void setup();
	void loop(unsigned long delta);


	void resetRadio0();
	void resetRadio1();
	void write();
	void read();
	void stop_quit(){stop();}

	std::string* telemetry_collect(const unsigned long delta){



		return (returnvector);
	};

protected:
	void stop();

	std::string *returnvector = nullptr;

};

