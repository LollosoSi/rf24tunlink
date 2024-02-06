/*
 * Telemetry.h
 *
 *  Created on: 6 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include <string>
#include <vector>

class Telemetry {

protected:
	std::string name;
	std::vector<std::string> element_names;

public:
	Telemetry(std::string name){this->name = name;}
	virtual ~Telemetry(){}

	void register_elements(std::vector<std::string> names){element_names = names;}

	virtual std::vector<std::string> telemetry_collect() = 0;


};
