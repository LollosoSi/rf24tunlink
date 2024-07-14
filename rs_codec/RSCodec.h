/*
 *  RSCodec.h
 *
 *  Created on: 4 Mar 2024
 *      Author: Andrea Roccaccino
 */
#pragma once

#include "ReedSolomon/ReedSolomon.h"
#include "../Settings.h"

class RSCodec : public SettingsCompliant {

	unsigned int bits = 8, k, nsym;
	ReedSolomon rs = ReedSolomon(8);

public:

	RSCodec(const Settings &settings);
	virtual ~RSCodec();

	bool efficient_encode(uint8_t* inout, unsigned int size);
	bool efficient_decode(uint8_t* inout, unsigned int size, int* error_count = nullptr);

	void apply_settings(const Settings &settings);
};

