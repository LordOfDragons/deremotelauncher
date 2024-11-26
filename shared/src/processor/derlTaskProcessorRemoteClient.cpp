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

#include <stdexcept>
#include <mutex>

#include "derlTaskProcessorRemoteClient.h"
#include "../derlRemoteClient.h"
#include "../derlServer.h"
#include "../derlFile.h"
#include "../derlFileBlock.h"
#include "../derlFileLayout.h"


// Class derlTaskProcessorRemoteClient
////////////////////////////////////////

derlTaskProcessorRemoteClient::derlTaskProcessorRemoteClient(derlRemoteClient &client) :
pClient(client){
	pLogClassName = "derlTaskProcessorRemoteClient";
}

// Management
///////////////

void derlTaskProcessorRemoteClient::RunTask(){
	derlBaseTask::Ref task;
	if(!NextPendingTask(task)){
		return;
	}
	
	{
	const std::lock_guard guard(pClient.GetMutex());
	pBaseDir = pClient.GetPathDataDir();
	pEnableDebugLog = pClient.GetEnableDebugLog();
	}
	
	switch(task->GetType()){
	case derlBaseTask::Type::fileLayout:
		ProcessFileLayoutServer(static_cast<derlTaskFileLayout&>(*task));
		break;
		
	case derlBaseTask::Type::fileWriteBlock:
		ProcessReadFileBlock(static_cast<derlTaskFileWriteBlock&>(*task));
		break;
		
	case derlBaseTask::Type::syncClient:{
		derlTaskSyncClient &taskSync = static_cast<derlTaskSyncClient&>(*task);
		if(taskSync.GetStatus() == derlTaskSyncClient::Status::pending){
			ProcessPrepareHashing(taskSync);
		}
		if(taskSync.GetStatus() == derlTaskSyncClient::Status::prepareTasksWriting){
			ProcessPrepareWriting(taskSync);
		}
		}break;
		
	default:
		break;
	}
}

bool derlTaskProcessorRemoteClient::NextPendingTask(derlBaseTask::Ref &task){
	if(pExit){
		return false;
	}
	
	std::unique_lock guard(pClient.GetMutexPendingTasks());
	derlBaseTask::Queue &tasks = pClient.GetPendingTasks();
	if(tasks.empty()){
		pClient.GetConditionPendingTasks().wait(guard);
		if(tasks.empty() || pExit){
			return false;
		}
	}
	
	derlBaseTask::Queue::const_iterator iter;
	for(iter=tasks.cbegin(); iter!=tasks.cend(); iter++){
		const derlBaseTask::Ref &pendingTask = *iter;
		bool found = false;
		
		switch(pendingTask->GetType()){
		case derlBaseTask::Type::fileLayout:
			found = true;
			break;
			
		case derlBaseTask::Type::fileWriteBlock:
			found = true;
			break;
			
		case derlBaseTask::Type::syncClient:
			switch(static_cast<derlTaskSyncClient&>(*pendingTask).GetStatus()){
			case derlTaskSyncClient::Status::pending:
			case derlTaskSyncClient::Status::prepareTasksWriting:
				found = true;
				break;
				
			default:
				break;
			}
			break;
			
		default:
			break;
		}
		
		if(found){
			task = pendingTask;
			tasks.erase(iter);
			return true;
		}
	}
	
	return false;
}

void derlTaskProcessorRemoteClient::ProcessFileLayoutServer(derlTaskFileLayout &task){
	LogDebug("ProcessFileLayoutServer", "Build file layout");
	const derlTaskSyncClient::Ref taskSync(pClient.GetTaskSyncClient());
	if(!taskSync){
		return;
	}
	
	const uint64_t blockSize = 1024000L;
	
	try{
		const derlFileLayout::Ref layout(std::make_shared<derlFileLayout>());
		CalcFileLayout(*layout, "");
		
		derlFile::Map::const_iterator iterFile;
		for(iterFile=layout->GetFilesBegin(); iterFile!=layout->GetFilesEnd(); iterFile++){
			derlFile &file = *iterFile->second;
			file.SetBlockSize(blockSize);
			
			if(file.GetSize() <= blockSize){
				const derlFileBlock::Ref block(std::make_shared<derlFileBlock>(0, file.GetSize()));
				block->SetHash(file.GetHash());
				file.AddBlock(block);
				
			}else{
				derlFileBlock::List blocks;
				CalcFileBlockHashes(blocks, file.GetPath(), file.GetBlockSize());
				file.SetBlocks(blocks);
			}
		}
		
		task.SetStatus(derlTaskFileLayout::Status::success);
		pClient.SetFileLayoutServer(layout);
		
		const std::lock_guard guard(taskSync->GetMutex());
		taskSync->SetTaskFileLayoutServer(nullptr);
		if(!taskSync->GetTaskFileLayoutClient()){
			pClient.AddPendingTaskSync(taskSync);
		}
		
	}catch(const std::exception &e){
		LogException("ProcessFileLayoutServer", e, "Failed");
		std::stringstream ss;
		ss << "Build server file layout failed: " << e.what();
		pClient.FailSynchronization(ss.str());
		
	}catch(...){
		Log(denLogger::LogSeverity::error, "ProcessFileLayoutServer", "Failed");
		pClient.FailSynchronization("Build server file layout failed: unknown error");
	}
}

void derlTaskProcessorRemoteClient::ProcessPrepareHashing(derlTaskSyncClient &task){
	LogDebug("ProcessPrepareHashing", "Run");
	
	try{
		pClient.SetSynchronizeStatus(derlRemoteClient::SynchronizeStatus::processing, "Synchronize...");
		
		const derlFileLayout::Ref layoutServer(pClient.GetFileLayoutServer());
		const derlFileLayout::Ref layoutClient(pClient.GetFileLayoutClient());
		if(!layoutServer || !layoutClient){
			throw std::runtime_error("Missing layouts");
		}
		
		const std::lock_guard guard(task.GetMutex());
		AddFileBlockHashTasks(task, *layoutServer, *layoutClient);
		
		if(task.GetTasksFileBlockHashes().empty()){
			task.SetStatus(derlTaskSyncClient::Status::prepareTasksWriting);
			
		}else{
			task.SetStatus(derlTaskSyncClient::Status::processHashing);
		}
		
	}catch(const std::exception &e){
		LogException("ProcessPrepareHashing", e, "Failed");
		std::stringstream ss;
		ss << "Synchronize client failed: " << e.what();
		pClient.FailSynchronization(ss.str());
		
	}catch(...){
		Log(denLogger::LogSeverity::error, "ProcessPrepareHashing", "Failed");
		pClient.FailSynchronization();
	}
}

void derlTaskProcessorRemoteClient::ProcessPrepareWriting(derlTaskSyncClient &task){
	LogDebug("ProcessPrepareWriting", "Run");
	
	try{
		pClient.SetSynchronizeStatus(derlRemoteClient::SynchronizeStatus::processing, "Synchronize...");
		
		const derlFileLayout::Ref layoutServer(pClient.GetFileLayoutServer());
		const derlFileLayout::Ref layoutClient(pClient.GetFileLayoutClient());
		if(!layoutServer || !layoutClient){
			throw std::runtime_error("Missing layouts");
		}
		
		bool finished;
		{
		const std::lock_guard guard(task.GetMutex());
		AddFileDeleteTasks(task, *layoutServer, *layoutClient);
		AddFileWriteTasks(task, *layoutServer, *layoutClient);
		
		task.SetStatus(derlTaskSyncClient::Status::processWriting);
		finished = task.GetTasksDeleteFile().empty() && task.GetTasksWriteFile().empty();
		}
		
		if(finished){
			pClient.SucceedSynchronization();
			
		}else{
			pClient.GetConnection().SendNextWriteRequests(task);
		}
		
	}catch(const std::exception &e){
		LogException("ProcessSyncClient", e, "Failed");
		std::stringstream ss;
		ss << "Synchronize client failed: " << e.what();
		pClient.FailSynchronization(ss.str());
		
	}catch(...){
		Log(denLogger::LogSeverity::error, "ProcessSyncClient", "Failed");
		pClient.FailSynchronization();
	}
}

void derlTaskProcessorRemoteClient::ProcessReadFileBlock(derlTaskFileWriteBlock &task){
	if(task.GetStatus() != derlTaskFileWriteBlock::Status::readingData){
		return;
	}
	
	if(pEnableDebugLog){
		std::stringstream ss;
		ss << "Read file blocks: " << task.GetParentTask().GetPath();
		LogDebug("ProcessReadFileBlocks", ss.str());
	}
	
	try{
		OpenFile(task.GetParentTask().GetPath(), false);
		
		std::string &data = task.GetData();
		data.assign(task.GetSize(), 0);
		ReadFile((void*)data.c_str(), task.GetParentTask().GetBlockSize() * task.GetIndex(), task.GetSize());
		task.SetStatus(derlTaskFileWriteBlock::Status::dataReady);
		
		const derlTaskSyncClient::Ref taskSync(pClient.GetTaskSyncClient());
		if(taskSync){
			pClient.GetConnection().SendNextWriteRequests(*taskSync);
		}
		
	}catch(const std::exception &e){
		task.SetStatus(derlTaskFileWriteBlock::Status::failure);
		std::stringstream ss;
		ss << "Failed size " << task.GetSize() << " block " << task.GetIndex()
			<< " path " << task.GetParentTask().GetPath();
		LogException("ProcessReadFileBlocks", e, ss.str());
		pClient.FailSynchronization(ss.str());
		
	}catch(...){
		task.SetStatus(derlTaskFileWriteBlock::Status::failure);
		std::stringstream ss;
		ss << "Failed size " << task.GetSize() << " block " << task.GetIndex()
			<< " path " << task.GetParentTask().GetPath();
		Log(denLogger::LogSeverity::error, "ProcessReadFileBlocks", ss.str());
		pClient.FailSynchronization(ss.str());
	}
	
	CloseFile();
}



// Protected Functions
////////////////////////

void derlTaskProcessorRemoteClient::AddFileDeleteTasks(derlTaskSyncClient &task,
const derlFileLayout &layoutServer, const derlFileLayout &layoutClient){
	derlTaskFileDelete::Map &tasksDelete = task.GetTasksDeleteFile();
	derlFile::Map::const_iterator iter;
	
	for(iter=layoutClient.GetFilesBegin(); iter!=layoutClient.GetFilesEnd(); iter++){
		const std::string &path = iter->second->GetPath();
		if(layoutServer.GetFileAt(path)){
			continue;
		}
		
		const derlTaskFileDelete::Ref task(std::make_shared<derlTaskFileDelete>(path));
		tasksDelete[path] = task;
		
		try{
			pClient.GetConnection().SendRequestDeleteFile(*task);
			
		}catch(const std::exception &e){
			task->SetStatus(derlTaskFileDelete::Status::failure);
			LogException("pSendRequestsDeleteFile", e, "Failed");
			throw;
			
		}catch(...){
			task->SetStatus(derlTaskFileDelete::Status::failure);
			Log(denLogger::LogSeverity::error, "pSendRequestsDeleteFile", "Failed");
			throw;
		}
	}
}

void derlTaskProcessorRemoteClient::AddFileBlockHashTasks(derlTaskSyncClient &task,
const derlFileLayout &layoutServer, const derlFileLayout &layoutClient){
	derlFile::Map::const_iterator iter;
	
	for(iter=layoutServer.GetFilesBegin(); iter!=layoutServer.GetFilesEnd(); iter++){
		const derlFile &fileServer = *iter->second;
		const std::string &path = fileServer.GetPath();
		
		const derlFile::Ref fileClientRef(layoutClient.GetFileAt(path));
		if(!fileClientRef){
			continue;
		}
		
		derlFile &fileClient = *fileClientRef;
		
		if(fileClient.GetSize() != fileServer.GetSize()){
			continue;
		}
		if(fileClient.GetHash() == fileServer.GetHash()){
			continue;
		}
		
		fileClient.SetBlockSize(fileServer.GetBlockSize());
		fileClient.RemoveAllBlocks();
		derlFileBlock::List::const_iterator iterBlock;
		for(iterBlock=fileServer.GetBlocksBegin(); iterBlock!=fileServer.GetBlocksEnd(); iterBlock++){
			const derlFileBlock &blockServer = **iterBlock;
			fileClient.AddBlock(std::make_shared<derlFileBlock>(
				blockServer.GetOffset(), blockServer.GetSize()));
		}
		
		const derlTaskFileBlockHashes::Ref taskHashes(std::make_shared<derlTaskFileBlockHashes>(
			fileServer.GetPath(), fileServer.GetBlockSize()));
		task.GetTasksFileBlockHashes()[fileServer.GetPath()] = taskHashes;
		
		try{
			taskHashes->SetStatus(derlTaskFileBlockHashes::Status::processing);
			pClient.GetConnection().SendRequestFileBlockHashes(*taskHashes);
			
		}catch(const std::exception &e){
			taskHashes->SetStatus(derlTaskFileBlockHashes::Status::failure);
			LogException("SendRequestFileBlockHashes", e, "Failed");
			throw;
			
		}catch(...){
			taskHashes->SetStatus(derlTaskFileBlockHashes::Status::failure);
			Log(denLogger::LogSeverity::error, "SendRequestFileBlockHashes", "Failed");
			throw;
		}
	}
}

void derlTaskProcessorRemoteClient::AddFileWriteTasks(derlTaskSyncClient &task,
const derlFileLayout &layoutServer, const derlFileLayout &layoutClient){
	derlFile::Map::const_iterator iter;
	
	for(iter=layoutServer.GetFilesBegin(); iter!=layoutServer.GetFilesEnd(); iter++){
		const derlFile &fileServer = *iter->second;
		const std::string &path = fileServer.GetPath();
		
		const derlFile::Ref fileClientRef(layoutClient.GetFileAt(path));
		if(fileClientRef){
			const derlFile &fileClient = *fileClientRef;
			
			if(fileClient.GetHash() == fileServer.GetHash()
			&& fileClient.GetSize() == fileServer.GetSize()){
				continue;
			}
			
			if(fileClient.GetBlockSize() == fileServer.GetBlockSize()
			&& fileClient.GetBlockCount() == fileServer.GetBlockCount()){
				AddFileWriteTaskPartial(task, fileServer, fileClient);
				
			}else{
				AddFileWriteTaskFull(task, fileServer);
			}
			
		}else{
			AddFileWriteTaskFull(task, fileServer);
		}
	}
}

void derlTaskProcessorRemoteClient::AddFileWriteTaskFull(derlTaskSyncClient &task, const derlFile &file){
	const derlTaskFileWrite::Ref taskWrite(std::make_shared<derlTaskFileWrite>(file.GetPath()));
	derlTaskFileWriteBlock::List &taskBlocks = taskWrite->GetBlocks();
	taskWrite->SetFileSize(file.GetSize());
	taskWrite->SetBlockSize(file.GetBlockSize());
	taskWrite->SetBlockCount(file.GetBlockCount());
	
	derlFileBlock::List::const_iterator iter;
	int index;
	for(iter=file.GetBlocksBegin(), index=0; iter!=file.GetBlocksEnd(); iter++, index++){
		const derlFileBlock &block = **iter;
		
		taskBlocks.push_back(std::make_shared<derlTaskFileWriteBlock>(
			*taskWrite, index, block.GetSize()));
	}
	
	task.GetTasksWriteFile()[file.GetPath()] = taskWrite;
}

void derlTaskProcessorRemoteClient::AddFileWriteTaskPartial(derlTaskSyncClient &task,
const derlFile &fileServer, const derlFile &fileClient){
	const derlTaskFileWrite::Ref taskWrite(std::make_shared<derlTaskFileWrite>(fileServer.GetPath()));
	derlTaskFileWriteBlock::List &taskBlocks = taskWrite->GetBlocks();
	taskWrite->SetFileSize(fileServer.GetSize());
	taskWrite->SetBlockSize(fileServer.GetBlockSize());
	taskWrite->SetBlockCount(fileServer.GetBlockCount());
	
	derlFileBlock::List::const_iterator iterServer, iterClient;
	int index;
	
	for(iterClient=fileClient.GetBlocksBegin(), index=0, iterServer=fileServer.GetBlocksBegin();
	iterServer!=fileServer.GetBlocksEnd(); iterClient++, iterServer++, index++){
		const derlFileBlock &blockServer = **iterServer;
		const derlFileBlock &blockClient = **iterClient;
		
		if(blockClient.GetHash() == blockServer.GetHash()
		&& blockClient.GetOffset() == blockServer.GetOffset()
		&& blockClient.GetSize() == blockServer.GetSize()){
			continue;
		}
		
		taskBlocks.push_back(std::make_shared<derlTaskFileWriteBlock>(
			*taskWrite, index, blockServer.GetSize()));
	}
	
	task.GetTasksWriteFile()[fileServer.GetPath()] = taskWrite;
}
