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
private:
	int bits, k, nsym;
	ReedSolomon* rs = nullptr;

public:

	RSCodec();
	virtual ~RSCodec();

	bool efficient_encode(uint8_t* inout, int size);
	bool efficient_decode(uint8_t* inout, int size);

	bool encode(unsigned char **out, int &outsize, unsigned char *in, int insize);
	bool decode(unsigned char **out, int &outsize, unsigned char *in, int insize);
};

