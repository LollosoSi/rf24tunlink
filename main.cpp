/**
 * @author Andrea Roccaccino
 * Created on 03/02/2024
 */

// Basic
#include <iostream>
using namespace std;

// Utilities
#include <cstring>

// Custom implementations
#include "settings/DefaultSettings.h"
#include "tun/TUNHandler.h"



int main(int argc, char **argv) {

	bool role = 1;

	if(role){
		strcpy(Settings::address, "192.168.1.1");
		strcpy(Settings::destination, "192.168.1.2");
	}else{
		strcpy(Settings::address, "192.168.1.2");
		strcpy(Settings::destination, "192.168.1.1");
	}
	strcpy(Settings::netmask, "255.255.255.0");

	Settings::mtu=900;

	TUNHandler tunh = TUNHandler();

	tunh.startThread();

	while(true){

	}


}
