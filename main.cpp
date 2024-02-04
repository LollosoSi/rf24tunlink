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

	TUNHandler tunh = TUNHandler();
	TUNMessage t;
	t.data = 0x0;
	t.size = 1;

	tunh.send(t);

	strcpy(address, "192.168.1.10");
	cout << address << endl;
}
