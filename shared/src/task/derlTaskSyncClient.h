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

#ifndef _DERLTASKSYNCCLIENT_H_
#define _DERLTASKSYNCCLIENT_H_

#include <string>
#include <mutex>
#include <atomic>

#include "derlBaseTask.h"

#include "derlTaskFileLayout.h"
#include "derlTaskFileWrite.h"
#include "derlTaskFileDelete.h"
#include "derlTaskFileBlockHashes.h"
#include "../derlFileLayout.h"


/**
 * \brief Synchronize client task.
 */
class derlTaskSyncClient : public derlBaseTask{
public:
	/** \brief Reference type. */
	typedef std::shared_ptr<derlTaskSyncClient> Ref;
	
	/** \brief Status. */
	enum class Status{
		pending,
		prepareTasksHashing,
		processHashing,
		prepareTasksWriting,
		processWriting,
		success,
		failure
	};
	
	
private:
	std::atomic<Status> pStatus;
	std::string pError;
	derlTaskFileLayout::Ref pTaskFileLayoutServer, pTaskFileLayoutClient;
	
	derlTaskFileWrite::Map pTasksWriteFile;
	derlTaskFileDelete::Map pTaskDeleteFiles;
	derlTaskFileBlockHashes::Map pTasksFileBlockHashes;
	std::mutex pMutex;
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/** \brief Create task. */
	derlTaskSyncClient();
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Status. */
	inline Status GetStatus() const{ return pStatus; }
	void SetStatus(Status status);
	
	/** \brief Error. */
	inline const std::string &GetError() const{ return pError; }
	void SetError(const std::string &error);
	
	/** \brief Server file layout task or nullptr. */
	inline const derlTaskFileLayout::Ref &GetTaskFileLayoutServer() const{ return pTaskFileLayoutServer; }
	void SetTaskFileLayoutServer(const derlTaskFileLayout::Ref &task);
	
	/** \brief Client file layout task or nullptr. */
	inline const derlTaskFileLayout::Ref &GetTaskFileLayoutClient() const{ return pTaskFileLayoutClient; }
	void SetTaskFileLayoutClient(const derlTaskFileLayout::Ref &task);
	
	/** \brief Delete file tasks. */
	inline const derlTaskFileDelete::Map &GetTasksDeleteFile() const{ return pTaskDeleteFiles; }
	inline derlTaskFileDelete::Map &GetTasksDeleteFile(){ return pTaskDeleteFiles; }
	
	/** \brief Write file tasks. */
	inline const derlTaskFileWrite::Map &GetTasksWriteFile() const{ return pTasksWriteFile; }
	inline derlTaskFileWrite::Map &GetTasksWriteFile(){ return pTasksWriteFile; }
	
	/** \brief File block hashes tasks. */
	inline const derlTaskFileBlockHashes::Map &GetTasksFileBlockHashes() const{ return pTasksFileBlockHashes; }
	inline derlTaskFileBlockHashes::Map &GetTasksFileBlockHashes(){ return pTasksFileBlockHashes; }
	
	/** \brief Mutex. */
	inline std::mutex &GetMutex(){ return pMutex; }
	/*@}*/
};

#endif
