/*
 * TUNHandler.cpp
 *
 *  Created on: 4 Feb 2024
 *      Author: Andrea Roccaccino
 */

#include "TUNHandler.h"

// #include "../utils.h"

#include <thread>

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

#include <iostream>
using namespace std;

TUNHandler::TUNHandler() :
		Telemetry("TUNHandler") {

	register_elements(new std::string[5] { "Bytes Successful", "Bytes Failed",
			"Bytes Out", "Frames Out", "Frames In" }, 5);

	returnvector = new std::string[5] { std::to_string(bytes_successful),
			std::to_string(bytes_failed), std::to_string(0), std::to_string(0), std::to_string(0) };

	tunnel_fd = interface_setup(Settings::interface_name,
	IFF_TUN | IFF_UP | IFF_RUNNING, Settings::address, Settings::destination,
			Settings::netmask, Settings::mtu);

}

TUNHandler::~TUNHandler() {

	close(tunnel_fd);

}

std::string* TUNHandler::telemetry_collect(const unsigned long delta) {
	returnvector[0] = (std::to_string(bytes_successful));
	returnvector[1] = (std::to_string(bytes_failed));
	returnvector[2] = (std::to_string(bytes_out));
	returnvector[3] = (std::to_string(frames_out));
	returnvector[4] = (std::to_string(frames_in));

	//bytes_successful = bytes_failed = bytes_out = 0;
	return (returnvector);
}

void TUNHandler::startThread() {

	this->running = true;

	std::thread read_thread([&] {

		int nread = 0;
		TUNMessage *message = new TUNMessage;
		message->data = new uint8_t[2 * Settings::mtu];

		while (this->running) {
			/* Note that "buffer" should be at least the MTU size of the interface, eg 1500 bytes */
			//printf("Waiting for tun packet\n");
			nread = read(tunnel_fd, message->data, 2 * Settings::mtu);
			if (nread < 0) {
				perror("Reading from interface");
				close(tunnel_fd);
				exit(1);
			}

			/* Do whatever with the data */
			message->size = nread;
			bytes_out += nread;
			//printf("From TUN (%i)\n",message->size);
			//print_hex(message->data, message->size);
			frames_out++;
			this->packet_handler->send(message);

		}

	});
	read_thread.detach();

}

bool TUNHandler::receive_message(TUNMessage *tunmsg) {

	unsigned int tot = 0;
	int n = 0;
	while (tot < tunmsg->size) {
		if ((n = write(tunnel_fd, tunmsg->data + tot, tunmsg->size - tot))
				< 0) {
			perror("Writing to interface");
			fprintf(stderr,
					"Error: %s (errno: %d), tunnel fd: %d, data length: %d, bytes written tot: %d, write returned: %d \n",
					strerror(errno), errno, tunnel_fd, tunmsg->size, tot, n);
			//close(tunnel_fd);
			//exit(1);
			bytes_failed += tunmsg->size;
			return (false);
		}
		if (n > 0)
			tot += n;
		else {
			printf("Data not written");
			return (false);
		}
	}

	bytes_successful += tunmsg->size;
	frames_in++;
	//printf("Wrote to buffer: %d bytes, (%i)\n", tot, tunmsg->size);
	//print_hex(tunmsg->data, tunmsg->size);
	return (true);
}

int TUNHandler::interface_setup(char *dev, int flags, const char *ip,
		const char *destination, const char *mask, int mtu) {

	printf(
			"Interface '%s' will have IP '%s', Destination '%s', mask '%s', MTU '%d'\n",
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

	printf("Setting interface UP \n");
	interface_set_flags(dev, IFF_UP);
	printf("....\n");

	printf("Setting Destination IP \n");
	interface_set_destination(dev, destination);
	printf("....\n");

	printf("Configuration finished \n");

	return (fd);
}

bool TUNHandler::interface_set_destination(const char *deviceName,
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

void TUNHandler::interface_set_mtu(const char *dev, unsigned int mtu) {

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
void TUNHandler::interface_set_flags(const char *device_name, int flags) {
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

void TUNHandler::interface_set_ip(const char *device_name, const char *ip,
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
int TUNHandler::open_tunnel(const char *device_name) {
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

