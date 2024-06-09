/*
 * Packetizer.h
 *
 *  Created on: 27 May 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "../generic_structures.h"
#include "../rs_codec/RSCodec.h"

#include <thread>
// Thread naming
#include <sys/prctl.h>

#include <mutex>
#include <condition_variable>

#include <deque>
#include <queue>

#include <functional>


template<class tun_message_class, class radio_message_class>
class Packetizer : public SettingsCompliant, public SyncronizedShutdown {
	public:
		class Frame {
		public:
		    Frame() : metadata(0), resend_time(0) {}
		    ~Frame() {}

		    // Move constructor
		    Frame(Frame&& other) noexcept = default;

		    // Move assignment operator
		    Frame& operator=(Frame&& other) noexcept = default;

		    // Delete copy constructor and copy assignment operator
		    Frame(const Frame&) = delete;
		    Frame& operator=(const Frame&) = delete;

		    // Members
		    std::vector<radio_message_class> packets;
		    uint8_t metadata;
		    uint16_t resend_time;
		};


	protected:
		std::mutex tun_in_mtx;
		std::condition_variable tun_cv;

		std::mutex radio_in_mtx;
		std::condition_variable radio_cv;

		std::mutex outgoing_frames_mtx;
		std::condition_variable frames_cv;

		std::mutex radio_worker_mtx;
		std::condition_variable radio_worker_cv;

		std::deque<tun_message_class> incoming_tun;
		std::deque<radio_message_class> incoming_packets;

		std::deque<Frame> outgoing_frames;

		bool running_wpk = false, running_wrd = false, running_wtun = false, running_nxf = false;

		class TunReceiver : public SystemInterface<tun_message_class> {
				Packetizer<tun_message_class, radio_message_class> *p;
			public:
				TunReceiver(Packetizer<tun_message_class, radio_message_class> *pp) {
					p = pp;
				}
				~TunReceiver() {
				}
				inline bool input(tun_message_class &m) {
					return (p->receive_tun(m));
				}
				inline void apply_settings(const Settings &settings){};
				inline void stop_module(){};
		};

		class RadioReceiver : public SystemInterface<radio_message_class> {
				Packetizer<tun_message_class, radio_message_class> *p;
			public:
				RadioReceiver(Packetizer<tun_message_class, radio_message_class> *pp) {
					p = pp;
				}
				~RadioReceiver() {
				}
				inline bool input(radio_message_class &m) {
					return (p->receive_radio(m));
				}
				inline bool input(std::deque<radio_message_class> &m) {
					return (p->receive_radio(m));
				}
				inline void apply_settings(const Settings &settings){};
				inline void stop_module(){};
		};

		SystemInterface<radio_message_class> *radio = nullptr;
		SystemInterface<tun_message_class> *tun = nullptr;
		RSCodec *rsc = nullptr;
		TunReceiver *tr = nullptr;
		RadioReceiver *rr = nullptr;
		PacketMessageFactory *pmf = nullptr;

		std::unique_ptr<std::thread> tun_thread, radio_thread, radio_worker_thread;

	public:
		Packetizer() {
			tr = new TunReceiver(this);
			rr = new RadioReceiver(this);
		}

		virtual ~Packetizer() {
			delete tr;
			delete rr;
		}

		inline SystemInterface<tun_message_class>* expose_tun_receiver() {
			return tr;
		}
		inline SystemInterface<radio_message_class>* expose_radio_receiver() {
			return rr;
		}

		inline bool receive_tun(tun_message_class &m) {
			//{
			//	std::lock_guard lock(tun_in_mtx);
			//	incoming_tun.push_back(std::move(m));
			//}
			//tun_cv.notify_one();
			process_tun(m);
			return (true);
		}
		inline bool receive_radio(radio_message_class &m) {
			{
				std::lock_guard lock(radio_in_mtx);
				incoming_packets.push_back(std::move(m));
			}
			radio_cv.notify_all();
			//process_packet(m);
			return (true);
		}
		inline bool receive_radio(std::deque<radio_message_class> &m) {
			{
				std::lock_guard lock(radio_in_mtx);
				incoming_packets.insert(incoming_packets.end(),
				  std::make_move_iterator(m.begin()),
				  std::make_move_iterator(m.end()));
			}
			radio_cv.notify_all();
			//process_packet(m);
			return (true);
		}

		inline void worker_tun() {
			printf("Worker tun thread is disabled for this instance\n");
			return;
			while (running_wtun) {
				//tun_message_class f = nullptr;
				{
					std::unique_lock lock(tun_in_mtx);
					tun_cv.wait(lock, [&] {
						return !running_wtun || !incoming_tun.empty();
					});

					if (!running_wtun)
						break;

					tun_message_class f = std::move(incoming_tun.front());
					incoming_tun.pop_front();
					lock.unlock();
					process_tun(f);
				}

			}
		}

		inline void worker_packet() {
			//printf("Worker packet thread is disabled for this instance\n");
			//return;
			std::vector<radio_message_class> temp_in;

			while (running_wpk) {
				//radio_message_class f = nullptr;
				{
					std::unique_lock lock(radio_in_mtx);
					radio_cv.wait(lock, [&] {
						return !running_wpk || !incoming_packets.empty();
					});

					if (!running_wpk)
						break;
					temp_in.reserve(incoming_packets.size());
					temp_in.insert(temp_in.end(),
									  std::make_move_iterator(incoming_packets.begin()),
									  std::make_move_iterator(incoming_packets.end()));
					incoming_packets.clear();
				}
				auto it = temp_in.begin();
				while(it != temp_in.end()){
					process_packet(*(it++));
				}
				temp_in.clear();
			}
		}

		virtual inline void process_tun(tun_message_class &m) = 0;
		virtual inline void process_packet(radio_message_class &m) = 0;
		virtual inline void apply_settings(const Settings &settings) override {
			SettingsCompliant::apply_settings(settings);
			stop_module();
			running_wpk = running_wtun = running_wrd = running_nxf = true;
			tun_thread = std::make_unique<std::thread>([&]{prctl(PR_SET_NAME, "Pk tun Worker", 0, 0, 0);worker_tun();});
			radio_thread = std::make_unique<std::thread>([&]{prctl(PR_SET_NAME, "Pk Radio Worker", 0, 0, 0);worker_packet();});
			radio_worker_thread = std::make_unique<std::thread>([&]{prctl(PR_SET_NAME, "Radio OUT", 0, 0, 0);worker_sendradio();});

		}
		virtual inline unsigned int get_mtu() = 0;

		virtual inline void stop_module() {
			running_wpk = running_wtun = running_wrd = running_nxf = false;
			radio_cv.notify_all();
			tun_cv.notify_all();
			frames_cv.notify_all();
			radio_worker_cv.notify_all();
			if(tun_thread && tun_thread.get()->joinable())
				tun_thread.get()->join();
			if(radio_thread && radio_thread.get()->joinable())
				radio_thread.get()->join();
			if(radio_worker_thread && radio_worker_thread.get()->joinable())
				radio_worker_thread.get()->join();
		}

		inline void add_frame_to_queue(Frame& f){
			{
				std::unique_lock<std::mutex> lock(outgoing_frames_mtx);
				outgoing_frames.push_back(std::move(f));
			}
			frames_cv.notify_one();
		}

		inline Frame next_frame() {
			std::unique_lock<std::mutex> lock(outgoing_frames_mtx);
			frames_cv.wait(lock, [&] {
				return (!running_nxf || !outgoing_frames.empty());
			});
			if (!running_nxf)
				return {};

			auto f = std::move(outgoing_frames.front());
			outgoing_frames.pop_front();
			return f;
		}


		std::deque<Frame*> queued_out_frames;
		std::deque<RFMessage*> queued_out_messages;

		inline void worker_sendradio() {
			printf("Worker send radio thread is disabled for this instance\n");
			return;
			while (running_wrd) {
				{
					std::unique_lock lock(radio_worker_mtx);
					radio_worker_cv.wait(lock, [&] {
						return !running_wrd || !queued_out_frames.empty() || !queued_out_messages.empty();
					});

					if (!running_wrd)
						break;

					for(auto f : queued_out_frames)
						radio->input(f->packets);

					for(auto outpacket : queued_out_messages)
						radio->input(*outpacket);
				}

			}
		}

		inline void send_radio_finish(){
			radio->input_finished();
		}
		inline void send_to_radio(RFMessage &rfm, bool auto_finish = true) {
			radio->input(rfm);
			if(auto_finish)
				send_radio_finish();
		}
		inline void send_to_radio(Frame& f) {
			radio->input(f.packets);
		}
		/*inline void send_to_radio(RFMessage &rfm, bool auto_finish = true) {
			{
			std::unique_lock lock(radio_worker_mtx);
			queued_out_messages.push_back(&rfm);
			}
			radio_worker_cv.notify_all();
		}
		inline void send_to_radio(Frame &f) {
			{
			std::unique_lock lock(radio_worker_mtx);
			queued_out_frames.push_back(&f);
			}
			radio_worker_cv.notify_all();
		}*/
		inline void register_rsc(RSCodec *reference) {
			rsc = reference;
		}
		inline void register_radio(SystemInterface<radio_message_class> *receiver) {
			radio = receiver;
		}
		inline void register_tun(SystemInterface<tun_message_class> *receiver) {
			tun = receiver;
		}
		inline void register_message_factory(PacketMessageFactory *pmfr) {
			pmf = pmfr;
		}

};

class TimedFrameHandler {

		uint64_t timeout_millis;
		uint16_t timeout_resend_millis;
		uint64_t send_time = 0;
		uint64_t resend_time = 0;
		std::unique_ptr<std::thread> timer_thread;
		std::function<void()> finish_call = nullptr, resend_call = nullptr,
				invalidated_call = nullptr;
		bool valid = false;

		std::mutex mtx;
		std::condition_variable cv;

	public:
		TimedFrameHandler(uint64_t timeout_millis_p, uint16_t timeout_resend_millis_p) : timeout_millis(timeout_millis_p), timeout_resend_millis(
				timeout_resend_millis_p) {
		}
		~TimedFrameHandler() {
			invalidate_timer();
			//if (timer_thread)
			//	if(timer_thread->joinable())
			//	timer_thread->join();

		}
		inline void apply_parameters(uint64_t timeout_millis_p, uint16_t timeout_resend_millis_p){
			timeout_millis=timeout_millis_p;
			timeout_resend_millis=timeout_resend_millis_p;
		}
		inline void reset_resend(){
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
		inline bool finished(){
			return (!valid);
		}
		inline void run() {
			// Wait for resend time until send_time-millis > timeout or until the timer is invalidated
			std::unique_lock<std::mutex> lock(mtx);
			while (valid) {
				uint64_t now = current_millis();

				if ((now >= send_time + timeout_millis) && valid) {
					// Timeout logic
					valid=false;
					finish_call();
					break;
				} else if (now >= resend_time && valid) {
					// Resend logic
					resend_time = now + timeout_resend_millis;
					resend_call();
				}

				cv.wait_until(lock, std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_resend_millis),
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
