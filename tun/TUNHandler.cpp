/*
 * TUNHandler.cpp
 *
 *  Created on: 4 Feb 2024
 *      Author: Andrea Roccaccino
 */

#include "TUNHandler.h"

#include <iostream>
using namespace std;

TUNHandler::TUNHandler() {

}

TUNHandler::~TUNHandler() {

}

bool TUNHandler::receive_message(TUNMessage &tunmsg) {

	cout << "Received size: " << tunmsg.size << endl;

	return (true);
}

