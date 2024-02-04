/*
 * Settings.h
 *
 *  Created on: 3 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

namespace Settings{

extern char* address;			// Address of the interface		NOTE: Address and destination must be swapped based on the radio role
extern char* destination;		// Destination of the interface
extern char* netmask;			// Network address mask
extern char* interface_name;	// Interface name (arocc)
extern unsigned int mtu;		// MTU, must be determined later

}
