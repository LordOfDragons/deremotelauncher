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

bool derlTaskProcessorRemoteClient::RunTask(){
	derlTaskFileLayout::Ref taskLayoutServer;
	derlTaskSyncClient::Ref taskSyncClient;
	derlTaskFileWrite::Ref taskFileWrite;
	bool fastReturn = false;
	
	{
	std::lock_guard guard(pClient.GetMutex());
	pBaseDir = pClient.GetPathDataDir();
	fastReturn = pClient.GetTaskSyncClient() != nullptr;
	
	FindNextTaskLayoutServer(taskLayoutServer)
	|| !pClient.GetFileLayoutServer()
	|| !pClient.GetFileLayoutClient()
	|| FindNextTaskSyncClient(taskSyncClient)
	|| FindNextTaskReadFileBlocks(taskFileWrite);
	}
	
	if(taskLayoutServer){
		ProcessFileLayoutServer(*taskLayoutServer);
		return true;
		
	}else if(taskSyncClient){
		ProcessSyncClient(*taskSyncClient);
		return true;
		
	}else if(taskFileWrite){
		ProcessReadFileBlocks(*taskFileWrite);
		return true;
		
	}else{
		return fastReturn;
	}
}

bool derlTaskProcessorRemoteClient::FindNextTaskLayoutServer(derlTaskFileLayout::Ref &task) const{
	const derlTaskFileLayout::Ref checkTask(pClient.GetTaskFileLayoutServer());
	if(checkTask && checkTask->GetStatus() == derlTaskFileLayout::Status::pending){
		task = checkTask;
		task->SetStatus(derlTaskFileLayout::Status::processing);
		return true;
	}
	return false;
}

bool derlTaskProcessorRemoteClient::FindNextTaskSyncClient(derlTaskSyncClient::Ref &task) const{
	const derlTaskSyncClient::Ref checkTask(pClient.GetTaskSyncClient());
	if(checkTask && checkTask->GetStatus() == derlTaskSyncClient::Status::pending){
		task = checkTask;
		task->SetStatus(derlTaskSyncClient::Status::preparing);
		return true;
	}
	return false;
}

bool derlTaskProcessorRemoteClient::FindNextTaskReadFileBlocks(derlTaskFileWrite::Ref &task) const{
	const derlTaskSyncClient::Ref &taskSync = pClient.GetTaskSyncClient();
	if(!taskSync || taskSync->GetStatus() != derlTaskSyncClient::Status::processing){
		return false;
	}
	
	const derlTaskFileWrite::Map &tasksWrite = taskSync->GetTasksWriteFile();
	derlTaskFileWrite::Map::const_iterator iter;
	
	for(iter = tasksWrite.cbegin(); iter != tasksWrite.cend(); iter++){
		if(iter->second->GetStatus() != derlTaskFileWrite::Status::processing){
			continue;
		}
		
		const derlTaskFileWriteBlock::List &blocks = iter->second->GetBlocks();
		derlTaskFileWriteBlock::List::const_iterator iterBlock;
		bool hasBlocksToProcess = false;
		
		for(iterBlock = blocks.cbegin(); iterBlock != blocks.cend(); iterBlock++){
			derlTaskFileWriteBlock &block = **iterBlock;
			if(block.GetStatus() != derlTaskFileWriteBlock::Status::pending){
				continue;
			}
			if(block.GetSize() == 0){
				continue;
			}
			
			block.SetStatus(derlTaskFileWriteBlock::Status::readingData);
			hasBlocksToProcess = true;
		}
		
		{
		std::stringstream ss;
		ss << "CHECK: " << iter->second->GetPath() << " " << iter->second->GetBlocks().size();
		((derlTaskProcessorRemoteClient&)*this).LogDebug("FindNextTaskReadFileBlocks", ss.str());
		}
		
		if(hasBlocksToProcess){
			task = iter->second;
			return true;
		}
	}
	
	return false;
}

void derlTaskProcessorRemoteClient::ProcessFileLayoutServer(derlTaskFileLayout &task){
	LogDebug("ProcessFileLayoutServer", "Build file layout");
	
	derlTaskFileLayout::Status status;
	derlFileLayout::Ref layout;
	std::string syncError;
	
	try{
		layout = std::make_shared<derlFileLayout>();
		CalcFileLayout(*layout, "");
		status = derlTaskFileLayout::Status::success;
		
	}catch(const std::exception &e){
		LogException("ProcessFileLayoutServer", e, "Failed");
		status = derlTaskFileLayout::Status::failure;
		
		std::stringstream ss;
		ss << "Build server file layout failed: " << e.what();
		syncError = ss.str();
		
	}catch(...){
		Log(denLogger::LogSeverity::error, "ProcessFileLayoutServer", "Failed");
		status = derlTaskFileLayout::Status::failure;
		syncError = "Build server file layout failed: unknown error";
	}
	
	std::lock_guard guard(pClient.GetMutex());
	
	switch(status){
	case derlTaskFileLayout::Status::success:
		pClient.SetFileLayoutServer(layout);
		pClient.SetTaskFileLayoutServer(nullptr);
		break;
		
	case derlTaskFileLayout::Status::failure:
		pClient.SetFileLayoutServer(nullptr);
		pClient.SetTaskFileLayoutServer(nullptr);
		
		if(pClient.GetTaskSyncClient()){
			pClient.GetTaskSyncClient()->SetError(syncError);
			pClient.GetTaskSyncClient()->SetStatus(derlTaskSyncClient::Status::failure);
		}
		break;
		
	default:
		break;
	}
}

void derlTaskProcessorRemoteClient::ProcessSyncClient(derlTaskSyncClient &task){
	LogDebug("ProcessSyncClient", "Prepare synchronization");
	
	derlTaskSyncClient::Status status;
	std::string syncError;
	
	try{
		derlFileLayout::Ref layoutServer, layoutClient;
		{
		std::lock_guard guard(pClient.GetMutex());
		layoutServer = pClient.GetFileLayoutServer();
		layoutClient = pClient.GetFileLayoutClient();
		}
		
		if(!layoutServer || !layoutClient){
			return;
		}
		
		AddFileDeleteTasks(task, *layoutServer, *layoutClient);
		AddFileWriteTasks(task, *layoutServer, *layoutClient);
		
		status = derlTaskSyncClient::Status::processing;
		
	}catch(const std::exception &e){
		LogException("ProcessSyncClient", e, "Failed");
		status = derlTaskSyncClient::Status::failure;
		
		std::stringstream ss;
		ss << "Synchronize client failed: " << e.what();
		syncError = ss.str();
		
	}catch(...){
		Log(denLogger::LogSeverity::error, "ProcessSyncClient", "Failed");
		status = derlTaskSyncClient::Status::failure;
		syncError = "Synchronize client failed: unknown error";
	}
	
	std::lock_guard guard(pClient.GetMutex());
	
	task.SetStatus(status);
	if(status == derlTaskSyncClient::Status::failure){
		task.SetError(syncError);
	}
}

void derlTaskProcessorRemoteClient::ProcessReadFileBlocks(derlTaskFileWrite &task){
	if(pEnableDebugLog){
		std::stringstream ss;
		ss << "Read file blocks: " << task.GetPath();
		LogDebug("ProcessReadFileBlocks", ss.str());
	}
	
	const derlTaskFileWriteBlock::List &blocks = task.GetBlocks();
	derlTaskFileWriteBlock::List::const_iterator iterBlock;
	derlTaskFileWriteBlock::List readyBlocks, failedBlocks;
	std::string syncError;
	bool failure = false;
	
	OpenFile(task.GetPath(), true);
	
	for(iterBlock = blocks.cbegin(); iterBlock != blocks.cend(); iterBlock++){
		derlTaskFileWriteBlock &block = **iterBlock;
		if(block.GetStatus() != derlTaskFileWriteBlock::Status::readingData){
			continue;
		}
		
		try{
			std::string data;
			data.assign(block.GetSize(), 0);
			ReadFile((void*)data.c_str(), block.GetOffset(), block.GetSize());
			block.SetData(data);
			readyBlocks.push_back(*iterBlock);
			
		}catch(const std::exception &e){
			failedBlocks.push_back(*iterBlock);
			std::stringstream ss;
			ss << "Failed size " << block.GetSize() << " at " << block.GetOffset() << " to " << task.GetPath();
			LogException("ProcessReadFileBlocks", e, ss.str());
			syncError = ss.str();
			failure = true;
			break;
			
		}catch(...){
			failedBlocks.push_back(*iterBlock);
			std::stringstream ss;
			ss << "Failed size " << block.GetSize() << " at " << block.GetOffset() << " to " << task.GetPath();
			Log(denLogger::LogSeverity::error, "ProcessReadFileBlocks", ss.str());
			CloseFile();
			syncError = ss.str();
			failure = true;
			break;
		}
	}
	
	CloseFile();
	
	std::lock_guard guard(pClient.GetMutex());
	
	for(iterBlock = readyBlocks.cbegin(); iterBlock != readyBlocks.cend(); iterBlock++){
		(*iterBlock)->SetStatus(derlTaskFileWriteBlock::Status::dataReady);
	}
	
	if(failure){
		for(iterBlock = failedBlocks.cbegin(); iterBlock != failedBlocks.cend(); iterBlock++){
			(*iterBlock)->SetStatus(derlTaskFileWriteBlock::Status::failure);
		}
		
		task.SetStatus(derlTaskFileWrite::Status::failure);
		
		const derlTaskSyncClient::Ref &taskSync = pClient.GetTaskSyncClient();
		if(taskSync){
			taskSync->SetError(syncError);
			taskSync->SetStatus(derlTaskSyncClient::Status::failure);
		}
	}
}



// Protected Functions
////////////////////////

void derlTaskProcessorRemoteClient::AddFileDeleteTasks(derlTaskSyncClient &task,
const derlFileLayout &layoutServer, const derlFileLayout &layoutClient){
	derlTaskFileDelete::Map &tasksDelete = task.GetTasksDeleteFile();
	derlFile::Map::const_iterator iter;
	
	for(iter=layoutClient.GetFilesBegin(); iter!=layoutClient.GetFilesEnd(); iter++){
		const std::string &path = iter->second->GetPath();
		if(!layoutServer.GetFileAt(path)){
			tasksDelete[path] = std::make_shared<derlTaskFileDelete>(path);
		}
	}
}

void derlTaskProcessorRemoteClient::AddFileWriteTasks(derlTaskSyncClient &task,
const derlFileLayout &layoutServer, const derlFileLayout &layoutClient){
	derlFile::Map::const_iterator iter;
	derlFile::Ref filePtrClient;
	
	for(iter=layoutServer.GetFilesBegin(); iter!=layoutServer.GetFilesEnd(); iter++){
		const derlFile &fileServer = *iter->second;
		const std::string &path = fileServer.GetPath();
		
		filePtrClient = layoutClient.GetFileAt(path);
		if(filePtrClient){
			const derlFile &fileClient = *filePtrClient;
			
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
	
	derlFileBlock::List::const_iterator iter;
	for(iter=file.GetBlocksBegin(); iter!=file.GetBlocksEnd(); iter++){
		const derlFileBlock &block = **iter;
		taskBlocks.push_back(std::make_shared<derlTaskFileWriteBlock>(block.GetOffset(), block.GetSize()));
	}
	
	task.GetTasksWriteFile()[file.GetPath()] = taskWrite;
}

void derlTaskProcessorRemoteClient::AddFileWriteTaskPartial(derlTaskSyncClient &task,
const derlFile &fileServer, const derlFile &fileClient){
	const derlTaskFileWrite::Ref taskWrite(std::make_shared<derlTaskFileWrite>(fileServer.GetPath()));
	derlTaskFileWriteBlock::List &taskBlocks = taskWrite->GetBlocks();
	taskWrite->SetFileSize(fileServer.GetSize());
	
	derlFileBlock::List::const_iterator iterServer, iterClient;
	for(iterClient=fileClient.GetBlocksBegin(), iterServer=fileServer.GetBlocksBegin();
	iterServer!=fileServer.GetBlocksEnd(); iterClient++, iterServer++){
		const derlFileBlock &blockServer = **iterServer;
		const derlFileBlock &blockClient = **iterClient;
		
		if(blockClient.GetHash() == blockServer.GetHash()
		&& blockClient.GetOffset() == blockServer.GetOffset()
		&& blockClient.GetSize() == blockServer.GetSize()){
			continue;
		}
		
		taskBlocks.push_back(std::make_shared<derlTaskFileWriteBlock>(
			blockServer.GetOffset(), blockServer.GetSize()));
	}
	
	task.GetTasksWriteFile()[fileServer.GetPath()] = taskWrite;
}
