/*
 * RSio.h
 *
 *  Created on: 4 Mar 2024
 *      Author: Andrea Roccaccino
 */
#pragma once

#include <time.h>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <string>

#include "../ReedSolomon/ReedSolomon.h"
#include <cmath>
#include <vector>

#include "../settings/Settings.h"


class RSCodec {

	unsigned int bits = 8, k, nsym;
	ReedSolomon rs = ReedSolomon(8);

public:

	RSCodec();
	virtual ~RSCodec();

	bool efficient_encode(uint8_t* inout, unsigned int size);
	bool efficient_decode(uint8_t* inout, unsigned int size, int* error_count = nullptr);

	bool encode(unsigned char **out, unsigned int &outsize, unsigned char *in, unsigned int insize);
	bool decode(unsigned char **out, unsigned int &outsize, unsigned char *in, unsigned int insize);
};

