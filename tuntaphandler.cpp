/*
 * tuntaphandler.cpp
 *
 *  Created on: Oct 10, 2023
 *      Author: andrea
 */

#include "tuntaphandler.h"

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


void tuntaphandler::TunnelThread() {

	this->running_ = true;

	uint8_t buffer[mtu + (mtu / 2)];
	int nread = 0;
	std::thread t([&] {

		while (this->running_) {
			//while(nread!=0){}
			/* Note that "buffer" should be at least the MTU size of the interface, eg 1500 bytes */
			nread = read(tunnel_fd_, buffer, sizeof(buffer));
			if (nread < 0) {
				perror("Reading from interface");
				close(tunnel_fd_);
				exit(1);
			}

			/* Do whatever with the data */
			//printf("Read from buffer: %d bytes, sum %d, CRC: %d \n", nread, (int) checksum(buffer, nread), gencrc(buffer, nread));
			//radioppppprintchararray((uint8_t*)buffer, nread);
			rph2.handleData(buffer, nread);
			//writeback(nread, buffer);
			//usleep(1000000);
		}
	});
	t.detach();

	//std::thread t2([&] {
	while (running_) {
		//if(nread!=0){
		//	pth.handlepacket(nread, buffer);
		//	nread = 0;
		//}
		rph2.loop();
		//usleep(500);
		//std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	//});
	//t2.detach();

}

bool tuntaphandler::writeback(uint8_t *data, int size) {

	int tot = 0;
	int n = 0;
	while (tot < size) {
		if ((n = write(tunnel_fd_, data + tot, size - tot)) < 0) {
			perror("Writing to interface");
			fprintf(stderr,
					"Error: %s (errno: %d), tunnel fd: %d, data length: %d, bytes written tot: %d, write returned: %d \n",
					strerror(errno), errno, tunnel_fd_, size, tot, n);
			//close(tunnel_fd_);
			//exit(1);
			return false;
		}
		if (n > 0)
			tot += n;
		else {
			printf("Data not written");
			return false;
		}
	}

//printf("Wrote to buffer: %d bytes, sum %d, CRC %d\n", tot, (int) checksum(data, size), gencrc(data,size));
	return true;
}

tuntaphandler::tuntaphandler(bool primary) :
		Callback() {
	const char addr1[] = "192.168.10.1";
	const char addr2[] = "192.168.10.2";
	const char mask[] = "255.255.255.0";

	char *d = new char[30] { '\0' };
	strcpy(d, "arocc");

	rph2.setup(this, primary, primary ? Settings::address1 : Settings::address2, primary ? Settings::address2 : Settings::address1);

	tunnel_fd_ = setup_interface(d, IFF_TUN | IFF_UP | IFF_RUNNING,
			primary ? addr1 : addr2, mask, 5, primary);
	setDestination(d, primary ? addr2 : addr1);
	std::cout << tunnel_fd_ << " " << d << std::endl;

//networkthread = new std::thread(&tuntaphandler::TunnelThread, this);
//networkthread->detach();

}

int tuntaphandler::setup_interface(char *dev, int flags, const char *ip,
		const char *mask, uint8_t segmentbits, bool primary) {

	this->mtu = Settings::mtu;
	std::cout << "MTU: " << mtu << std::endl;
	int fd = OpenTunnel(dev);

	SetFlags(dev, IFF_UP);
	printf("tunnel '%s' up", dev);
	SetIP(dev, ip, mask);
	setMTU(dev);
	printf("tunnel '%s' configured with '%s' mask '%s', MTU '%d'", dev, ip, mask,
			mtu);

	return fd;
}

bool tuntaphandler::setDestination(const char *deviceName,
		const char *destinationIP) {
	int socketFD = socket(AF_INET, SOCK_DGRAM, 0);
	if (socketFD < 0) {
		std::cerr << "Failed to open socket: " << strerror(errno) << " ("
				<< errno << ")" << std::endl;
		return false;
	}

	struct ifreq ifr;
	std::strncpy(ifr.ifr_name, deviceName, IFNAMSIZ);
	ifr.ifr_addr.sa_family = AF_INET;

	if (inet_pton(AF_INET, destinationIP,
			&((struct sockaddr_in*) (&ifr.ifr_addr))->sin_addr) <= 0) {
		std::cerr << "Failed to convert destination IP: " << strerror(errno)
				<< " (" << errno << ")" << std::endl;
		close(socketFD);
		return false;
	}

	if (ioctl(socketFD, SIOCSIFDSTADDR, &ifr) < 0) {
		std::cerr << "Failed to set destination address: " << strerror(errno)
				<< " (" << errno << ")" << std::endl;
		close(socketFD);
		return false;
	}

	close(socketFD);
	return true;
}

void tuntaphandler::setMTU(const char *dev) {

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
void tuntaphandler::SetFlags(const char *device_name, int flags) {
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(fd >= 0) printf("Error opening socket: %s (%d)", strerror(errno), errno);

	struct ifreq ifr = { };
	ifr.ifr_flags = flags;
	strncpy(ifr.ifr_name, std::string(device_name).c_str(), IFNAMSIZ);
	int status = ioctl(fd, SIOCSIFFLAGS, &ifr);
	if(status >= 0)
		printf("Couldn't to set tunnel interface: %s (%d)",
			strerror(errno), errno);
	close(fd);
}

void tuntaphandler::SetIP(const char *device_name, const char *ip,
		const char *ip_mask) {
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(fd >= 0)printf("Error opening socket: %s (%d)", strerror(errno), errno);

	struct ifreq ifr = { };
	strncpy(ifr.ifr_name, std::string(device_name).c_str(), IFNAMSIZ);

	ifr.ifr_addr.sa_family = AF_INET;
	if(inet_pton(AF_INET, std::string(ip).c_str(), &reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_addr)->sin_addr) == 1)
		printf("Failed to assign IP address: %s (%d)", strerror(errno), errno);
	int status = ioctl(fd, SIOCSIFADDR, &ifr);
	if(status >= 0)printf("Couldn't set tunnel interface ip: %s (%d)",
			strerror(errno), errno);

	ifr.ifr_netmask.sa_family = AF_INET;
	if(inet_pton(AF_INET, std::string(ip_mask).c_str(), &reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_netmask)->sin_addr) == 1)
		printf("Couldn't assign IP mask: %s (%d)", strerror(errno), errno);
	status = ioctl(fd, SIOCSIFNETMASK, &ifr);
	if(status >= 0)
		printf("Failed setting interface mask: %s (%d)", strerror(errno), errno);
	close(fd);
}

// Opens the tunnel interface to listen on. Always returns a valid file
// descriptor or quits and logs the error.
int tuntaphandler::OpenTunnel(const char *device_name) {
	int fd = open("/dev/net/tun", O_RDWR);
	if(fd >= 0)
		printf("Error opening tunnel file: %s (%d)", strerror(errno),
			errno);

	struct ifreq ifr = { };
	ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
	strncpy(ifr.ifr_name, std::string(device_name).c_str(), IFNAMSIZ);

	int status = ioctl(fd, TUNSETIFF, &ifr);
	if(status >= 0)
		printf("Error setting tunnel interface: %s (%d)",
			strerror(errno), errno);
	return fd;
}

void tuntaphandler::stop_interface() {

	running_ = false;
//networkthread->join();

}

tuntaphandler::~tuntaphandler() {
// TODO Auto-generated destructor stub
}

