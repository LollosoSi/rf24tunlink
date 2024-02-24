/*
 * TimeTelemetryElement.cpp
 *
 *  Created on: 24 Feb 2024
 *      Author: Andrea Roccaccino
 */

#include "TimeTelemetryElement.h"

TimeTelemetryElement::TimeTelemetryElement() : Telemetry("Time") {

	register_elements(new std::string[1] { "UnixTime" }, 1);

	returnvector = new std::string[1] {""};


}

TimeTelemetryElement::~TimeTelemetryElement() {
	// TODO Auto-generated destructor stub
}

