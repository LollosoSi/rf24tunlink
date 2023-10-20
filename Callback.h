/*
 * Callback.h
 *
 *  Created on: Oct 11, 2023
 *      Author: andrea
 */

#ifndef CALLBACK_H_
#define CALLBACK_H_

#include <RF24/RF24.h>

class Callback {
public:
	Callback();

	virtual bool writeback(uint8_t* data, int size) = 0;

	virtual ~Callback();
};

#endif /* CALLBACK_H_ */
