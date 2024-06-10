/*
 * PicoRF24.cpp
 *
 *  Created on: 8 Jun 2024
 *      Author: Andrea Roccaccino
 */

#include "PicoRF24.h"

//#define debug_reader

inline void print_hex(uint8_t *d, int l) {
	for (int i = 0; i < l; i++) {
		std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0')
				<< static_cast<int>(d[i]) << " ";
	}
	std::cout << std::endl;
}

PicoRF24::PicoRF24() {
	uart_file_descriptor = 0;

	if (system("which picotool > /dev/null 2>&1")) {
	    // Command doesn't exist...
		printf("To use this radio, you need to install picotool\n");
		exit(2);
	} else {
	    // Command does exist, do something with it...
		printf("Found picotool\n");
		restart_pico();
	}

}

PicoRF24::~PicoRF24() {
	stop_module();
	restart_pico();

}

void PicoRF24::initialize_uart(){

	printf("Opening device file: %s\n",current_settings()->pico_device_file.c_str());
	if (current_settings()->pico_device_file.length() == 0) {
		printf("Empty serial device was passed!\n");
		return;
	}
	uart_file_descriptor = open(current_settings()->pico_device_file.c_str(), O_RDWR | O_NOCTTY);

	struct termios tty;

	if (tcgetattr(uart_file_descriptor, &tty) != 0) {
		printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
		throw std::invalid_argument("Invalid Pico file descriptor");
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
		tty.c_cc[VMIN] = 32;
#endif

	//cfsetispeed(&tty, B115200);
	//cfsetospeed(&tty, B115200);
	if (tcsetattr(uart_file_descriptor, TCSANOW, &tty) != 0) {
		printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
		return;
	}

}

void PicoRF24::transfer_settings(PicoSettingsBlock *psb){

	uint8_t* cast_psb = (uint8_t*)psb;
	int transfersize = sizeof(PicoSettingsBlock);
	printf("Writing %d bytes of PSB\n",transfersize);
	write(uart_file_descriptor, cast_psb, transfersize);

}

void PicoRF24::stop_read_thread(){
	running = false;
	if(readthread)
		if(readthread->joinable())
			readthread->join();
}

void PicoRF24::restart_pico(){
	 system("picotool reboot -a -f");
	 std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}

void PicoRF24::close_file(){
	close(uart_file_descriptor);
	uart_file_descriptor = 0;
}

void PicoRF24::apply_settings(const Settings &settings) {
		RadioInterface::apply_settings(settings);

		std::unique_lock<std::mutex>(out_mtx);

		if(uart_file_descriptor != 0){
			stop_module();
			restart_pico();
		}

		if(uart_file_descriptor == 0){
			PicoSettingsBlock psb;
			apply_settings_to_pico_block(psb, current_settings());

			initialize_uart();
			running = true;
			transfer_settings(&psb);
			readthread = std::make_unique<std::thread>([this] {

				int nread = 0;


				//printf("Sending: \n");
				//			print_hex(bfs, bfs_size);
				//			write(file, bfs, bfs_size);

				while (running) {
					RFMessage message(50000);
					memset(message.data.get(), '\0', 50000);
					nread = read(uart_file_descriptor, message.data.get(), 50000);
					if (nread < 0) {
						perror("Reading from interface");
						close(uart_file_descriptor);
						exit(1);
					}
					if (nread != 0) {
						message.length = nread;
						//receivedbytes += nread;
						//printf("From UART %i %s\n",message.length, message.data.get());
						//printf("Compares? %d\n",
						//		std::memcmp(message.data.get(), bfs, bfs_size));
						//print_hex(message.data.get(), nread);
						//print_hex(bfs, bfs_size);
						//write(file, bfs, 32);
						#ifdef debug_reader
						std::cout << "From UART " << message.length << " . "<< message.data.get()<< std::endl;

						#else
						/*if((nread%current_settings()->payload_size == 0) && nread > current_settings()->payload_size){
							for(int i = 0; i < nread/current_settings()->payload_size; i++){
								TunMessage ms(current_settings()->payload_size);
								memcpy(ms.data.get(), message.data.get()+(i*current_settings()->payload_size),current_settings()->payload_size);
								this->packetizer->input(ms);
							}
						}else*/
						//print_hex(message.data.get(), nread);
							this->packetizer->input(message);
						#endif

					}
				}

			});
			//std::this_thread::sleep_for(std::chrono::milliseconds(10));

			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
}

inline void PicoRF24::input_finished(){}

inline bool PicoRF24::input(std::vector<RFMessage> &ms) {
	std::unique_lock<std::mutex>(out_mtx);
	for (auto &m : ms)
		write(uart_file_descriptor, m.data.get(), m.length);

	return true;
}

inline bool PicoRF24::input(RFMessage &m){

	std::unique_lock<std::mutex>(out_mtx);

	write(uart_file_descriptor, m.data.get(), m.length);

	return true;
}
inline void PicoRF24::stop_module(){
	stop_read_thread();
	close_file();
}
