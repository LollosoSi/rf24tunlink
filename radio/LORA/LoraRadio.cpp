/*
 * LoraRadio.cpp
 *
 *  Created on: 6 Feb 2024
 *      Author: Andrea Roccaccino
 */

#include "LoraRadio.h"

LoraRadio::LoraRadio() {
	setup();

}

LoraRadio::~LoraRadio() {
	stop();
}


void LoraRadio::setup(){}
void LoraRadio::loop(unsigned long delta){}
void LoraRadio::stop(){}
