/*
 * TUNInterface.h
 *
 *  Created on: 26 May 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../../generic_structures.h"

#include <thread>
// Thread naming
#include <sys/prctl.h>

class TUNInterface : public SystemInterface<TunMessage>, public SyncronizedShutdown {

		std::thread *read_thread = nullptr;
		void stop_read_thread(bool wait_join = true);

		SystemInterface<TunMessage>* packetizer = nullptr;

		// TUN interface setups
		int tunnel_fd = 0;
		int interface_setup(const char *dev, int flags, const char *ip, const char *destination, const char *mask, int mtu);
		int open_tunnel(const char *device_name);
		void interface_set_ip(const char *device_name, const char *ip, const char *ip_mask);
		void interface_set_flags(const char *device_name, int flags);
		void interface_set_mtu(const char *dev, unsigned int mtu);
		bool interface_set_destination(const char *deviceName, const char *destinationIP);

	public:
		TUNInterface();
		virtual ~TUNInterface();

		bool input(TunMessage &m);
		void apply_settings(const Settings &settings);
		void stop_module();
		void stop_interface();

		void register_packet_target(SystemInterface<TunMessage> *receiver) {
			packetizer = receiver;
		}

};
