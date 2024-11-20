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
#include "../derlGlobal.h"
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
pPartSize(1357),
pEnableDebugLog(false),
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

void derlLauncherClientConnection::SetEnableDebugLog(bool enable){
	pEnableDebugLog = enable;
}

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
	if(pConnectionAccepted){
		pQueueReceived.Add(message);
		
	}else{
		pMessageReceivedConnect(message->Item());
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

void derlLauncherClientConnection::SendQueuedMessages(){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	denMessage::Ref message;
	while(pQueueSend.Pop(message)){
		SendReliableMessage(message);
	}
}

void derlLauncherClientConnection::ProcessReceivedMessages(){
	derlMessageQueue::Messages messages;
	{
	std::lock_guard guard(derlGlobal::mutexNetwork);
	pQueueReceived.PopAll(messages);
	}
	
	{
	std::unique_lock lockClient(pClient.GetMutex());
	derlMessageQueue::Messages::const_iterator iter;
	
	for(iter=messages.cbegin(); iter!=messages.cend(); iter++){
		denMessageReader reader((*iter)->Item());
		const derlProtocol::MessageCodes code = (derlProtocol::MessageCodes)reader.ReadByte();
		
		switch(code){
		case derlProtocol::MessageCodes::requestFileLayout:
			pProcessRequestLayout(lockClient);
			break;
			
		case derlProtocol::MessageCodes::requestFileBlockHashes:
			pProcessRequestFileBlockHashes(lockClient, reader);
			break;
			
		case derlProtocol::MessageCodes::requestDeleteFile:
			pProcessRequestDeleteFile(reader);
			break;
			
		case derlProtocol::MessageCodes::requestWriteFile:
			pProcessRequestWriteFile(reader);
			break;
			
		case derlProtocol::MessageCodes::sendFileData:
			pProcessSendFileData(reader);
			break;
			
		case derlProtocol::MessageCodes::requestFinishWriteFile:
			pProcessRequestFinishWriteFile(lockClient, reader);
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
	}
	
	{
	std::lock_guard guard(derlGlobal::mutexNetwork);
	messages.clear();
	}
}

void derlLauncherClientConnection::FinishPendingOperations(){
	std::unique_lock lockClient(pClient.GetMutex());
	
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
			lockClient.unlock();
			pSendResponseFileLayout(*pClient.GetFileLayout());
			lockClient.lock();
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
					pFinishFileBlockHashes(lockClient, *iter->second);
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
		derlTaskFileWrite::List tasksSendReady;
		
		for(citer = tasks.cbegin(); citer != tasks.cend(); citer++){
			switch(citer->second->GetStatus()){
			case derlTaskFileWrite::Status::pending:
				tasksSendReady.push_back(citer->second);
				break;
				
			case derlTaskFileWrite::Status::processing:{
				derlTaskFileWriteBlock::List &blocks = citer->second->GetBlocks();
				derlTaskFileWriteBlock::List::const_iterator citerBlock;
				derlTaskFileWriteBlock::List finished, batch;
				
				for(citerBlock = blocks.cbegin(); citerBlock != blocks.cend(); citerBlock++){
					derlTaskFileWriteBlock &taskBlock = **citerBlock;
					
					switch(taskBlock.GetStatus()){
					case derlTaskFileWriteBlock::Status::success:
					case derlTaskFileWriteBlock::Status::failure:
						finished.push_back(*citerBlock);
						break;
						
					case derlTaskFileWriteBlock::Status::readingData:
						if(taskBlock.GetBatchesFinished() > 0){
							taskBlock.SetBatchesFinished(taskBlock.GetBatchesFinished() - 1);
							batch.push_back(*citerBlock);
						}
						break;
						
					default:
						break;
					}
				}
				
				if(!batch.empty() && GetConnected()){
					lockClient.unlock();
					pSendFileDataReceivedBatch(citer->first, batch);
					lockClient.lock();
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
						lockClient.unlock();
						pSendFileDataReceivedFinished(citer->first, finished);
						lockClient.lock();
					}
				}
				}break;
				
			default:
				break;
				continue;
			}
		}
		
		if(!tasksSendReady.empty() && GetConnected()){
			lockClient.unlock();
			pSendResponseWriteFiles(tasksSendReady);
			lockClient.lock();
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
				lockClient.unlock();
				pSendResponsesDeleteFile(finished);
				lockClient.lock();
			}
		}
	}
	}
}

void derlLauncherClientConnection::LogException(const std::string &functionName,
const std::exception &exception, const std::string &message){
	std::stringstream ss;
	ss << message << ": " << exception.what();
	Log(denLogger::LogSeverity::error, functionName, ss.str());
}

void derlLauncherClientConnection::Log(denLogger::LogSeverity severity,
const std::string &functionName, const std::string &message){
	if(!GetLogger()){
		return;
	}
	
	std::stringstream ss;
	ss << "[derlLauncherClientConnection::" << functionName << "] " << message;
	GetLogger()->Log(severity, ss.str());
}

void derlLauncherClientConnection::LogDebug(const std::string &functionName, const std::string &message){
	if(pEnableDebugLog){
		Log(denLogger::LogSeverity::debug, functionName, message);
	}
}

// Private Functions
//////////////////////

void derlLauncherClientConnection::pMessageReceivedConnect(denMessage &message){
	denMessageReader reader(message);
	const derlProtocol::MessageCodes code = (derlProtocol::MessageCodes)reader.ReadByte();
	if(code != derlProtocol::MessageCodes::connectAccepted){
		return;
	}
	
	std::string signature(16, 0);
	reader.Read((void*)signature.c_str(), 16);
	if(signature != derlProtocol::signatureServer){
		Log(denLogger::LogSeverity::error, "MessageReceived",
			"Server answered with wrong signature, disconnecting.");
		Disconnect();
		return;
	}
	
	pEnabledFeatures = reader.ReadUInt();
	pConnectionAccepted = true;
	pClient.OnConnectionEstablished();
}

void derlLauncherClientConnection::pFinishFileBlockHashes(Lock &lockClient,
derlTaskFileBlockHashes &task){
	if(task.GetStatus() != derlTaskFileBlockHashes::Status::success){
		lockClient.unlock();
		pSendResponseFileBlockHashes(task.GetPath());
		lockClient.lock();
		return;
	}
	
	if(!pClient.GetFileLayout()){
		std::stringstream log;
		log << "Block hashes for file requested but file layout is not present: "
			<< task.GetPath() << ". Answering with empty file.";
		Log(denLogger::LogSeverity::warning, "pFinishFileBlockHashes", log.str());
		lockClient.unlock();
		pSendResponseFileBlockHashes(task.GetPath());
		lockClient.lock();
		return;
	}
	
	const derlFile::Ref file(pClient.GetFileLayout()->GetFileAt(task.GetPath()));
	if(!file){
		std::stringstream log;
		log << "Block hashes for non-existing file requested: "
			<< task.GetPath() << ". Answering with empty file.";
		Log(denLogger::LogSeverity::warning, "pFinishFileBlockHashes", log.str());
		lockClient.unlock();
		pSendResponseFileBlockHashes(task.GetPath());
		lockClient.lock();
		return;
	}
	
	lockClient.unlock();
	pSendResponseFileBlockHashes(*file);
	lockClient.lock();
}

void derlLauncherClientConnection::pProcessRequestLayout(Lock &lockClient){
	if(pClient.GetFileLayout()){
		pPendingRequestLayout = false;
		lockClient.unlock();
		pSendResponseFileLayout(*pClient.GetFileLayout());
		lockClient.lock();
		
	}else{
		pPendingRequestLayout = true;
		if(!pClient.GetTaskFileLayout()){
			pClient.SetTaskFileLayout(std::make_shared<derlTaskFileLayout>());
		}
	}
}

void derlLauncherClientConnection::pProcessRequestFileBlockHashes(Lock &lockClient,
denMessageReader &reader){
	const std::string path(reader.ReadString16());
	const uint32_t blockSize = reader.ReadUInt();
	
	if(!pClient.GetFileLayout()){
		std::stringstream log;
		log << "Block hashes for file requested but file layout is not present: "
			<< path << ". Answering with empty file.";
		Log(denLogger::LogSeverity::warning, "pProcessRequestFileBlockHashes", log.str());
		lockClient.unlock();
		pSendResponseFileBlockHashes(path);
		lockClient.lock();
		return;
	}
	
	const derlFile::Ref file(pClient.GetFileLayout()->GetFileAt(path));
	if(!file){
		std::stringstream log;
		log << "Block hashes for non-existing file requested: "
			<< path << ". Answering with empty file.";
		Log(denLogger::LogSeverity::warning, "pProcessRequestFileBlockHashes", log.str());
		lockClient.unlock();
		pSendResponseFileBlockHashes(path);
		lockClient.lock();
		return;
	}
	
	if(!file->GetHasBlocks() || file->GetBlockSize() != blockSize){
		file->RemoveAllBlocks();
		file->SetBlockSize(blockSize);
		
		pClient.GetTasksFileBlockHashes()[path] =
			std::make_shared<derlTaskFileBlockHashes>(path, blockSize);
		return;
	}
	
	lockClient.unlock();
	pSendResponseFileBlockHashes(*file);
	lockClient.lock();
}

void derlLauncherClientConnection::pProcessRequestDeleteFile(denMessageReader &reader){
	derlTaskFileDelete::Map &tasks = pClient.GetTasksDeleteFile();
	
	const std::string path(reader.ReadString16());
	tasks[path] = std::make_shared<derlTaskFileDelete>(path);
}

void derlLauncherClientConnection::pProcessRequestWriteFile(denMessageReader &reader){
	derlTaskFileWrite::Map &tasks = pClient.GetTasksWriteFile();
	
	const std::string path(reader.ReadString16());
	const derlTaskFileWrite::Ref task(std::make_shared<derlTaskFileWrite>(path));
	task->SetFileSize(reader.ReadULong());
	task->SetBlockSize(reader.ReadULong());
	task->SetBlockCount((int)reader.ReadUInt());
	
	tasks[path] = task;
}

void derlLauncherClientConnection::pProcessSendFileData(denMessageReader &reader){
	const std::string path(reader.ReadString16());
	
	const derlTaskFileWrite::Map::const_iterator iter(pClient.GetTasksWriteFile().find(path));
	if(iter == pClient.GetTasksWriteFile().cend()){
		std::stringstream log;
		log << "Send file data received but task does not exist: " << path;
		Log(denLogger::LogSeverity::warning, "pProcessSendFileData", log.str());
		return; // ignore
	}
	
	derlTaskFileWrite &taskWrite = *iter->second;
	const int indexBlock = reader.ReadUInt();
	const int indexPart = reader.ReadUInt();
	const uint8_t flags = reader.ReadByte();
	
	const uint64_t size = (uint64_t)(reader.GetLength() - reader.GetPosition());
	
	derlTaskFileWriteBlock::List &blocks = taskWrite.GetBlocks();
	
	derlTaskFileWriteBlock::List::const_iterator iterBlock(std::find_if(
		blocks.cbegin(), blocks.cend(), [&](const derlTaskFileWriteBlock::Ref &block){
			return block->GetIndex() == indexBlock;
		}));
	
	derlTaskFileWriteBlock::Ref taskBlock;
	if(iterBlock == blocks.cend()){
		if(indexBlock < 0 || indexBlock >= taskWrite.GetBlockCount()){
			std::stringstream log;
			log << "Send file data received but block index is out of range: "
				<< path << " index " << indexBlock << " count " << taskWrite.GetBlockCount();
			Log(denLogger::LogSeverity::warning, "pProcessSendFileData", log.str());
			taskWrite.SetStatus(derlTaskFileWrite::Status::failure);
			return;
		}
		
		const uint64_t blockOffset = taskWrite.GetBlockSize() * indexBlock;
		const uint64_t blockSize = std::min(
			taskWrite.GetBlockSize(), taskWrite.GetFileSize() - blockOffset);
		
		taskBlock = std::make_shared<derlTaskFileWriteBlock>(indexBlock, blockSize);
		taskBlock->GetData().assign(blockSize, 0);
		taskBlock->SetStatus(derlTaskFileWriteBlock::Status::readingData);
		taskBlock->CalcPartCount(pPartSize);
		blocks.push_back(taskBlock);
		
	}else{
		taskBlock = *iterBlock;
	}
	
	const int partCount = taskBlock->GetPartCount();
	if(indexPart < 0 || indexPart >= partCount){
		std::stringstream log;
		log << "Send file data received but part index is out of range: " << path
			<< " block " << indexBlock << " part " << indexPart << " count " << partCount;
		Log(denLogger::LogSeverity::warning, "pProcessSendFileData", log.str());
		taskWrite.SetStatus(derlTaskFileWrite::Status::failure);
		return;
	}
	
	reader.Read(taskBlock->PartDataPointer(pPartSize, indexPart), size);
	
	if((flags & (uint8_t)derlProtocol::SendFileDataFlags::finish) != 0){
		taskBlock->SetStatus(derlTaskFileWriteBlock::Status::dataReady);
		
	}else if((flags & (uint8_t)derlProtocol::SendFileDataFlags::batch) != 0){
		taskBlock->SetBatchesFinished(taskBlock->GetBatchesFinished() + 1);
	}
	
	if(pEnableDebugLog){
		std::stringstream log;
		log << "Send file data received: " << path << " " << indexPart << " " << (int)flags;
		LogDebug("pProcessSendFileData", log.str());
	}
}

void derlLauncherClientConnection::pProcessRequestFinishWriteFile(Lock &lockClient,
denMessageReader &reader){
	derlTaskFileWrite::Map &tasks = pClient.GetTasksWriteFile();
	
	const std::string path(reader.ReadString16());
	const derlTaskFileWrite::Map::iterator iter(tasks.find(path));
	
	if(iter != tasks.end()){
		const derlTaskFileWrite::Ref task(iter->second);
		if(task->GetStatus() == derlTaskFileWrite::Status::processing){
			if(task->GetBlocks().empty()){
				task->SetStatus(derlTaskFileWrite::Status::success);
				
			}else{
				task->SetStatus(derlTaskFileWrite::Status::failure);
			}
		}
		tasks.erase(iter);
		lockClient.unlock();
		pSendResponseFinishWriteFile(task);
		lockClient.lock();
		
	}else{
		const derlTaskFileWrite::Ref task(std::make_shared<derlTaskFileWrite>(path));
		task->SetStatus(derlTaskFileWrite::Status::failure);
		lockClient.unlock();
		pSendResponseFinishWriteFile(task);
		lockClient.lock();
	}
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
	const std::lock_guard guard(derlGlobal::mutexNetwork);

	if(layout.GetFileCount() == 0){
		const denMessage::Ref message(denMessage::Pool().Get());
		{
			denMessageWriter writer(message->Item());
			writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseFileLayout);
			writer.WriteByte((uint8_t)derlProtocol::FileLayoutFlags::finish
				| (uint8_t)derlProtocol::FileLayoutFlags::empty);
			writer.WriteString16("");
			writer.WriteULong(0L);
			writer.WriteString8("");
		}
		pQueueSend.Add(message);
		return;
	}
	
	derlFile::Map::const_iterator iter;
	int count = layout.GetFileCount();
	for(iter=layout.GetFilesBegin(); iter!=layout.GetFilesEnd(); iter++, count--){
		const derlFile &file = *iter->second.get();
		
		const denMessage::Ref message(denMessage::Pool().Get());
		{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseFileLayout);
		
		uint8_t flags = 0;
		if(count == 1){
			flags |= (uint8_t)derlProtocol::FileLayoutFlags::finish;
		}
		writer.WriteByte(flags);
		
		writer.WriteString16(file.GetPath());
		writer.WriteULong(file.GetSize());
		writer.WriteString8(file.GetHash());
		}
		pQueueSend.Add(message);
	}
}

void derlLauncherClientConnection::pSendResponseFileBlockHashes(
const std::string &path, uint32_t blockSize){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseFileBlockHashes);
		writer.WriteString16(path);
		writer.WriteByte((uint8_t)derlProtocol::FileBlockHashesFlags::finish
			| (uint8_t)derlProtocol::FileBlockHashesFlags::empty);
		writer.WriteUInt(0);
		writer.WriteString8("");
	}
	pQueueSend.Add(message);
}

void derlLauncherClientConnection::pSendResponseFileBlockHashes(const derlFile &file){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	const std::string &path = file.GetPath();
	const int count = file.GetBlockCount();
	int i;
	
	if(count == 0){
		const denMessage::Ref message(denMessage::Pool().Get());
		{
			denMessageWriter writer(message->Item());
			writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseFileBlockHashes);
			writer.WriteString16(path);
			writer.WriteByte((uint8_t)derlProtocol::FileBlockHashesFlags::finish
				| (uint8_t)derlProtocol::FileBlockHashesFlags::empty);
			writer.WriteUInt(0);
			writer.WriteString8("");
		}
		pQueueSend.Add(message);
		return;
	}
	
	for(i=0; i<count; i++){
		const denMessage::Ref message(denMessage::Pool().Get());
		{
			denMessageWriter writer(message->Item());
			writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseFileBlockHashes);
			writer.WriteString16(path);
			
			uint8_t flags = 0;
			if(i == count - 1){
				flags |= (uint8_t)derlProtocol::FileBlockHashesFlags::finish;
			}
			writer.WriteByte(flags);
			
			writer.WriteUInt(i);
			writer.WriteString8(file.GetBlockAt(i)->GetHash());
		}
		pQueueSend.Add(message);
	}
}

void derlLauncherClientConnection::pSendResponsesDeleteFile(const derlTaskFileDelete::List &tasks){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	derlTaskFileDelete::List::const_iterator iter;
	
	for(iter=tasks.cbegin(); iter!=tasks.cend(); iter++){
		const derlTaskFileDelete &task = **iter;
		
		const denMessage::Ref message(denMessage::Pool().Get());
		{
			denMessageWriter writer(message->Item());
			writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseDeleteFile);
			writer.WriteString16(task.GetPath());
			
			if(task.GetStatus() == derlTaskFileDelete::Status::success){
				writer.WriteByte((uint8_t)derlProtocol::DeleteFileResult::success);
				
			}else{
				writer.WriteByte((uint8_t)derlProtocol::DeleteFileResult::failure);
			}
		}
		pQueueSend.Add(message);
	}
}

void derlLauncherClientConnection::pSendResponseWriteFiles(const derlTaskFileWrite::List &tasks){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	derlTaskFileWrite::List::const_iterator iter;
	
	for(iter=tasks.cbegin(); iter!=tasks.cend(); iter++){
		derlTaskFileWrite &task = **iter;
		
		const denMessage::Ref message(denMessage::Pool().Get());
		{
			denMessageWriter writer(message->Item());
			writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseWriteFile);
			writer.WriteString16(task.GetPath());
			
			if(task.GetStatus() == derlTaskFileWrite::Status::pending){
				writer.WriteByte((uint8_t)derlProtocol::WriteFileResult::success);
				task.SetStatus(derlTaskFileWrite::Status::processing);
				
			}else{
				writer.WriteByte((uint8_t)derlProtocol::WriteFileResult::failure);
				task.SetStatus(derlTaskFileWrite::Status::failure);
			}
		}
		pQueueSend.Add(message);
	}
}

void derlLauncherClientConnection::pSendFileDataReceivedBatch(
const std::string &path, const derlTaskFileWriteBlock::List &blocks){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	derlTaskFileWriteBlock::List::const_iterator iter;
	
	for(iter = blocks.cbegin(); iter != blocks.cend(); iter++){
		const denMessage::Ref message(denMessage::Pool().Get());
		{
			denMessageWriter writer(message->Item());
			writer.WriteByte((uint8_t)derlProtocol::MessageCodes::fileDataReceived);
			writer.WriteString16(path);
			writer.WriteUInt((uint32_t)(*iter)->GetIndex());
			writer.WriteByte((uint8_t)derlProtocol::FileDataReceivedResult::batch);
		}
		pQueueSend.Add(message);
		
		LogDebug("pSendFileDataReceivedBatch", "Batch finished");
	}
}

void derlLauncherClientConnection::pSendFileDataReceivedFinished(
const std::string &path, const derlTaskFileWriteBlock::List &blocks){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	derlTaskFileWriteBlock::List::const_iterator iter;
	
	for(iter = blocks.cbegin(); iter != blocks.cend(); iter++){
		const denMessage::Ref message(denMessage::Pool().Get());
		{
			denMessageWriter writer(message->Item());
			writer.WriteByte((uint8_t)derlProtocol::MessageCodes::fileDataReceived);
			writer.WriteString16(path);
			writer.WriteUInt((uint32_t)(*iter)->GetIndex());
			
			if((*iter)->GetStatus() == derlTaskFileWriteBlock::Status::success){
				writer.WriteByte((uint8_t)derlProtocol::FileDataReceivedResult::success);
				
			}else{
				writer.WriteByte((uint8_t)derlProtocol::FileDataReceivedResult::failure);
			}
		}
		pQueueSend.Add(message);
		
		LogDebug("pSendFileDataReceivedFinished", "Block finished");
	}
}

void derlLauncherClientConnection::pSendResponseFinishWriteFile(const derlTaskFileWrite::Ref &task){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseFinishWriteFile);
		writer.WriteString16(task->GetPath());
		
		if(task->GetStatus() == derlTaskFileWrite::Status::success){
			writer.WriteByte((uint8_t)derlProtocol::FinishWriteFileResult::success);
			
		}else{
			writer.WriteByte((uint8_t)derlProtocol::FinishWriteFileResult::failure);
		}
	}
	pQueueSend.Add(message);
}
