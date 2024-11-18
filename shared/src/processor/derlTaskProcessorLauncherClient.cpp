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

#include "derlTaskProcessorLauncherClient.h"
#include "../derlLauncherClient.h"
#include "../derlFile.h"
#include "../derlFileBlock.h"
#include "../derlFileLayout.h"


// Class derlTaskProcessorLauncherClient
////////////////////////////

derlTaskProcessorLauncherClient::derlTaskProcessorLauncherClient(derlLauncherClient &client) :
pClient(client){
	pLogClassName = "derlTaskProcessorLauncherClient";
}

derlTaskProcessorLauncherClient::~derlTaskProcessorLauncherClient(){
}

// Management
///////////////

bool derlTaskProcessorLauncherClient::RunTask(){
	derlTaskFileBlockHashes::Ref taskFileBlockHashes;
	derlTaskFileWriteBlock::Ref taskWriteFileBlock;
	derlTaskFileLayout::Ref taskFileLayout;
	derlTaskFileDelete::Ref taskDeleteFile;
	derlTaskFileWrite::Ref taskWriteFile;
	
	{
	std::lock_guard guard(pClient.GetMutex());
	pBaseDir = pClient.GetPathDataDir();
	pPartSize = pClient.GetPartSize();
	
	FindNextTaskFileLayout(taskFileLayout)
	|| !pClient.GetFileLayout()
	|| FindNextTaskFileBlockHashes(taskFileBlockHashes)
	|| FindNextTaskDelete(taskDeleteFile)
	|| FindNextTaskWriteFileBlock(taskWriteFile, taskWriteFileBlock);
	}
	
	if(taskFileLayout){
		ProcessFileLayout(*taskFileLayout);
		return true;
		
	}else if(taskFileBlockHashes){
		ProcessFileBlockHashes(*taskFileBlockHashes);
		return true;
		
	}else if(taskDeleteFile){
		ProcessDeleteFile(*taskDeleteFile);
		return true;
		
	}else if(taskWriteFile && taskWriteFileBlock){
		ProcessWriteFileBlock(*taskWriteFile, *taskWriteFileBlock);
		return true;
		
	}else{
		return false;
	}
}



// Protected Functions
////////////////////////

bool derlTaskProcessorLauncherClient::FindNextTaskFileLayout(derlTaskFileLayout::Ref &task) const{
	const derlTaskFileLayout::Ref checkTask(pClient.GetTaskFileLayout());
	if(checkTask && checkTask->GetStatus() == derlTaskFileLayout::Status::pending){
		task = checkTask;
		task->SetStatus(derlTaskFileLayout::Status::processing);
		return true;
	}
	return false;
}

bool derlTaskProcessorLauncherClient::FindNextTaskFileBlockHashes(derlTaskFileBlockHashes::Ref &task) const{
	const derlTaskFileBlockHashes::Map &tasks = pClient.GetTasksFileBlockHashes();
	derlTaskFileBlockHashes::Map::const_iterator iter;
	for(iter = tasks.cbegin(); iter != tasks.cend(); iter++){
		if(iter->second->GetStatus() != derlTaskFileBlockHashes::Status::pending){
			continue;
		}
		
		task = iter->second;
		task->SetStatus(derlTaskFileBlockHashes::Status::processing);
		return true;
	}
	
	return false;
}

bool derlTaskProcessorLauncherClient::FindNextTaskDelete(derlTaskFileDelete::Ref &task) const{
	const derlTaskFileDelete::Map &tasks = pClient.GetTasksDeleteFile();
	derlTaskFileDelete::Map::const_iterator iter;
	for(iter = tasks.cbegin(); iter != tasks.cend(); iter++){
		if(iter->second->GetStatus() != derlTaskFileDelete::Status::pending){
			continue;
		}
		
		task = iter->second;
		task->SetStatus(derlTaskFileDelete::Status::processing);
		return true;
	}
	
	return false;
}

bool derlTaskProcessorLauncherClient::FindNextTaskWriteFileBlock(
derlTaskFileWrite::Ref &task, derlTaskFileWriteBlock::Ref &block) const{
	const derlTaskFileWrite::Map &tasks = pClient.GetTasksWriteFile();
	derlTaskFileWrite::Map::const_iterator iter;
	for(iter = tasks.cbegin(); iter != tasks.cend(); iter++){
		if(iter->second->GetStatus() != derlTaskFileWrite::Status::processing){
			continue;
		}
		
		const derlTaskFileWriteBlock::List &blocks = iter->second->GetBlocks();
		derlTaskFileWriteBlock::List::const_iterator iterBlock;
		for(iterBlock = blocks.cbegin(); iterBlock != blocks.cend(); iterBlock++){
			if((*iterBlock)->GetStatus() != derlTaskFileWriteBlock::Status::dataReady){
				continue;
			}
			
			task = iter->second;
			block = *iterBlock;
			block->SetStatus(derlTaskFileWriteBlock::Status::processing);
			return true;
		}
	}
	
	return false;
}



void derlTaskProcessorLauncherClient::ProcessFileBlockHashes(derlTaskFileBlockHashes &task){
	if(pEnableDebugLog){
		std::stringstream ss;
		ss << "Calculate block hashes size " << task.GetBlockSize() << " for " << task.GetPath();
		LogDebug("ProcessFileBlockHashes", ss.str());
	}
	
	derlTaskFileBlockHashes::Status status;
	derlFileBlock::List blocks;
	
	try{
		CalcFileBlockHashes(blocks, task.GetPath(), task.GetBlockSize());
		status = derlTaskFileBlockHashes::Status::success;
		
	}catch(const std::exception &e){
		std::stringstream ss;
		ss << "Failed size " << task.GetBlockSize() << " for " << task.GetPath();
		LogException("ProcessFileBlockHashes", e, ss.str());
		status = derlTaskFileBlockHashes::Status::failure;
		
	}catch(...){
		std::stringstream ss;
		ss << "Failed size " << task.GetBlockSize() << " for " << task.GetPath();
		Log(denLogger::LogSeverity::error, "ProcessFileBlockHashes", ss.str());
		status = derlTaskFileBlockHashes::Status::failure;
	}
	
	std::lock_guard guard(pClient.GetMutex());
	if(status == derlTaskFileBlockHashes::Status::success){
		const derlFileLayout::Ref layout(pClient.GetFileLayout());
		derlFile::Ref file;
		
		if(layout){
			file = layout->GetFileAt(task.GetPath());
		}
		
		if(file){
			file->SetBlockSize(task.GetBlockSize());
			file->SetBlocks(blocks);
			
		}else{
			std::stringstream message;
			message << "File not found in layout: " << task.GetPath();
			Log(denLogger::LogSeverity::error, "ProcessFileBlockHashes", message.str());
			status = derlTaskFileBlockHashes::Status::failure;
		}
	}
	task.SetStatus(status);
}

void derlTaskProcessorLauncherClient::ProcessFileLayout(derlTaskFileLayout &task){
	LogDebug("ProcessFileLayout", "Build file layout");
	
	derlTaskFileLayout::Status status;
	derlFileLayout::Ref layout;
	
	try{
		layout = std::make_shared<derlFileLayout>();
		CalcFileLayout(*layout, "");
		status = derlTaskFileLayout::Status::success;
		
	}catch(const std::exception &e){
		LogException("ProcessFileLayout", e, "Failed");
		status = derlTaskFileLayout::Status::failure;
		
	}catch(...){
		Log(denLogger::LogSeverity::error, "ProcessFileLayout", "Failed");
		status = derlTaskFileLayout::Status::failure;
	}
	
	std::lock_guard guard(pClient.GetMutex());
	task.SetStatus(status);
	if(status == derlTaskFileLayout::Status::success){
		task.SetLayout(layout);
	}
}

void derlTaskProcessorLauncherClient::ProcessDeleteFile(derlTaskFileDelete &task){
	if(pEnableDebugLog){
		std::stringstream ss;
		ss << "Delete file " << task.GetPath();
		LogDebug("ProcessDeleteFile", ss.str());
	}
	
	derlTaskFileDelete::Status status;
	
	try{
		DeleteFile(task);
		status = derlTaskFileDelete::Status::success;
		
	}catch(const std::exception &e){
		std::stringstream ss;
		ss << "Failed " << task.GetPath();
		LogException("ProcessDeleteFile", e, ss.str());
		status = derlTaskFileDelete::Status::failure;
		
	}catch(...){
		std::stringstream ss;
		ss << "Failed " << task.GetPath();
		Log(denLogger::LogSeverity::error, "ProcessDeleteFile", ss.str());
		status = derlTaskFileDelete::Status::failure;
	}
	
	std::lock_guard guard(pClient.GetMutex());
	task.SetStatus(status);
}

void derlTaskProcessorLauncherClient::ProcessWriteFileBlock(derlTaskFileWrite &task, derlTaskFileWriteBlock &block){
	if(pEnableDebugLog){
		std::stringstream ss;
		ss << "Write block size " << block.GetSize()
			<< " index " << block.GetIndex() << " path " << task.GetPath();
		LogDebug("ProcessWriteFileBlock", ss.str());
	}
	
	derlTaskFileWriteBlock::Status status;
	
	try{
		OpenFile(task.GetPath(), true);
		WriteFile(block.GetData().c_str(), task.GetBlockSize() * block.GetIndex(), block.GetSize());
		CloseFile();
		status = derlTaskFileWriteBlock::Status::success;
		
	}catch(const std::exception &e){
		std::stringstream ss;
		ss << "Failed size " << block.GetSize()
			<< " index " << block.GetIndex() << " path " << task.GetPath();
		LogException("ProcessWriteFileBlock", e, ss.str());
		CloseFile();
		status = derlTaskFileWriteBlock::Status::failure;
		
	}catch(...){
		std::stringstream ss;
		ss << "Failed size " << block.GetSize()
			<< " index " << block.GetIndex() << " path " << task.GetPath();
		Log(denLogger::LogSeverity::error, "ProcessWriteFileBlock", ss.str());
		CloseFile();
		status = derlTaskFileWriteBlock::Status::failure;
	}
	
	std::lock_guard guard(pClient.GetMutex());
	block.SetStatus(status);
}



void derlTaskProcessorLauncherClient::DeleteFile(const derlTaskFileDelete &task){
	try{
		if(!std::filesystem::remove(pBaseDir / task.GetPath())){
			//throw std::runtime_error("File does not exist");
			// if the file does not exist be fine with it
		}
		
	}catch(const std::exception &e){
		LogException("DeleteFile", e, task.GetPath());
		throw;
		
	}catch(...){
		Log(denLogger::LogSeverity::error, "DeleteFile", task.GetPath());
		throw;
	}
}

void derlTaskProcessorLauncherClient::WriteFile(const void *data, uint64_t offset, uint64_t size){
	try{
		pFileStream.seekp(offset, pFileStream.beg);
		if(pFileStream.fail()){
			pFileStream.clear();
			throw std::runtime_error("Failed seeking to offset");
		}
		
		pFileStream.write((const char*)data, size);
		if(pFileStream.fail()){
			pFileStream.clear();
			throw std::runtime_error("Failed writing to file");
		}
		
	}catch(const std::exception &e){
		LogException("WriteFile", e, pFilePath);
		throw;
		
	}catch(...){
		Log(denLogger::LogSeverity::error, "WriteFile", pFilePath);
		throw;
	}
}
