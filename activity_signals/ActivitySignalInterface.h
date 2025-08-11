/*
 * ActivitySignalInterface.h
 *
 *  Created on: 11 Aug 2025
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "Settings.h"

// TimedFrameHandler has poorer performance for this specific application, better to spawn one thread
//#include "timer_handlers.h"

#include "generic_structures.h"

// For GPIO handling
#include <iostream>

// For threaded execution
#include <sys/prctl.h>
#include <thread>
#include <condition_variable>
#include <mutex>

class ActivitySignalInterface : public SyncronizedShutdown {

	public:
		std::condition_variable cv_wait_trigger;
		std::mutex listener_mtx;
		bool trigger_led = false;

		ActivitySignalInterface(const Settings s) {
			std::unique_ptr < std::thread > timer_thread = std::make_unique
					< std::thread > ([this]() {
				prctl(PR_SET_NAME, "TFNB", 0, 0, 0);
				signal_thread();
			});
			timer_thread.get()->detach();

			running = true;
		}
		virtual ~ActivitySignalInterface() {
			stop_module();

		}


		// Called by the packetizer when a packet is received correctly
		inline void trigger() {
			{
				// Acquire the lock to edit the trigger boolean
				std::lock_guard < std::mutex > lock(listener_mtx);

				// Set the trigger
				trigger_led = true;

				// Notify the listener that it should blink the led once
				cv_wait_trigger.notify_all();
			}

		}

		void signal_thread() {
			while (running) {

				// Acquire the mutex
				std::unique_lock < std::mutex > lock(listener_mtx);
				// Wait until the led action is triggered or the program should exit
				cv_wait_trigger.wait(lock, [this] {
					return trigger_led || !running;
				});

				// Disable the trigger boolean immediately in order to wait on the next loop iteration
				// or catch more triggers while this thread is busy, so it's not going to wait at all in the next iteration
				trigger_led = false;

				// Don't blink if we're exiting
				if (!running)
					break;

				// Blink
				set_state(1);
				std::this_thread::sleep_for(std::chrono::microseconds(100));
				set_state(0);

			}

			printf("Activity Signal thread is exiting\n");
		}

		inline void stop_module() {
			trigger();
		}

		virtual void set_state(bool on_off) = 0;
		virtual void stop() = 0;

};
