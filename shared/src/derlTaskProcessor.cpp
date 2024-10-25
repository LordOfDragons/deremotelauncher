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
#include <thread>
#include <chrono>
#include <cstring>

#include "derlTaskProcessor.h"
#include "derlLauncherClient.h"
#include "derlFile.h"
#include "derlFileBlock.h"
#include "derlFileLayout.h"
#include "hashing/sha256.h"


// Class derlTaskProcessor
////////////////////////////

derlTaskProcessor::derlTaskProcessor(derlLauncherClient &client) :
pClient(client),
pNoTaskDelay(500),
pFileHashReadSize(1024L * 8L),
pLogClassName("derlTaskProcessor"){
}

derlTaskProcessor::~derlTaskProcessor(){
}

// Management
///////////////

void derlTaskProcessor::SetBaseDirectory(const std::filesystem::path &path){
	pBaseDir = path;
}

void derlTaskProcessor::Exit(){
	pExit = true;
}

void derlTaskProcessor::Run(){
	while(!pExit){
		if(!RunTask()){
			std::this_thread::sleep_for(pNoTaskDelay);
		}
	}
}

bool derlTaskProcessor::RunTask(){
	derlTaskFileBlockHashes::Ref taskFileBlockHashes;
	derlTaskFileWriteBlock::Ref taskWriteFileBlock;
	derlTaskFileLayout::Ref taskFileLayout;
	derlTaskFileDelete::Ref taskDeleteFile;
	derlTaskFileWrite::Ref taskWriteFile;
	
	{
	std::lock_guard guard(pClient.GetMutex());
	pBaseDir = pClient.GetPathDataDir();
	
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

bool derlTaskProcessor::FindNextTaskFileLayout(derlTaskFileLayout::Ref &task) const{
	const derlTaskFileLayout::Ref checkTask(pClient.GetTaskFileLayout());
	if(checkTask && checkTask->GetStatus() == derlTaskFileLayout::Status::pending){
		task = checkTask;
		return true;
	}
	return false;
}

bool derlTaskProcessor::FindNextTaskFileBlockHashes(derlTaskFileBlockHashes::Ref &task) const{
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

bool derlTaskProcessor::FindNextTaskDelete(derlTaskFileDelete::Ref &task) const{
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

bool derlTaskProcessor::FindNextTaskWriteFileBlock(
derlTaskFileWrite::Ref &task, derlTaskFileWriteBlock::Ref &block) const{
	const derlTaskFileWrite::Map &tasks = pClient.GetTasksWriteFile();
	derlTaskFileWrite::Map::const_iterator iter;
	for(iter = tasks.cbegin(); iter != tasks.cend(); iter++){
		if(iter->second->GetStatus() != derlTaskFileWrite::Status::pending){
			continue;
		}
		
		const derlTaskFileWriteBlock::List &blocks = iter->second->GetBlocks();
		derlTaskFileWriteBlock::List::const_iterator iterBlock;
		for(iterBlock = blocks.cbegin(); iterBlock != blocks.cend(); iterBlock++){
			if((*iterBlock)->GetStatus() != derlTaskFileWriteBlock::Status::pending){
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



void derlTaskProcessor::ProcessFileBlockHashes(derlTaskFileBlockHashes &task){
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

void derlTaskProcessor::ProcessFileLayout(derlTaskFileLayout &task){
	if(pEnableDebugLog){
		LogDebug("ProcessFileLayout", "Build file layout");
	}
	
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

void derlTaskProcessor::ProcessDeleteFile(derlTaskFileDelete &task){
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

void derlTaskProcessor::ProcessWriteFileBlock(derlTaskFileWrite &task, derlTaskFileWriteBlock &block){
	if(pEnableDebugLog){
		std::stringstream ss;
		ss << "Write block size " << block.GetSize() << " at " << block.GetOffset() << " to " << task.GetPath();
		LogDebug("ProcessWriteFileBlock", ss.str());
	}
	
	derlTaskFileWriteBlock::Status status;
	
	try{
		OpenFile(task.GetPath(), true);
		WriteFile(block.GetData().c_str(), block.GetOffset(), block.GetSize());
		CloseFile();
		status = derlTaskFileWriteBlock::Status::success;
		
	}catch(const std::exception &e){
		std::stringstream ss;
		ss << "Failed size " << block.GetSize() << " at " << block.GetOffset() << " to " << task.GetPath();
		LogException("ProcessWriteFileBlock", e, ss.str());
		status = derlTaskFileWriteBlock::Status::failure;
		
	}catch(...){
		std::stringstream ss;
		ss << "Failed size " << block.GetSize() << " at " << block.GetOffset() << " to " << task.GetPath();
		Log(denLogger::LogSeverity::error, "ProcessWriteFileBlock", ss.str());
		CloseFile();
		status = derlTaskFileWriteBlock::Status::failure;
	}
	
	std::lock_guard guard(pClient.GetMutex());
	block.SetStatus(status);
}



void derlTaskProcessor::CalcFileLayout(derlFileLayout &layout, const std::string &pathDir){
	ListDirEntries entries;
	ListDirectoryFiles(entries, pathDir);
	
	ListDirEntries::const_iterator iter;
	for(iter = entries.cbegin(); iter != entries.cend(); iter++){
		if(iter->isDirectory){
			CalcFileLayout(layout, iter->path);
			
		}else{
			const derlFile::Ref file(std::make_shared<derlFile>(iter->path));
			file->SetSize(iter->fileSize);
			CalcFileHash(*file);
			layout.AddFile(file);
		}
	}
}

void derlTaskProcessor::ListDirectoryFiles(ListDirEntries &entries, const std::string &pathDir){
	const std::filesystem::path fspathDir(pathDir);
	
	std::filesystem::directory_iterator iter{pBaseDir / pathDir};
	for (const std::filesystem::directory_entry &entry : iter){
		const std::string filename(entry.path().filename());
		
		if(entry.is_directory()){
			entries.push_back({filename, fspathDir / filename, 0, true});
			
		}else if(entry.is_regular_file()){
			entries.push_back({filename, fspathDir / filename, entry.file_size(), false});
		}
	}
}

void derlTaskProcessor::CalcFileHash(derlFile &file){
	const uint64_t fileSize = file.GetSize();
	SHA256 hash;
	
	if(fileSize > 0L){
		const uint64_t blockCount = ((fileSize - 1L) / pFileHashReadSize) + 1L;
		std::string blockData;
		uint64_t i;
		
		try{
			OpenFile(file.GetPath(), false);
			for(i=0L; i<blockCount; i++){
				const uint64_t blockOffset = pFileHashReadSize * i;
				const uint64_t blockSize = std::min(pFileHashReadSize, fileSize - blockOffset);
				blockData.assign(blockSize, 0);
				ReadFile((void*)blockData.c_str(), blockOffset, blockSize);
				hash.add(blockData.c_str(), blockSize);
			}
			CloseFile();
			
		}catch(const std::exception &e){
			LogException("CalcFileHash", e, file.GetPath());
			CloseFile();
			throw;
			
		}catch(...){
			Log(denLogger::LogSeverity::error, "CalcFileHash", file.GetPath());
			CloseFile();
			throw;
		}
	}
	
	file.SetHash(hash.getHash());
}

void derlTaskProcessor::CalcFileBlockHashes(derlFileBlock::List &blocks,
const std::string &path, uint64_t blockSize){
	blocks.clear();
	
	try{
		OpenFile(path, false);
		
		const uint64_t fileSize = GetFileSize();
		if(fileSize > 0L){
			const uint64_t blockCount = ((fileSize - 1L) / blockSize) + 1L;
			uint64_t i;
			
			for(i=0L; i<blockCount; i++){
				const uint64_t nextOffset = blockSize * i;
				const uint64_t nextSize = std::min(blockSize, fileSize - nextOffset);
				
				std::string blockData;
				blockData.assign(nextSize, 0);
				ReadFile((void*)blockData.c_str(), nextOffset, nextSize);
				
				const derlFileBlock::Ref block(std::make_shared<derlFileBlock>(nextOffset, nextSize));
				block->SetHash(SHA256()(blockData));
				blocks.push_back(block);
			}
		}
		
		CloseFile();
		
	}catch(const std::exception &e){
		LogException("CalcFileBlockHashes", e, path);
		CloseFile();
		throw;
		
	}catch(...){
		Log(denLogger::LogSeverity::error, "CalcFileBlockHashes", path);
		CloseFile();
		throw;
	}
}

void derlTaskProcessor::DeleteFile(const derlTaskFileDelete &task){
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

void derlTaskProcessor::OpenFile(const std::string &path, bool write){
	CloseFile();
	pFilePath = pBaseDir / path;
	
	try{
		pFileStream.open(pFilePath, pFileStream.binary | (write ? pFileStream.out : pFileStream.in));
		if(pFileStream.fail()){
			throw std::runtime_error(std::strerror(errno));
		}
		
	}catch(const std::exception &e){
		LogException("OpenFile", e, path);
		throw;
		
	}catch(...){
		Log(denLogger::LogSeverity::error, "OpenFile", path);
		throw;
	}
}

uint64_t derlTaskProcessor::GetFileSize(){
	try{
		pFileStream.seekg(0, pFileStream.end);
		const uint64_t size = pFileStream.tellg();
		
		if(pFileStream.fail()){
			pFileStream.clear();
			throw std::runtime_error("Failed getting file size");
		}
		
		return size;
		
	}catch(const std::exception &e){
		LogException("GetFileSize", e, pFilePath);
		throw;
		
	}catch(...){
		Log(denLogger::LogSeverity::error, "GetFileSize", pFilePath);
		throw;
	}
}

void derlTaskProcessor::ReadFile(void *data, uint64_t offset, uint64_t size){
	try{
		pFileStream.seekg(offset, pFileStream.beg);
		if(pFileStream.fail()){
			pFileStream.clear();
			throw std::runtime_error("Failed seeking to offset");
		}
		
		pFileStream.read((char*)data, size);
		if(pFileStream.fail()){
			pFileStream.clear();
			throw std::runtime_error("Failed reading from file");
		}
		
	}catch(const std::exception &e){
		LogException("ReadFile", e, pFilePath);
		throw;
		
	}catch(...){
		Log(denLogger::LogSeverity::error, "ReadFile", pFilePath);
		throw;
	}
}

void derlTaskProcessor::WriteFile(const void *data, uint64_t offset, uint64_t size){
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

void derlTaskProcessor::CloseFile(){
	pFilePath.clear();
	
	pFileStream.close();
	pFileStream.clear();
}



void derlTaskProcessor::SetLogClassName(const std::string &name){
	pLogClassName = name;
}

void derlTaskProcessor::SetLogger(const denLogger::Ref &logger){
	pLogger = logger;
}

void derlTaskProcessor::SetEnableDebugLog(bool enable){
	pEnableDebugLog = enable;
}

void derlTaskProcessor::LogException(const std::string &functionName,
const std::exception &exception, const std::string &message){
	std::stringstream ss;
	ss << message << ": " << exception.what();
	Log(denLogger::LogSeverity::error, functionName, ss.str());
}

void derlTaskProcessor::Log(denLogger::LogSeverity severity,
const std::string &functionName, const std::string &message){
	if(!pLogger){
		return;
	}
	
	std::stringstream ss;
	ss << "[" << pLogClassName << "::" << functionName << "] " << message;
	pLogger->Log(severity, ss.str());
}

void derlTaskProcessor::LogDebug(const std::string &functionName, const std::string &message){
	if(pEnableDebugLog){
		Log(denLogger::LogSeverity::debug, functionName, message);
	}
}
