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

#ifndef _DERLTASKFILEWRITEBLOCK_H_
#define _DERLTASKFILEWRITEBLOCK_H_

#include <string>
#include <atomic>

#include "derlBaseTask.h"

class derlFileBlock;
class derlTaskFileWrite;


/**
 * \brief File write task block.
 */
class derlTaskFileWriteBlock : public derlBaseTask{
public:
	/** \brief Reference type. */
	typedef std::shared_ptr<derlTaskFileWriteBlock> Ref;
	
	/** \brief Reference list. */
	typedef std::vector<Ref> List;
	
	/** \brief Status. */
	enum class Status{
		pending,
		readingData,
		dataReady,
		dataSent,
		success,
		failure
	};
	
	
private:
	derlTaskFileWrite &pParentTask;
	std::atomic<Status> pStatus;
	int pIndex;
	uint64_t pSize;
	std::string pData;
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/** \brief Create task. */
	derlTaskFileWriteBlock(derlTaskFileWrite &parentTask, int index, uint64_t size);
	
	/** \brief Create task. */
	derlTaskFileWriteBlock(derlTaskFileWrite &parentTask, int index, uint64_t size,
		const std::string &data);
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Parent task. */
	inline derlTaskFileWrite &GetParentTask() const{ return pParentTask; }
	
	/** \brief Status. */
	inline Status GetStatus() const{ return pStatus; }
	void SetStatus(Status status);
	
	/** \brief Index. */
	inline int GetIndex() const{ return pIndex; }
	
	/** \brief Size. */
	inline uint64_t GetSize() const{ return pSize; }
	
	/** \brief Data. */
	inline std::string &GetData(){ return pData; }
	inline const std::string &GetData() const{ return pData; }
	/*@}*/
};

#endif
