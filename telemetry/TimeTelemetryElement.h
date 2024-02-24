/*
 * TimeTelemetryElement.h
 *
 *  Created on: 24 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "Telemetry.h"
#include "../utils.h"

#include <cstring>
#include <unistd.h>

class TimeTelemetryElement: public Telemetry {
public:
	TimeTelemetryElement();
	virtual ~TimeTelemetryElement();

	std::string* telemetry_collect(const unsigned long delta){
		returnvector[0]=std::to_string(current_millis());

		return returnvector;
	}

private:
	std::string* returnvector = nullptr;


};


