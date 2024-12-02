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
#include "../internal/derlLauncherClientConnection.h"


// Class derlTaskProcessorLauncherClient
//////////////////////////////////////////

derlTaskProcessorLauncherClient::derlTaskProcessorLauncherClient(derlLauncherClient &client) :
pClient(client)
{
	pLogClassName = "derlTaskProcessorLauncherClient";
}

// Management
///////////////

void derlTaskProcessorLauncherClient::RunTask(){
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
		ProcessFileLayout(*std::static_pointer_cast<derlTaskFileLayout>(task));
		break;
		
	case derlBaseTask::Type::fileBlockHashes:
		ProcessFileBlockHashes(*std::static_pointer_cast<derlTaskFileBlockHashes>(task));
		break;
		
	case derlBaseTask::Type::fileDelete:
		ProcessDeleteFile(*std::static_pointer_cast<derlTaskFileDelete>(task));
		break;
		
	case derlBaseTask::Type::fileWrite:{
		derlTaskFileWrite &taskWrite = *std::static_pointer_cast<derlTaskFileWrite>(task);
		if(taskWrite.GetStatus() == derlTaskFileWrite::Status::pending){
			ProcessWriteFile(taskWrite);
			
		}else{ //derlTaskFileWrite::Status::finishing:
			ProcessFinishWriteFile(taskWrite);
		}
		}break;
		
	case derlBaseTask::Type::fileWriteBlock:
		ProcessWriteFileBlock(*std::static_pointer_cast<derlTaskFileWriteBlock>(task));
		break;
		
	default:
		break;
	}
}

bool derlTaskProcessorLauncherClient::NextPendingTask(derlBaseTask::Ref &task){
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
	
	const derlFileLayout::Ref layout(pClient.GetFileLayout());
	derlBaseTask::Queue::const_iterator iter;
	
	for(iter=tasks.cbegin(); iter!=tasks.cend(); iter++){
		const derlBaseTask::Ref &pendingTask = *iter;
		bool found = false;
		
		switch(pendingTask->GetType()){
		case derlBaseTask::Type::fileLayout:
			found = true;
			break;
			
		case derlBaseTask::Type::fileBlockHashes:
		case derlBaseTask::Type::fileDelete:
		case derlBaseTask::Type::fileWriteBlock:
			found = layout != nullptr;
			break;
			
		case derlBaseTask::Type::fileWrite:
			if(layout){
				switch(std::static_pointer_cast<derlTaskFileWrite>(pendingTask)->GetStatus()){
				case derlTaskFileWrite::Status::pending:
				case derlTaskFileWrite::Status::finishing:
					found = true;
					break;
					
				default:
					break;
				}
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

void derlTaskProcessorLauncherClient::ProcessFileBlockHashes(derlTaskFileBlockHashes &task){
	const uint64_t blockSize = task.GetBlockSize();
	const std::string &path = task.GetPath();
	
	if(pEnableDebugLog){
		std::stringstream ss;
		ss << "Calculate block hashes size " << blockSize << " for " << path;
		LogDebug("ProcessFileBlockHashes", ss.str());
	}
	
	const derlFileLayout::Ref layout(pClient.GetFileLayout());
	
	try{
		if(!layout){
			throw std::runtime_error("Layout missing, internal error");
		}
		
		derlFileBlock::List blocks;
		CalcFileBlockHashes(blocks, path, blockSize);
		
		derlFile::Ref file;
		{
		const std::lock_guard guard(layout->GetMutex());
		file = layout->GetFileAt(path);
		if(!file){
			throw std::runtime_error("File not found in layout");
		}
		
		file = std::make_shared<derlFile>(*file);
		file->SetBlockSize((uint32_t)blockSize);
		file->SetBlocks(blocks);
		layout->SetFileAt(path, file);
		}
		
		task.SetStatus(derlTaskFileBlockHashes::Status::success);
		pClient.GetConnection().SendResponseFileBlockHashes(*file);
		
	}catch(const std::exception &e){
		std::stringstream ss;
		ss << "Failed size " << blockSize << " for " << path;
		LogException("ProcessFileBlockHashes", e, ss.str());
		task.SetStatus(derlTaskFileBlockHashes::Status::failure);
		pClient.GetConnection().SendResponseFileBlockHashes(path);
		
	}catch(...){
		std::stringstream ss;
		ss << "Failed size " << blockSize << " for " << path;
		Log(denLogger::LogSeverity::error, "ProcessFileBlockHashes", ss.str());
		task.SetStatus(derlTaskFileBlockHashes::Status::failure);
		pClient.GetConnection().SendResponseFileBlockHashes(path);
	}
}

void derlTaskProcessorLauncherClient::ProcessFileLayout(derlTaskFileLayout &task){
	LogDebug("ProcessFileLayout", "Build file layout");
	
	try{
		const derlFileLayout::Ref layout(std::make_shared<derlFileLayout>());
		CalcFileLayout(*layout, "");
		task.SetLayout(layout);
		task.SetStatus(derlTaskFileLayout::Status::success);
		pClient.SetFileLayoutSync(layout);
		
	}catch(const std::exception &e){
		LogException("ProcessFileLayout", e, "Failed");
		task.SetStatus(derlTaskFileLayout::Status::failure);
		pClient.SetFileLayoutSync(nullptr);
		
	}catch(...){
		Log(denLogger::LogSeverity::error, "ProcessFileLayout", "Failed");
		task.SetStatus(derlTaskFileLayout::Status::failure);
		pClient.SetFileLayoutSync(nullptr);
	}
}

void derlTaskProcessorLauncherClient::ProcessDeleteFile(derlTaskFileDelete &task){
	const std::string &path = task.GetPath();
	
	if(pEnableDebugLog){
		std::stringstream ss;
		ss << "Delete file " << path;
		LogDebug("ProcessDeleteFile", ss.str());
	}
	
	const derlFileLayout::Ref layout(pClient.GetFileLayout());
	
	try{
		if(!layout){
			throw std::runtime_error("Layout missing, internal error");
		}
		
		DeleteFile(task);
		task.SetStatus(derlTaskFileDelete::Status::success);
		layout->RemoveFileIfPresentSync(path);
		
	}catch(const std::exception &e){
		std::stringstream ss;
		ss << "Failed " << task.GetPath();
		LogException("ProcessDeleteFile", e, ss.str());
		task.SetStatus(derlTaskFileDelete::Status::failure);
		if(layout){
			layout->RemoveFileIfPresentSync(path);
		}
		pClient.SetDirtyFileLayoutSync(true);
		
	}catch(...){
		std::stringstream ss;
		ss << "Failed " << task.GetPath();
		Log(denLogger::LogSeverity::error, "ProcessDeleteFile", ss.str());
		task.SetStatus(derlTaskFileDelete::Status::failure);
		if(layout){
			layout->RemoveFileIfPresentSync(path);
		}
		pClient.SetDirtyFileLayoutSync(true);
	}
	
	pClient.GetConnection().SendResponseDeleteFile(task);
}

void derlTaskProcessorLauncherClient::ProcessWriteFile(derlTaskFileWrite &task){
	if(pEnableDebugLog){
		std::stringstream ss;
		ss << "Write file " << task.GetPath();
		LogDebug("ProcessWriteFile", ss.str());
	}
	
	try{
		if(task.GetTruncate()){
			TruncateFile(task.GetPath());
		}
		task.SetStatus(derlTaskFileWrite::Status::processing);
		
	}catch(const std::exception &e){
		std::stringstream ss;
		ss << "Failed " << task.GetPath();
		LogException("ProcessWriteFile", e, ss.str());
		CloseFile();
		task.SetStatus(derlTaskFileWrite::Status::failure);
		pClient.SetDirtyFileLayoutSync(true);
		
	}catch(...){
		std::stringstream ss;
		ss << "Failed " << task.GetPath();
		Log(denLogger::LogSeverity::error, "ProcessWriteFile", ss.str());
		CloseFile();
		task.SetStatus(derlTaskFileWrite::Status::failure);
		pClient.SetDirtyFileLayoutSync(true);
	}
	
	pClient.GetConnection().SendResponseWriteFile(task);
}

void derlTaskProcessorLauncherClient::ProcessWriteFileBlock(derlTaskFileWriteBlock &task){
	const uint64_t blockSize = task.GetParentTask().GetBlockSize();
	const std::string &path = task.GetParentTask().GetPath();
	
	if(pEnableDebugLog){
		std::stringstream ss;
		ss << "Write block size " << task.GetSize()
			<< " index " << task.GetIndex() << " path " << path;
		LogDebug("ProcessWriteFileBlock", ss.str());
	}
	
	try{
		OpenFile(path, true);
		WriteFile(task.GetData().c_str(), blockSize * task.GetIndex(), task.GetSize());
		CloseFile();
		task.SetStatus(derlTaskFileWriteBlock::Status::success);
		
	}catch(const std::exception &e){
		std::stringstream ss;
		ss << "Failed size " << task.GetSize()
			<< " index " << task.GetIndex() << " path " << path;
		LogException("ProcessWriteFileBlock", e, ss.str());
		CloseFile();
		task.SetStatus(derlTaskFileWriteBlock::Status::failure);
		pClient.SetDirtyFileLayoutSync(true);
		
	}catch(...){
		std::stringstream ss;
		ss << "Failed size " << task.GetSize()
			<< " index " << task.GetIndex() << " path " << path;
		Log(denLogger::LogSeverity::error, "ProcessWriteFileBlock", ss.str());
		CloseFile();
		task.SetStatus(derlTaskFileWriteBlock::Status::failure);
		pClient.SetDirtyFileLayoutSync(true);
	}
	
	pClient.GetConnection().SendFileDataReceived(task);
}

void derlTaskProcessorLauncherClient::ProcessFinishWriteFile(derlTaskFileWrite &task){
	try{
		const derlFile::Ref file(std::make_shared<derlFile>(task.GetPath()));
		file->SetSize(task.GetFileSize());
		file->SetBlockSize((uint32_t)task.GetBlockSize());
		CalcFileHash(*file);
		CloseFile();
		
		if(file->GetHash() == task.GetHash()){
			task.SetStatus(derlTaskFileWrite::Status::success);
			
			const derlFileLayout::Ref layout(pClient.GetFileLayout());
			if(!layout){
				throw std::runtime_error("Layout missing, internal error");
			}
			layout->AddFileSync(file);
			
		}else{
			std::stringstream ss;
			ss << "Finish write failed (hash mismatch) " << task.GetPath();
			Log(denLogger::LogSeverity::error, "ProcessFinishWriteFile", ss.str());
			task.SetStatus(derlTaskFileWrite::Status::validationFailed);
			pClient.SetDirtyFileLayoutSync(true);
		}
		
	}catch(const std::exception &e){
		std::stringstream ss;
		ss << "Finish write failed " << task.GetPath();
		LogException("ProcessFinishWriteFile", e, ss.str());
		CloseFile();
		task.SetStatus(derlTaskFileWrite::Status::failure);
		pClient.SetDirtyFileLayoutSync(true);
		
	}catch(...){
		std::stringstream ss;
		ss << "Finish write failed " << task.GetPath();
		Log(denLogger::LogSeverity::error, "ProcessFinishWriteFile", ss.str());
		CloseFile();
		task.SetStatus(derlTaskFileWrite::Status::failure);
		pClient.SetDirtyFileLayoutSync(true);
	}
	
	pClient.GetConnection().SendResponseFinishWriteFile(task);
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
		LogException("WriteFile", e, pFilePath.string());
		throw;
		
	}catch(...){
		Log(denLogger::LogSeverity::error, "WriteFile", pFilePath.string());
		throw;
	}
}
