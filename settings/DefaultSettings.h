/*
 * DefaultSettings.h
 *
 *  Created on: 3 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "Settings.h"
#include <string>

namespace Settings {

char *address = new char[18] { '\0' };
char *destination = new char[18] { '\0' };
char *netmask = new char[18] { '\0' };
char *interface_name = new char[6] { 'a', 'r', 'o', 'c', 'c', '\0' };
unsigned int mtu = 9600;
bool control_packets = true;

}
