/*
 * RSio.cpp
 *
 *  Created on: 4 Mar 2024
 *      Author: Andrea Roccaccino
 */

#include "../rs_codec/RSCodec.h"

RSCodec::RSCodec() {
	bits = Settings::ReedSolomon::bits;
	k = Settings::ReedSolomon::k;
	nsym = Settings::ReedSolomon::nsym;

	rs = new ReedSolomon(bits);

}

RSCodec::~RSCodec() {

}

bool RSCodec::encode(unsigned char **out, int &outsize, unsigned char *in,
		int insize) {

	if (insize != k) {
		printf(
				"Encoder requires messages of size %i, it was given %i. Correct the mistake and recompile\n",
				k, insize);
		exit(1);
	}

	RS_WORD m[k];
	for (int i = 0; i < k; i++)
		m[i] = (unsigned long) in[i];

	Poly msg(k, m);
	Poly a(k + nsym, m);

	rs->encode(a.coef, msg.coef, k, nsym);

	if ((*out) == nullptr || outsize != k + nsym)
		*out = new unsigned char[k + nsym];
	outsize = k + nsym;
	//printf("OutMSG: ");
	for (int i = 0; i < k + nsym; i++) {
		(*out)[i] = static_cast<char>(a.coef[i]);
		//	printf("%i ", (*out)[i]);

	}
	//printf("\n");

	//std::cout << "After encoding: " << std::endl;
	//a.print();
	//std::cout << std::endl << "Encoded message: " << std::endl;
	//msg.print();

	return (true);
}

bool RSCodec::decode(unsigned char **out, int &outsize, unsigned char *in,
		int insize) {

	//std::vector<unsigned int> erasePos;

	if (insize != (k + nsym)) {
		printf("Decoder requires messages of size %i, it was given %i. ",
				k + nsym, insize);
		if (insize == 0) {
			printf("Packet is a failure\n");
			return (false);
		} else {
			printf("Padding remainder\n");
		}
		//for (int i = insize; i < k; i++)
		//	erasePos.push_back(i);
		//exit(1);
	}

	RS_WORD m[k + nsym] = { 0 };

	RS_WORD c[k] = { 0 };
	//printf("Packet:");
	for (int i = 0; i < insize; i++) {
		m[i] = (unsigned long) in[i];
		//	printf("%i ", in[i]);
	}
	//printf("\n");

	//printf("Copied to m\n");

	Poly msg(k, c);
	Poly a(k + nsym, m);

	if (!rs->decode(a.coef, msg.coef, a.coef, k, nsym, nullptr, false)) {
		//cout << "Decoding failed!" << endl;
		return (false);
	}
	//std::cout << "After decoding: " << std::endl;
	//a.print();
	//std::cout << std::endl << "Decoded message: " << std::endl;
	//msg.print();
	if ((*out) == nullptr)
		*out = new unsigned char[k] { 0 };

	for (int i = 0; i < k; i++)
		(*out)[i] = (unsigned char) msg.coef[i];
	outsize = k;

	return (true);
}
