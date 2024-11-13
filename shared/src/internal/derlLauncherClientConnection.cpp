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

#include <algorithm>
#include <sstream>

#include "derlLauncherClientConnection.h"
#include "../derlLauncherClient.h"
#include "../derlProtocol.h"
#include "../derlRunParameters.h"

#include <denetwork/message/denMessage.h>
#include <denetwork/message/denMessageReader.h>
#include <denetwork/message/denMessageWriter.h>
#include <denetwork/value/denValueInteger.h>


#define DERL_MAX_UINT_SIZE 0xffffffff
#define DERL_MAX_INT ((int)0x7fffffff)


// Class derlLauncherClientConnection
///////////////////////////////////////

derlLauncherClientConnection::derlLauncherClientConnection(derlLauncherClient &client) :
pClient(client),
pConnectionAccepted(false),
pEnabledFeatures(0),
pStateRun(std::make_shared<denState>(false)),
pValueRunStatus(std::make_shared<denValueInt>(denValueIntegerFormat::uint8)),
pPendingRequestLayout(false)
{
	pValueRunStatus->SetValue((uint64_t)derlProtocol::RunStateStatus::stopped);
	pStateRun->AddValue(pValueRunStatus);
}

derlLauncherClientConnection::~derlLauncherClientConnection(){
}


// Management
///////////////

void derlLauncherClientConnection::SetRunStatus(RunStatus status){
	switch(status){
	case RunStatus::running:
		pValueRunStatus->SetValue((uint64_t)derlProtocol::RunStateStatus::running);
		break;
		
	case RunStatus::stopped:
	default:
		pValueRunStatus->SetValue((uint64_t)derlProtocol::RunStateStatus::stopped);
	}
}

void derlLauncherClientConnection::SetLogger(const denLogger::Ref &logger){
	denConnection::SetLogger(logger);
	pStateRun->SetLogger(logger);
}



void derlLauncherClientConnection::ConnectionEstablished(){
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		uint32_t supportedFeatures = 0;
		
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::connectRequest);
		writer.Write(derlProtocol::signatureClient, 16);
		writer.WriteUInt(supportedFeatures);
		writer.WriteString8(pClient.GetName());
	}
	SendReliableMessage(message);
}

void derlLauncherClientConnection::ConnectionFailed(ConnectionFailedReason reason){
	pConnectionAccepted = false;
	pClient.OnConnectionFailed(reason);
}

void derlLauncherClientConnection::ConnectionClosed(){
	pConnectionAccepted = false;
	pClient.OnConnectionClosed();
}

void derlLauncherClientConnection::MessageProgress(size_t bytesReceived){
}

void derlLauncherClientConnection::MessageReceived(const denMessage::Ref &message){
	denMessageReader reader(message->Item());
	const derlProtocol::MessageCodes code = (derlProtocol::MessageCodes)reader.ReadByte();
	
	if(!pConnectionAccepted){
		if(code == derlProtocol::MessageCodes::connectAccepted){
			std::string signature(16, 0);
			reader.Read((void*)signature.c_str(), 16);
			if(signature != derlProtocol::signatureServer){
				GetLogger()->Log(denLogger::LogSeverity::error,
					"Server answered with wrong signature, disconnecting.");
				Disconnect();
				return;
			}
			
			pEnabledFeatures = reader.ReadUInt();
			pConnectionAccepted = true;
			pClient.OnConnectionEstablished();
		}
		return;
	}
	
	switch(code){
	case derlProtocol::MessageCodes::requestFileLayout:
		pProcessRequestLayout();
		break;
		
	case derlProtocol::MessageCodes::requestFileBlockHashes:
		pProcessRequestFileBlockHashes(reader);
		break;
		
	case derlProtocol::MessageCodes::requestDeleteFiles:
		pProcessRequestDeleteFiles(reader);
		break;
		
	case derlProtocol::MessageCodes::requestWriteFiles:
		pProcessRequestWriteFiles(reader);
		break;
		
	case derlProtocol::MessageCodes::sendFileData:
		pProcessSendFileData(reader);
		break;
		
	case derlProtocol::MessageCodes::requestFinishWriteFiles:
		pProcessRequestFinishWriteFiles(reader);
		break;
		
	case derlProtocol::MessageCodes::startApplication:
		pProcessStartApplication(reader);
		break;
		
	case derlProtocol::MessageCodes::stopApplication:
		pProcessStopApplication(reader);
		break;
		
	default:
		break; // ignore all other messages
	}
}

denState::Ref derlLauncherClientConnection::CreateState(const denMessage::Ref &message, bool readOnly){
	if(!pConnectionAccepted){
		return nullptr;
	}
	
	denMessageReader reader(message->Item());
	const derlProtocol::LinkCodes code = (derlProtocol::LinkCodes)reader.ReadByte();
	
	switch(code){
	case derlProtocol::LinkCodes::runState:
		return pStateRun;
		
	default:
		return nullptr; // ignore all other messages
	}
}

void derlLauncherClientConnection::FinishPendingOperations(){
	const std::lock_guard guard(pClient.GetMutex());
	
	{
	const derlTaskFileLayout::Ref &task = pClient.GetTaskFileLayout();
	if(task){
		switch(task->GetStatus()){
		case derlTaskFileLayout::Status::success:
			pClient.SetFileLayout(task->GetLayout());
			pClient.SetTaskFileLayout(nullptr);
			break;
			
		case derlTaskFileLayout::Status::failure:
			pClient.SetFileLayout(nullptr);
			pClient.SetTaskFileLayout(nullptr);
			
			if(pPendingRequestLayout){
				pClient.SetTaskFileLayout(std::make_shared<derlTaskFileLayout>());
			}
			break;
			
		default:
			break;
		}
	}
	}
	
	if(!pClient.GetFileLayout()){
		return;
	}
	
	if(pPendingRequestLayout && pClient.GetFileLayout()){
		pPendingRequestLayout = false;
		if(GetConnected()){
			pSendResponseFileLayout(*pClient.GetFileLayout());
		}
	}
	
	{
	derlTaskFileBlockHashes::Map &tasks = pClient.GetTasksFileBlockHashes();
	if(!tasks.empty()){
		typedef std::vector<std::string> ListPath;
		ListPath finished;
		derlTaskFileBlockHashes::Map::const_iterator iter;
		for(iter = tasks.cbegin(); iter != tasks.cend(); iter++){
			switch(iter->second->GetStatus()){
			case derlTaskFileBlockHashes::Status::success:
			case derlTaskFileBlockHashes::Status::failure:
				finished.push_back(iter->second->GetPath());
				if(GetConnected()){
					pFinishFileBlockHashes(*iter->second);
				}
				break;
				
			default:
				break;
			}
		}
		
		ListPath::const_iterator iterRemove;
		for(iterRemove = finished.cbegin(); iterRemove != finished.cend(); iterRemove++){
			const derlTaskFileBlockHashes::Map::iterator iterErase = tasks.find(*iterRemove);
			if(iterErase != tasks.end()){
				tasks.erase(iterErase);
			}
		}
	}
	}
	
	{
	derlTaskFileWrite::Map &tasks = pClient.GetTasksWriteFile();
	if(!tasks.empty()){
		derlTaskFileWrite::Map::const_iterator citer;
		for(citer = tasks.cbegin(); citer != tasks.cend(); citer++){
			if(citer->second->GetStatus() != derlTaskFileWrite::Status::pending){
				continue;
			}
			
			derlTaskFileWriteBlock::List &blocks = citer->second->GetBlocks();
			derlTaskFileWriteBlock::List::const_iterator citerBlock;
			derlTaskFileWriteBlock::List finished;
			for(citerBlock = blocks.cbegin(); citerBlock != blocks.cend(); citerBlock++){
				switch((*citerBlock)->GetStatus()){
				case derlTaskFileWriteBlock::Status::success:
				case derlTaskFileWriteBlock::Status::failure:
					finished.push_back(*citerBlock);
					break;
					
				default:
					break;
				}
			}
			
			if(!finished.empty()){
				derlTaskFileWriteBlock::List::const_iterator iter;
				for(iter = finished.cbegin(); iter != finished.cend(); iter++){
					derlTaskFileWriteBlock::List::iterator iterErase(
						std::find(blocks.begin(), blocks.end(), *iter));
					if(iterErase != blocks.end()){
						blocks.erase(iterErase);
					}
				}
				
				if(GetConnected()){
					pSendResponseWriteFiles(citer->first, finished);
				}
			}
		}
	}
	}
	
	{
	derlTaskFileDelete::Map &tasks = pClient.GetTasksDeleteFile();
	if(!tasks.empty()){
		derlTaskFileDelete::Map::const_iterator iter;
		derlTaskFileDelete::List finished;
		for(iter = tasks.cbegin(); iter != tasks.cend(); iter++){
			switch(iter->second->GetStatus()){
			case derlTaskFileDelete::Status::success:
			case derlTaskFileDelete::Status::failure:
				finished.push_back(iter->second);
				break;
				
			default:
				break;
			}
		}
		
		if(!finished.empty()){
			derlTaskFileDelete::List::const_iterator iterFinished;
			for(iterFinished = finished.cbegin(); iterFinished != finished.cend(); iterFinished++){
				derlTaskFileDelete::Map::iterator iterErase(tasks.find((*iterFinished)->GetPath()));
				if(iterErase != tasks.end()){
					tasks.erase(iterErase);
				}
			}
			
			if(GetConnected()){
				pSendResponseDeleteFiles(finished);
			}
		}
	}
	}
}

// Private Functions
//////////////////////

void derlLauncherClientConnection::pFinishFileBlockHashes(derlTaskFileBlockHashes &task){
	if(task.GetStatus() != derlTaskFileBlockHashes::Status::success){
		pSendResponseFileBlockHashes(task.GetPath());
		return;
	}
	
	if(!pClient.GetFileLayout()){
		std::stringstream log;
		log << "Block hashes for file requested but file layout is not present: "
			<< task.GetPath() << ". Answering with empty file.";
		GetLogger()->Log(denLogger::LogSeverity::warning, log.str());
		pSendResponseFileBlockHashes(task.GetPath());
		return;
	}
	
	const derlFile::Ref file(pClient.GetFileLayout()->GetFileAt(task.GetPath()));
	if(!file){
		std::stringstream log;
		log << "Block hashes for non-existing file requested: "
			<< task.GetPath() << ". Answering with empty file.";
		GetLogger()->Log(denLogger::LogSeverity::warning, log.str());
		pSendResponseFileBlockHashes(task.GetPath());
		return;
	}
	
	pSendResponseFileBlockHashes(*file);
}

void derlLauncherClientConnection::pProcessRequestLayout(){
	if(pClient.GetFileLayout()){
		pPendingRequestLayout = false;
		pSendResponseFileLayout(*pClient.GetFileLayout());
		
	}else{
		pPendingRequestLayout = true;
		if(!pClient.GetTaskFileLayout()){
			pClient.SetTaskFileLayout(std::make_shared<derlTaskFileLayout>());
		}
	}
}

void derlLauncherClientConnection::pProcessRequestFileBlockHashes(denMessageReader &reader){
	const std::string path(reader.ReadString16());
	const uint32_t blockSize = reader.ReadUInt();
	
	if(!pClient.GetFileLayout()){
		std::stringstream log;
		log << "Block hashes for file requested but file layout is not present: "
			<< path << ". Answering with empty file.";
		GetLogger()->Log(denLogger::LogSeverity::warning, log.str());
		pSendResponseFileBlockHashes(path);
		return;
	}
	
	const derlFile::Ref file(pClient.GetFileLayout()->GetFileAt(path));
	if(!file){
		std::stringstream log;
		log << "Block hashes for non-existing file requested: "
			<< path << ". Answering with empty file.";
		GetLogger()->Log(denLogger::LogSeverity::warning, log.str());
		pSendResponseFileBlockHashes(path);
		return;
	}
	
	if(!file->GetHasBlocks() || file->GetBlockSize() != blockSize){
		file->RemoveAllBlocks();
		file->SetBlockSize(blockSize);
		
		const std::lock_guard guard(pClient.GetMutex());
		pClient.GetTasksFileBlockHashes()[path] =
			std::make_shared<derlTaskFileBlockHashes>(path, blockSize);
		return;
	}
	
	pSendResponseFileBlockHashes(*file);
}

void derlLauncherClientConnection::pProcessRequestDeleteFiles(denMessageReader &reader){
	const std::lock_guard guard(pClient.GetMutex());
	derlTaskFileDelete::Map &tasks = pClient.GetTasksDeleteFile();
	const int count = reader.ReadUInt();
	int i;
	
	for(i=0; i<count; i++){
		const std::string path(reader.ReadString16());
		tasks[path] = std::make_shared<derlTaskFileDelete>(path);
	}
}

void derlLauncherClientConnection::pProcessRequestWriteFiles(denMessageReader &reader){
	const std::lock_guard guard(pClient.GetMutex());
	derlTaskFileWrite::Map &tasks = pClient.GetTasksWriteFile();
	const int count = reader.ReadUInt();
	int i;
	
	for(i=0; i<count; i++){
		const std::string path(reader.ReadString16());
		const derlTaskFileWrite::Ref task(std::make_shared<derlTaskFileWrite>(path));
		task->SetFileSize(reader.ReadULong());
		tasks[path] = task;
	}
}

void derlLauncherClientConnection::pProcessSendFileData(denMessageReader &reader){
	const std::string path(reader.ReadString16());
	
	const std::lock_guard guard(pClient.GetMutex());
	const derlTaskFileWrite::Map::const_iterator iter(pClient.GetTasksWriteFile().find(path));
	if(iter == pClient.GetTasksWriteFile().cend()){
		return; // ignore
	}
	
	const uint64_t offset = reader.ReadULong();
	const uint64_t size = (uint64_t)(reader.GetLength() - reader.GetPosition());
	
	std::string data;
	data.assign(size, 0);
	reader.Read((void*)data.c_str(), size);
	
	iter->second->GetBlocks().push_back(std::make_shared<derlTaskFileWriteBlock>(offset, size, data));
}

void derlLauncherClientConnection::pProcessRequestFinishWriteFiles(denMessageReader &reader){
	const std::lock_guard guard(pClient.GetMutex());
	derlTaskFileWrite::Map &tasks = pClient.GetTasksWriteFile();
	derlTaskFileWrite::List finished;
	const int count = reader.ReadUInt();
	int i;
	
	for(i=0; i<count; i++){
		const std::string path(reader.ReadString16());
		const derlTaskFileWrite::Map::iterator iter(tasks.find(path));
		
		if(iter != tasks.end()){
			const derlTaskFileWrite::Ref &task = iter->second;
			if(task->GetStatus() == derlTaskFileWrite::Status::pending){
				if(task->GetBlocks().empty()){
					task->SetStatus(derlTaskFileWrite::Status::success);
					
				}else{
					task->SetStatus(derlTaskFileWrite::Status::failure);
				}
			}
			finished.push_back(task);
			tasks.erase(iter);
			
		}else{
			const derlTaskFileWrite::Ref task(std::make_shared<derlTaskFileWrite>(path));
			task->SetStatus(derlTaskFileWrite::Status::failure);
			finished.push_back(task);
		}
	}
	
	pSendResponseFinishWriteFiles(finished);
}

void derlLauncherClientConnection::pProcessStartApplication(denMessageReader &reader){
	derlRunParameters runParams;
	runParams.SetGameConfig(reader.ReadString16());
	runParams.SetProfileName(reader.ReadString8());
	runParams.SetArguments(reader.ReadString16());
	
	// TODO
}

void derlLauncherClientConnection::pProcessStopApplication(denMessageReader &reader){
	const derlProtocol::StopApplicationMode mode = (derlProtocol::StopApplicationMode)reader.ReadByte();
	(void)mode;
}

void derlLauncherClientConnection::pSendResponseFileLayout(const derlFileLayout &layout){
	if(layout.GetFileCount() > DERL_MAX_INT){
		throw std::runtime_error("Too many files in layout");
	}
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseFileLayout);
		writer.WriteUInt((uint32_t)layout.GetFileCount());
		
		derlFile::Map::const_iterator iter;
		for(iter=layout.GetFilesBegin(); iter!=layout.GetFilesEnd(); iter++){
			const derlFile &file = *iter->second.get();
			writer.WriteString16(file.GetPath());
			writer.WriteULong(file.GetSize());
			writer.WriteString8(file.GetHash());
		}
	}
	SendReliableMessage(message);
}

void derlLauncherClientConnection::pSendResponseFileBlockHashes(
const std::string &path, uint32_t blockSize){
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseFileBlockHashes);
		writer.WriteString16(path);
		writer.WriteUInt(0);
		writer.WriteUInt(blockSize);
	}
	SendReliableMessage(message);
}

void derlLauncherClientConnection::pSendResponseFileBlockHashes(const derlFile &file){
	const int count = file.GetBlockCount();
	if(count > DERL_MAX_INT){
		throw std::runtime_error("Too many blocks in file");
	}
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseFileBlockHashes);
		writer.WriteString16(file.GetPath());
		writer.WriteUInt((uint32_t)count);
		writer.WriteUInt((uint32_t)file.GetBlockSize());
		
		int i;
		for(i=0; i<count; i++){
			writer.WriteString8(file.GetBlockAt(i)->GetHash());
		}
	}
	SendReliableMessage(message);
}

void derlLauncherClientConnection::pSendResponseDeleteFiles(const derlTaskFileDelete::List &tasks){
	if(tasks.size() > DERL_MAX_UINT_SIZE){
		throw std::runtime_error("Too many files");
	}
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseDeleteFiles);
		writer.WriteUInt((uint32_t)tasks.size());
		
		derlTaskFileDelete::List::const_iterator iter;
		for(iter=tasks.cbegin(); iter!=tasks.cend(); iter++){
			const derlTaskFileDelete &task = **iter;
			writer.WriteString16(task.GetPath());
			
			if(task.GetStatus() == derlTaskFileDelete::Status::success){
				writer.WriteByte((uint8_t)derlProtocol::DeleteFileResult::success);
				
			}else{
				writer.WriteByte((uint8_t)derlProtocol::DeleteFileResult::failure);
			}
		}
	}
	SendReliableMessage(message);
}

void derlLauncherClientConnection::pSendResponseWriteFiles(const std::string &path,
const derlTaskFileWriteBlock::List &blocks){
	if(blocks.size() > DERL_MAX_UINT_SIZE){
		throw std::runtime_error("Too many blocks");
	}
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseWriteFiles);
		writer.WriteUInt((uint32_t)blocks.size());
		
		derlTaskFileWriteBlock::List::const_iterator iter;
		for(iter = blocks.cbegin(); iter != blocks.cend(); iter++){
			const derlTaskFileWriteBlock &block = **iter;
			writer.WriteString16(path);
			
			if(block.GetStatus() == derlTaskFileWriteBlock::Status::success){
				writer.WriteByte((uint8_t)derlProtocol::WriteFileResult::success);
				
			}else{
				writer.WriteByte((uint8_t)derlProtocol::WriteFileResult::failure);
			}
		}
	}
	SendReliableMessage(message);
}

void derlLauncherClientConnection::pSendResponseFinishWriteFiles(const derlTaskFileWrite::List &tasks){
	if(tasks.size() > DERL_MAX_UINT_SIZE){
		throw std::runtime_error("Too many files");
	}
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseFinishWriteFiles);
		writer.WriteUInt((uint32_t)tasks.size());
		
		derlTaskFileWrite::List::const_iterator iter;
		for(iter=tasks.cbegin(); iter!=tasks.cend(); iter++){
			const derlTaskFileWrite &task = **iter;
			writer.WriteString16(task.GetPath());
			
			if(task.GetStatus() == derlTaskFileWrite::Status::success){
				writer.WriteByte((uint8_t)derlProtocol::FinishWriteFileResult::success);
				
			}else{
				writer.WriteByte((uint8_t)derlProtocol::FinishWriteFileResult::failure);
			}
		}
	}
	SendReliableMessage(message);
}
