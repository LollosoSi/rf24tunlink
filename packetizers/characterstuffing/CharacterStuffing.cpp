/*
 * CharacterStuffing.cpp
 *
 *  Created on: 27 May 2024
 *      Author: Andrea Roccaccino
 */

#include "CharacterStuffing.h"

CharacterStuffing::CharacterStuffing() : Packetizer() {
	
}

CharacterStuffing::~CharacterStuffing() {

}

void CharacterStuffing::process_tun(TunMessage &m) {

}
void CharacterStuffing::process_packet(RFMessage &m) {

}
void CharacterStuffing::apply_settings(const Settings &settings) {
	this->settings = &settings;
}
unsigned int CharacterStuffing::get_mtu() {
	return (mtu);
}
