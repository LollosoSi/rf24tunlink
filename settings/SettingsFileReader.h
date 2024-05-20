/*
 * SettingsFileReader.h
 *
 *  Created on: 24 Feb 2024
 *      Author: Andrea Roccaccino
 */
#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>

inline void read_settings(const char *settings_file) {

	if (!settings_file) {
		printf("No settings to apply\n");
		return;
	}

	std::ifstream file;
	file.open(settings_file);
	if (!file.good()) {
		printf("Problem reading settings file\n");
		file.close();
		return;
	}
	std::string a;
	std::string b;
	while (!file.eof()) {
		char buf[2000];
		file.getline(buf, 2000, '=');
		printf("Buf A: %s\n", buf);
		a = std::string(buf);
		file.getline(buf, 2000);
		printf("Buf B: %s\n", buf);
		b = std::string(buf);

		Settings::apply_settings(a, b);
		Settings::RF24::apply_settings(a, b);
		Settings::DUAL_RF24::apply_settings(a, b);
		Settings::ReedSolomon::apply_settings(a, b);
	}

	file.close();

}
