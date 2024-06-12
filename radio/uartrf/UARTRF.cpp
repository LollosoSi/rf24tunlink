/*
 * UARTRF.cpp
 *
 *  Created on: 12 Jun 2024
 *      Author: Andrea Roccaccino
 */

#include "UARTRF.h"

UARTRF::UARTRF() {
	// TODO Auto-generated constructor stub
	
}

UARTRF::~UARTRF() {
	stop_module();
}

void UARTRF::initialize_uart() {

	printf("Opening device file: %s\n",
			current_settings()->uart_device_file.c_str());
	if (current_settings()->uart_device_file.length() == 0) {
		printf("Empty serial device was passed!\n");
		return;
	}
	uart_file_descriptor = open(current_settings()->uart_device_file.c_str(),
	O_RDWR | O_NOCTTY);

	struct termios tty;

	if (tcgetattr(uart_file_descriptor, &tty) != 0) {
		printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
		throw std::invalid_argument("Invalid UART file descriptor");
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

#ifdef debug_reader
	tty.c_cc[VTIME] = 10; // Wait for up to 1s (10 deciseconds), return as soon as 1 byte is received
		tty.c_cc[VMIN] = 0;
#else
	tty.c_cc[VTIME] = 0; // Wait for up to 1s (10 deciseconds), return as soon as 1 byte is received
	tty.c_cc[VMIN] = 2;
#endif

	cfsetispeed(&tty, current_settings()->uart_baudrate);
	cfsetospeed(&tty, current_settings()->uart_baudrate);
	if (tcsetattr(uart_file_descriptor, TCSANOW, &tty) != 0) {
		printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
		return;
	}

}

inline void UARTRF::apply_settings(const Settings &settings) {
	RadioInterface::apply_settings(settings);

	std::unique_lock<std::mutex>(out_mtx);

	payload_size = settings.payload_size;
	receive_buffer_max_size = 2 * payload_size;
	if (receive_buffer)
		delete[] receive_buffer;
	receive_buffer = new uint8_t[receive_buffer_max_size];

	if (uart_file_descriptor != 0) {
		stop_module();
	}

	if (uart_file_descriptor == 0) {

		initialize_uart();
		running = true;

		readthread =
				std::make_unique<std::thread>(
						[this] {

							int nread = 0;
							uint8_t reception[350];
							while (running) {

								RFMessage message = pmf->make_new_packet();
								nread = read(uart_file_descriptor, reception, 350);

								if (nread < 0) {
									perror("Reading from interface");
									close(uart_file_descriptor);
									exit(1);
								}
								receive_bytes(reception, nread);

						}

					});
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
}

void UARTRF::message_reset() {
	receive_buffer_cursor = 0;
}

void UARTRF::message_read_completed() {
	RFMessage rfm = pmf->make_new_packet(receive_buffer, receive_buffer_cursor);
	this->packetizer->input(rfm);
}

void UARTRF::receive_bytes(uint8_t *data, unsigned int length) {
	for (unsigned int i = 0; i < length; i++) {
		if (is_waiting_next_escape) {
			switch (data[i]) {
			case UARTRF::byte_escape:
				receive_buffer[receive_buffer_cursor++] = data[i];
				break;
			case UARTRF::byte_SOF:
				message_reset();
				break;
			case UARTRF::byte_EOF:
				if (receive_buffer_cursor <= payload_size)
					message_read_completed();
				break;
			}
			is_waiting_next_escape = false;
		} else if (data[i] == byte_escape) {
			is_waiting_next_escape = true;
		} else {
			receive_buffer[receive_buffer_cursor++] = data[i];
		}
	}

}

void UARTRF::write_stuff(uint8_t *data, unsigned int length) {
	uint8_t final_string[length * 3];
	unsigned int cur = 0, i = 0;
	final_string[cur++] = byte_escape;
	final_string[cur++] = byte_SOF;
	for (; i < length; i++) {
		if (i == byte_escape) {
			final_string[cur++] = byte_escape;
		}
		final_string[cur++] = data[i];
	}
	final_string[cur++] = byte_escape;
	final_string[cur++] = byte_EOF;
	write(uart_file_descriptor, final_string, cur);
}

void UARTRF::stop_read_thread() {
	running = false;
	if (readthread)
		if (readthread->joinable())
			readthread->join();
}

void UARTRF::close_file() {
	close(uart_file_descriptor);
	uart_file_descriptor = 0;
}

inline void UARTRF::input_finished() {
}

inline bool UARTRF::input(std::vector<RFMessage> &ms) {
	//printf("Writing vector\n");
	std::unique_lock<std::mutex>(out_mtx);
	for (auto &m : ms) {
		write_stuff(m.data.get(), m.length);
	}

	return true;
}

inline bool UARTRF::input(RFMessage &m) {
	//printf("Writing packet\n");
	std::unique_lock<std::mutex>(out_mtx);

	write_stuff(m.data.get(), m.length);

	return true;
}
inline void UARTRF::stop_module() {
	stop_read_thread();
	close_file();
}
