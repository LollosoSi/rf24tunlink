# rf24tunlink
Point to Point TUN link via NRF24L01 radio modules

## Video or audio streaming demo
[![Video or PTT](https://img.youtube.com/vi/3pRVBQoNrP4/0.jpg)](https://www.youtube.com/watch?v=3pRVBQoNrP4)

## Preamble
This software is distributed free of charge under GNU GPLv2 license.</br>
If you found my work helpful, consider [supporting it through a donation](https://www.paypal.com/donate/?hosted_button_id=BCZNKHFWP3M4L)</br>

# Index
- [How it works](#how-it-works)
- [Error Correction](#error-correction)
- [Performance](#performance)
- [PCB](#pcb)
- [Installation (RPi)](#installation-raspberry-pi)
- [Usage](#usage)
- [Systemd Service](#systemd-service)
- [Wiring](#wiring)
- [Stability Advice](#stability-advice)
- [Setting up video streaming](#setting-up-video-streaming)
- [RPi as wireless AP](#RPi-as-hotspot)

## How it works
Upon launch, the program creates the TUN interface (arocc) and sets up for the selected radio.</br>
The code structure is designed with expandability in mind, enabling fast addition of different radio modules, packetizers and packet sources.</br>
Scheme:
![rf24tunlink components](https://github.com/user-attachments/assets/18c9e97e-79b2-4d4e-b5f7-f69450a8bd90)


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
Have a look at this [announcement page](https://github.com/LollosoSi/rf24tunlink/discussions/6)</br>
Sneak peek:
<table cellspacing="0" cellpadding="0" border="none">
<tr>
  <td>
<img src="https://github.com/user-attachments/assets/5fadfd92-74bc-4307-a9de-0bbd12a77170" width="680"></img>
</td><td><img src="https://github.com/user-attachments/assets/aaf06b4e-a1b9-43d3-92aa-75a44f440abc" width="480"></img>
</td>
</tr>
</table>




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
<table>
  <tr>
    <td>
Radio 0</br>
CE: GPIO 25</br>
CSN: GPIO 8 (CE 0) - write 0 in the config for CE 0, or 1 for CE 1</br>
MISO: GPIO 9</br>
MOSI: GPIO 10</br>
SCK: GPIO 11</br>
IRQ: GPIO 5
  </td><td>
Radio 1</br>
CE: GPIO 26</br>
CSN: GPIO 16 (CE 1.2) - write 12 in the config (for CE 1.2. for 1.1, write 11. And for 1.0, write 10)</br>
MISO: GPIO 19</br>
MOSI: GPIO 20</br>
SCK: GPIO 21</br>
IRQ: GPIO 6
  </td>
</tr>
</table>

## Stability advice
- Radio on higher power settings may be unstable if the supply isn't adequate
- Buy shielded PA+LNA modules, or consider making [this DIY shielding](https://github.com/nRF24/RF24/blob/master/COMMON_ISSUES.md#my-palna-module-fails-to-transmit)
- Use short cables, usage of jumper cables is discouraged in the final product
- If using jumpers, wrap the GND cable at least around MOSI, MISO, VCC cables
- [Citing the RF24 page](https://github.com/nRF24/RF24/blob/master/COMMON_ISSUES.md#my-palna-module-doesnt-perform-as-well-as-id-hoped-or-the-nrf-radio-works-better-on-touching-it): Add capacitor(s) close to the VCC and GND pins of the radio. Typically, 10uF is enough
- If not using capacitors, [one of these modules should take care of everything](https://www.google.com/url?sa=i&url=https%3A%2F%2Fforum.arduino.cc%2Ft%2Fnrf24l01-com-problems%2F929219&psig=AOvVaw3U8yzDqmUAWnt6mlGg5U2-&ust=1697918511988000&source=images&cd=vfe&opi=89978449&ved=0CBEQjRxqFwoTCJj8i8C1hYIDFQAAAAAdAAAAABAE)
- Software fix: try setting a slower SPI speed. Default is 10MHz, try 5MHz.

## Checklist for long range links
In order to land a long range communication you must check a couple things first: 

- [x] In all cases, line of sight is required. 2.4GHz radio waves tend to move straight, thus it's hard for the signal to pass around objects in the way.</br>
Some people mentioned the fresnel zone under [this video](https://www.youtube.com/watch?v=zddC8rQasrg), that's where my knowledge ends (for now).

- [x] Are both ends of your bridge fixed? If that is, you could consider using a Yagi antenna (directional). Similar to the one in the linked video.</br>
Otherwise, you could consider an omnidirectional antenna of 6dBi or higher.</br>
<b>Note: some antennas are RP-SMA, you will need an adapter to attach to the RF24L01 module (RP-SMA to SMA).</b>

- [x] Tune the radio power appropriately to comply with the law, at least when you've found a stable configuration.</br>
e.g. EIRP states a 20dB limit, that means antenna dBi + transmit power must be 20 or less.

- [x] Determine if you want a two way or one way channel.</br>
One way used to work in the first iterations of the program, but isn't supported at the moment.</br>
Could be cheaper if you intend to use the yagi antennas.</br>
Range with mixed yagi for tx and omni for rx or vice versa is currently untested. Feel free to report your findings.

## Setting up video streaming
I've tested this command for streaming to the <b>primary</b> device of the link:</br>
`rpicam-vid -t 0 --inline --level 4.2 -b 50k --framerate 24 --width 480 --height 360 --codec libav --libav-video-codec h264_v4l2m2m -o udp://192.168.10.1:5000`</br>
Edit bitrate (-b 50k) and resolution (--width 480 --height 360) to fit your needs.</br>
Consider the worst case scenario first: must work in 250Kbps modulation, then tune it up.</br>
Later on, we could always consider including some kind of speed negotiation based on packet latency and signal losses.</br></br>

Optional: in the receiver RPi, reroute the packets to multicast in the local network:</br>
`sudo socat udp-listen:5000,reuseaddr,fork udp-datagram:239.255.0.1:5001,sp=5000`</br>
Note that you can't do this in things like phone hotspots and these might add latency. It's best to use your home network or use the receiving [RPi as access point](#-RPi-as-hotspot)</br></br>

And in the machine(s) where you want to watch the stream:</br>
`ffplay udp://@239.255.0.1:5001 -fflags nobuffer -flags low_delay -framedrop`</br>
Adjust the address if you aren't using multicast.</br>


## RPi as hotspot
<details><summary>To enable RPi as hotspot I put together this bash script and seems to work with low enough latency (RPi 4)</summary>
  
```
#!/bin/bash

# Stop dnsmasq service
systemctl stop dnsmasq

# Delete any existing connection named TEST-AP
nmcli con delete TEST-AP

# Add a new Wi-Fi connection in Access Point mode
nmcli con add type wifi ifname wlan0 mode ap con-name TEST-AP ssid TEST autoconnect false

# Configure Wi-Fi settings
nmcli con modify TEST-AP wifi.band bg
nmcli con modify TEST-AP wifi.channel 3
nmcli con modify TEST-AP wifi.cloned-mac-address 00:12:34:56:78:9a

# Configure Wi-Fi security
nmcli con modify TEST-AP wifi-sec.key-mgmt wpa-psk
nmcli con modify TEST-AP wifi-sec.proto rsn
nmcli con modify TEST-AP wifi-sec.group ccmp
nmcli con modify TEST-AP wifi-sec.pairwise ccmp
nmcli con modify TEST-AP wifi-sec.psk "mypassword"

# Configure IP settings
nmcli con modify TEST-AP ipv4.method shared ipv4.address 192.168.4.1/24
nmcli con modify TEST-AP ipv6.method disabled

# Bring up the connection
nmcli con up TEST-AP

# Set the transmit power of the wlan0 interface to a low value (e.g., 1 dBm)
iw dev wlan0 set txpower fixed 100

# Enable multicast on wlan0 interface
# ip link set dev wlan0 multicast on

ip route add 224.0.0.0/4 dev wlan0
```

</details>


Dependencies: RF24 library, CMake, pigpio
