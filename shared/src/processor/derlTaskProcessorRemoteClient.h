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

#ifndef _DERLTASKPROCESSORREMOTECLIENT_H_
#define _DERLTASKPROCESSORREMOTECLIENT_H_

#include "derlBaseTaskProcessor.h"

#include "../task/derlTaskSyncClient.h"

class derlRemoteClient;


/** \brief Process tasks. */
class derlTaskProcessorRemoteClient : public derlBaseTaskProcessor{
public:
	/** \brief Reference type. */
	typedef std::shared_ptr<derlTaskProcessorRemoteClient> Ref;
	
	
protected:
	derlRemoteClient &pClient;
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/** \brief Create processor. */
	derlTaskProcessorRemoteClient(derlRemoteClient &client);
	
	/** \brief Clean up processor. */
	~derlTaskProcessorRemoteClient() = default;
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Client. */
	inline derlRemoteClient &GetClient() const{ return pClient; }
	
	/**
	 * \brief Process one task if possible.
	 * \returns true if task is run or false otherwise.
	 */
	bool RunTask() override;
	
	/**
	 * \brief Find next sync client task.
	 * \returns true if task is found otherwise false.
	 */
	bool FindNextTaskSyncClient(derlTaskSyncClient::Ref &task) const;
	
	/** \brief Process task sync client. */
	virtual void ProcessSyncClient(derlTaskSyncClient &task);
};

#endif
