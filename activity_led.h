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

		int led_gpio = 10;
		std::unique_ptr<NonBlockingTimer> tfh;

	public:
		ActivityLed(const Settings s) {
			led_gpio = s.activity_led_gpio;


			if (gpioInitialise() == PI_INIT_FAILED) {
				printf("Failed to initialize GPIO. Fix the problem or disable the activity LED\n");
				exit(1);
			}

			gpioSetMode(led_gpio, PI_OUTPUT);

			tfh = std::make_unique < NonBlockingTimer > ();
			tfh->apply_parameters(500, 10);

			tfh->set_resend_call([this] {
				tfh->invalidate_timer();
			});

			tfh->set_fire_call([this] {

			});
			tfh->set_invalidated_call([this] {
				gpioWrite(led_gpio, PI_LOW);
			});

		}
		~ActivityLed() {
			stop_module();
			gpioTerminate();
		}

		inline void trigger() {
			if (running) {
				gpioWrite(led_gpio, PI_HIGH);
				tfh->start();
			}
		}

		inline void stop_module() {
			if (!tfh->finished())
				tfh->invalidate_timer();

		}

};
