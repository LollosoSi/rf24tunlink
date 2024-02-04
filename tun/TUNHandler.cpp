/*
 * TUNHandler.cpp
 *
 *  Created on: 4 Feb 2024
 *      Author: andrea
 */

#include "TUNHandler.h"

#include <iostream>
using namespace std;

TUNHandler::TUNHandler() {
	// TODO Auto-generated constructor stub

}

TUNHandler::~TUNHandler() {
	// TODO Auto-generated destructor stub
}

bool TUNHandler::receive_message(TUNMessage &tunmsg) {

	cout << "Received size: " << tunmsg.size << endl;

	return (true);
}

