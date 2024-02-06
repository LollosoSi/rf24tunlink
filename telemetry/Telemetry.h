/*
 * Telemetry.h
 *
 *  Created on: 6 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include <string>
#include <vector>

#include <iostream>

class Telemetry {

protected:
	std::string name;
	std::string* element_names = nullptr;
	int element_length = 0;

public:
	Telemetry(std::string name) {this->name = name;}
	virtual ~Telemetry(){}

	const std::string get_name(){return (name);}
	const std::string* get_element_names(){return (element_names);}
	int size(){return (element_length);}

	void register_elements(std::string* names, int length){element_names = names; element_length = length;}

	virtual std::string* telemetry_collect(const unsigned long delta) = 0;


};
