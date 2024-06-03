/*
 * TUNInterface.cpp
 *
 *  Created on: 26 May 2024
 *      Author: Andrea Roccaccino
 */

#include "TUNInterface.h"

#include <linux/if.h>
#include <linux/if_tun.h>

#include <cstring>
#include <string>

#include <fstream>
#include <iostream>

#include <fcntl.h>
#include <sys/ioctl.h>  // Per ioctl
#include <unistd.h>      // Per close

#include <arpa/inet.h>

#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

TUNInterface::TUNInterface() {

}

TUNInterface::~TUNInterface() {

}


bool TUNInterface::input(TunMessage& m){
	unsigned int tot = 0;
	int n = 0;
	while (tot < m.length) {
		if ((n = write(tunnel_fd, m.data.get() + tot, m.length - tot))
				< 0) {
			perror("Writing to interface");
			fprintf(stderr,
					"Error: %s (errno: %d), tunnel fd: %d, data length: %d, bytes written tot: %d, write returned: %d \n",
					strerror(errno), errno, tunnel_fd, m.length, tot, n);
			//close(tunnel_fd);
			//exit(1);

			return (false);
		}
		if (n > 0)
			tot += n;
		else {
			printf("Data not written");
			return (false);
		}
	}

	return (true);
}

void TUNInterface::apply_settings(const Settings &settings){

	stop_read_thread(true);
	stop_interface();

	tunnel_fd = interface_setup(settings.interface_name.c_str(),
		IFF_TUN | IFF_UP | IFF_RUNNING, settings.address.c_str(), settings.destination.c_str(),
				settings.netmask.c_str(), settings.mtu, settings.tx_queuelength);


	running = true;
	read_thread = new std::thread ([&] {
		prctl(PR_SET_NAME, "TUN Read", 0, 0, 0);


			int nread = 0;

			while (this->running) {
				Message message(2 * settings.mtu);
				/* Note that "buffer" should be at least the MTU size of the interface, eg 1500 bytes */
				//printf("Waiting for tun packet\n");
				nread = read(tunnel_fd, message.data.get(), 2 * settings.mtu);
				if (nread < 0) {
					perror("Error reading from interface");
					break;
					//close(tunnel_fd);
					//exit(1);
				}

				/* Do whatever with the data */
				message.length = nread;
				this->packetizer->input(message);

			}

		});
}

void TUNInterface::stop_read_thread(bool wait_join){
	running = false;
	if (read_thread) {
		if (read_thread->joinable()) {
			if(wait_join){
				printf("Waiting for the TUN read thread to quit (send something through the link if blocked here) . . . . .");
				read_thread->join();
				printf(". . . . . TUN read thread has finished");
				delete read_thread;
				read_thread = nullptr;
			}
			else
				read_thread->detach();


		}
	}
}

void TUNInterface::stop_interface() {
	if (tunnel_fd != 0) {
		close (tunnel_fd);
		tunnel_fd = 0;
	}
}

void TUNInterface::stop_module(){


	stop_read_thread(false);
	stop_interface();

}


int TUNInterface::interface_setup(const char *dev, int flags, const char *ip,
		const char *destination, const char *mask, int mtu, int tx_queuelen) {

	printf("Interface '%s' will have IP '%s', Destination '%s', mask '%s', MTU '%d'\n",
			dev, ip, destination, mask, mtu);

	printf("Opening Interface \n");
	int fd = open_tunnel(dev);
	printf("....\n");

	printf("Setting IP and MASK \n");
	interface_set_ip(dev, ip, mask);
	printf("....\n");

	printf("Setting MTU \n");
	interface_set_mtu(dev, mtu);
	printf("....\n");

	printf("Setting tx_queuelength \n");
	interface_set_txqueuelen(dev, tx_queuelen);
	printf("....\n");

	printf("Setting interface UP \n");
	interface_set_flags(dev, IFF_UP);
	printf("....\n");

	printf("Setting Destination IP \n");
	interface_set_destination(dev, destination);
	printf("....\n");

	printf("Configuration finished \n");

	return (fd);
}

void TUNInterface::interface_set_txqueuelen(const char *deviceName, int txQueueLen) {
	// Preparare la struttura ifreq
	int socketFD = socket(AF_INET, SOCK_DGRAM, 0);
	if (socketFD < 0) {
		std::cerr << "Failed to open socket: " << strerror(errno) << " ("
				<< errno << ")" << std::endl;
		return;
	}

	struct ifreq ifr;
	std::memset(&ifr, 0, sizeof(ifr));
	std::strncpy(ifr.ifr_name, deviceName, IFNAMSIZ);

	// Impostare la lunghezza della coda di trasmissione
	ifr.ifr_qlen = txQueueLen;

	// Effettuare la chiamata ioctl per impostare la lunghezza della coda di trasmissione
	if (ioctl(socketFD, SIOCSIFTXQLEN, &ifr) < 0) {
		perror("ioctl");
		close (socketFD);
		exit(EXIT_FAILURE);
	}
	close(socketFD);

}

bool TUNInterface::interface_set_destination(const char *deviceName,
		const char *destinationIP) {
	int socketFD = socket(AF_INET, SOCK_DGRAM, 0);
	if (socketFD < 0) {
		std::cerr << "Failed to open socket: " << strerror(errno) << " ("
				<< errno << ")" << std::endl;
		return (false);
	}

	struct ifreq ifr;
	std::strncpy(ifr.ifr_name, deviceName, IFNAMSIZ);
	ifr.ifr_addr.sa_family = AF_INET;

	if (inet_pton(AF_INET, destinationIP,
			&((struct sockaddr_in*) (&ifr.ifr_addr))->sin_addr) < 0) {
		std::cerr << "Failed to convert destination IP: " << strerror(errno)
				<< " (" << errno << ")" << std::endl;
		close(socketFD);
		return (false);
	}

	if (ioctl(socketFD, SIOCSIFDSTADDR, &ifr) < 0) {
		std::cerr << "Failed to set destination address: " << strerror(errno)
				<< " (" << errno << ")" << std::endl;
		close(socketFD);
		return (false);
	}

	close(socketFD);
	return (true);
}

void TUNInterface::interface_set_mtu(const char *dev, unsigned int mtu) {

	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	struct ifreq ifr;
	strcpy(ifr.ifr_name, dev);
	if (!ioctl(sock, SIOCGIFMTU, &ifr)) {

	}
	ifr.ifr_mtu = mtu; // Change value if it needed
	if (!ioctl(sock, SIOCSIFMTU, &ifr)) {
		// Mtu changed successfully
	}

}

// Sets flags for a given interface
void TUNInterface::interface_set_flags(const char *device_name, int flags) {
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == 0)
		printf("Error opening socket: %s (%d)\n", strerror(errno), errno);

	struct ifreq ifr = { };
	ifr.ifr_flags = flags;
	strncpy(ifr.ifr_name, std::string(device_name).c_str(), IFNAMSIZ);
	int status = ioctl(fd, SIOCSIFFLAGS, &ifr);
	if (status > 0)
		printf("Couldn't to set tunnel interface: %s (%d)\n", strerror(errno),
		errno);
	close(fd);
}

void TUNInterface::interface_set_ip(const char *device_name, const char *ip,
		const char *ip_mask) {
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == 0)
		printf("Error opening socket: %s (%d)\n", strerror(errno), errno);

	struct ifreq ifr = { };
	strncpy(ifr.ifr_name, std::string(device_name).c_str(), IFNAMSIZ);

	ifr.ifr_addr.sa_family = AF_INET;
	if (inet_pton(AF_INET, std::string(ip).c_str(),
			&reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_addr)->sin_addr)
			!= 1)
		printf("Failed to assign IP address: %s (%d)\n", strerror(errno),
		errno);
	int status = ioctl(fd, SIOCSIFADDR, &ifr);
	if (status > 0)
		printf("Couldn't set tunnel interface ip: %s (%d)\n", strerror(errno),
		errno);

	ifr.ifr_netmask.sa_family = AF_INET;
	if (inet_pton(AF_INET, std::string(ip_mask).c_str(),
			&reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_netmask)->sin_addr)
			!= 1)
		printf("Couldn't assign IP mask: %s (%d)\n", strerror(errno), errno);
	status = ioctl(fd, SIOCSIFNETMASK, &ifr);
	if (status > 0)
		printf("Failed setting interface mask: %s (%d)\n", strerror(errno),
		errno);
	close(fd);
}

// Opens the tunnel interface to listen on. Always returns a valid file
// descriptor or quits and logs the error.
int TUNInterface::open_tunnel(const char *device_name) {
	int fd = open("/dev/net/tun", O_RDWR);
	if (fd == 0)
		printf("Error opening tunnel file: %s (%d)\n", strerror(errno),
		errno);

	struct ifreq ifr = { };
	ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
	strncpy(ifr.ifr_name, std::string(device_name).c_str(), IFNAMSIZ);

	int status = ioctl(fd, TUNSETIFF, &ifr);
	if (status > 0)
		printf("Error setting tunnel interface: %s (%d)\n", strerror(errno),
		errno);
	return (fd);
}

