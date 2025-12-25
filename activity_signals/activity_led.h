/*
 * activity_led.h
 *
 *  Created on: 27 Jun 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "ActivitySignalInterface.h"

// For GPIO handling
#include <iostream>
#include <gpiod.hpp>
#include <unistd.h>
#include <vector>
#include <filesystem>

class ActivityLed : public ActivitySignalInterface {

		volatile int led_gpio = 10;
		std::string chip_name;
		::gpiod::chip chip;
		::gpiod::line led_gpioline;

	public:
		ActivityLed(const Settings s) : ActivitySignalInterface(s) {

			led_gpio = s.activity_led_gpio;
			printf("Set Activity LED to: %i\n", led_gpio);

			find_gpiochip();

			std::cout << "Using " << chip_name << std::endl;
			chip = ::gpiod::chip(chip_name);

			led_gpioline = chip.get_line(led_gpio);
			led_gpioline.request( { "rf24tunlink_activity_led",
					gpiod::line_request::DIRECTION_OUTPUT, 0 }, 1);

		}
		~ActivityLed() {
			stop();
		}

		void stop() {
			led_gpioline.release();
		}
		void set_state(bool on_off) {
			led_gpioline.set_value(on_off);
		}

		void find_gpiochip() {

			// Iterate through /dev/ to find available gpiochips
			for (const auto &entry : std::filesystem::directory_iterator(
					"/dev/")) {
				if (entry.path().string().find("gpiochip")
						!= std::string::npos) {
					try {
						::gpiod::chip chip(entry.path().string());
						auto line = chip.get_line(led_gpio);
						if (line) {  // If this gpiochip has GPIO 17
							chip_name = entry.path().string();
							break;
						}
					} catch (...) {
						continue; // Ignore invalid chips
					}
				}
			}

			if (chip_name.empty()) {
				std::cerr << "No valid gpiochip found for GPIO " << led_gpio
						<< std::endl;
				throw new std::invalid_argument(
						"Asked to use a gpio, but no gpiochip is available with that entry!");
				exit(1);
			}
		}

};
