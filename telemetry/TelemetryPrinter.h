/*
 * TelemetryPrinter.h
 *
 *  Created on: 24 Feb 2024
 *      Author: Andrea Roccaccino
 */
#pragma once

#include "Telemetry.h"
#include "../settings/Settings.h"
#include <vector>

#include <fstream>

class TelemetryPrinter {
public:
	TelemetryPrinter(const char* filename);
	virtual ~TelemetryPrinter();

	void add_element(Telemetry *t) {
		elements.push_back(t);
	}

	void tick();

private:
	const char* filename = nullptr;
	std::ofstream* outfile;
	std::vector<Telemetry*> elements;

};
