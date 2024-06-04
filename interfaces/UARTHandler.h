/*
 * UARTHandler.h
 *
 *  Created on: 24 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

#include <thread>

class UARTHandler: public Messenger<TUNMessage> {
public:
	unsigned long receivedbytes = 0;
	UARTHandler(const char *device) {
		if (!device) {
			printf("Null serial device was passed!\n");
			return;
		}
		file = open(device, O_RDWR);

		struct termios tty;

		if (tcgetattr(file, &tty) != 0) {
			printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
			return;
		}

		tty.c_cflag &= ~PARENB;
		tty.c_cflag &= ~CSTOPB;
		tty.c_cflag &= ~CSIZE;
		tty.c_cflag |= CS8;
		tty.c_cflag &= ~CRTSCTS;
		tty.c_cflag |= CREAD | CLOCAL;
		tty.c_lflag &= ~ICANON;
		tty.c_lflag &= ~ECHO;
		tty.c_lflag &= ~ECHOE;
		tty.c_lflag &= ~ECHONL;
		tty.c_lflag &= ~ISIG;
		tty.c_iflag &= ~(IXON | IXOFF | IXANY);
		tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR
				| ICRNL);
		tty.c_oflag &= ~OPOST;
		tty.c_oflag &= ~ONLCR;
		tty.c_cc[VTIME] = 10;
		tty.c_cc[VMIN] = 0;
		cfsetispeed(&tty, B115200);
		cfsetospeed(&tty, B115200);
		if (tcsetattr(file, TCSANOW, &tty) != 0) {
			printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
			return;
		}

		running = true;
		readthread = new std::thread([&] {

			int nread = 0;
			TUNMessage *message = new TUNMessage;
			message->data = new uint8_t[5000];

			while (running) {
				/* Note that "buffer" should be at least the MTU size of the interface, eg 1500 bytes */
				//printf("Waiting for tun packet\n");
				nread = read(file, message->data, 5000);
				if (nread < 0) {
					perror("Reading from interface");
					close(file);
					exit(1);
				}

				/* Do whatever with the data */
				message->size = nread;
				receivedbytes+=nread;
				message->data[message->size]=0;
				//printf("From UART %s (%i)\n", message->data, message->size);
				//print_hex(message->data, message->size);

				//this->process_message(message);


			}

			delete [] message->data;
			delete message;

		});
		readthread->detach();
	}

	virtual ~UARTHandler() {
		running = false;
		close(file);
		if (readthread)
			delete readthread;

	}

	bool receive_message(TUNMessage *tunmsg){

		write(file, tunmsg->data, tunmsg->size);

		return (true);
	}

protected:
	bool running = false;
	int file;
	std::thread *readthread = nullptr;

};

