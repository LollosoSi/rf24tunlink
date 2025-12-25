/*
 * UdpActivity.h
 *
 *  Created on: 11 Aug 2025
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "ActivitySignalInterface.h"

// For UDP Socket
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h> // close()
#include <cstring>  // memset
#include <stdexcept>

class UdpActivity : public ActivitySignalInterface {

	private:
    int sock_fd = -1;
    struct sockaddr_in dest_addr;

	public:
		UdpActivity(const Settings s) : ActivitySignalInterface(s) {
			// Address: s.udp_send_address
			// Port: s.udp_send_port

			sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
			        if (sock_fd < 0) {
			            throw std::runtime_error("UdpActivity: Cannot create UDP Socket");
			        }

			        // Configure destination address
			        memset(&dest_addr, 0, sizeof(dest_addr));
			        dest_addr.sin_family = AF_INET;
			        dest_addr.sin_port = htons(s.udp_send_port);

			        if (inet_pton(AF_INET, s.udp_send_address.c_str(), &dest_addr.sin_addr) <= 0) {
			            throw std::runtime_error("UdpActivity: Invalid IP Address: " + s.udp_send_address);
			        }

		}
		~UdpActivity(){
			stop();
		}


		  // Chiude il socket
		    void stop() override {
		        if (sock_fd >= 0) {
		            close(sock_fd);
		            sock_fd = -1;
		        }
		    }

		    // Invia via UDP il valore booleano
		    void set_state(bool on_off) override {
		        if (sock_fd < 0) return;

		        uint8_t value = on_off ? 1 : 0;

		        ssize_t sent = sendto(
		            sock_fd,
		            &value,
		            sizeof(value),
		            0,
		            reinterpret_cast<struct sockaddr*>(&dest_addr),
		            sizeof(dest_addr)
		        );

		        if (sent < 0) {
		            std::cerr << "UdpActivity: Error during UDP send\n";
		        }
		    }

};
