/*
 * activity_led.h
 *
 *  Created on: 27 Jun 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "Settings.h"

// TimedFrameHandler has poorer performance for this specific application, better to spawn one thread
//#include "timer_handlers.h"

#include "generic_structures.h"

// For GPIO handling
#include <iostream>
#include <gpiod.hpp>
#include <unistd.h>
#include <vector>
#include <filesystem>

// For threaded execution
#include <sys/prctl.h>
#include <thread>
#include <condition_variable>
#include <mutex>

class ActivityLed : public SyncronizedShutdown {

		volatile int led_gpio = 10;
		std::string chip_name;
		::gpiod::chip chip;
		::gpiod::line line;

	public:

		std::condition_variable fire_cv;
		std::mutex worker_mtx;
		bool fire = false;

		ActivityLed(const Settings s) {
			led_gpio = s.activity_led_gpio;
			printf("Set Activity LED to: %i\n", led_gpio);

			find_gpiochip();

			std::cout << "Using " << chip_name << std::endl;
			chip = ::gpiod::chip(chip_name);

			line = chip.get_line(led_gpio);
			line.request( { "rf24tunlink_activity_led",
					gpiod::line_request::DIRECTION_OUTPUT, 0 }, 1);

			line.set_value(1);
			std::this_thread::sleep_for(std::chrono::microseconds(5000));
			line.set_value(0);

			std::unique_ptr < std::thread > timer_thread = std::make_unique
					< std::thread > ([this]() {
				prctl(PR_SET_NAME, "TFNB", 0, 0, 0);
				worker_thread();
			});
			timer_thread.get()->detach();

			running = true;
		}
		~ActivityLed() {
			stop_module();
			line.release();
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

		inline void trigger() {
			{
				std::lock_guard < std::mutex > lock(worker_mtx);
				fire = true;
			}
			fire_cv.notify_all();
		}

		inline void stop_module() {
			trigger();
		}

		void worker_thread() {
			while (running) {

				std::unique_lock < std::mutex > lock(worker_mtx);
				fire_cv.wait(lock, [this] {
					return fire || !running;
				}); // Wait until ready is true
				fire = false;
				if (!running)
					break;

				line.set_value(1);
				std::this_thread::sleep_for(std::chrono::microseconds(100));
				line.set_value(0);
			}
			printf("Activity led thread is exiting\n");
		}

};
