/*
 *  RSCodec.cpp
 *
 *  Created on: 4 Mar 2024
 *      Author: Andrea Roccaccino
 */

#include "RSCodec.h"

#include <iostream>
#include "../crc.h"


RSCodec::RSCodec(const Settings &settings) {

	bits = 8;
	apply_settings(settings);

}

RSCodec::~RSCodec() {

}

void RSCodec::apply_settings(const Settings &settings) {

	k = settings.data_bytes;
	nsym = settings.ecc_bytes;

	if (nsym == 0) {
		std::cout << "nsym can't be zero!\n";
		throw std::invalid_argument("nsym is zero");
	}

	if(nsym + k != settings.payload_size){
		std::cout << "nsym + k isn't a full packet!\n";
		throw std::invalid_argument("invalid packet size");
	}

	printf("RSCodec settings applied. Bits: %d, Data Bytes: %d, ECC Bytes: %d\n", bits, k, nsym);
}

// NOTE: size must be correct k+nsym or the function will fail.
bool RSCodec::efficient_encode(uint8_t* inout, unsigned int size) {
	if (size != (k + nsym)) {
		printf("Encoder requires messages of size %i, it was given %i. Correct the mistake and recompile\n", k + nsym, size);
		exit(1);
	}

	inout[k]=gencrc(inout,k);

	RS_WORD data[k+nsym];
	RS_WORD data_out[k+nsym];
	for (unsigned int i = 0; i < k + nsym; i++)
		data[i] = inout[i];
	//Poly conv(k + nsym, data);
	rs.encode(data_out, data, k+1, nsym-1);
	for (unsigned int i = 0; i < k + nsym; i++)
		inout[i] = static_cast<uint8_t>(data_out[i]);

	return (true);
}

// NOTE: size must be correct k+nsym or the function will fail.
bool RSCodec::efficient_decode(uint8_t* inout, unsigned int size, int* error_count) {
	if (size != (k + nsym)) {
		printf("Decoder requires messages of size %i, it was given %i. Correct the mistake and recompile\n", k + nsym, size);
		exit(1);
	}

	RS_WORD data[k + nsym];
	for (unsigned int i = 0; i < k + nsym; i++)
		data[i] = static_cast<RS_WORD>(inout[i]);
	//Poly conv(k + nsym, data);

	Poly msg(k+1, data);

	if (!rs.decode(nullptr, msg.coef, data, k+1, nsym-1, nullptr, false, error_count))
		return (false);

	for (unsigned int i = 0; i < k + nsym; i++)
		inout[i] = static_cast<unsigned char>(data[i]);

	if(inout[k]!=gencrc(inout,k)){
		//printf("CRC FAILED\n");
		return (false);
	}

	return (true);
}
