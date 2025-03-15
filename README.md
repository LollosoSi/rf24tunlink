Pico - RF24 Adapter
==============
Here is a tiny example that demonstrates how to build an adapter for devices with no gpio exposed.

This program is intended to be used for the **Raspberry Pi Pico to Dual RF24 adapter**.

You can write code for any device that fits your purpose.

# Goal
Allows linux devices to become rf24tunlink endpoints, via USB.

You can have one or both Pico endpoints into a link.

Since the link is built of two endpoints, the one of the following combinations is possible:
- RPi to RPi
- RPi to Pico
- Pico to Pico

# How it works
The program follows this workflow:
1. Connect via USB
2. Receive the radio configurations. 
<details>
   <summary>Using this common data structure</summary>
  
  ```
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
  ```
</details>

3. Initialize the radio modules
4. Start a radio inbound and outbound thread (Pico can run 2 tasks simultaneously)
5. Send and receive packets of a predefined size (*payload_size*) via USB.
6. Change radio module settings at runtime by writing another `PicoSettingsBlock`

Anything that is read from the USB is sent to the radio module and viceversa.

**NOTE: Doesnt work with dynamic payload sizes**, feel free to experiment and find better settings. As mentioned, this is an example that will be very useful to develop future nodes.

# Setting up
You need to set up the Pico SDK before compiling this!

Here is some guidance
https://datasheets.raspberrypi.com/pico/raspberry-pi-pico-c-sdk.pdf
https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html

Once you're able to compile basic programs, [download the RF24 source code from the repo](https://github.com/nRF24/RF24/) and extract it in the libs folder.

You should end up with this structure:
/libs/RF24/*source code files here*

You can finally compile the program and upload it to a Pico via USB.
