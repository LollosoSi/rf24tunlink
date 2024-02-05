/*
 * Messenger.h
 *
 *  Created on: 4 Feb 2024
 *      Author: Andrea Roccaccino
 */

#pragma once

template<typename Message>
class Messenger{
public:
		virtual bool receive_message(Message *m) = 0;
		bool send(Message *m){return (receive_message(m));}
		virtual ~Messenger(){}
		Messenger() = default;
};
