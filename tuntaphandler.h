/*
 * tuntaphandler.h
 *
 *  Created on: Oct 10, 2023
 *      Author: andrea
 */

#ifndef TUNTAPHANDLER_H_
#define TUNTAPHANDLER_H_

#include <thread>

#include "RadioAndPacketHandler2.h"
#include "Callback.h"

class tuntaphandler : public Callback {
public:
	bool running_;
	int tunnel_fd_;
	std::thread* networkthread;
	RadioHandler2 rph2;


	tuntaphandler(bool primary);

	int setup_interface(char *dev, int flags, const char* ip, const char* mask, uint8_t segmentbits, bool primary);

	void TunnelThread();

	int OpenTunnel(const char* device_name);
	void SetIP(const char* device_name,const char* ip, const char* ip_mask);
	void SetFlags(const char* device_name, int flags);
	void setMTU(const char* dev);
	bool setDestination(const char* deviceName, const char* destinationIP);

	void stop_interface();

	bool writeback(uint8_t* data, int size) override;

	virtual ~tuntaphandler();
};

#endif /* TUNTAPHANDLER_H_ */
