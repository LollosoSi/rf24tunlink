/*
 * timer_handlers.h
 *
 *  Created on: 27 Jun 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "utils.h"

#include <thread>
// Thread naming
#include <sys/prctl.h>

#include <mutex>
#include <condition_variable>

#include <deque>
#include <queue>

#include <functional>

class TimedFrameHandler {

		uint64_t timeout_millis = 0;
		uint16_t timeout_resend_millis = 0;
		uint64_t send_time = 0;
		uint64_t resend_time = 0;
		std::unique_ptr<std::thread> timer_thread;
		std::function<void()> finish_call = nullptr, resend_call = nullptr,
				invalidated_call = nullptr;
		bool valid = false;

		std::mutex mtx;
		std::condition_variable cv;

	public:
		TimedFrameHandler() {}
		TimedFrameHandler(uint64_t timeout_millis_p,
				uint16_t timeout_resend_millis_p) : timeout_millis(
				timeout_millis_p), timeout_resend_millis(
				timeout_resend_millis_p) {
		}
		~TimedFrameHandler() {
			invalidate_timer();
			//if (timer_thread)
			//	if(timer_thread->joinable())
			//	timer_thread->join();

		}
		inline void apply_parameters(uint64_t timeout_millis_p,
				uint16_t timeout_resend_millis_p) {
			timeout_millis = timeout_millis_p;
			timeout_resend_millis = timeout_resend_millis_p;
		}
		inline void reset_resend() {
			resend_time = send_time + timeout_resend_millis;
		}
		inline void start() {
			send_time = current_millis();
			reset_resend();
			if (timeout_resend_millis == 0 || timeout_millis == 0) {
				throw std::invalid_argument("Timeout is zero");
			}
			valid = true;
			//timer_thread = std::make_unique<std::thread>([this]() {
			//	prctl(PR_SET_NAME, "TFPacket", 0, 0, 0);
			this->run();
			//});
		}
		inline void invalidate_timer() {
			if (valid) {
				valid = false;
				cv.notify_one();
			}
		}
		inline void set_resend_call(std::function<void()> function) {
			resend_call = function;
		}
		inline void set_fire_call(std::function<void()> function) {
			finish_call = function;
		}
		inline void set_invalidated_call(std::function<void()> function) {
			invalidated_call = function;
		}
		inline bool finished() {
			return (!valid);
		}
		inline void run() {
			// Wait for resend time until send_time-millis > timeout or until the timer is invalidated
			std::unique_lock < std::mutex > lock(mtx);
			while (valid) {
				uint64_t now = current_millis();

				if ((now >= send_time + timeout_millis) && valid) {
					// Timeout logic
					valid = false;
					finish_call();
					break;
				} else if (now >= resend_time && valid) {
					// Resend logic
					resend_time = now + timeout_resend_millis;
					resend_call();
				}

				cv.wait_until(lock,
						std::chrono::steady_clock::now()
								+ std::chrono::milliseconds(
										timeout_resend_millis),
						[&] {
							return !valid || current_millis() >= resend_time
									|| current_millis()
											>= (send_time + timeout_millis);
						});

				if (!valid) {
					invalidated_call();
					break;
				}
			}
		}
};

class NonBlockingTimer {

		bool valid = false;
		bool consumed = false;

		uint64_t timeout_millis = 0, timeout_resend_millis = 0, send_time = 0,
				resend_time = 0;
		std::function<void()> finish_call = nullptr, resend_call = nullptr,
				invalidated_call = nullptr;

	public:
		NonBlockingTimer() {
		}
		~NonBlockingTimer() {
			invalidate_timer();
		}
		inline void apply_parameters(uint64_t timeout_millis_p,
				uint16_t timeout_resend_millis_p) {
			timeout_millis = timeout_millis_p;
			timeout_resend_millis = timeout_resend_millis_p;
		}
		inline void reset_resend() {
			resend_time = send_time + timeout_resend_millis;
		}
		inline void start() {
			send_time = current_millis();
			reset_resend();
			if (timeout_resend_millis == 0 || timeout_millis == 0)
				throw std::invalid_argument("Timeout is zero");

			valid = true;
			consumed = false;

		}

		inline void invalidate_timer() {
			if (valid) {
				valid = false;
			}
		}
		inline void set_resend_call(std::function<void()> function) {
			resend_call = function;
		}
		inline void set_fire_call(std::function<void()> function) {
			finish_call = function;
		}
		inline void set_invalidated_call(std::function<void()> function) {
			invalidated_call = function;
		}
		inline bool finished() {
			return (consumed);
		}
		inline void run() {

			if (consumed)
				return;

			if (!valid) {
				consumed = true;
				invalidated_call();
				return;
			}

			uint64_t now = current_millis();

			if ((now >= send_time + timeout_millis) && valid) {
				// Timeout logic
				valid = false;
				consumed = true;
				finish_call();
				return;
			} else if (now >= resend_time && valid) {
				// Resend logic
				resend_time = now + timeout_resend_millis;
				resend_call();
			}
		}
};
