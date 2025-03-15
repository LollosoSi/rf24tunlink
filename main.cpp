#include <stdio.h>
#include <unistd.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "tusb.h"

#include "hardware/spi.h"
#include <RF24.h>

//#define CFG_TUD_CDC_EP_BUFSIZE  (1600) //add this
//#define CFG_TUD_CDC_RX_BUFSIZE  (256)
//#define CFG_TUD_CDC_TX_BUFSIZE  (1600)

#define LED_PIN 25

//#define debug_reader

const int radio0_ce = 17, radio0_csn = 21, radio0_sck = 18, radio0_mo = 19,
		radio0_mi = 16, radio0_irq = 20;
const int radio1_ce = 13, radio1_csn = 9, radio1_sck = 10, radio1_mo = 11,
		radio1_mi = 8, radio1_irq = 12;

RF24 radio0(radio0_ce, radio0_csn);
RF24 radio1(radio1_ce, radio1_csn);
SPI spi0_object; // we specify the `spi0` bus interface below
SPI spi1_object; // we specify the `spi1` bus interface below

uint8_t* read_buffer = nullptr;

#pragma pack(push, 1)
struct PicoSettingsBlock {
		bool auto_ack;
		bool dynamic_payloads;
		bool ack_payloads;
		bool primary;

		uint8_t payload_size;
		uint8_t data_rate;
		uint8_t radio_power;
		uint8_t crc_length;
		uint8_t radio_delay;
		uint8_t radio_retries;
		uint8_t channel_0;
		uint8_t channel_1;
		uint8_t address_bytes;
		uint8_t address_0_1[6];
		uint8_t address_0_2[6];
		uint8_t address_0_3[6];
		uint8_t address_1_1[6];
		uint8_t address_1_2[6];
		uint8_t address_1_3[6];
};
#pragma pack(pop)

struct PicoSettingsBlock global_settings;

bool check_radio0_status() {
	bool c1 = radio0.failureDetected;
	bool c2 = radio0.getDataRate()
			!= ((rf24_datarate_e) global_settings.data_rate);
	bool c3 = !radio0.isChipConnected();
	if (c1 || c2 || c3) {
		//printf("Radio %d has failure. C1 %d, C2 %d, C3 %d\n", radio==&radio0 ? 0 : 1, c1, c2, c3);
		gpio_put(LED_PIN, 1);
		sleep_ms((c1 ? 100 : 0) + (c2 ? 500 : 0) + (c3 ? 1000 : 0));
		gpio_put(LED_PIN, 0);
		radio0.failureDetected = 0;
		return (true);
	} else {
		return (false);
	}
}

bool check_radio1_status() {
	bool c1 = radio1.failureDetected;
	bool c2 = radio1.getDataRate()
			!= ((rf24_datarate_e) global_settings.data_rate);
	bool c3 = !radio1.isChipConnected();
	if (c1 || c2 || c3) {
		printf("Radio %d has failure. C1 %d, C2 %d, C3 %d\n", 1, c1, c2, c3);

		write(1, "Radio 1 has failure: ", 21);
		if (c1)
			write(1, "C1 ", 3);
		if (c2)
			write(1, "C2 ", 3);
		if (c3)
			write(1, "C3 ", 3);
		write(1, "\n", 1);
		gpio_put(LED_PIN, 1);
		sleep_ms((c1 ? 1000 : 0) + (c2 ? 2000 : 0) + (c3 ? 5000 : 0));
		gpio_put(LED_PIN, 0);
		radio1.failureDetected = 0;
		return (true);
	} else {
		return (false);
	}
}

void print_global_settings() {

	//tud_cdc_write((uint8_t*) &global_settings, sizeof(global_settings));
	//fprintf(stdout, "Channel 0 - 1: %d - %d, Delay: %d, Retries: %d, DataRate: %d, RadioPower: %d\n",
	//		global_settings.channel_0, global_settings.channel_1,
	//		global_settings.radio_delay, global_settings.radio_retries,
	//		global_settings.data_rate, global_settings.radio_power);
	//tud_cdc_write_flush();
}

inline uint64_t string_to_address(const uint8_t *str) {
	uint64_t result = 0;
	result |= static_cast<uint64_t>(static_cast<uint8_t>(str[0])) << 16;
	result |= static_cast<uint64_t>(static_cast<uint8_t>(str[1])) << 8;
	result |= static_cast<uint64_t>(static_cast<uint8_t>(str[2]));

	//std::cout << "Byte 0: 0x" << std::hex << static_cast<int>(static_cast<uint8_t>(str[0])) << std::endl;
	//std::cout << "Byte 1: 0x" << std::hex << static_cast<int>(static_cast<uint8_t>(str[1])) << std::endl;
	//std::cout << "Byte 2: 0x" << std::hex << static_cast<int>(static_cast<uint8_t>(str[2])) << std::endl;
	//std::cout << "Result: 0x" << std::hex << result << std::endl;

	return result;
}

void resetRadio0() {
	uint8_t t = 1;
	while (!radio0.begin(&spi0_object)) {
#ifdef debug_reader
		printf("Radio 0 is not responsive\n");
#endif
		sleep_ms(100);

		if (!t++) {
			// handleRadioUnresponsiveTimeout();
		}
	}

	radio0.flush_rx();
	radio0.flush_tx();

	radio0.setAddressWidth(global_settings.address_bytes);

	radio0.setPayloadSize(global_settings.payload_size);

	radio0.setPALevel(global_settings.radio_power);
	radio0.setDataRate((rf24_datarate_e) global_settings.data_rate);
	radio0.setRetries(global_settings.radio_delay,
			global_settings.radio_retries);

	radio0.setAutoAck(global_settings.auto_ack);

	if (global_settings.dynamic_payloads)
		radio0.enableDynamicPayloads();
	else radio0.disableDynamicPayloads();

	if (global_settings.ack_payloads)
		radio0.enableAckPayload();
	else radio0.disableAckPayload();

	radio0.setCRCLength((rf24_crclength_e) global_settings.crc_length);

	if (global_settings.primary) {
		uint64_t ln = string_to_address(global_settings.address_0_1);
		radio0.openReadingPipe(1, ln);
		radio0.setChannel(global_settings.channel_0);

	} else {
		uint64_t ln = string_to_address(global_settings.address_0_2);
		radio0.openReadingPipe(1, ln);
		radio0.setChannel(global_settings.channel_1);

	}

	// Configure radio0 to interrupt for read events
	radio0.maskIRQ(true, true, false);

	//if (!attached_rx) {
	//	attached_rx = true;
	//gpioInitialise();
	//pinMode(radio0_irq, INPUT);
	//	attachInterrupt(radio0_irq, INT_EDGE_FALLING, &radio0_data_rx);
	//}
#ifdef debug_reader
	radio0.printPrettyDetails();
#endif

	radio0.startListening();

#ifdef debug_reader
	printf("Radio 0 setup finished\n");
#endif

}

void resetRadio1() {
	//std::unique_lock<std::mutex> lock(radio1_mtx);
	uint8_t t = 1;
	while (!radio1.begin(&spi1_object)) {
		sleep_ms(100);
#ifdef debug_reader
		printf("Radio 1 is not responsive\n");
#endif
		if (!t++) {
			// handleRadioUnresponsiveTimeout();
		}
	}

	radio1.flush_rx();
	radio1.flush_tx();

	radio1.setAddressWidth(global_settings.address_bytes);

	radio1.setPayloadSize(global_settings.payload_size);

	radio1.setPALevel(global_settings.radio_power);
	radio1.setDataRate((rf24_datarate_e) global_settings.data_rate);
	radio1.setRetries(global_settings.radio_delay,
			global_settings.radio_retries);

	radio1.setAutoAck(global_settings.auto_ack);

	if (global_settings.dynamic_payloads)
		radio1.enableDynamicPayloads();
	else radio1.disableDynamicPayloads();

	if (global_settings.ack_payloads)
		radio1.enableAckPayload();
	else radio1.disableAckPayload();

	radio1.setCRCLength((rf24_crclength_e) global_settings.crc_length);

	if (global_settings.primary) {
		uint64_t ln1 = string_to_address(global_settings.address_1_1);
		uint64_t ln2 = string_to_address(global_settings.address_0_2);

		//radio1.openReadingPipe(1, ln1);
		radio1.openWritingPipe(ln2);
		radio1.setChannel(global_settings.channel_1);
	} else {
		uint64_t ln1 = string_to_address(global_settings.address_1_2);
		uint64_t ln2 = string_to_address(global_settings.address_0_1);
		//radio1.openReadingPipe(1, ln1);
		radio1.openWritingPipe(ln2);
		radio1.setChannel(global_settings.channel_0);

	}

	// Configure radio1 to interrupt for write events
	radio1.maskIRQ(false, false, true);

	radio1.stopListening();
#ifdef debug_reader
	printf("Radio 1 setup finished\n");
	radio1.printPrettyDetails();
	print_global_settings();
#endif
}

uint16_t ledtime = 100;
uint16_t read_length = sizeof(struct PicoSettingsBlock);

bool started = false;

void data_read(uint8_t *readbuf, int c) {
	//uint8_t readbuf[read_length];
	//int c = tud_cdc_read(readbuf, read_length);

	//printf("Received data size: %d\n", c);
	if (c == sizeof(struct PicoSettingsBlock)) {
		//print_global_settings();
		//const char* cc1 = "Settings received and applied 1\r\n";
		//               write(1, cc1, 34);
		//tud_cdc_write_flush();
		global_settings = *((PicoSettingsBlock*) readbuf);

		read_buffer = new uint8_t[global_settings.payload_size];

		//printf("Settings received and applied\n");
		//print_global_settings();
		//const char* cc2 = "Settings received and applied 2\r\n";
		//write(1, cc2, 34);
		// tud_cdc_write_flush();
		//tud_cdc_write_flush();
		resetRadio0();
		resetRadio1();
		//const char* cc3 = "Settings received and applied 3\r\n";
		//write(1, cc3, 34);
		// tud_cdc_write_flush();

		//ledtime = 500;
		read_length = 32;
		started = true;
	} else if (c == global_settings.payload_size) {
		// gpio_put(LED_PIN, 1);
		// sleep_ms(1);
		// gpio_put(LED_PIN, 0);
		//printf("Invalid settings size received: %d\n", c);
		// write(1, readbuf, c);
		//return;
		//tud_cdc_write_flush();

		if (check_radio1_status()) {
			resetRadio1();
		}

		/*
		 if (readbuf[0] != 0xf3) {
		 gpio_put(LED_PIN, 1);
		 sleep_ms(800);
		 gpio_put(LED_PIN, 0);
		 sleep_ms(200);
		 }*/

		/*uint8_t cnt = 0;
		if (++cnt == 3) {
			cnt = 0;
			if (!radio1.txStandBy())
				radio1.flush_tx();
		}*/

		if (!radio1.writeFast(readbuf, global_settings.payload_size,
				!global_settings.auto_ack))
			if (!radio1.txStandBy())
				radio1.flush_tx();
		//if (!radio1.writeFast(readbuf, c, !global_settings.auto_ack))
		//	if (!radio1.txStandBy()) {
		//		radio1.flush_tx();
		//	radio1.flush_rx();
		//	}
		//radio1.write(readbuf, c);

	} else {
		gpio_put(LED_PIN, 1);
		sleep_ms(800);
		gpio_put(LED_PIN, 0);
		sleep_ms(200);
	}
}


void radio0_data_rx(uint gpio, uint32_t events) {

	if ((gpio != radio0_irq && !(events | GPIO_IRQ_EDGE_FALL)) || !started) {
	 // the gpio pin and event does not match the configuration we specified
	 return;
	 }
	if (check_radio0_status()) {
		resetRadio0();
	}

	bool tx_ds, tx_df, rx_dr;                // declare variables for IRQ masks
	radio0.whatHappened(tx_ds, tx_df, rx_dr);

	//if (rx_dr) {

	int i = 0;
	while (radio0.available()) {
		radio0.read(read_buffer, global_settings.payload_size);
		tud_cdc_write(read_buffer, global_settings.payload_size);
	}

	//}

}

void core1_entry() {

	while(!started){
		sleep_ms(100);
	}

	//gpio_set_irq_enabled_with_callback(radio0_irq, GPIO_IRQ_EDGE_FALL, true, &radio0_data_rx);
	/*while (1) {
	 gpio_put(LED_PIN, 1);
	 sleep_ms(ledtime);
	 gpio_put(LED_PIN, 0);
	 sleep_ms(ledtime);
	 tight_loop_contents();

	 }*/

	while (1) {

		while (radio0.available()) {
			radio0.read(read_buffer, global_settings.payload_size);
			tud_cdc_write(read_buffer, global_settings.payload_size);
			//write(1, buf, global_settings.payload_size);
			//fflush(stdout);
		}

		if (check_radio0_status()) {
				resetRadio0();
		}

	}
}

int main() {
	global_settings.payload_size = 32;
	// Inizializzazione dei pin GPIO
	gpio_init(radio1_ce);
	gpio_set_dir(radio1_ce, GPIO_OUT);
	gpio_put(radio1_ce, 0);

	gpio_init(radio1_csn);
	gpio_set_dir(radio1_csn, GPIO_OUT);
	gpio_put(radio1_csn, 1);

	//spi_init(spi0, 10000000); // 10 MHz

	// Inizializzazione dell'interfaccia SPI1
	//spi_init(spi1, 10000000); // 10 MHz
	gpio_set_function(radio1_sck, GPIO_FUNC_SPI); // SCK
	gpio_set_function(radio1_mi, GPIO_FUNC_SPI); // MOSI
	gpio_set_function(radio1_mo, GPIO_FUNC_SPI); // MISO

	// Assicurati che il pin CE sia basso per iniziare
	gpio_put(radio1_ce, 0);

	stdio_init_all();
	tusb_init();

	// Initialize the LED pin
	gpio_init(LED_PIN);
	gpio_set_dir(LED_PIN, GPIO_OUT);
	ledtime = 100;

	//irq_set_enabled(radio0_irq, true);

	spi0_object.begin(spi0, radio0_sck, radio0_mi, radio0_mo);
	spi1_object.begin(spi1, radio1_sck, radio1_mi, radio1_mo);

	multicore_launch_core1(core1_entry);

	// Wait for USB CDC connection
	while (!tud_cdc_connected()) {
		sleep_ms(100);
	}

	//printf("USB CDC connected\n");

	while (true) {

		uint8_t readbuf[read_length];
		int c = read(0, readbuf, read_length);
		data_read(readbuf, c);
		tight_loop_contents();
	}
}
