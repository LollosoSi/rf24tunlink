/*
 * TUNHandler.h
 *
 *  Created on: 4 Feb 2024
 *      Author: andrea
 */

#pragma once

#include "../interfaces/Messenger.h"
#include "../structures/TUNMessage.h"



class TUNHandler : public Messenger<TUNMessage> {
	public:
		TUNHandler();
		~TUNHandler();
	protected:
		bool receive_message(TUNMessage &tunmsg);
};


