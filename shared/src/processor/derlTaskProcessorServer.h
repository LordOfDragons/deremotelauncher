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

#ifndef _DERLTASKPROCESSORSERVER_H_
#define _DERLTASKPROCESSORSERVER_H_

#include "derlBaseTaskProcessor.h"

#include "../task/derlTaskFileLayout.h"

class derlServer;


/** \brief Process tasks. */
class derlTaskProcessorServer : public derlBaseTaskProcessor{
public:
	/** \brief Reference type. */
	typedef std::shared_ptr<derlTaskProcessorServer> Ref;
	
	/** \brief List type. */
	typedef std::vector<Ref> List;
	
	
protected:
	derlServer &pServer;
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/** \brief Create processor. */
	derlTaskProcessorServer(derlServer &server);
	
	/** \brief Clean up processor. */
	virtual ~derlTaskProcessorServer() noexcept;
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Server. */
	inline derlServer &GetServer() const{ return pServer; }
	
	/**
	 * \brief Process one task if possible.
	 * \returns true if task is run or false otherwise.
	 */
	bool RunTask() override;
	
	
	
	/**
	 * \brief Find next file layout task.
	 * \returns true if task is found otherwise false.
	 */
	bool FindNextTaskFileLayout(derlTaskFileLayout::Ref &task) const;
	
	
	
	/** \brief Process task file layout. */
	virtual void ProcessFileLayout(derlTaskFileLayout &task);
};

#endif
