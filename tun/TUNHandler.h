/*
 * TUNHandler.h
 *
 *  Created on: 4 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../interfaces/Messenger.h"
#include "../structures/TUNMessage.h"
#include "../settings/Settings.h"

class TUNHandler: public Messenger<TUNMessage> {
public:
	TUNHandler();
	~TUNHandler();
protected:
	bool receive_message(TUNMessage &tunmsg);
private:
	int tunnel_fd = 0;
	int interface_setup(char *dev, int flags, const char* ip, const char *destination, const char* mask, int mtu);
	int open_tunnel(const char *device_name);
	void interface_set_ip(const char *device_name, const char *ip, const char *ip_mask);
	void interface_set_flags(const char *device_name, int flags);
	void interface_set_mtu(const char *dev, unsigned int mtu);
	bool interface_set_destination(const char *deviceName, const char *destinationIP);

};

