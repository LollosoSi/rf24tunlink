#!/bin/bash

# Disable TCP slow start after idle. Helps with performance albeit slightly
sudo sysctl -w net.ipv4.tcp_slow_start_after_idle=0

read -p "Do you intend to use the dual rf24 setup? (y/n) " yn
case $yn in
    [Yy]*) 
        sudo grep -qxF 'dtoverlay=spi1-3cs' /boot/firmware/config.txt || 
        sudo printf '[all]\ndtoverlay=spi1-3cs\n' >> /boot/firmware/config.txt
        ;;
    [Nn]*) 
        break
        ;;
    *) 
        echo "Please answer yes or no."
        ;;
esac

read -p "Do you intend to use the action button? (y/n) " yn
case $yn in
    [Yy]*) 
        sudo grep -qxF 'dtoverlay=gpio-key,gpio=4,active_low=1,gpio_pull=up,keycode=30' /boot/firmware/config.txt || 
        sudo printf '[all]\ndtoverlay=gpio-key,gpio=4,active_low=1,gpio_pull=up,keycode=30\n' >> /boot/firmware/config.txt
        ;;
    [Nn]*) 
        break
        ;;
    *) 
        echo "Please answer yes or no."
        ;;
esac



# Install prerequisites
cd ~
sudo apt-get install build-essential cmake make git g++ wget gpiod libgpiod-dev libgpiod-doc -y

# Install RF24 Library
cd ~
sudo rm -r rf24libs
wget https://raw.githubusercontent.com/nRF24/.github/main/installer/install.sh
chmod +x install.sh
rm -rf master.zip
printf "n\ny\nn\nn\nn\n1\nn\nn\n" | ./install.sh
rm -rf install.sh

# Install rf24tunlink
git clone https://github.com/LollosoSi/rf24tunlink.git
cd rf24tunlink
cmake .
make
