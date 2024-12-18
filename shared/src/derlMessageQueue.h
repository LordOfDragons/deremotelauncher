/*
 * MIT License
 *
 * Copyright (C) 2024, DragonDreams GmbH (info@dragondreams.ch)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _DERLMESSAGEQUEUE_H_
#define _DERLMESSAGEQUEUE_H_

#include <memory>
#include <queue>
#include <vector>
#include <denetwork/message/denMessage.h>


/**
 * \brief Message queue.
 */
class derlMessageQueue{
public:
	/** \brief Message list type. */
	typedef std::vector<denMessage::Ref> Messages;
	
	
private:
	typedef std::queue<denMessage::Ref> Queue;
	
	Queue pQueue;
	
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/**
	 * \brief Create message queue.
	 */
	derlMessageQueue() = default;
	
	/** \brief Clean up message queue. */
	~derlMessageQueue() = default;
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Add message to queue. */
	void Add(const denMessage::Ref &message);
	
	/**
	 * \brief Pop message from queue.
	 * \returns true if successful or false if queue is empty.
	 */
	bool Pop(denMessage::Ref &message);
	
	/** \brief Pop all messages from queue adding them to messages. */
	void PopAll(Messages &messages);
	/*@}*/
};

#endif
