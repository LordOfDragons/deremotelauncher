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

#ifndef _DERLTASKFILEWRITE_H_
#define _DERLTASKFILEWRITE_H_

#include <mutex>
#include <atomic>

#include "derlTaskFileWriteBlock.h"


/**
 * \brief File write task.
 */
class derlTaskFileWrite : public derlBaseTask{
public:
	/** \brief Reference type. */
	typedef std::shared_ptr<derlTaskFileWrite> Ref;
	
	/** \brief Reference list. */
	typedef std::vector<Ref> List;
	
	/** \brief Reference map keyed by path. */
	typedef std::unordered_map<std::string, Ref> Map;
	
	/** \brief Status. */
	enum class Status{
		pending,
		preparing,
		processing,
		finishing,
		success,
		failure,
		validationFailed
	};
	
	
private:
	const std::string pPath;
	std::atomic<Status> pStatus;
	uint64_t pFileSize;
	uint64_t pBlockSize;
	int pBlockCount;
	derlTaskFileWriteBlock::List pBlocks;
	bool pTruncate;
	std::string pHash;
	std::mutex pMutex;
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/** \brief Create task. */
	derlTaskFileWrite(const std::string &path);
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Path. */
	inline const std::string &GetPath() const{ return pPath; }
	
	/** \brief Status. */
	inline Status GetStatus() const{ return pStatus; }
	void SetStatus(Status status);
	
	/** \brief File size. */
	inline uint64_t GetFileSize() const{ return pFileSize; }
	void SetFileSize(uint64_t fileSize);
	
	/** \brief Block size. */
	inline uint64_t GetBlockSize() const{ return pBlockSize; }
	void SetBlockSize(uint64_t blockSize);
	
	/** \brief Block count. */
	inline int GetBlockCount() const{ return pBlockCount; }
	void SetBlockCount(int blockCount);
	
	/** \brief Truncate file. */
	inline bool GetTruncate() const{ return pTruncate; }
	void SetTruncate(bool truncate);
	
	/** \brief File hash. */
	inline const std::string &GetHash() const{ return pHash; }
	void SetHash(const std::string &hash);
	
	/**
	 * \brief Blocks.
	 * 
	 * Access with mutex locked.
	 */
	inline derlTaskFileWriteBlock::List &GetBlocks(){ return pBlocks; }
	
	/** \brief Mutex. */
	inline std::mutex &GetMutex(){ return pMutex; }
	/*@}*/
};

#endif
