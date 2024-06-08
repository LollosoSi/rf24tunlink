/*
 * UARTHandler.h
 *
 *  Created on: 24 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../../generic_structures.h"

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

#include <cstring>

#include <thread>

inline void print_hex(uint8_t *d, int l) {
	for (int i = 0; i < l; i++) {
		std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0')
				<< static_cast<int>(d[i]) << " ";
	}
	std::cout << std::endl;
}

class UARTHandler : public Messenger<TunMessage> {
	public:
		unsigned long receivedbytes = 0;
		UARTHandler(const char *device) {
			const int bfs_size = 512;
			uint8_t* bfs = new uint8_t[bfs_size];
			uint8_t c = 255;
			for (int i = 0; i < bfs_size; i++) {
				bfs[i] = c++;
			}

			if (!device) {
				printf("Null serial device was passed!\n");
				return;
			}
			file = open(device, O_RDWR | O_NOCTTY);

			struct termios tty;

			if (tcgetattr(file, &tty) != 0) {
				printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
				return;
			}

			tty.c_cflag &= ~PARENB; // No parity bit
			    tty.c_cflag &= ~CSTOPB; // 1 stop bit
			    tty.c_cflag &= ~CSIZE;
			    tty.c_cflag |= CS8; // 8 data bits
			    tty.c_cflag &= ~CRTSCTS; // No hardware flow control
			    tty.c_cflag |= CREAD | CLOCAL; // Enable receiver, ignore modem status lines

			    tty.c_lflag &= ~ICANON;
			    tty.c_lflag &= ~ECHO;
			    tty.c_lflag &= ~ECHOE;
			    tty.c_lflag &= ~ECHONL;
			    tty.c_lflag &= ~ISIG;

			    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // No software flow control
			    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

			    tty.c_oflag &= ~OPOST; // No special interpretation of output bytes
			    tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

			    tty.c_cc[VTIME] = 10; // Wait for up to 1s (10 deciseconds), return as soon as 1 byte is received
			    tty.c_cc[VMIN] = bfs_size;
			cfsetispeed(&tty, B115200);
			cfsetospeed(&tty, B115200);
			if (tcsetattr(file, TCSANOW, &tty) != 0) {
				printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
				return;
			}

			running = true;
			readthread = new std::thread([&, bfs] {

				int nread = 0;
				TunMessage message(bfs_size);

				//printf("Sending: \n");
				//			print_hex(bfs, bfs_size);
				//			write(file, bfs, bfs_size);

				while (running) {
					nread = read(file, message.data.get(), bfs_size);
					if (nread < 0) {
						perror("Reading from interface");
						close(file);
						exit(1);
					}
					if (nread != 0) {
						message.length = nread;
						receivedbytes += nread;
						//printf("From UART %i\n",message.length);
						//printf("Compares? %d\n",
						//		std::memcmp(message.data.get(), bfs, bfs_size));
						//print_hex(message.data.get(), nread);
						//print_hex(bfs, bfs_size);
						//write(file, bfs, 32);
					}
				}

			});
			readthread->detach();


			//write(file, "012345678901234567890123456789010123456789012345678901234567890012345678901234567890123456789010123456789012345678901234567890012345678901234567890123456789010123456789012345678901234567890012345678901234567890123456789010123456789012345678901234567890123", 256);

		}

		virtual ~UARTHandler() {
			running = false;
			close(file);
			if (readthread)
				delete readthread;

		}

		bool input(TunMessage &tunmsg) {

			//write(file, tunmsg->data, tunmsg->size);

			return (true);
		}

	protected:
		bool running = false;
		int file;
		std::thread *readthread = nullptr;

};

