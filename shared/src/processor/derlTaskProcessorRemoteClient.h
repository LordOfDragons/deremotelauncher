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
#include "../task/derlTaskFileLayout.h"

class derlRemoteClient;


/** \brief Process tasks. */
class derlTaskProcessorRemoteClient : public derlBaseTaskProcessor{
public:
	/** \brief Reference type. */
	typedef std::shared_ptr<derlTaskProcessorRemoteClient> Ref;
	
	/** \brief List type. */
	typedef std::vector<Ref> List;
	
	
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
	
	/** \brief Process one task if possible. */
	void RunTask() override;
	
	
	
	/**
	 * \brief Next pending tasks or nullptr.
	 * 
	 * If no task is pending waits on the client pending task condition. If the condition is
	 * signled the next pending tasks is popped from the queue and stored in the task parameter
	 * if present and true is returned. Otherwise the task parameter stays unchanged and false
	 * is returned. If the condition is spuriously signled the task parameter stays unchanged
	 * and false is returned.
	 */
	bool NextPendingTask(derlBaseTask::Ref &task);
	
	
	
	/** \brief Process prepare hashing. */
	virtual void ProcessPrepareHashing(derlTaskSyncClient &task);
	
	/** \brief Process prepare writing. */
	virtual void ProcessPrepareWriting(derlTaskSyncClient &task);
	
	/** \brief Process task read file block. */
	virtual void ProcessReadFileBlock(derlTaskFileWriteBlock &task);
	
	/** \brief Process task file layout server. */
	virtual void ProcessFileLayoutServer(derlTaskFileLayout &task);
	
	
	
protected:
	/**
	 * \brief Prepare run task.
	 * \note Client mutex is held while this method is called.
	 */
	virtual void PrepareRunTask();
	
	/** \brief Compare file layouts and add delete file tasks. */
	void AddFileDeleteTasks(derlTaskSyncClient &task,
		const derlFileLayout &layoutServer, const derlFileLayout &layoutClient);
	
	/** \brief Compare file layouts and add write file block hash tasks. */
	void AddFileBlockHashTasks(derlTaskSyncClient &task,
		const derlFileLayout &layoutServer, const derlFileLayout &layoutClient);
	
	/** \brief Compare file layouts and add write file tasks. */
	void AddFileWriteTasks(derlTaskSyncClient &task, const derlFileLayout &layoutServer,
		const derlFileLayout &layoutClient);
	
	/** \brief Create write file task writing the entire file. */
	void AddFileWriteTaskFull(derlTaskSyncClient &task, const derlFile &file);
	
	/** \brief Create write file task writing only changed blocks. */
	void AddFileWriteTaskPartial(derlTaskSyncClient &task, const derlFile &fileServer,
		const derlFile &fileClient);
};

#endif
