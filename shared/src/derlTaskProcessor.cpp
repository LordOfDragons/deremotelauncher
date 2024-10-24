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

#include "derlTaskProcessor.h"
#include "derlLauncherClient.h"
#include "derlFile.h"
#include "derlFileBlock.h"
#include "derlFileLayout.h"
#include "derlUtils.h"
#include "hashing/sha256.h"


// Class derlTaskProcessor
////////////////////////////

derlTaskProcessor::derlTaskProcessor(derlLauncherClient &client) :
pClient(client),
pNoTaskDelay(500),
pFileHashReadSize(1024L * 8L){
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
	derlTaskFileDelete::Ref taskDeleteFile;
	derlTaskFileWrite::Ref taskWriteFile;
	derlTaskFileWriteBlock::Ref taskWriteFileBlock;
	
	{
	std::lock_guard guard(pClient.GetMutex());
	pBaseDir = pClient.GetPathDataDir();
	
	// TODO check request file layout update
	
	if(pClient.GetFileLayout()){
		FindNextTaskFileBlockHashes(taskFileBlockHashes)
		|| FindNextTaskDelete(taskDeleteFile)
		|| FindNextTaskFileBlock(taskWriteFile, taskWriteFileBlock);
	}
	}
	
	if(taskFileBlockHashes){
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

bool derlTaskProcessor::FindNextTaskFileBlock(
derlTaskFileWrite::Ref &task, derlTaskFileWriteBlock::Ref &block) const{
	const derlTaskFileWrite::Map &tasks = pClient.GetTasksWriteFile();
	derlTaskFileWrite::Map::const_iterator iter;
	for(iter = tasks.cbegin(); iter != tasks.cend(); iter++){
		if(iter->second->GetStatus() != derlTaskFileWrite::Status::pending){
			continue;
		}
		
		const derlTaskFileWriteBlock::List &blocks = iter->second->GetBlocks();
		if(blocks.empty()){
			continue;
		}
		
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
	derlTaskFileBlockHashes::Status status;
	derlFileBlock::List blocks;
	
	try{
		CalcFileBlockHashes(blocks, task.GetPath(), task.GetBlockSize());
		status = derlTaskFileBlockHashes::Status::success;
		
	}catch(...){
		CloseFile();
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
			file->SetBlocks(blocks);
			
		}else{
			std::stringstream message;
			message << "File not found in layout: " << task.GetPath();
			pClient.GetLogger()->Log(denLogger::LogSeverity::error, message.str());
			status = derlTaskFileBlockHashes::Status::failure;
		}
	}
	task.SetStatus(status);
}

void derlTaskProcessor::ProcessDeleteFile(derlTaskFileDelete &task){
	derlTaskFileDelete::Status status;
	
	try{
		DeleteFile(task);
		status = derlTaskFileDelete::Status::success;
		
	}catch(...){
		status = derlTaskFileDelete::Status::failure;
	}
	
	std::lock_guard guard(pClient.GetMutex());
	task.SetStatus(status);
}

void derlTaskProcessor::ProcessWriteFileBlock(derlTaskFileWrite &task, derlTaskFileWriteBlock &block){
	derlTaskFileWriteBlock::Status status;
	
	try{
		OpenFile(task.GetPath(), true);
		WriteFile(block.GetData().c_str(), block.GetOffset(), block.GetSize());
		CloseFile();
		status = derlTaskFileWriteBlock::Status::success;
		
	}catch(...){
		CloseFile();
		status = derlTaskFileWriteBlock::Status::failure;
	}
	
	std::lock_guard guard(pClient.GetMutex());
	block.SetStatus(status);
}



derlFileLayout::Ref derlTaskProcessor::CalcFileLayout(){
	const derlFileLayout::Ref layout(std::make_shared<derlFileLayout>());
	CalcFileLayout(*layout, "");
	return layout;
}

void derlTaskProcessor::CalcFileLayout(derlFileLayout &layout, const std::string &pathDir){
	ListDirEntries entries;
	ListDirectoryFiles(entries, pathDir);
	
	ListDirEntries::const_iterator iter;
	for(iter = entries.cbegin(); iter != entries.cend(); iter++){
		if(iter->isDirectory){
			
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
	
	for (const std::filesystem::directory_entry &entry :
	std::filesystem::directory_iterator{pBaseDir / pathDir}){
		const std::filesystem::path fsepath(entry.path());
		if(entry.is_directory()){
			entries.push_back({fspathDir / fsepath.filename(), 0, true});
			
		}else if(entry.is_regular_file()){
			entries.push_back({fspathDir / fsepath.filename(), entry.file_size(), false});
		}
	}
}

void derlTaskProcessor::CalcFileHash(derlFile &file){
	const uint64_t fileSize = file.GetSize();
	if(fileSize == 0){
		return;
	}
	
	const uint64_t blockCount = ((fileSize - 1L) / pFileHashReadSize) + 1L;
	std::string blockData;
	SHA256 hash;
	uint64_t i;
	
	try{
		OpenFile(file.GetPath(), false);
		for(i=0L; i<blockCount; i++){
			const uint64_t blockOffset = pFileHashReadSize * i;
			const uint64_t blockSize = std::min(pFileHashReadSize, fileSize - blockOffset);
			blockData.assign(0, blockSize);
			ReadFile((void*)blockData.c_str(), blockOffset, blockSize);
			hash.add(blockData.c_str(), blockSize);
		}
		CloseFile();
		
	}catch(...){
		CloseFile();
		throw;
	}
	
	file.SetHash(hash.getHash());
}

void derlTaskProcessor::CalcFileBlockHashes(derlFileBlock::List &blocks,
const std::string &path, uint64_t blockSize){
	blocks.clear();
	
	try{
		OpenFile(path, false);
		const uint64_t fileSize = GetFileSize();
		const uint64_t blockCount = ((fileSize - 1L) / blockSize) + 1L;
		uint64_t i;
		
		for(i=0L; i<blockCount; i++){
			const uint64_t nextOffset = blockSize * i;
			const uint64_t nextSize = std::min(blockSize, fileSize - nextOffset);
			
			std::string blockData;
			blockData.assign(0, nextSize);
			ReadFile((void*)blockData.c_str(), nextOffset, nextSize);
			
			const derlFileBlock::Ref block(std::make_shared<derlFileBlock>(nextOffset, nextSize));
			block->SetHash(derlUtils::Sha256(blockData));
			blocks.push_back(block);
		}
		
		CloseFile();
		
	}catch(...){
		CloseFile();
		throw;
	}
}

void derlTaskProcessor::DeleteFile(const derlTaskFileDelete &task){
	std::filesystem::remove(pBaseDir / task.GetPath());
}

void derlTaskProcessor::OpenFile(const std::string &path, bool write){
	CloseFile();
	pFilePath = pBaseDir / path;
	pFileStream.open(pFilePath, pFileStream.binary | (write ? pFileStream.out : pFileStream.in));
}

uint64_t derlTaskProcessor::GetFileSize(){
	pFileStream.seekg(0, pFileStream.end);
	const uint64_t size = pFileStream.tellg();
	
	if(pFileStream.fail()){
		pFileStream.clear();
		
		std::stringstream message;
		message << "Failed getting file size: " << pFilePath;
		throw std::runtime_error(message.str());
	}
	
	return size;
}

void derlTaskProcessor::ReadFile(void *data, uint64_t offset, uint64_t size){
	pFileStream.seekg(offset, pFileStream.beg);
	
	if(pFileStream.good()){
		pFileStream.read((char*)data, size);
	}
	
	if(pFileStream.fail()){
		pFileStream.clear();
		
		std::stringstream message;
		message << "Failed reading from file: " << pFilePath;
		throw std::runtime_error(message.str());
	}
}

void derlTaskProcessor::WriteFile(const void *data, uint64_t offset, uint64_t size)
{
	pFileStream.seekp(offset, pFileStream.beg);
	pFileStream.write((const char*)data, size);
}

void derlTaskProcessor::CloseFile(){
	pFilePath.clear();
	
	pFileStream.close();
	pFileStream.clear();
}
