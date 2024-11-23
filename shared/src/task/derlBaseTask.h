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

#ifndef _DERLBASETASK_H_
#define _DERLBASETASK_H_

#include <memory>
#include <vector>
#include <queue>
#include <unordered_map>


/**
 * \brief Base class for tasks.
 */
class derlBaseTask{
public:
	/** \brief Reference type. */
	typedef std::shared_ptr<derlBaseTask> Ref;
	
	/** \brief Reference list. */
	typedef std::vector<Ref> List;
	
	/** \brief Reference map keyed by path. */
	typedef std::unordered_map<std::string, Ref> Map;
	
	/** \brief Reference queue. */
	typedef std::deque<Ref> Queue;
	
	/** \brief Task type. */
	enum class Type{
		/** \brief derlTaskSyncClient. */
		syncClient,
		
		/** \brief derlTaskFileLayout. */
		fileLayout,
		
		/** \brief derlTaskFileBlockHashes. */
		fileBlockHashes,
		
		/** \brief derlTaskFileDelete. */
		fileDelete,
		
		/** \brief derlTaskFileWrite. */
		fileWrite,
		
		/** \brief derlTaskFileWriteBlock. */
		fileWriteBlock
	};
	
	
private:
	const Type pType;
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/** \brief Create task. */
	derlBaseTask(Type type);
	
	/** \brief Clean up task. */
	virtual ~derlBaseTask() = default;
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Type. */
	inline Type GetType() const{ return pType; }
	/*@}*/
};

#endif
