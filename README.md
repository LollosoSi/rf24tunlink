# rf24tunlink
Point to Point TUN link via NRF24L01 radio modules

## Preamble
This software is distributed free of charge under GNU GPLv2 license.</br>
If you found my work helpful, consider [supporting it through a donation](https://www.paypal.com/donate/?hosted_button_id=BCZNKHFWP3M4L)</br>

## How it works
Upon launch, the program creates the TUN interface (arocc) and sets up for the selected radio.</br>
The code structure is designed with expandability in mind, enabling fast addition of different radio modules, packetizers and packet sources.</br>
There are 3 core components:
<table>
  <tr>
 <td><b>TUN handler</b></td><td>Handles packets from the packetizer, into the linux system and vice versa. Sets everything about the system interface (be it TUN, UART or else).</td>
  </tr>
    <tr>
 <td><b>Packetizer</b></td><td>Receives packets from the TUN handler, splits them into segments suitable for radio transfer and recomposes them on the latter end of the bridge.</td>
     </tr>
  <tr>
 <td><b>Radio</b></td><td>Handles everything regarding a particular radio. Receives segments from the packetizer and sends them through the medium. Reads segments from the medium and sends them to the packetizer.</td>
 </tr>
</table>

You must select each component using the config files (look into the `presets` folder)
You must also declare which is the primary and secondary radio.</br>
Almost everything can be done through the config files.</br>
Primary and secondary have these addresses respectively: 192.168.10.1 and 192.168.10.2 (unless you change them)
<h3>Notable setups</h3>
<table>
  <tr>
 <td><b>two-RF24</b></td><td>The primary radio streams their packets (and if configured, empty packets) and reads the Ack Payloads of the secondary radio.</br>Note: A good portion of the total received packets are discarded automatically from the modules in the two-RF24 setup, just because there usually are a couple corrupted bytes out of 32.
</td>
  </tr>
   <tr>
  <td><b>four-RF24</b></td><td>Full duplex communication with higher performance compared to the two-RF24 setup.</br>Each end of the bridge has two NRF24L01 radio modules.</br><b>The four-RF24 setup is strongly recommended</b> because it allows for advanced error correction, meaning theoretically larger range.</td>
   </tr>
   <tr>
  <td><b>Pico RF24 adapter</b></td><td>You can enable non-raspberry debian computers to connect to the bridge by building a pico adapter: it's a raspberry pico board with two NRF24L01 modules and a program to exchange settings and packets with rf24tunlink.</br>Works via USB</td>
   </tr>
</table>
You should be able to ping each other when everything is set up correctly. Test the bridge speed using iperf.</br>

## Error Correction
</br>Here is a representation of the errors in close range, before RS ECC was implemented</br>
![Just out of my room](https://github.com/user-attachments/assets/030ba1eb-4e7a-4324-a6ae-4c61f67b9fd6)
</br>Here is one test done after implementing the RS ECC</br>
![RS ECC](https://github.com/user-attachments/assets/1bd38d37-7a09-49e1-b92b-7d0838d1c436)
</br>Note that error correction is available only for the four-RF24 setup.</br>Unfortunately for two-RF24 you cannot disable ack payloads unless you intend to send a one way stream (unsupported at the moment)

## Performance
_Results depend on the configuration used._</br>
For a four-RF24 setup, with 30 bytes of data and 2 bytes for ECC:</br>
Throughput (close range): 1.2Mbps</br>
Ping: 2-4ms</br></br>
A realistic expectation would be using more bytes for ECC and streaming continously no more than 200-400Kbps. You will experience increased ping and/or dropped packets if signal is lost for too long.

## PCB
A PCB is in the works and will be released later this year.</br>
Meant for a stable point to point communication or a remote-RC car/drone setup.
Sneak peek:
![PCB development sneak peek](https://github.com/user-attachments/assets/62ae1e27-5be0-4276-a51b-902cc5fce84a)


## Installation (Raspberry Pi)
- You need a pair of Raspberry Pi boards. Raspberry 3B and 4 are tested but virtually every linux board should work.
- Enable the SPI interface using `raspi-config`
- If you intend to use the dual radio setup add this in your config.txt `dtoverlay=spi1-3cs`
- Install the build essential, make, cmake, git and g++: `sudo apt-get install build-essential cmake make git g++ -y`
- Installing [pigpio](https://abyz.me.uk/rpi/pigpio/download.html) is recommended at this point 
- Install the [RF24 library](https://nrf24.github.io/RF24/md_docs_2linux__install.html) following the instructions from their site.
   - This project is set to look for the **RF24 Core** installation from your home folder (~/). You will end up with `~/rf24libs` (run install.sh in your home folder).
   - SPIDEV is the tested driver for this application.
   - If you can't compile rf24tunlink due to rf24 not found, try running install.sh with sudo
- Download the rf24tunlink source code in your home folder. `cd ~; git clone https://github.com/LollosoSi/rf24tunlink.git`
- In case of issues with permissions, run commands using sudo.
- With the terminal into the folder, run:
  `./build.sh`
  or
  `cmake .; make`
- rf24tunlink executable should appear

## Usage
Note: During the transition to version 2, not all features might be available.</br>CSV exporting or the bridge info are some of the unfinished features.</br></br>
You can generate a text file with all the available settings (most have comments with explanations in them) by running the program with no arguments: `./rf24tunlink2`.</br>
Just create your files or use the ones in presets.</br>
Presets might not always be updated and I might change them as a result of testing, it's a good idea to have a look into them when downloading for the first time.

Examples of config files can be found in the `presets` folder.</br>
Once everything is tuned to your liking, run the program with all your files as arguments.</br>
Here is an example which runs the four-RF24 setup:</br>
Primary device:</br>`sudo ./rf24tunlink2 presets/tunlink_config.txt presets/harq_config.txt presets/radio_config.txt presets/primary`</br>
Secondary device:</br>`sudo ./rf24tunlink2 presets/tunlink_config.txt presets/harq_config.txt presets/radio_config.txt presets/secondary`</br>
This is cool beacuse you can edit your configs once then sync everything in one folder.</br>

## Systemd service
_Use this after you've found and tested a working configuration_</br>
Install the service to run rf24tunlink at boot by running
`./installservice.sh primary/secondary`</br>
A file named `.progconf` will be created. This one contains the config file paths. Edit as necessary.</br>
Note: rs_config and radio_pb_config are used to change the data bytes, ECC bytes and the NRF24L01 modules data rate and power on the fly.</br>
You lose this ability if these lines are removed.</br>
To change these settings during execution, use the scripts</br>`./power_bitrate_config.sh (0 to 3: power) (0 to 2: bitrate) ` and `./ecc_config.sh (data) (ecc)` with the correct arguments.</br>
Data+ECC should always be 32 or less if using NRF24L01. Performance will be worse if you use less than 32 bytes total.</br></br>
Shortcuts to control the service are provided: `./stopservice.sh` `./startservice.sh` `./servicestatus.sh` `./restartservice.sh` </br>

## Wiring
You can select CE, CSN and IRQ based on your project needs.</br></br>
Radio 0</br>
CE: GPIO 25</br>
CSN: GPIO 8 (CE 0) - write 0 in the config for CE 0, or 1 for CE 1</br>
MISO: GPIO 9</br>
MOSI: GPIO 10</br>
SCK: GPIO 11</br>
IRQ: GPIO 5</br></br>
Radio 1</br>
CE: GPIO 26</br>
CSN: GPIO 16 (CE 1.2) - write 12 in the config (for CE 1.2. for 1.1, write 11. And for 1.0, write 10)</br>
MISO: GPIO 19</br>
MOSI: GPIO 20</br>
SCK: GPIO 21</br>
IRQ: GPIO 6</br>


## Stability advice
- Radio on higher power settings may be unstable if the supply isn't adequate
- Buy shielded PA+LNA modules, or consider making [this DIY shielding](https://github.com/nRF24/RF24/blob/master/COMMON_ISSUES.md#my-palna-module-fails-to-transmit)
- Use short cables, usage of jumper cables is discouraged in the final product
- If using jumpers, wrap the GND cable at least around MOSI, MISO, VCC cables
- [Citing the RF24 page](https://github.com/nRF24/RF24/blob/master/COMMON_ISSUES.md#my-palna-module-doesnt-perform-as-well-as-id-hoped-or-the-nrf-radio-works-better-on-touching-it): Add capacitor(s) close to the VCC and GND pins of the radio. Typically, 10uF is enough
- If not using capacitors, [one of these modules should take care of everything](https://www.google.com/url?sa=i&url=https%3A%2F%2Fforum.arduino.cc%2Ft%2Fnrf24l01-com-problems%2F929219&psig=AOvVaw3U8yzDqmUAWnt6mlGg5U2-&ust=1697918511988000&source=images&cd=vfe&opi=89978449&ved=0CBEQjRxqFwoTCJj8i8C1hYIDFQAAAAAdAAAAABAE)
- Software fix: try setting a slower SPI speed. Default is 10MHz, try 5MHz.

Dependencies: RF24 library, CMake, pigpio
