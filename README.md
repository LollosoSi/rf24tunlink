# rf24tunlink
Point to Point TUN link via NRF24L01 radio modules

## Preamble
This software is distributed free of charge under GNU GPLv2 license.</br>
If you found my work helpful, consider [supporting it through a donation](https://www.paypal.com/donate/?hosted_button_id=BCZNKHFWP3M4L)</br>
Donating is not an option? No problem: contribute through github

## How it works
Upon launch, the program creates the TUN interface (arocc) and looks for an available NRF24L01 module.</br>
You must choose which is the primary and secondary radio.</br>
Primary and secondary have these addresses: 192.168.10.1 and 192.168.10.2.</br>
Primary radio streams their packets (and if configured, empty packets) and reads the Ack Payloads of the secondary radio (configurable)
You should be able to ping each other when everything is set up correctly.

Two working modes:
- One way: highest throughput, ideal for UDP streams, no way to recover lost packets.
- Bidirectional: lower throughput, detection of malformed packets and selective retransmission (WIP). Alternatively, bidirectional data stream with no packet recovery can be set up.

The primary radio usually is the one with highest throughput.</br>

## Performance
_Results are heterogeneous while the algorithm is finalized._</br>
Expected: 30Kbps to 600Kbps, mixed</br>
Peak seen during testing: 900Kbps, one directional.

## Installation and usage
- You need a pair of Raspberry Pi boards. Raspberry 3B and 4 are tested but virtually every linux board should work.
- Enable the SPI interface using `raspi-config`
- Install the build essential, make, cmake, git and g++: `sudo apt-get install build-essential cmake make git g++ -y`
- Installing pigpio is recommended at this point
- Install the [RF24 library](https://nrf24.github.io/RF24/md_docs_linux_install.html) following the instructions from their site.
   - This project is set to look for the **RF24 Core** installation from your home folder (~/) you should be able to reach ~/rf24libs (run install.sh in your home folder).
   - SPIDEV is the tested driver for this application, the BCM driver should work too.
   - If you can't compile rf24tunlink due to rf24 not found, try running install.sh with sudo
- Download the rf24tunlink source code.
- In case of issues with permissions, running commands using sudo is okay.
- With the terminal into the folder, run:
  `cmake .`
  `make`
- rf24tunlink executable should appear
To run the program as primary and secondary radio:
Primary: `./rf24tunlink 1`
Secondary: `./rf24tunlink 2`

## Wiring
CE: GPIO 25</br>
CSN: GPIO 8 (CE 0)</br>
MISO: GPIO 9</br>
MOSI: GPIO 10</br>
SCK: GPIO 11</br>
IRQ: GPIO 24</br>

## Stability advice
- Buy shielded PA+LNA modules, or consider making [this DIY shielding](https://github.com/nRF24/RF24/blob/master/COMMON_ISSUES.md#my-palna-module-fails-to-transmit)
- Use short cables, usage of jumper cables is discouraged in the final product
- If using jumpers, wrap the GND cable at least around MOSI, MISO, VCC cables
- [Citing the RF24 page](https://github.com/nRF24/RF24/blob/master/COMMON_ISSUES.md#my-palna-module-doesnt-perform-as-well-as-id-hoped-or-the-nrf-radio-works-better-on-touching-it): Add capacitor(s) close to the VCC and GND pins of the radio. Typically, 10uF is enough
- If not using capacitors, [one of these modules should take care of everything](https://www.google.com/url?sa=i&url=https%3A%2F%2Fforum.arduino.cc%2Ft%2Fnrf24l01-com-problems%2F929219&psig=AOvVaw3U8yzDqmUAWnt6mlGg5U2-&ust=1697918511988000&source=images&cd=vfe&opi=89978449&ved=0CBEQjRxqFwoTCJj8i8C1hYIDFQAAAAAdAAAAABAE)
- Software fix: try setting a slower SPI speed. Default is 10MHz, try 5MHz.

Dependencies: RF24 library, CMake, pigpio
