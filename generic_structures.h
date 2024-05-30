/*
 * generic_structures.h
 *
 *  Created on: 26 May 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

#include "Settings.h"
#include "utils.h"

template<typename message>
class Messenger{
	public:
		Messenger() = default;
		virtual ~Messenger(){}
		virtual bool input(message& m) = 0;
};

class SyncronizedShutdown{
	public:
		SyncronizedShutdown() = default;
		virtual ~SyncronizedShutdown(){};
		bool running = false;
		void stop(){
			running = false;
			stop_module();
		}
		virtual void stop_module() = 0;
};

template<typename message>
class SystemInterface : public Messenger<message>, public SettingsCompliant {

	public:
		SystemInterface() = default;
		virtual ~SystemInterface(){}

		virtual void input_finished(){}
		virtual bool input(message& m) = 0;
		virtual void apply_settings(const Settings &settings) override {
			SettingsCompliant::apply_settings(settings);
		}
		virtual void stop_module() = 0;
};

class Message{
	public:
		Message(unsigned int size) : data(new uint8_t[size]), length(size){
			//printf("Created message of length %d\n", length);
		}
		virtual ~Message(){
			//printf("Message destructor. Had length: %d\n", length);
		}

		// Costruttore di spostamento
		Message(Message &&other) noexcept : data(std::move(other.data)), length(other.length) {
			other.length = 0;
			other.data = nullptr;
			//printf("Copied message of length %d\n", length);
		}

		// Operatore di assegnazione per spostamento
		Message& operator=(Message &&other) noexcept {
			if (this != &other) {
				data = std::move(other.data);
				length = other.length;
				other.length = 0;
				other.data = nullptr;
			}
			return *this;
		}

		// Disabilitare il costruttore di copia e l'operatore di assegnazione di copia
		Message(const Message&) = delete;
		Message& operator=(const Message&) = delete;

		std::unique_ptr<uint8_t[]> data;
		unsigned int length;
};
typedef Message TunMessage;
typedef Message RFMessage;

class PacketMessageFactory : public SettingsCompliant{
	public:
		PacketMessageFactory(const Settings& ref){apply_settings(ref);}
		Message make_new_packet(){
			return Message(current_settings()->payload_size);
		}
		virtual ~PacketMessageFactory(){printf("Factory destructor\n");}
};

