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
#include "../derlGlobal.h"

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
pPartSize(1357),
pBatchSize(10),
pEnableDebugLog(false),
pStateRun(std::make_shared<denState>(false)),
pValueRunStatus(std::make_shared<denValueInt>(denValueIntegerFormat::uint8)),
pMaxInProgressFiles(3), //1
pMaxInProgressBlocks(3), //1
pMaxInProgressBatches(5), //2
pCountInProgressFiles(0),
pCountInProgressBlocks(0),
pCountInProgressBatches(0)
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

void derlRemoteClientConnection::SetEnableDebugLog(bool enable){
	pEnableDebugLog = enable;
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
	if(pClient){
		pClient->OnConnectionClosed();
	}
}

void derlRemoteClientConnection::MessageProgress(size_t bytesReceived){
}

void derlRemoteClientConnection::MessageReceived(const denMessage::Ref &message){
	if(pClient){
		pQueueReceived.Add(message);
		
	}else{
		pMessageReceivedConnect(message->Item());
	}
}

void derlRemoteClientConnection::SendQueuedMessages(){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	denMessage::Ref message;
	while(pQueueSend.Pop(message)){
		SendReliableMessage(message);
	}
}

void derlRemoteClientConnection::ProcessReceivedMessages(){
	if(!pClient){
		return;
	}
	
	derlMessageQueue::Messages messages;
	{
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	pQueueReceived.PopAll(messages);
	}
	
	{
	const std::lock_guard guard(pClient->GetMutex());
	derlMessageQueue::Messages::const_iterator iter;
	
	for(iter=messages.cbegin(); iter!=messages.cend(); iter++){
		denMessageReader reader((*iter)->Item());
		const derlProtocol::MessageCodes code = (derlProtocol::MessageCodes)reader.ReadByte();
		
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
			
		case derlProtocol::MessageCodes::responseDeleteFile:
			pProcessResponseDeleteFile(reader);
			break;
			
		case derlProtocol::MessageCodes::responseWriteFile:
			pProcessResponseWriteFile(reader);
			break;
			
		case derlProtocol::MessageCodes::fileDataReceived:
			pProcessFileDataReceived(reader);
			break;
			
		case derlProtocol::MessageCodes::responseFinishWriteFile:
			pProcessResponseFinishWriteFile(reader);
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
	
	switch(taskSync->GetStatus()){
	case derlTaskSyncClient::Status::processHashing:
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
		break;
		
	case derlTaskSyncClient::Status::processWriting:
		{
		derlTaskFileDelete::Map &tasks = taskSync->GetTasksDeleteFile();
		if(!tasks.empty()){
			derlTaskFileDelete::Map::const_iterator iter;
			derlTaskFileDelete::List deletes;
			
			for(iter = tasks.cbegin(); iter != tasks.cend(); iter++){
				if(iter->second->GetStatus() == derlTaskFileDelete::Status::pending){
					iter->second->SetStatus(derlTaskFileDelete::Status::processing);
					deletes.push_back(iter->second);
				}
			}
			
			if(!deletes.empty()){
				try{
					pSendRequestsDeleteFile(deletes);
					
				}catch(const std::exception &e){
					LogException("pSendRequestsDeleteFile", e, "Failed");
					taskSync->SetStatus(derlTaskSyncClient::Status::failure);
					
					std::stringstream ss;
					ss << "Synchronize client failed: " << e.what();
					taskSync->SetError(ss.str());
					return;
					
				}catch(...){
					Log(denLogger::LogSeverity::error, "pSendRequestsDeleteFile", "Failed");
					taskSync->SetStatus(derlTaskSyncClient::Status::failure);
					taskSync->SetError("Synchronize client failed: unknown error");
					return;
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
					if(pCountInProgressFiles >= pMaxInProgressFiles){
						break;
					}
					task->SetStatus(derlTaskFileWrite::Status::preparing);
					prepareTasks.push_back(task);
					pCountInProgressFiles++;
					break;
					
				case derlTaskFileWrite::Status::processing:{
					const derlTaskFileWriteBlock::List &blocks = task->GetBlocks();
					if(blocks.empty()){
						task->SetStatus(derlTaskFileWrite::Status::finishing);
						finishTasks.push_back(task);
						break;
					}
					
					derlTaskFileWriteBlock::List::const_iterator iterBlock;
					for(iterBlock=blocks.cbegin(); iterBlock!=blocks.cend(); iterBlock++){
						derlTaskFileWriteBlock &block = **iterBlock;
						bool claimBlockCount = false;
						
						if(block.GetStatus() == derlTaskFileWriteBlock::Status::dataReady){
							if(pCountInProgressBlocks >= pMaxInProgressBlocks){
								continue;
							}
							
							claimBlockCount = true;
							block.SetStatus(derlTaskFileWriteBlock::Status::processing);
						}
						
						if(block.GetStatus() == derlTaskFileWriteBlock::Status::processing){
							if(pCountInProgressBatches >= pMaxInProgressBatches){
								break;
							}
							
							try{
								pSendSendFileData(*task, block);
								
								if(claimBlockCount){
									pCountInProgressBlocks++;
								}
								
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
					
				default:
					break;
				}
			}
			
			if(!prepareTasks.empty()){
				try{
					pSendRequestsWriteFile(prepareTasks);
					
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
					pSendRequestsFinishWriteFile(finishTasks);
					
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
		
		if(taskSync->GetStatus() == derlTaskSyncClient::Status::processWriting
		&& taskSync->GetTasksFileBlockHashes().empty()
		&& taskSync->GetTasksDeleteFile().empty()
		&& taskSync->GetTasksWriteFile().empty()){
			taskSync->SetStatus(derlTaskSyncClient::Status::success);
		}
		break;
		
	default:
		break;
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
	if(pEnableDebugLog){
		Log(denLogger::LogSeverity::debug, functionName, message);
	}
}


// Private Functions
//////////////////////

void derlRemoteClientConnection::pMessageReceivedConnect(denMessage &message){
	denMessageReader reader(message);
	const derlProtocol::MessageCodes code = (derlProtocol::MessageCodes)reader.ReadByte();
	
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
	
	denMessage::Ref response(denMessage::Pool().Get());
	{
		denMessageWriter writer(response->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::connectAccepted);
		writer.Write(derlProtocol::signatureServer, 16);
		writer.WriteUInt(pEnabledFeatures);
	}
	SendReliableMessage(response);
	
	response = denMessage::Pool().Get();
	{
		denMessageWriter writer(response->Item());
		writer.WriteByte((uint8_t)derlProtocol::LinkCodes::runState);
	}
	LinkState(response, pStateRun, false);
	
	const denServer::Connections &connections = GetParentServer()->GetConnections();
	denServer::Connections::const_iterator iter;
	for(iter=connections.cbegin(); iter!=connections.cend(); iter++){
		if(iter->get() == this){
			{
			const std::lock_guard guard(pServer.GetMutex());
			const derlRemoteClient::Ref client(pServer.CreateClient(
				std::dynamic_pointer_cast<derlRemoteClientConnection>(*iter)));
			pServer.GetClients().push_back(client);
			pClient = client.get();
			}
			
			pClient->OnConnectionEstablished();
			pClient->pInternalStartTaskProcessors();
			return;
		}
	}
	
	Log(denLogger::LogSeverity::error, "MessageReceived",
		"Connection not found (internal error), disconnecting.");
	Disconnect();
	return;
}

void derlRemoteClientConnection::pProcessRequestLogs(denMessageReader &reader){
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

void derlRemoteClientConnection::pProcessResponseFileLayout(denMessageReader &reader){
	const derlTaskSyncClient::Ref taskSync(pGetSyncTask(
		"pProcessResponseFileLayout", derlTaskSyncClient::Status::pending));
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
	
	if(!taskLayout->GetLayout()){
		taskLayout->SetLayout(std::make_shared<derlFileLayout>());
	}
	
	const uint8_t flags = reader.ReadByte();
	
	if((flags & (uint8_t)derlProtocol::FileLayoutFlags::empty) == 0){
		const derlFile::Ref file(std::make_shared<derlFile>(reader.ReadString16()));
		file->SetSize(reader.ReadULong());
		file->SetHash(reader.ReadString8());
		taskLayout->GetLayout()->AddFile(file);
	}
	
	if((flags & (uint8_t)derlProtocol::FileLayoutFlags::finish) != 0){
		pClient->SetFileLayoutClient(taskLayout->GetLayout());
		pClient->SetTaskFileLayoutClient(nullptr);
		
		std::stringstream ss;
		ss << "File layout received. " << pClient->GetFileLayoutClient()->GetFileCount() << " file(s)";
		Log(denLogger::LogSeverity::info, "pProcessResponseFileLayout", ss.str());
	}
}

void derlRemoteClientConnection::pProcessResponseFileBlockHashes(denMessageReader &reader){
	const derlTaskSyncClient::Ref taskSync(pGetSyncTask(
		"pProcessResponseFileBlockHashes", derlTaskSyncClient::Status::processHashing));
	if(!taskSync){
		return;
	}
	
	const std::string path(reader.ReadString16());
	const uint8_t flags = reader.ReadByte();
	const bool finished = (flags & (uint8_t)derlProtocol::FileBlockHashesFlags::finish) != 0;
	
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
	
	if(finished){
		taskSync->GetTasksFileBlockHashes().erase(iterTaskHashes);
	}
	}
	
	if((flags & (uint8_t)derlProtocol::FileBlockHashesFlags::empty) == 0){
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
			
			const int index = reader.ReadUInt();
			if(index < 0 || index >= file->GetBlockCount()){
				std::stringstream ss;
				ss << "Block hashes for file received but with index ouf of range: "
					<< path << " index " << index;
				throw std::runtime_error(ss.str());
			}
			
			file->GetBlockAt(index)->SetHash(reader.ReadString8());
			
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
	}
	
	if(finished){
		std::stringstream log;
		log << "Block hashes received: " << path;
		Log(denLogger::LogSeverity::info, "ProcessResponseFileBlockHashes", log.str());
	}
}

void derlRemoteClientConnection::pProcessResponseDeleteFile(denMessageReader &reader){
	const derlTaskSyncClient::Ref taskSync(pGetSyncTask(
		"pProcessResponseDeleteFile", derlTaskSyncClient::Status::processWriting));
	if(!taskSync){
		return;
	}
	
	derlTaskFileDelete::Map &tasksDelete = taskSync->GetTasksDeleteFile();
	
	const std::string path(reader.ReadString16());
	const derlProtocol::DeleteFileResult result = (derlProtocol::DeleteFileResult)reader.ReadByte();
	
	{
	derlTaskFileDelete::Map::const_iterator iterDelete(tasksDelete.find(path));
	if(iterDelete == tasksDelete.cend()){
		std::stringstream log;
		log << "Delete file response received with invalid path: " << path;
		Log(denLogger::LogSeverity::warning, "pProcessResponseDeleteFile", log.str());
		return;
	}
	
	derlTaskFileDelete &taskDelete = *iterDelete->second;
	if(taskDelete.GetStatus() != derlTaskFileDelete::Status::processing){
		std::stringstream log;
		log << "Delete file response received but it is not processing: " << path;
		Log(denLogger::LogSeverity::warning, "pProcessResponseDeleteFile", log.str());
		return;
	}
	
	tasksDelete.erase(iterDelete);
	}
	
	if(result == derlProtocol::DeleteFileResult::success){
		std::stringstream log;
		log << "File deleted: " << path;
		Log(denLogger::LogSeverity::info, "pProcessResponseDeleteFile", log.str());
		
	}else{
		std::stringstream log;
		log << "Failed deleting file: " << path;
		
		taskSync->SetError(log.str());
		taskSync->SetStatus(derlTaskSyncClient::Status::failure);
		
		Log(denLogger::LogSeverity::error, "pProcessResponseDeleteFile", taskSync->GetError());
	}
}

void derlRemoteClientConnection::pProcessResponseWriteFile(denMessageReader &reader){
	const derlTaskSyncClient::Ref taskSync(pGetSyncTask(
		"pProcessResponseWriteFile", derlTaskSyncClient::Status::processWriting));
	if(!taskSync){
		return;
	}
	
	derlTaskFileWrite::Map &tasksWrite = taskSync->GetTasksWriteFile();
	
	const std::string path(reader.ReadString16());
	const derlProtocol::WriteFileResult result = (derlProtocol::WriteFileResult)reader.ReadByte();
	
	derlTaskFileWrite::Map::const_iterator iterWrite(tasksWrite.find(path));
	if(iterWrite == tasksWrite.cend()){
		std::stringstream log;
		log << "Write file response received with invalid path: " << path;
		Log(denLogger::LogSeverity::warning, "pProcessResponseWriteFile", log.str());
		return;
	}
	
	derlTaskFileWrite &taskWrite = *iterWrite->second;
	if(taskWrite.GetStatus() != derlTaskFileWrite::Status::preparing){
		std::stringstream log;
		log << "Write file response received but it is not preparing: " << path;
		Log(denLogger::LogSeverity::warning, "pProcessResponseWriteFile", log.str());
		return;
	}
	
	if(result == derlProtocol::WriteFileResult::success){
		taskWrite.SetStatus(derlTaskFileWrite::Status::processing);
		
	}else{
		taskWrite.SetStatus(derlTaskFileWrite::Status::failure);
		
		std::stringstream log;
		log << "Failed writing file: " << path;
		
		taskSync->SetError(log.str());
		taskSync->SetStatus(derlTaskSyncClient::Status::failure);
		
		Log(denLogger::LogSeverity::error, "pProcessResponseWriteFile", taskSync->GetError());
	}
}

void derlRemoteClientConnection::pProcessFileDataReceived(denMessageReader &reader){
	const derlTaskSyncClient::Ref taskSync(pGetSyncTask(
		"pProcessFileDataReceived", derlTaskSyncClient::Status::processWriting));
	if(!taskSync){
		return;
	}
	
	derlTaskFileWrite::Map &tasksWrite = taskSync->GetTasksWriteFile();
	
	const std::string path(reader.ReadString16());
	const int indexBlock = reader.ReadUInt();
	const derlProtocol::FileDataReceivedResult result = (derlProtocol::FileDataReceivedResult)reader.ReadByte();
	
	derlTaskFileWrite::Map::const_iterator iterWrite(tasksWrite.find(path));
	if(iterWrite == tasksWrite.cend()){
		std::stringstream log;
		log << "Write file data response received with invalid path: " << path;
		Log(denLogger::LogSeverity::warning, "pProcessResponseSendFileData", log.str());
		return;
	}
	
	derlTaskFileWrite &taskWrite = *iterWrite->second;
	if(taskWrite.GetStatus() != derlTaskFileWrite::Status::processing){
		std::stringstream log;
		log << "Write file data response received but it is not processing: " << path;
		Log(denLogger::LogSeverity::warning, "pProcessResponseSendFileData", log.str());
		return;
	}
	
	{
	derlTaskFileWriteBlock::List &blocks = taskWrite.GetBlocks();
	derlTaskFileWriteBlock::List::const_iterator iterBlock(std::find_if(
		blocks.cbegin(), blocks.cend(), [&](const derlTaskFileWriteBlock::Ref &block){
			return block->GetIndex() == indexBlock;
		}));
	
	if(iterBlock == blocks.cend()){
		std::stringstream log;
		log << "Write file data response received with invalid block: "
			<< path << " block " << indexBlock;
		Log(denLogger::LogSeverity::warning, "pProcessResponseSendFileData", log.str());
		return;
	}
	
	derlTaskFileWriteBlock &block = **iterBlock;
	if(block.GetStatus() != derlTaskFileWriteBlock::Status::processing){
		std::stringstream log;
		log << "Write file data response received but block is not processing: "
			<< path << " block " << indexBlock;
		Log(denLogger::LogSeverity::warning, "pProcessResponseSendFileData", log.str());
		return;
	}
	
	if(pCountInProgressBatches > 0){
		pCountInProgressBatches--;
	}
	
	if(result == derlProtocol::FileDataReceivedResult::batch){
		if(pEnableDebugLog){
			std::stringstream log;
			log << "Batch finished: " << path << " block " << indexBlock;
			LogDebug("pProcessResponseSendFileData", log.str());
		}
		return;
	}
	
	blocks.erase(iterBlock);
	if(pCountInProgressBlocks > 0){
		pCountInProgressBlocks--;
	}
	}
	
	if(result != derlProtocol::FileDataReceivedResult::success){
		taskWrite.SetStatus(derlTaskFileWrite::Status::failure);
		
		std::stringstream log;
		log << "Failed sending data: " << path << " block " << indexBlock;
		
		taskSync->SetError(log.str());
		taskSync->SetStatus(derlTaskSyncClient::Status::failure);
		
		Log(denLogger::LogSeverity::error, "pProcessResponseSendFileData", taskSync->GetError());
	}
}

void derlRemoteClientConnection::pProcessResponseFinishWriteFile(denMessageReader &reader){
	const derlTaskSyncClient::Ref taskSync(pGetSyncTask(
		"pProcessResponseFinishWriteFile", derlTaskSyncClient::Status::processWriting));
	if(!taskSync){
		return;
	}
	
	derlTaskFileWrite::Map &tasksWrite = taskSync->GetTasksWriteFile();
	
	const std::string path(reader.ReadString16());
	const derlProtocol::WriteFileResult result = (derlProtocol::WriteFileResult)reader.ReadByte();
	
	{
	derlTaskFileWrite::Map::iterator iterWrite(tasksWrite.find(path));
	if(iterWrite == tasksWrite.end()){
		std::stringstream log;
		log << "Finish write file response received with invalid path: " << path;
		Log(denLogger::LogSeverity::warning, "pProcessResponseFinishWriteFile", log.str());
		return;
	}
	
	derlTaskFileWrite &taskWrite = *iterWrite->second;
	if(taskWrite.GetStatus() != derlTaskFileWrite::Status::finishing){
		std::stringstream log;
		log << "Finish write file response received but it is not finishing: " << path;
		Log(denLogger::LogSeverity::warning, "pProcessResponseFinishWriteFile", log.str());
		return;
	}
	
	tasksWrite.erase(iterWrite);
	if(pCountInProgressFiles > 0){
		pCountInProgressFiles--;
	}
	}
	
	if(result == derlProtocol::WriteFileResult::success){
		std::stringstream log;
		log << "File written: " << path;
		Log(denLogger::LogSeverity::info, "pProcessResponseFinishWriteFile", log.str());
		
	}else{
		std::stringstream log;
		log << "Writing file failed: " << path;
		Log(denLogger::LogSeverity::error, "pProcessResponseFinishWriteFile", log.str());
		
		taskSync->SetError(log.str());
		taskSync->SetStatus(derlTaskSyncClient::Status::failure);
	}
}

void derlRemoteClientConnection::pSendRequestLayout(){
	Log(denLogger::LogSeverity::info, "pSendRequestLayout", "Request file layout");
	
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::requestFileLayout);
	}
	pQueueSend.Add(message);
}

void derlRemoteClientConnection::pSendRequestFileBlockHashes(
const std::string &path, uint32_t blockSize){
	{
	std::stringstream log;
	log << "Request file blocks: " << path << " blockSize " << blockSize;
	Log(denLogger::LogSeverity::info, "pSendRequestFileBlockHashes", log.str());
	}
	
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::requestFileBlockHashes);
		writer.WriteString16(path);
		writer.WriteUInt(blockSize);
	}
	pQueueSend.Add(message);
}

void derlRemoteClientConnection::pSendRequestsDeleteFile(const derlTaskFileDelete::List &tasks){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	derlTaskFileDelete::List::const_iterator iter;
	
	for(iter=tasks.cbegin(); iter!=tasks.cend(); iter++){
		const derlTaskFileDelete &task = **iter;
		
		const denMessage::Ref message(denMessage::Pool().Get());
		{
			denMessageWriter writer(message->Item());
			writer.WriteByte((uint8_t)derlProtocol::MessageCodes::requestDeleteFile);
			writer.WriteString16(task.GetPath());
			
			std::stringstream log;
			log << "Request delete file: " << task.GetPath();
			Log(denLogger::LogSeverity::info, "pSendRequestsDeleteFile", log.str());
		}
		pQueueSend.Add(message);
	}
}

void derlRemoteClientConnection::pSendRequestsWriteFile(const derlTaskFileWrite::List &tasks){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	derlTaskFileWrite::List::const_iterator iter;
	
	for(iter=tasks.cbegin(); iter!=tasks.cend(); iter++){
		const derlTaskFileWrite &task = **iter;
		
		const denMessage::Ref message(denMessage::Pool().Get());
		{
			denMessageWriter writer(message->Item());
			writer.WriteByte((uint8_t)derlProtocol::MessageCodes::requestWriteFile);
			writer.WriteString16(task.GetPath());
			writer.WriteULong(task.GetFileSize());
			writer.WriteULong(task.GetBlockSize());
			writer.WriteUInt(task.GetBlockCount());
			
			std::stringstream log;
			log << "Request write file: " << task.GetPath() << " size " << task.GetFileSize();
			Log(denLogger::LogSeverity::info, "pSendRequestsWriteFile", log.str());
		}
		pQueueSend.Add(message);
	}
}

void derlRemoteClientConnection::pSendSendFileData(const derlTaskFileWrite &task, derlTaskFileWriteBlock &block){
	const int partCount = block.GetPartCount();
	if(block.GetNextPartIndex() == partCount){
		return;
	}
	
	if(pEnableDebugLog){
		std::stringstream log;
		log << "Send file data: " << task.GetPath() << " block " << block.GetIndex()
			<< " size " << block.GetSize() << " part " << block.GetNextPartIndex()
			<< "/" << block.GetPartCount();
		LogDebug("pSendSendFileData", log.str());
	}
	
	const uint64_t blockSize = block.GetSize();
	const uint8_t * const blockData = (const uint8_t *)block.GetData().c_str();
	const int lastIndex = partCount - 1;
	int i, batchCounter = 0;
	
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	
	for(i=block.GetNextPartIndex(); i<partCount; i++, batchCounter++){
		if(batchCounter == pBatchSize){
			batchCounter = 0;
			pCountInProgressBatches++;
			if(pCountInProgressBatches >= pMaxInProgressBatches){
				break;
			}
		}
		
		const uint64_t partOffset = (uint64_t)pPartSize * i;
		const int partSize = std::min(pPartSize, (int)(blockSize - partOffset));
		
		const denMessage::Ref message(denMessage::Pool().Get());
		{
			denMessageWriter writer(message->Item());
			writer.WriteByte((uint8_t)derlProtocol::MessageCodes::sendFileData);
			writer.WriteString16(task.GetPath());
			writer.WriteUInt((uint32_t)block.GetIndex());
			writer.WriteUInt((uint32_t)i);
			
			uint8_t flags = 0;
			if(i == lastIndex){
				flags |= (uint8_t)derlProtocol::SendFileDataFlags::finish;
				pCountInProgressBatches++;
				
			}else if(batchCounter == pBatchSize - 1){
				flags |= (uint8_t)derlProtocol::SendFileDataFlags::batch;
			}
			writer.WriteByte(flags);
			
			writer.Write(blockData + partOffset, (size_t)partSize);
		}
		pQueueSend.Add(message);
	}
	
	block.SetNextPartIndex(i);
}

void derlRemoteClientConnection::pSendRequestsFinishWriteFile(const derlTaskFileWrite::List &tasks){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	derlTaskFileWrite::List::const_iterator iter;
	
	for(iter=tasks.cbegin(); iter!=tasks.cend(); iter++){
		const derlTaskFileWrite &task = **iter;
		
		const derlFile::Ref file(pClient->GetFileLayoutServer()->GetFileAt(task.GetPath()));
		if(!file){
			std::stringstream ss;
			ss << "File missing in layout: " << task.GetPath();
			throw std::runtime_error(ss.str());
		}
		
		const denMessage::Ref message(denMessage::Pool().Get());
		{
			denMessageWriter writer(message->Item());
			writer.WriteByte((uint8_t)derlProtocol::MessageCodes::requestFinishWriteFile);
			writer.WriteString16(task.GetPath());
			writer.WriteString8(file->GetHash());
			
			std::stringstream log;
			log << "Request finish write file: " << task.GetPath();
			Log(denLogger::LogSeverity::info, "pSendRequestsFinishWriteFile", log.str());
		}
		pQueueSend.Add(message);
	}
}

void derlRemoteClientConnection::pSendStartApplication(const derlRunParameters &parameters){
	{
	std::stringstream log;
	log << "Start application: ";
	Log(denLogger::LogSeverity::info, "pSendStartApplication", log.str());
	}
	
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::startApplication);
		writer.WriteString16(parameters.GetGameConfig());
		writer.WriteString8(parameters.GetProfileName());
		writer.WriteString16(parameters.GetArguments());
	}
	pQueueSend.Add(message);
}

void derlRemoteClientConnection::pSendStopApplication(derlProtocol::StopApplicationMode mode){
	{
	std::stringstream log;
	log << "Stop application: " << (int)mode;
	Log(denLogger::LogSeverity::info, "pSendStopApplication", log.str());
	}
	
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::stopApplication);
		writer.WriteByte((uint8_t)mode);
	}
	pQueueSend.Add(message);
}

derlTaskSyncClient::Ref derlRemoteClientConnection::pGetSyncTask(
const std::string &functionName, derlTaskSyncClient::Status status){
	const derlTaskSyncClient::Ref &task = pClient->GetTaskSyncClient();
	
	if(!task){
		Log(denLogger::LogSeverity::warning, functionName,
			"Received response but no sync task is present");
		return nullptr;
	}
	
	if(task->GetStatus() != status){
		std::stringstream ss;
		ss << "Received response but sync task is not in the right state: "
			<< (int)task->GetStatus() << " instead of " << (int)status;
		Log(denLogger::LogSeverity::warning, functionName, ss.str());
		return nullptr;
	}
	
	return task;
}
