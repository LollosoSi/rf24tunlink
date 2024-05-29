/*
 * Settings.h
 *
 *  Created on: 26 May 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include <algorithm>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <iostream>

#include <sstream>

class Settings {

	public:
		Settings() {
			std::filesystem::path cwd = std::filesystem::current_path();
			printf("Relative directory:\t%s\n", cwd.string().c_str());

			if (settings_types.size() != settings_names.size()
					|| settings_types.size() != references.size()
					|| references.size() != settings_names.size()
					|| settings_descriptions.size() != references.size()) {
				std::cout
						<< "Inappropriate size of settings vectors, this may be an hardcoded problem. Fix it and recompile.\n";
				throw std::invalid_argument("Illegal settings");
			}
		}
		~Settings() {
		}
		void print_all_settings(std::ostream &out = std::cout, bool file_ready =
				false, bool print_descriptions = false) {
			if (!file_ready)
				out << "Printing all settings:\nType\t\t\tName\t\t\tValue\n";
			for (unsigned int k = 0; k < name_types.size(); k++)
				for (unsigned int i = 0; i < settings_names.size(); i++) {
					if (settings_types[i] != k)
						continue;
					if (!file_ready)
						out << name_types[settings_types[i]] << "\t\t\t";

					if (print_descriptions) {
						out << "\n#\t" << settings_descriptions[i];
						out << "\n#\tType:\t" << name_types[settings_types[i]]
								<< "\n";
					}
					out << settings_names[i] << (file_ready ? "=" : "\t\t\t");

					switch (settings_types[i]) {
					case boolean:
						out << ((*((bool*) references[i])) ? "yes" : "no");
						break;
					case uint8:
						out << (int) (*((uint8_t*) references[i]));
						break;
					case uint16:
						out << (int) (*((uint16_t*) references[i]));
						break;
					case uint32:
						out << (int) (*((uint32_t*) references[i]));
						break;
					case integer:
						out << (*((int*) references[i]));
						break;
					case string:
						out << (*((std::string*) references[i]));
						break;
					}

					out << "\n";
				}

		}
		std::string address = "192.168.10.1"; // Address of the interface		NOTE: Address and destination must be swapped based on the radio role
		std::string destination = "192.168.10.2"; // Destination of the interface
		std::string netmask = "255.255.255.0";			// Network address mask
		std::string interface_name = "arocc";	// Interface name (arocc)

		uint16_t minimum_ARQ_wait = 10;
		uint16_t maximum_frame_time = 150;

		bool display_telemetry = false;
		std::string csv_out_filename = "";	// CSV output, NULLPTR for no output
		char csv_divider = ',';

		std::string tunnel_handler = "tun";
		std::string packetizer = "harq";
		std::string radio_handler = "dualrf24";

		bool auto_ack = false;

		bool dynamic_payloads = false;
		bool ack_payloads = false;

		bool primary = true;

		int ce_0_pin = 24;
		int csn_0_pin = 0;

		int ce_1_pin = 26;
		int csn_1_pin = 12;

		int irq_pin_radio0 = -1;
		int irq_pin_radio1 = -1;

		uint32_t spi_speed = 4000000;

		uint8_t payload_size = 32;
		uint8_t data_bytes = 30;
		uint8_t ecc_bytes = 2;

		uint8_t data_rate = 1;
		uint8_t radio_power = 0;
		uint8_t crc_length = 0;

		uint8_t radio_delay = 1;
		uint8_t radio_retries = 15;

		uint8_t channel_0 = 10;
		uint8_t channel_1 = 120;
		uint8_t address_bytes = 3;
		std::string address_0_1 = "11n";
		std::string address_0_2 = "21n";
		std::string address_0_3 = "31n";

		std::string address_1_1 = "41n";
		std::string address_1_2 = "51n";
		std::string address_1_3 = "61n";

		unsigned int mtu = 500;

		void apply_settings(std::string &name, std::string &value) {

			auto name_handle = std::find(settings_names.begin(),
					settings_names.end(), name);
			if (name_handle == settings_names.end()) {
				std::cout << "Couldn't find setting of name: " << name
						<< ". Please check typos.\n";
				return;
			}

			int i = name_handle - settings_names.begin();

			switch (settings_types[i]) {
			case boolean:
				((*((bool*) references[i])) = (value == "yes"));
				break;
			case uint8:
				(*((uint8_t*) references[i])) = atoi(value.c_str());
				break;
			case uint16:
				(*((uint16_t*) references[i])) = atoi(value.c_str());
				break;
			case uint32:
				(*((uint32_t*) references[i])) = atol(value.c_str());
				break;
			case integer:
				(*((int*) references[i])) = atoi(value.c_str());
				break;
			case string:
				(*((std::string*) references[i])) = value;
				break;
			}
		}

		void read_settings(const char *settings_file) {

			uint64_t entries = 0, lines = 0;

			if (!settings_file) {
				printf("No settings to apply\n");
				return;
			}

			printf("Opening file:\t%s\t\t", settings_file);

			std::ifstream file;
			file.open(settings_file);
			if (!file.good()) {
				printf("Problem reading settings file\n");

				if (file.fail()) {
					// Get the error code
					std::ios_base::iostate state = file.rdstate();

					// Check for specific error bits
					if (state & std::ios_base::eofbit)
						std::cout << "End of file reached." << std::endl;

					if (state & std::ios_base::failbit)
						std::cout << "Non-fatal I/O error occurred."
								<< std::endl;

					if (state & std::ios_base::badbit)
						std::cout << "Fatal I/O error occurred." << std::endl;

					// Print system error message
					std::perror("Error: ");
				}

				file.close();
				return;
			}
			std::string a;
			std::string b;
			while (!file.eof()) {
				char buf[2000] = { 0 };
				file.getline(buf, 2000);
				std::istringstream buffer(buf);

				lines++;

				if (buffer.str().size() == 0)
					continue;
				else if (buffer.str()[0] == '#')
					continue;

				entries++;

				std::getline(buffer, a, '=');
				std::getline(buffer, b);
				std::transform(a.begin(), a.end(), a.begin(),
						[](unsigned char c) {
							return std::tolower(c);
						});
				std::transform(b.begin(), b.end(), b.begin(),
						[](unsigned char c) {
							return std::tolower(c);
						});
				//printf("Buf A: %s\n", a.c_str());
				//printf("Buf B: %s\n", b.c_str());

				apply_settings(a, b);

			}
			file.close();

			std::cout << "Loaded " << entries << " entries and removed "
					<< (lines - entries) << " extra lines\n";
		}

		void to_file(const char *settings_file) {

			std::ofstream file;
			file.open(settings_file);
			if (!file.good()) {
				printf("Problem opening file\n");

				if (file.fail()) {
					// Get the error code
					std::ios_base::iostate state = file.rdstate();

					// Check for specific error bits
					if (state & std::ios_base::eofbit) {
						std::cout << "End of file reached." << std::endl;
					}
					if (state & std::ios_base::failbit) {
						std::cout << "Non-fatal I/O error occurred."
								<< std::endl;
					}
					if (state & std::ios_base::badbit) {
						std::cout << "Fatal I/O error occurred." << std::endl;
					}

					// Print system error message
					std::perror("Error: ");
				}

				file.close();
				return;
			}

			print_all_settings(file, true, true);

			file.close();
		}

		// NOTE: names and enum have to be in corresponding order!
		std::vector<std::string> packetizers_names = { "harq" };
		enum packetizers_available {
			harq
		};

		const packetizers_available find_packetizer_index() const {
			auto name_handle = std::find(packetizers_names.begin(),
					packetizers_names.end(), packetizer);
			if (name_handle == packetizers_names.end()) {
				std::cout << "Couldn't find packetizer of name: " << packetizer
						<< ". Please check typos.\n";
				throw std::invalid_argument("Invalid packetizer");
			}

			return (packetizers_available) (name_handle
					- packetizers_names.begin());
		}

		// NOTE: names and enum have to be in corresponding order!
		std::vector<std::string> radios_names = { "dualrf24" };
		enum radios_available {
			dualrf24
		};

		const radios_available find_radio_index() const {
			auto name_handle = std::find(radios_names.begin(),
					radios_names.end(), radio_handler);
			if (name_handle == radios_names.end()) {
				std::cout << "Couldn't find radio handler of name: " << radio_handler
						<< ". Please check typos.\n";
				throw std::invalid_argument("Invalid radio");
			}

			return (radios_available) (name_handle
					- radios_names.begin());
		}

	private:
		enum types {
			boolean, string, uint8, uint16, uint32, integer
		};
		std::vector<std::string> name_types = { "bool", "string", "uint8",
				"uint16", "uint32", "integer" };

		std::vector<void*> references = { &address, &destination, &netmask,
				&interface_name, &minimum_ARQ_wait, &maximum_frame_time,
				&display_telemetry, &csv_out_filename, &csv_divider,
				&tunnel_handler, &packetizer, &radio_handler, &auto_ack,
				&ce_0_pin, &csn_0_pin, &ce_1_pin, &csn_1_pin, &spi_speed,
				&payload_size, &data_bytes, &ecc_bytes, &data_rate,
				&radio_power, &crc_length, &radio_delay, &radio_retries,
				&channel_0, &channel_1, &address_bytes, &address_0_1,
				&address_0_2, &address_0_3, &address_1_1, &address_1_2,
				&address_1_3, &primary, &dynamic_payloads, &ack_payloads, &irq_pin_radio0, &irq_pin_radio1 };
		std::vector<std::string> settings_names = { "address", "destination",
				"netmask", "iname", "minimum_arq_wait", "maximum_frame_time",
				"display_telemetry", "csv_out_filename", "csv_divider",
				"tunnel_handler", "packetizer", "radio_handler", "auto_ack",
				"ce_0_pin", "csn_0_pin", "ce_1_pin", "csn_1_pin", "spi_speed",
				"payload_size", "data_bytes", "ecc_bytes", "data_rate",
				"radio_power", "crc_length", "radio_delay", "radio_retries",
				"channel_0", "channel_1", "address_bytes", "address_0_1",
				"address_0_2", "address_0_3", "address_1_1", "address_1_2",
				"address_1_3", "primary", "dynamic_payloads", "ack_payloads", "irq_pin_radio0", "irq_pin_radio1" };
		std::vector<types> settings_types = { string, string, string, string,
				uint16, uint16, boolean, string, uint8, string, string, string,
				boolean, integer, integer, integer, integer, uint32, uint8,
				uint8, uint8, uint8, uint8, uint8, uint8, uint8, uint8, uint8,
				uint8, string, string, string, string, string, string,	boolean,	boolean,	boolean,	integer,	integer};
		std::vector<std::string> settings_descriptions =
				{ "The address of the interface",
						"The destination address of the interface",
						"The network mask of the interface",
						"The interface name",
						"The minimum time in milliseconds the ARQ algorithm should wait before declaring one packet lost",
						"The maximum time in milliseconds to retry sending one frame before dropping it",
						"Displays telemetry to the command line",
						"Filename for the CSV output (leave empty for no output)",
						"Divider character for the output file values",
						"The tunnel handler (Accepted values: TUN, UART)",
						"The packetizer (Accepted values: HARQ)",
						"The radio handler (Accepted values: DualRF24)",
						"Whether the RF24 should auto ack. Used in one radio setups",
						"CE pin for the radio0",
						"CSN pin for the radio0 (it's system CE)",
						"CE pin for the radio1",
						"CSN pin for the radio1 (it's system CE)",
						"SPI Speed in Hz for all RF24 radios",
						"Size of the payload. This value is 32 and shouldn't be touched unless you intend to use radios different than RF24",
						"The data bytes for the Reed-Solomon codec",
						"The ECC bytes for the Reed-Solomon codec (note, one byte might be used as CRC, this element must be >0)",
						"The modulation speed of the radios (Accepted values: 0 - 1Mbps, 1 - 2Mbps, 2 - 250kbps )",
						"Power setting for all radios (Accepted values: 0 - MIN, 1 - LOW, 2 - HIGH, 3 - MAX)",
						"The CRC length for the RF24 module in bytes. (Accepted values 0 to 2). Should be disabled with the double radio setup.",
						"Delay for each automatic retransmission of failed packets from the radio",
						"How many retries the radio should do before skipping the packet",
						"Channel of the radio 0 (Accepted values: 0 to 124)",
						"Channel of the radio 1 (Accepted values: 0 to 124)",
						"Bytes for the radio address. (Accepted values: 3 to 5). Recommended to leave this as 3.",
						"Pipe address 0\n# (NOTE: all pipes must share the last 2 bytes. Example: 1pe, 2pe, 3pe are valid addresses and should work)\n# (NOTE: All pipes must have the byte length specified in address_bytes)",
						"Pipe address 1", "Pipe address 2", "Pipe address 3",
						"Pipe address 4", "Pipe address 5",
						"Role of this instance (sets up the correct addresses) (Accepted values: yes, no)",
						"Whether the RF24 radio should use dynamic sized payloads (best performance is no)",
						"Whether the RF24 radio should use ACK payloads (only for single radio setups)",
						"IRQ pin of the radio 0",
						"IRQ pin of the radio 1"};
};

class SettingsCompliant {
	protected:
		const Settings* settings = nullptr;
	public:
		SettingsCompliant() = default;
		virtual ~SettingsCompliant() {
			settings = nullptr;
		}
		virtual void apply_settings(const Settings &settings) {
			this->settings = &settings;
		}
		inline const Settings* current_settings() const {return (settings);}
};
