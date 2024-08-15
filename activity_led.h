/*
 * activity_led.h
 *
 *  Created on: 27 Jun 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "Settings.h"
#include "timer_handlers.h"
#include "generic_structures.h"

#include <pigpio.h>

class ActivityLed : public SyncronizedShutdown {

		volatile int led_gpio = 10;
		TimedFrameHandler tfh;

	public:
		ActivityLed(const Settings s) {
			led_gpio = s.activity_led_gpio;
			printf("Set Activity LED to: %i\n", led_gpio);
			printf("Pigpio version: %d, Hardware revision: %d\n", gpioVersion(), gpioHardwareRevision());

			if (gpioInitialise() == PI_INIT_FAILED) {
				printf("Failed to initialize GPIO. Fix the problem or disable the activity LED\n");
				exit(1);
			}

			// Test init sequence
			gpioSetMode(led_gpio, PI_OUTPUT);

			gpioWrite(led_gpio, 1);
			std::this_thread::sleep_for(std::chrono::microseconds(5000));
			gpioWrite(led_gpio, 0);

			tfh.apply_parameters(1, 1);

			tfh.set_resend_call([this] {
				//tfh->invalidate_timer();
			});

			tfh.set_fire_call([this] {
			//	printf("fire\n");
				off();
			});
			tfh.set_invalidated_call([this] {

			});
			running = true;
		}
		~ActivityLed() {
			stop_module();
			gpioTerminate();
		}

		inline void trigger() {
			if (running) {
				tfh.invalidate_timer();
				gpioWrite(led_gpio, 1);
				//printf("LED triggered\n");


				std::unique_ptr < std::thread > timer_thread = std::make_unique< std::thread > ([this]() {
					prctl(PR_SET_NAME, "TFNB", 0, 0, 0);
					tfh.start();
				});
				timer_thread.get()->detach();
			}else{
				printf("LED triggered, but program is not running\n");
			}
		}

		inline void off(){
			//printf("off\n");
			gpioWrite(led_gpio, 0);
		}

		inline void stop_module() {
			tfh.invalidate_timer();
		}

};
