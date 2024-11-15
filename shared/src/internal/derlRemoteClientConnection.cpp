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

#include "derlRemoteClientConnection.h"
#include "../derlRemoteClient.h"
#include "../derlProtocol.h"
#include "../derlServer.h"

#include <denetwork/denServer.h>
#include <denetwork/message/denMessage.h>
#include <denetwork/message/denMessageReader.h>
#include <denetwork/message/denMessageWriter.h>
#include <denetwork/value/denValueInteger.h>


#define DERL_MAX_UINT_SIZE 0xffffffff
#define DERL_MAX_INT ((int)0x7fffffff)


// Class derlRemoteClientConnection
/////////////////////////////////////

derlRemoteClientConnection::derlRemoteClientConnection(derlServer &server) :
pServer(server),
pClient(nullptr),
pSupportedFeatures(0),
pEnabledFeatures(0),
pStateRun(std::make_shared<denState>(false)),
pValueRunStatus(std::make_shared<denValueInt>(denValueIntegerFormat::uint8))
{
	pValueRunStatus->SetValue((uint64_t)derlProtocol::RunStateStatus::stopped);
	pStateRun->AddValue(pValueRunStatus);
	SetLogger(server.GetLogger());
}

derlRemoteClientConnection::~derlRemoteClientConnection(){
}


// Management
///////////////

void derlRemoteClientConnection::SetClient(derlRemoteClient *client){
	pClient = client;
}

derlRemoteClientConnection::RunStatus derlRemoteClientConnection::GetRunStatus() const{
	switch((derlProtocol::RunStateStatus)pValueRunStatus->GetValue()){
	case derlProtocol::RunStateStatus::running:
		return RunStatus::running;
		
	case derlProtocol::RunStateStatus::stopped:
	default:
		return RunStatus::stopped;
	}
}

void derlRemoteClientConnection::SetLogger(const denLogger::Ref &logger){
	denConnection::SetLogger(logger);
	pStateRun->SetLogger(logger);
}

void derlRemoteClientConnection::ConnectionClosed(){
	if(!pClient){
		return;
	}
	
	derlRemoteClient::List &clients = pServer.GetClients();
	derlRemoteClient::Ref client;
	
	{
	derlRemoteClient::List::iterator iter;
	for(iter=clients.begin(); iter!=clients.end(); iter++){
		if(iter->get() == pClient){
			client = *iter;
			clients.erase(iter);
			break;
		}
	}
	}
	
	pClient = nullptr;
	client->OnConnectionClosed();
}

void derlRemoteClientConnection::MessageProgress(size_t bytesReceived){
}

void derlRemoteClientConnection::MessageReceived(const denMessage::Ref &message){
	denMessageReader reader(message->Item());
	const derlProtocol::MessageCodes code = (derlProtocol::MessageCodes)reader.ReadByte();
	
	if(!pClient){
		if(!GetParentServer()){
			Log(denLogger::LogSeverity::error, "MessageReceived",
				"Server link missing (internal error), disconnecting.");
			Disconnect();
			return;
		}
		
		if(code != derlProtocol::MessageCodes::connectRequest){
			Log(denLogger::LogSeverity::error, "MessageReceived",
				"Client send request other than ConnectRequest, disconnecting.");
			Disconnect();
			return;
		}
		
		std::string signature(16, 0);
		reader.Read((void*)signature.c_str(), 16);
		if(signature != derlProtocol::signatureClient){
			Log(denLogger::LogSeverity::error, "MessageReceived",
				"Client requested with wrong signature, disconnecting.");
			Disconnect();
			return;
		}
		
		pEnabledFeatures = reader.ReadUInt() & pSupportedFeatures;
		pName = reader.ReadString8();
		
		denMessage::Ref message(denMessage::Pool().Get());
		{
			denMessageWriter writer(message->Item());
			writer.WriteByte((uint8_t)derlProtocol::MessageCodes::connectAccepted);
			writer.Write(derlProtocol::signatureServer, 16);
			writer.WriteUInt(pEnabledFeatures);
		}
		SendReliableMessage(message);
		
		message = denMessage::Pool().Get();
		{
			denMessageWriter writer(message->Item());
			writer.WriteByte((uint8_t)derlProtocol::LinkCodes::runState);
		}
		LinkState(message, pStateRun, false);
		
		const denServer::Connections &connections = GetParentServer()->GetConnections();
		denServer::Connections::const_iterator iter;
		for(iter=connections.cbegin(); iter!=connections.cend(); iter++){
			if(iter->get() == this){
				const derlRemoteClient::Ref client(pServer.CreateClient(
					std::dynamic_pointer_cast<derlRemoteClientConnection>(*iter)));
				pServer.GetClients().push_back(client);
				pClient = client.get();
				pClient->OnConnectionEstablished();
				return;
			}
		}
		
		Log(denLogger::LogSeverity::error, "MessageReceived",
			"Connection not found (internal error), disconnecting.");
		Disconnect();
		return;
	}
	
	switch(code){
	case derlProtocol::MessageCodes::logs:
		pProcessRequestLogs(reader);
		break;
		
	case derlProtocol::MessageCodes::responseFileLayout:
		pProcessResponseFileLayout(reader);
		break;
		
	case derlProtocol::MessageCodes::responseFileBlockHashes:
		pProcessResponseFileBlockHashes(reader);
		break;
		
	case derlProtocol::MessageCodes::responseDeleteFiles:
		pProcessResponseDeleteFiles(reader);
		break;
		
	case derlProtocol::MessageCodes::responseWriteFiles:
		pProcessResponseWriteFiles(reader);
		break;
		
	case derlProtocol::MessageCodes::responseFinishWriteFiles:
		pProcessResponseFinishWriteFiles(reader);
		break;
		
	default:
		break; // ignore all other messages
	}
}

void derlRemoteClientConnection::FinishPendingOperations(){
	if(!pClient){
		return;
	}
	
	const std::lock_guard guard(pClient->GetMutex());
	
	const derlTaskSyncClient::Ref &taskSync = pClient->GetTaskSyncClient();
	if(!taskSync){
		return;
	}
	
	{
	const derlTaskFileLayout::Ref &task = pClient->GetTaskFileLayoutClient();
	if(task && task->GetStatus() == derlTaskFileLayout::Status::pending){
		task->SetStatus(derlTaskFileLayout::Status::processing);
		
		try{
			pSendRequestLayout();
			
		}catch(const std::exception &e){
			LogException("SendRequestLayout", e, "Failed");
			taskSync->SetStatus(derlTaskSyncClient::Status::failure);
			
			std::stringstream ss;
			ss << "Synchronize client failed: " << e.what();
			taskSync->SetError(ss.str());
			return;
			
		}catch(...){
			Log(denLogger::LogSeverity::error, "SendRequestLayout", "Failed");
			taskSync->SetStatus(derlTaskSyncClient::Status::failure);
			taskSync->SetError("Synchronize client failed: unknown error");
			return;
		}
	}
	}
	
	if(taskSync->GetStatus() != derlTaskSyncClient::Status::processing){
		return;
	}
	
	{
	derlTaskFileBlockHashes::Map &tasks = taskSync->GetTasksFileBlockHashes();
	if(!tasks.empty()){
		derlTaskFileBlockHashes::Map::const_iterator iter;
		for(iter = tasks.cbegin(); iter != tasks.cend(); iter++){
			derlTaskFileBlockHashes &task = *iter->second;
			if(task.GetStatus() == derlTaskFileBlockHashes::Status::pending){
				try{
					task.SetStatus(derlTaskFileBlockHashes::Status::processing);
					pSendRequestFileBlockHashes(task.GetPath(), task.GetBlockSize());
					
				}catch(const std::exception &e){
					task.SetStatus(derlTaskFileBlockHashes::Status::failure);
					LogException("SendRequestFileBlockHashes", e, "Failed");
					taskSync->SetStatus(derlTaskSyncClient::Status::failure);
					
					std::stringstream ss;
					ss << "Synchronize client failed: " << e.what();
					taskSync->SetError(ss.str());
					return;
					
				}catch(...){
					task.SetStatus(derlTaskFileBlockHashes::Status::failure);
					Log(denLogger::LogSeverity::error, "SendRequestFileBlockHashes", "Failed");
					taskSync->SetStatus(derlTaskSyncClient::Status::failure);
					taskSync->SetError("Synchronize client failed: unknown error");
					return;
				}
			}
		}
	}
	}
	
	{
	derlTaskFileWrite::Map &tasks = taskSync->GetTasksWriteFile();
	if(!tasks.empty()){
		derlTaskFileWrite::Map::const_iterator iter;
		derlTaskFileWrite::List prepareTasks, finishTasks;
		
		for(iter = tasks.cbegin(); iter != tasks.cend(); iter++){
			const derlTaskFileWrite::Ref &task = iter->second;
			switch(task->GetStatus()){
			case derlTaskFileWrite::Status::pending:
				task->SetStatus(derlTaskFileWrite::Status::preparing);
				prepareTasks.push_back(task);
				break;
				
			case derlTaskFileWrite::Status::processing:{
				const derlTaskFileWriteBlock::List &blocks = task->GetBlocks();
				if(blocks.empty()){
					break;
				}
				
				derlTaskFileWriteBlock::List::const_iterator iterBlock;
				for(iterBlock=blocks.cbegin(); iterBlock!=blocks.cend(); iterBlock++){
					derlTaskFileWriteBlock &block = **iterBlock;
					if(block.GetStatus() == derlTaskFileWriteBlock::Status::dataReady){
						block.SetStatus(derlTaskFileWriteBlock::Status::processing);
						try{
							pSendSendFileData(*task, block);
							
						}catch(const std::exception &e){
							task->SetStatus(derlTaskFileWrite::Status::failure);
							LogException("SendSendFileData", e, "Failed");
							taskSync->SetStatus(derlTaskSyncClient::Status::failure);
							
							std::stringstream ss;
							ss << "Synchronize client failed: " << e.what();
							taskSync->SetError(ss.str());
							return;
							
						}catch(...){
							task->SetStatus(derlTaskFileWrite::Status::failure);
							Log(denLogger::LogSeverity::error, "SendSendFileData", "Failed");
							taskSync->SetStatus(derlTaskSyncClient::Status::failure);
							taskSync->SetError("Synchronize client failed: unknown error");
							return;
						}
					}
				}
				}break;
				
			case derlTaskFileWrite::Status::finishing:{
				const derlTaskFileWriteBlock::List &blocks = task->GetBlocks();
				if(blocks.empty()){
					task->SetStatus(derlTaskFileWrite::Status::finishing);
					finishTasks.push_back(task);
				}
				}break;
				
			default:
				break;
			}
		}
		
		if(!prepareTasks.empty()){
			try{
				pSendRequestWriteFiles(prepareTasks);
				
			}catch(const std::exception &e){
				LogException("SendRequestWriteFiles", e, "Failed");
				taskSync->SetStatus(derlTaskSyncClient::Status::failure);
				
				std::stringstream ss;
				ss << "Synchronize client failed: " << e.what();
				taskSync->SetError(ss.str());
				return;
				
			}catch(...){
				Log(denLogger::LogSeverity::error, "SendRequestWriteFiles", "Failed");
				taskSync->SetStatus(derlTaskSyncClient::Status::failure);
				taskSync->SetError("Synchronize client failed: unknown error");
				return;
			}
		}
		
		if(!finishTasks.empty()){
			try{
				pSendRequestFinishWriteFiles(finishTasks);
				
			}catch(const std::exception &e){
				LogException("SendRequestFinishWriteFiles", e, "Failed");
				taskSync->SetStatus(derlTaskSyncClient::Status::failure);
				
				std::stringstream ss;
				ss << "Synchronize client failed: " << e.what();
				taskSync->SetError(ss.str());
				return;
				
			}catch(...){
				Log(denLogger::LogSeverity::error, "SendRequestFinishWriteFiles", "Failed");
				taskSync->SetStatus(derlTaskSyncClient::Status::failure);
				taskSync->SetError("Synchronize client failed: unknown error");
				return;
			}
		}
	}
	}
	
	if(taskSync->GetStatus() == derlTaskSyncClient::Status::processing
	&& taskSync->GetTasksFileBlockHashes().empty()
	&& taskSync->GetTasksDeleteFile().empty()
	&& taskSync->GetTasksWriteFile().empty()){
		taskSync->SetStatus(derlTaskSyncClient::Status::success);
	}
}

void derlRemoteClientConnection::LogException(const std::string &functionName,
const std::exception &exception, const std::string &message){
	std::stringstream ss;
	ss << message << ": " << exception.what();
	Log(denLogger::LogSeverity::error, functionName, ss.str());
}

void derlRemoteClientConnection::Log(denLogger::LogSeverity severity,
const std::string &functionName, const std::string &message){
	if(!GetLogger()){
		return;
	}
	
	std::stringstream ss;
	ss << "[derlRemoteClientConnection::" << functionName << "] " << message;
	GetLogger()->Log(severity, ss.str());
}

void derlRemoteClientConnection::LogDebug(const std::string &functionName, const std::string &message){
	//if(pEnableDebugLog){
		Log(denLogger::LogSeverity::debug, functionName, message);
	//}
}


// Private Functions
//////////////////////

void derlRemoteClientConnection::pProcessRequestLogs(denMessageReader &reader){
	const int count = (int)reader.ReadUShort();
	int i;
	
	for(i=0; i<count; i++){
		denLogger::LogSeverity severity;
		switch((derlProtocol::LogLevel)reader.ReadByte()){
		case derlProtocol::LogLevel::error:
			severity = denLogger::LogSeverity::error;
			break;
			
		case derlProtocol::LogLevel::warning:
			severity = denLogger::LogSeverity::warning;
			break;
			
		case derlProtocol::LogLevel::info:
		default:
			severity = denLogger::LogSeverity::info;
		}
		
		std::stringstream ss;
		ss << "Log [" << reader.ReadString8() << "]: ";
		ss << reader.ReadString16();
		
		Log(severity, "pProcessRequestLogs", ss.str());
	}
}

void derlRemoteClientConnection::pProcessResponseFileLayout(denMessageReader &reader){
	const derlTaskSyncClient::Ref taskSync(pGetPendingSyncTask("pProcessResponseFileLayout"));
	if(!taskSync){
		return;
	}
	
	const derlTaskFileLayout::Ref &taskLayout = pClient->GetTaskFileLayoutClient();
	if(!taskLayout){
		Log(denLogger::LogSeverity::warning, "pProcessResponseFileLayout",
			"Received ResponseFileLayout but task is absent");
		return;
	}
	
	if(taskLayout->GetStatus() != derlTaskFileLayout::Status::processing){
		Log(denLogger::LogSeverity::warning, "pProcessResponseFileLayout",
			"Received ResponseFileLayout but task is not processing");
		return;
	}
	
	const derlFileLayout::Ref layout(std::make_shared<derlFileLayout>());
	const int count = (int)reader.ReadUInt();
	int i;
	
	for(i=0; i<count; i++){
		const derlFile::Ref file(std::make_shared<derlFile>(reader.ReadString16()));
		file->SetSize(reader.ReadULong());
		file->SetHash(reader.ReadString8());
		layout->AddFile(file);
	}
	
	pClient->SetFileLayoutClient(layout);
	pClient->SetTaskFileLayoutClient(nullptr);
	Log(denLogger::LogSeverity::info, "pProcessResponseFileLayout", "File layout received.");
}

void derlRemoteClientConnection::pProcessResponseFileBlockHashes(denMessageReader &reader){
	const derlTaskSyncClient::Ref taskSync(pGetProcessingSyncTask("pProcessResponseFileBlockHashes"));
	if(!taskSync){
		return;
	}
	
	const std::string path(reader.ReadString16());
	
	{
	derlTaskFileBlockHashes::Map::iterator iterTaskHashes(taskSync->GetTasksFileBlockHashes().find(path));
	if(iterTaskHashes == taskSync->GetTasksFileBlockHashes().end()){
		std::stringstream log;
		log << "Block hashes for file received but task is absent: " << path;
		Log(denLogger::LogSeverity::warning, "pProcessResponseFileBlockHashes", log.str());
		return;
	}
	
	{
	derlTaskFileBlockHashes &taskHashes = *iterTaskHashes->second;
	if(taskHashes.GetStatus() != derlTaskFileBlockHashes::Status::processing){
		std::stringstream log;
		log << "Block hashes for file received but task is not processing: " << path;
		Log(denLogger::LogSeverity::warning, "pProcessResponseFileBlockHashes", log.str());
		return;
	}
	}
	
	taskSync->GetTasksFileBlockHashes().erase(iterTaskHashes);
	}
	
	try{
		if(!pClient->GetFileLayoutClient()){
			std::stringstream ss;
			ss << "Block hashes for file received but file layout is not present: " << path;
			throw std::runtime_error(ss.str());
		}
		
		const derlFile::Ref file(pClient->GetFileLayoutClient()->GetFileAt(path));
		if(!file){
			std::stringstream ss;
			ss << "Block hashes for file received but file does not exist in layout: " << path;
			throw std::runtime_error(ss.str());
		}
		
		const int blockCount = (int)reader.ReadUInt();
		const uint32_t blockSize = reader.ReadUInt();
		
		if(file->GetBlockCount() != blockCount){
			std::stringstream ss;
			ss << "Block hashes for file received but block count does not match: " << path;
			throw std::runtime_error(ss.str());
		}
		
		if(file->GetBlockSize() != blockSize){
			std::stringstream ss;
			ss << "Block hashes for file received but block size does not match: " << path;
			throw std::runtime_error(ss.str());
		}
		
		derlFileBlock::List::const_iterator iterBlock;
		for(iterBlock=file->GetBlocksBegin(); iterBlock!=file->GetBlocksEnd(); iterBlock++){
			(*iterBlock)->SetHash(reader.ReadString8());
		}
		
	}catch(const std::exception &e){
		LogException("ProcessResponseFileBlockHashes", e, "Failed");
		
		std::stringstream ss;
		ss << "Synchronize client failed: " << e.what();
		taskSync->SetError(ss.str());
		taskSync->SetStatus(derlTaskSyncClient::Status::failure);
		return;
		
	}catch(...){
		Log(denLogger::LogSeverity::error, "ProcessResponseFileBlockHashes", "Failed");
		taskSync->SetError("Synchronize client failed: unknown error");
		taskSync->SetStatus(derlTaskSyncClient::Status::failure);
		return;
	}
	
	std::stringstream log;
	log << "Block hashes: " << path;
	Log(denLogger::LogSeverity::info, "ProcessResponseFileBlockHashes", log.str());
}

void derlRemoteClientConnection::pProcessResponseDeleteFiles(denMessageReader &reader){
	const derlTaskSyncClient::Ref taskSync(pGetProcessingSyncTask("pProcessResponseDeleteFiles"));
	if(!taskSync){
		return;
	}
	
	derlTaskFileDelete::Map &tasksDelete = taskSync->GetTasksDeleteFile();
	const int count = (int)reader.ReadUInt();
	int i;
	
	for(i=0; i<count; i++){
		const std::string path(reader.ReadString16());
		const derlProtocol::DeleteFileResult result = (derlProtocol::DeleteFileResult)reader.ReadByte();
		
		{
		derlTaskFileDelete::Map::const_iterator iterDelete(tasksDelete.find(path));
		if(iterDelete == tasksDelete.cend()){
			std::stringstream log;
			log << "Delete file response received with invalid path: " << path;
			Log(denLogger::LogSeverity::warning, "pProcessResponseDeleteFiles", log.str());
			continue;
		}
		
		derlTaskFileDelete &taskDelete = *iterDelete->second;
		if(taskDelete.GetStatus() != derlTaskFileDelete::Status::pending){
			std::stringstream log;
			log << "Delete file response received but it is not pending: " << path;
			Log(denLogger::LogSeverity::warning, "pProcessResponseDeleteFiles", log.str());
			continue;
		}
		
		tasksDelete.erase(iterDelete);
		}
		
		if(result == derlProtocol::DeleteFileResult::success){
			std::stringstream log;
			log << "File deleted: " << path;
			Log(denLogger::LogSeverity::info, "pProcessResponseDeleteFiles", log.str());
			
		}else{
			std::stringstream log;
			log << "Failed deleting file: " << path;
			Log(denLogger::LogSeverity::error, "pProcessResponseDeleteFiles", log.str());
			
			taskSync->SetError(log.str());
			taskSync->SetStatus(derlTaskSyncClient::Status::failure);
		}
	}
}

void derlRemoteClientConnection::pProcessResponseWriteFiles(denMessageReader &reader){
	const derlTaskSyncClient::Ref taskSync(pGetProcessingSyncTask("pProcessResponseWriteFiles"));
	if(!taskSync){
		return;
	}
	
	derlTaskFileWrite::Map &tasksWrite = taskSync->GetTasksWriteFile();
	const int count = (int)reader.ReadUInt();
	int i;
	
	for(i=0; i<count; i++){
		const std::string path(reader.ReadString16());
		const derlProtocol::WriteFileResult result = (derlProtocol::WriteFileResult)reader.ReadByte();
		
		derlTaskFileWrite::Map::const_iterator iterWrite(tasksWrite.find(path));
		if(iterWrite == tasksWrite.cend()){
			std::stringstream log;
			log << "Write file response received with invalid path: " << path;
			Log(denLogger::LogSeverity::warning, "pProcessResponseWriteFiles", log.str());
			continue;
		}
		
		derlTaskFileWrite &taskWrite = *iterWrite->second;
		if(taskWrite.GetStatus() != derlTaskFileWrite::Status::preparing){
			std::stringstream log;
			log << "Write file response received but it is not preparing: " << path;
			Log(denLogger::LogSeverity::warning, "pProcessResponseWriteFiles", log.str());
			continue;
		}
		
		if(result == derlProtocol::WriteFileResult::success){
			taskWrite.SetStatus(derlTaskFileWrite::Status::processing);
			
		}else{
			taskWrite.SetStatus(derlTaskFileWrite::Status::failure);
			
			std::stringstream log;
			log << "Failed writing file: " << path;
			Log(denLogger::LogSeverity::error, "pProcessResponseWriteFiles", log.str());
			
			taskSync->SetError(log.str());
			taskSync->SetStatus(derlTaskSyncClient::Status::failure);
		}
	}
}

void derlRemoteClientConnection::pProcessResponseSendFileData(denMessageReader &reader){
	const derlTaskSyncClient::Ref taskSync(pGetProcessingSyncTask("pProcessResponseSendFileData"));
	if(!taskSync){
		return;
	}
	
	derlTaskFileWrite::Map &tasksWrite = taskSync->GetTasksWriteFile();
	const int count = (int)reader.ReadUInt();
	int i;
	
	for(i=0; i<count; i++){
		const std::string path(reader.ReadString16());
		const uint64_t offset = reader.ReadULong();
		const uint64_t size = (uint64_t)reader.ReadUInt();
		const derlProtocol::WriteDataResult result = (derlProtocol::WriteDataResult)reader.ReadByte();
		
		derlTaskFileWrite::Map::const_iterator iterWrite(tasksWrite.find(path));
		if(iterWrite == tasksWrite.cend()){
			std::stringstream log;
			log << "Write file data response received with invalid path: " << path;
			Log(denLogger::LogSeverity::warning, "pProcessResponseSendFileData", log.str());
			continue;
		}
		
		derlTaskFileWrite &taskWrite = *iterWrite->second;
		if(taskWrite.GetStatus() != derlTaskFileWrite::Status::processing){
			std::stringstream log;
			log << "Write file data response received but it is not processing: " << path;
			Log(denLogger::LogSeverity::warning, "pProcessResponseSendFileData", log.str());
			continue;
		}
		
		{
		derlTaskFileWriteBlock::List &blocks = taskWrite.GetBlocks();
		derlTaskFileWriteBlock::List::iterator iterBlock;
		for(iterBlock=blocks.begin(); iterBlock!=blocks.end(); iterBlock++){
			derlTaskFileWriteBlock &block = **iterBlock;
			if(block.GetOffset() == offset && block.GetSize() == size){
				break;
			}
		}
		
		if(iterBlock == blocks.cend()){
			std::stringstream log;
			log << "Write file data response received with invalid block: "
				<< path << " offset " << offset << " size " << size;
			Log(denLogger::LogSeverity::warning, "pProcessResponseSendFileData", log.str());
			continue;
		}
		
		derlTaskFileWriteBlock &block = **iterBlock;
		if(block.GetStatus() != derlTaskFileWriteBlock::Status::processing){
			std::stringstream log;
			log << "Write file data response received but block is not processing: "
				<< path << " offset " << offset << " size " << size;
			Log(denLogger::LogSeverity::warning, "pProcessResponseSendFileData", log.str());
			continue;
		}
		
		blocks.erase(iterBlock);
		}
		
		if(result != derlProtocol::WriteDataResult::success){
			taskWrite.SetStatus(derlTaskFileWrite::Status::failure);
			
			std::stringstream log;
			log << "Failed sending data: " << path << " offset " << offset << " size " << size;
			Log(denLogger::LogSeverity::error, "pProcessResponseSendFileData", log.str());
			
			taskSync->SetError(log.str());
			taskSync->SetStatus(derlTaskSyncClient::Status::failure);
		}
	}
}

void derlRemoteClientConnection::pProcessResponseFinishWriteFiles(denMessageReader &reader){
	const derlTaskSyncClient::Ref taskSync(pGetProcessingSyncTask("pProcessResponseFinishWriteFiles"));
	if(!taskSync){
		return;
	}
	
	derlTaskFileWrite::Map &tasksWrite = taskSync->GetTasksWriteFile();
	const int count = (int)reader.ReadUInt();
	int i;
	
	for(i=0; i<count; i++){
		const std::string path(reader.ReadString16());
		const derlProtocol::WriteFileResult result = (derlProtocol::WriteFileResult)reader.ReadByte();
		
		{
		derlTaskFileWrite::Map::iterator iterWrite(tasksWrite.find(path));
		if(iterWrite == tasksWrite.end()){
			std::stringstream log;
			log << "Finish write file response received with invalid path: " << path;
			Log(denLogger::LogSeverity::warning, "pProcessResponseFinishWriteFiles", log.str());
			continue;
		}
		
		derlTaskFileWrite &taskWrite = *iterWrite->second;
		if(taskWrite.GetStatus() != derlTaskFileWrite::Status::processing){
			std::stringstream log;
			log << "Finish write file response received but it is not processing: " << path;
			Log(denLogger::LogSeverity::warning, "pProcessResponseFinishWriteFiles", log.str());
			continue;
		}
		
		tasksWrite.erase(iterWrite);
		}
		
		if(result == derlProtocol::WriteFileResult::success){
			std::stringstream log;
			log << "File written: " << path;
			Log(denLogger::LogSeverity::info, "pProcessResponseFinishWriteFiles", log.str());
			
		}else{
			std::stringstream log;
			log << "Writing file failed: " << path;
			Log(denLogger::LogSeverity::error, "pProcessResponseFinishWriteFiles", log.str());
			
			taskSync->SetError(log.str());
			taskSync->SetStatus(derlTaskSyncClient::Status::failure);
		}
	}
}

void derlRemoteClientConnection::pSendRequestLayout(){
	Log(denLogger::LogSeverity::info, "pSendRequestLayout", "Request file layout");
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::requestFileLayout);
	}
	SendReliableMessage(message);
}

void derlRemoteClientConnection::pSendRequestFileBlockHashes(const std::string &path, uint32_t blockSize){
	std::stringstream log;
	log << "Request file blocks: " << path << " blockSize " << blockSize;
	Log(denLogger::LogSeverity::info, "pSendRequestFileBlockHashes", log.str());
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::requestFileBlockHashes);
		writer.WriteString16(path);
		writer.WriteUInt(blockSize);
	}
	SendReliableMessage(message);
}

void derlRemoteClientConnection::pSendRequestDeleteFiles(const derlTaskFileDelete::List &tasks){
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::requestDeleteFiles);
		writer.WriteUInt((int)tasks.size());
		
		derlTaskFileDelete::List::const_iterator iter;
		for(iter=tasks.cbegin(); iter!=tasks.cend(); iter++){
			const derlTaskFileDelete &task = **iter;
			writer.WriteString16(task.GetPath());
			
			std::stringstream log;
			log << "Request delete file: " << task.GetPath();
			Log(denLogger::LogSeverity::info, "pSendRequestDeleteFiles", log.str());
		}
	}
	SendReliableMessage(message);
}

void derlRemoteClientConnection::pSendRequestWriteFiles(const derlTaskFileWrite::List &tasks){
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::requestWriteFiles);
		writer.WriteUInt((int)tasks.size());
		
		derlTaskFileWrite::List::const_iterator iter;
		for(iter=tasks.cbegin(); iter!=tasks.cend(); iter++){
			const derlTaskFileWrite &task = **iter;
			writer.WriteString16(task.GetPath());
			writer.WriteULong(task.GetFileSize());
			
			std::stringstream log;
			log << "Request write file: " << task.GetPath() << " size " << task.GetFileSize();
			Log(denLogger::LogSeverity::info, "pSendRequestWriteFiles", log.str());
		}
	}
	SendReliableMessage(message);
}

void derlRemoteClientConnection::pSendSendFileData(const derlTaskFileWrite &task, const derlTaskFileWriteBlock &block){
	std::stringstream log;
	log << "Send file data: " << task.GetPath() << " offset " << block.GetOffset() << " size " << block.GetSize();
	Log(denLogger::LogSeverity::info, "pSendSendFileData", log.str());
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::sendFileData);
		writer.WriteString16(task.GetPath());
		writer.WriteULong(block.GetOffset());
		writer.Write(block.GetData().c_str(), (size_t)block.GetSize());
	}
	SendReliableMessage(message);
}

void derlRemoteClientConnection::pSendRequestFinishWriteFiles(const derlTaskFileWrite::List &tasks){
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::requestFinishWriteFiles);
		writer.WriteUInt((int)tasks.size());
		
		derlTaskFileWrite::List::const_iterator iter;
		for(iter=tasks.cbegin(); iter!=tasks.cend(); iter++){
			const derlTaskFileWrite &task = **iter;
			writer.WriteString16(task.GetPath());
			
			std::stringstream log;
			log << "Request finish write file: " << task.GetPath();
			Log(denLogger::LogSeverity::info, "pSendRequestFinishWriteFiles", log.str());
		}
	}
	SendReliableMessage(message);
}

void derlRemoteClientConnection::pSendStartApplication(const derlRunParameters &parameters){
	std::stringstream log;
	log << "Start application: ";
	Log(denLogger::LogSeverity::info, "pSendStartApplication", log.str());
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::startApplication);
		writer.WriteString16(parameters.GetGameConfig());
		writer.WriteString8(parameters.GetProfileName());
		writer.WriteString16(parameters.GetArguments());
	}
	SendReliableMessage(message);
}

void derlRemoteClientConnection::pSendStopApplication(derlProtocol::StopApplicationMode mode){
	std::stringstream log;
	log << "Stop application: " << (int)mode;
	Log(denLogger::LogSeverity::info, "pSendStopApplication", log.str());
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::stopApplication);
		writer.WriteByte((uint8_t)mode);
	}
	SendReliableMessage(message);
}

derlTaskSyncClient::Ref derlRemoteClientConnection::pGetPendingSyncTask(const std::string &functionName){
	const derlTaskSyncClient::Ref &task = pClient->GetTaskSyncClient();
	
	if(!task){
		Log(denLogger::LogSeverity::warning, functionName,
			"Received ResponseFileLayout but no sync task is present");
		return nullptr;
	}
	
	if(task->GetStatus() != derlTaskSyncClient::Status::pending){
		Log(denLogger::LogSeverity::warning, functionName,
			"Received ResponseFileLayout but sync task is not pending");
		return nullptr;
	}
	
	return task;
}

derlTaskSyncClient::Ref derlRemoteClientConnection::pGetProcessingSyncTask(const std::string &functionName){
	const derlTaskSyncClient::Ref &task = pClient->GetTaskSyncClient();
	
	if(!task){
		Log(denLogger::LogSeverity::warning, functionName,
			"Received ResponseFileLayout but no sync task is present");
		return nullptr;
	}
	
	if(task->GetStatus() != derlTaskSyncClient::Status::processing){
		Log(denLogger::LogSeverity::warning, functionName,
			"Received ResponseFileLayout but sync task is not processing");
		return nullptr;
	}
	
	return task;
}
