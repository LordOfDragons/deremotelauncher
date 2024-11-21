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

#include "derlBaseTaskProcessor.h"
#include "../derlFile.h"
#include "../derlFileBlock.h"
#include "../derlFileLayout.h"
#include "../hashing/sha256.h"


// Class derlBaseTaskProcessor
////////////////////////////////

derlBaseTaskProcessor::derlBaseTaskProcessor() :
pExit(false),
pNoTaskDelay(1000),
pNoTaskTimeout(3000),
pFileHashReadSize(1024L * 8L),
pLogClassName("derlBaseTaskProcessor"),
pEnableDebugLog(false){
}

// Management
///////////////

void derlBaseTaskProcessor::SetBaseDirectory(const std::filesystem::path &path){
	pBaseDir = path;
}

void derlBaseTaskProcessor::SetPartSize(uint64_t size){
	pPartSize = size;
}

void derlBaseTaskProcessor::Exit(){
	const std::lock_guard guard(pMutex);
	pExit = true;
}

void derlBaseTaskProcessor::Run(){
	pNoTaskTimeoutBegin = std::chrono::steady_clock::now();
	
	while(true){
		{
		const std::lock_guard guard(pMutex);
		if(pExit){
			break;
		}
		}
		
		if(RunTask()){
			pNoTaskTimeoutBegin = std::chrono::steady_clock::now();
			continue;
		}
		
		if(std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - pNoTaskTimeoutBegin) > pNoTaskTimeout){
			std::this_thread::sleep_for(pNoTaskDelay);
		}
	}
}

void derlBaseTaskProcessor::CalcFileLayout(derlFileLayout &layout, const std::string &pathDir){
	//LogDebug("CalcFileLayout", pathDir);
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

void derlBaseTaskProcessor::ListDirectoryFiles(ListDirEntries &entries, const std::string &pathDir){
	//LogDebug("ListDirectoryFiles", pathDir);
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

void derlBaseTaskProcessor::CalcFileHash(derlFile &file){
	//LogDebug("CalcFileHash", file.GetPath());
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

void derlBaseTaskProcessor::CalcFileBlockHashes(derlFileBlock::List &blocks,
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

void derlBaseTaskProcessor::TruncateFile(const std::string &path){
	CloseFile();
	pFilePath = pBaseDir / path;
	
	try{
		if(pFilePath.has_parent_path()){
			std::filesystem::create_directories(pFilePath.parent_path());
		}
		
		pFileStream.open(pFilePath, pFileStream.binary | pFileStream.out | pFileStream.trunc);
		if(pFileStream.fail()){
			throw std::runtime_error(std::strerror(errno));
		}
		
	}catch(const std::exception &e){
		LogException("TruncateFile", e, path);
		throw;
		
	}catch(...){
		Log(denLogger::LogSeverity::error, "TruncateFile", path);
		throw;
	}
	
	CloseFile();
}

void derlBaseTaskProcessor::OpenFile(const std::string &path, bool write){
	CloseFile();
	pFilePath = pBaseDir / path;
	
	try{
		if(write && pFilePath.has_parent_path()){
			std::filesystem::create_directories(pFilePath.parent_path());
		}
		
		std::ios_base::openmode openmode = pFileStream.binary;
		if(write){
			// std streams are stupid as hell. if you use std::ios_base::app then the data is
			// always appended and never overwritten which is not helping at all.
			//
			// if you use just std::ios_base::out then the file is destroyed before writing
			// which is again not helping at all.
			//
			// so the only working solution is to use std::ios_base::in together with
			// std::ios_base::out which is stupid. we want to overwrite the file not reading
			// from it. but anyways... at last it works
			openmode |= pFileStream.in | pFileStream.out;
			
		}else{
			openmode |= pFileStream.in;
		}
		
		pFileStream.open(pFilePath, openmode);
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

uint64_t derlBaseTaskProcessor::GetFileSize(){
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

void derlBaseTaskProcessor::ReadFile(void *data, uint64_t offset, uint64_t size){
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

void derlBaseTaskProcessor::CloseFile(){
	pFilePath.clear();
	
	pFileStream.close();
	pFileStream.clear();
}

void derlBaseTaskProcessor::SetLogClassName(const std::string &name){
	pLogClassName = name;
}

void derlBaseTaskProcessor::SetLogger(const denLogger::Ref &logger){
	pLogger = logger;
}

void derlBaseTaskProcessor::LogException(const std::string &functionName,
const std::exception &exception, const std::string &message){
	std::stringstream ss;
	ss << message << ": " << exception.what();
	Log(denLogger::LogSeverity::error, functionName, ss.str());
}

void derlBaseTaskProcessor::Log(denLogger::LogSeverity severity,
const std::string &functionName, const std::string &message){
	if(!pLogger){
		return;
	}
	
	std::stringstream ss;
	ss << "[" << pLogClassName << "::" << functionName << "] " << message;
	pLogger->Log(severity, ss.str());
}

void derlBaseTaskProcessor::LogDebug(const std::string &functionName, const std::string &message){
	if(pEnableDebugLog){
		Log(denLogger::LogSeverity::debug, functionName, message);
	}
}
