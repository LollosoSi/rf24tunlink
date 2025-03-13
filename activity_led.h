/*
 * activity_led.h
 *
 *  Created on: 27 Jun 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "Settings.h"
//#include "timer_handlers.h"
#include "generic_structures.h"

#include <pigpio.h>

#include <sys/prctl.h>
#include <thread>
#include <condition_variable>
#include <mutex>

class ActivityLed : public SyncronizedShutdown {

		volatile int led_gpio = 10;
		//TimedFrameHandler tfh;

	public:

		std::condition_variable fire_cv;
		std::mutex worker_mtx;
		bool fire = false;

		ActivityLed(const Settings s) {
			led_gpio = s.activity_led_gpio;
			printf("Set Activity LED to: %i\n", led_gpio);
			printf("Pigpio version: %d, Hardware revision: %d\n", gpioVersion(),
					gpioHardwareRevision());

			if (gpioInitialise() == PI_INIT_FAILED) {
				printf(
						"Failed to initialize GPIO. Fix the problem or disable the activity LED\n");
				exit(1);
			}

			// Test init sequence
			gpioSetMode(led_gpio, PI_OUTPUT);

			gpioWrite(led_gpio, 1);
			std::this_thread::sleep_for(std::chrono::microseconds(5000));
			gpioWrite(led_gpio, 0);

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
			gpioTerminate();
		}

		inline void trigger() {
			if (running) {

				{
					std::lock_guard < std::mutex > lock(worker_mtx);
					fire = true;
				}
				fire_cv.notify_one(); // No lock is required here

			} else {
				printf("LED triggered, but program is not running\n");
			}
		}

		inline void stop_module() {
			//tfh.invalidate_timer();
			trigger();

		}

		void worker_thread() {
			while (running) {

				std::unique_lock < std::mutex > lock(worker_mtx);
				fire_cv.wait(lock, [this] {
					return fire || !running;
				}); // Wait until ready is true
				fire = false;

				gpioWrite(led_gpio, 1);
				std::this_thread::sleep_for(std::chrono::microseconds(100));
				gpioWrite(led_gpio, 0);

			}
		}

};
