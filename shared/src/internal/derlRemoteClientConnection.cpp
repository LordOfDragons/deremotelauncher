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


// Class derlRemoteClientConnection::StateRun
///////////////////////////////////////////////

derlRemoteClientConnection::StateRun::StateRun(derlRemoteClientConnection &connection) :
denState(false),
pConnection(connection),
valueRunStatus(std::make_shared<denValueInt>(denValueIntegerFormat::uint8))
{
	valueRunStatus->SetValue((uint64_t)derlProtocol::RunStateStatus::stopped);
	AddValue(valueRunStatus);
}

void derlRemoteClientConnection::StateRun::RemoteValueChanged(denValue &value){
	derlRemoteClient * const client = pConnection.GetClient();
	if(!client){
		return;
	}
	
	if(&value == &*valueRunStatus){
		client->pNotifyRunStatusChanged = true;
	}
}


// Class derlRemoteClientConnection
/////////////////////////////////////

derlRemoteClientConnection::derlRemoteClientConnection(derlServer &server) :
pServer(server),
pClient(nullptr),
pSupportedFeatures(0),
pEnabledFeatures(0),
pEnableDebugLog(false),
pStateRun(std::make_shared<StateRun>(*this)),
pMaxInProgressFiles(1),
pCountInProgressFiles(0),
pMaxInProgressBlocks(2), //1
pCountInProgressBlocks(0)
{
	SetLogger(server.GetLogger());
}

derlRemoteClientConnection::~derlRemoteClientConnection() noexcept{
}


// Management
///////////////

void derlRemoteClientConnection::SetClient(derlRemoteClient *client){
	pClient = client;
}

void derlRemoteClientConnection::SetEnableDebugLog(bool enable){
	pEnableDebugLog = enable;
}

const denValueInt::Ref &derlRemoteClientConnection::GetValueRunStatus() const{
	return pStateRun->valueRunStatus;
}

void derlRemoteClientConnection::SetLogger(const denLogger::Ref &logger){
	denConnection::SetLogger(logger);
	pStateRun->SetLogger(logger);
}

void derlRemoteClientConnection::ConnectionClosed(){
	if(pClient){
		pClient->pNotifyConnectionClosed = true;
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

bool derlRemoteClientConnection::ProcessReceivedMessages(){
	if(!pClient){
		return false;
	}
	
	derlMessageQueue::Messages messages;
	{
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	pQueueReceived.PopAll(messages);
	}
	
	for(const denMessage::Ref &message : messages){
		denMessageReader reader(message->Item());
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
			
		case derlProtocol::MessageCodes::responseSystemProperty:
			pProcessResponseSystemProperty(reader);
			break;
			
		default:
			break; // ignore all other messages
		}
	}
	
	const bool returnValue = !messages.empty();
	{
	std::lock_guard guard(derlGlobal::mutexNetwork);
	messages.clear();
	}
	
	return returnValue;
}

void derlRemoteClientConnection::SendNextWriteRequests(derlTaskSyncClient &taskSync){
	const std::lock_guard guard(taskSync.GetMutex());
	
	derlTaskFileWrite::Map &tasksWrite = taskSync.GetTasksWriteFile();
	if(tasksWrite.empty()){
		return;
	}
	
	for(derlTaskFileWrite::Map::const_reference &eachWrite : tasksWrite){
		const derlTaskFileWrite::Ref &taskWrite = eachWrite.second;
		
		switch(taskWrite->GetStatus()){
		case derlTaskFileWrite::Status::pending:
			if(pCountInProgressFiles >= pMaxInProgressFiles){
				break;
			}
			
			taskWrite->SetStatus(derlTaskFileWrite::Status::preparing);
			pCountInProgressFiles++;
			
			try{
				pSendRequestWriteFile(*taskWrite);
				
			}catch(const std::exception &e){
				taskWrite->SetStatus(derlTaskFileWrite::Status::failure);
				LogException("SendRequestWriteFiles", e, "Failed");
				throw;
				
			}catch(...){
				taskWrite->SetStatus(derlTaskFileWrite::Status::failure);
				Log(denLogger::LogSeverity::error, "SendRequestWriteFiles", "Failed");
				throw;
			}
			break;
			
		case derlTaskFileWrite::Status::processing:{
			const derlTaskFileWriteBlock::List &blocks = taskWrite->GetBlocks();
			if(blocks.empty()){
				taskWrite->SetStatus(derlTaskFileWrite::Status::finishing);
				
				try{
					pSendRequestFinishWriteFile(*taskWrite);
					
				}catch(const std::exception &e){
					taskWrite->SetStatus(derlTaskFileWrite::Status::failure);
					LogException("SendRequestFinishWriteFiles", e, "Failed");
					throw;
					
				}catch(...){
					taskWrite->SetStatus(derlTaskFileWrite::Status::failure);
					Log(denLogger::LogSeverity::error, "SendRequestFinishWriteFiles", "Failed");
					throw;
				}
				
			}else{
				for(const derlTaskFileWriteBlock::Ref &eachBlock : blocks){
					derlTaskFileWriteBlock &block = *eachBlock;
					
					if(block.GetStatus() == derlTaskFileWriteBlock::Status::pending){
						if(pCountInProgressBlocks >= pMaxInProgressBlocks){
							break;
						}
						
						pCountInProgressBlocks++;
						
						if(block.GetSize() > 0){
							block.SetStatus(derlTaskFileWriteBlock::Status::readingData);
							pClient->AddPendingTaskSync(eachBlock);
							continue;
						}
						
						block.SetStatus(derlTaskFileWriteBlock::Status::dataReady);
					}
					
					if(block.GetStatus() == derlTaskFileWriteBlock::Status::dataReady){
						block.SetStatus(derlTaskFileWriteBlock::Status::dataSent);
						try{
							pSendSendFileData(block);
							
						}catch(const std::exception &e){
							taskWrite->SetStatus(derlTaskFileWrite::Status::failure);
							LogException("SendSendFileData", e, "Failed");
							throw;
							
						}catch(...){
							taskWrite->SetStatus(derlTaskFileWrite::Status::failure);
							Log(denLogger::LogSeverity::error, "SendSendFileData", "Failed");
							throw;
						}
					}
				}
			}
			}break;
			
		default:
			break;
		}
	}
}

void derlRemoteClientConnection::SendNextWriteRequestsFailSync(derlTaskSyncClient &taskSync){
	try{
		SendNextWriteRequests(taskSync);
		
	}catch(const std::exception &e){
		LogException("SendNextWriteRequests", e, "Failed");
		std::stringstream ss;
		ss << "Synchronize client failed: " << e.what();
		pClient->FailSynchronization(ss.str());
		
	}catch(...){
		Log(denLogger::LogSeverity::error, "SendNextWriteRequests", "Failed");
		pClient->FailSynchronization();
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

void derlRemoteClientConnection::SendRequestLayout(){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	if(!GetConnected()){
		return;
	}
	
	Log(denLogger::LogSeverity::info, "SendRequestLayout", "Request file layout");
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::requestFileLayout);
	}
	pQueueSend.Add(message);
}

void derlRemoteClientConnection::SendRequestFileBlockHashes(const derlTaskFileBlockHashes &task){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	if(!GetConnected()){
		return;
	}
	
	{
	std::stringstream log;
	log << "Request file blocks: " << task.GetPath() << " blockSize " << task.GetBlockSize();
	Log(denLogger::LogSeverity::info, "SendRequestFileBlockHashes", log.str());
	}
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::requestFileBlockHashes);
		writer.WriteString16(task.GetPath());
		writer.WriteUInt((uint32_t)task.GetBlockSize());
	}
	pQueueSend.Add(message);
}

void derlRemoteClientConnection::SendRequestDeleteFile(const derlTaskFileDelete &task){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::requestDeleteFile);
		writer.WriteString16(task.GetPath());
		
		std::stringstream log;
		log << "Request delete file: " << task.GetPath();
		Log(denLogger::LogSeverity::info, "SendRequestDeleteFile", log.str());
	}
	pQueueSend.Add(message);
}

void derlRemoteClientConnection::SendStartApplication(const derlRunParameters &parameters){
	{
	std::stringstream log;
	log << "Start application: profile='" << parameters.GetProfileName()
		<< "' args='" << parameters.GetArguments()
		<< "' config='" << parameters.GetGameConfig() << "'";
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

void derlRemoteClientConnection::SendStopApplication(derlProtocol::StopApplicationMode mode){
	{
	std::stringstream log;
	log << "Stop application: " << (int)mode;
	Log(denLogger::LogSeverity::info, "SendStopApplication", log.str());
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

void derlRemoteClientConnection::SendRequestSystemProperty(const std::string &property){
	{
	std::stringstream log;
	log << "Request system property '" << property << "'";
	Log(denLogger::LogSeverity::info, "SendRequestSystemProperty", log.str());
	}
	
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::requestSystemProperty);
		writer.WriteString8(property);
	}
	pQueueSend.Add(message);
}

void derlRemoteClientConnection::SendKeepAlive(){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::keepAlive);
	}
	pQueueSend.Add(message);
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
			
			pClient->pNotifyConnectionEstablished = true;
			return;
		}
	}
	
	Log(denLogger::LogSeverity::error, "MessageReceived",
		"Connection not found (internal error), disconnecting.");
	Disconnect();
	return;
}

void derlRemoteClientConnection::pProcessRequestLogs(denMessageReader &reader){
	if(!GetLogger()){
		return;
	}
	
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
	ss << "{" << reader.ReadString8() << "} ";
	ss << reader.ReadString16();
	
	//Log(severity, "pProcessRequestLogs", ss.str());
	GetLogger()->Log(severity, ss.str());
}

void derlRemoteClientConnection::pProcessResponseFileLayout(denMessageReader &reader){
	const derlTaskSyncClient::Ref taskSync(pGetSyncTask(
		"pProcessResponseFileLayout", derlTaskSyncClient::Status::pending));
	if(!taskSync){
		return;
	}
	
	const derlTaskFileLayout::Ref taskLayout(taskSync->GetTaskFileLayoutClient());
	if(!taskLayout){
		Log(denLogger::LogSeverity::warning, "pProcessResponseFileLayout",
			"Received ResponseFileLayout but task is done");
		return;
	}
	
	const derlFileLayout::Ref layout(taskLayout->GetLayout());
	const int count = reader.ReadUInt();
	int i;
	
	for(i=0; i<count; i++){
		const derlFile::Ref file(std::make_shared<derlFile>(reader.ReadString16()));
		file->SetSize(reader.ReadULong());
		file->SetHash(reader.ReadString8());
		layout->AddFile(file);
	}
	
	pClient->SetFileLayoutClient(taskLayout->GetLayout());
	
	std::stringstream ss;
	ss << "File layout received. " << count << " file(s)";
	Log(denLogger::LogSeverity::info, "pProcessResponseFileLayout", ss.str());
	
	const std::lock_guard guard(taskSync->GetMutex());
	taskSync->SetTaskFileLayoutClient(nullptr);
	if(!taskSync->GetTaskFileLayoutServer()){
		pClient->AddPendingTaskSync(taskSync);
	}
}

void derlRemoteClientConnection::pProcessResponseFileBlockHashes(denMessageReader &reader){
	const derlTaskSyncClient::Ref taskSync(pGetSyncTask(
		"pProcessResponseFileBlockHashes", derlTaskSyncClient::Status::processHashing));
	if(!taskSync){
		return;
	}
	
	const std::string path(reader.ReadString16());
	
	{
	const std::lock_guard guard(taskSync->GetMutex());
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
		const derlFileLayout::Ref layout(pClient->GetFileLayoutClient());
		if(!layout){
			std::stringstream ss;
			ss << "Block hashes for file received but file layout is not present: " << path;
			throw std::runtime_error(ss.str());
		}
		
		const derlFile::Ref file(layout->GetFileAt(path));
		if(!file){
			std::stringstream ss;
			ss << "Block hashes for file received but file does not exist in layout: " << path;
			throw std::runtime_error(ss.str());
		}
		
		const int count = reader.ReadUInt();
		if(count > file->GetBlockCount()){
			std::stringstream ss;
			ss << "Block hashes for file received but with count is out of range: "
				<< path << " count " << count << " allowed " << file->GetBlockCount();
			throw std::runtime_error(ss.str());
		}
		
		int i;
		for(i=0; i<count; i++){
			file->GetBlockAt(i)->SetHash(reader.ReadString8());
		}
		
	}catch(const std::exception &e){
		LogException("ProcessResponseFileBlockHashes", e, "Failed");
		
		std::stringstream ss;
		ss << "Synchronize client failed: " << e.what();
		pClient->FailSynchronization(ss.str());
		return;
		
	}catch(...){
		Log(denLogger::LogSeverity::error, "ProcessResponseFileBlockHashes", "Failed");
		pClient->FailSynchronization();
		return;
	}
	
	{
	std::stringstream ss;
	ss << "Block hashes received: " << path;
	Log(denLogger::LogSeverity::info, "ProcessResponseFileBlockHashes", ss.str());
	}
	
	pCheckFinishedHashes(taskSync);
}

void derlRemoteClientConnection::pProcessResponseDeleteFile(denMessageReader &reader){
	const derlTaskSyncClient::Ref taskSync(pGetSyncTask(
		"pProcessResponseDeleteFile", derlTaskSyncClient::Status::prepareTasksWriting,
		derlTaskSyncClient::Status::processWriting));
	if(!taskSync){
		return;
	}
	
	const std::string path(reader.ReadString16());
	const derlProtocol::DeleteFileResult result = (derlProtocol::DeleteFileResult)reader.ReadByte();
	
	{
	const std::lock_guard guard(taskSync->GetMutex());
	derlTaskFileDelete::Map &tasksDelete = taskSync->GetTasksDeleteFile();
	
	derlTaskFileDelete::Map::const_iterator iterDelete(tasksDelete.find(path));
	if(iterDelete == tasksDelete.cend()){
		std::stringstream log;
		log << "Delete file response received with invalid path: " << path;
		Log(denLogger::LogSeverity::warning, "pProcessResponseDeleteFile", log.str());
		return;
	}
	
	tasksDelete.erase(iterDelete);
	}
	
	if(result == derlProtocol::DeleteFileResult::success){
		std::stringstream log;
		log << "File deleted: " << path;
		Log(denLogger::LogSeverity::info, "pProcessResponseDeleteFile", log.str());
		pCheckFinishedWrite(taskSync);
		
	}else{
		std::stringstream ss;
		ss << "Failed deleting file: " << path;
		Log(denLogger::LogSeverity::error, "pProcessResponseDeleteFile", ss.str());
		pClient->FailSynchronization(ss.str());
	}
}

void derlRemoteClientConnection::pProcessResponseWriteFile(denMessageReader &reader){
	const derlTaskSyncClient::Ref taskSync(pGetSyncTask(
		"pProcessResponseWriteFile", derlTaskSyncClient::Status::processWriting));
	if(!taskSync){
		return;
	}
	
	const std::string path(reader.ReadString16());
	const derlProtocol::WriteFileResult result = (derlProtocol::WriteFileResult)reader.ReadByte();
	
	std::unique_lock guard(taskSync->GetMutex());
	derlTaskFileWrite::Map &tasksWrite = taskSync->GetTasksWriteFile();
	
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
		guard.unlock();
		SendNextWriteRequestsFailSync(*taskSync);
		
	}else{
		taskWrite.SetStatus(derlTaskFileWrite::Status::failure);
		guard.unlock();
		
		std::stringstream ss;
		ss << "Failed writing file: " << path;
		Log(denLogger::LogSeverity::error, "pProcessResponseWriteFile", ss.str());
		
		pClient->FailSynchronization(ss.str());
	}
}

void derlRemoteClientConnection::pProcessFileDataReceived(denMessageReader &reader){
	const derlTaskSyncClient::Ref taskSync(pGetSyncTask(
		"pProcessFileDataReceived", derlTaskSyncClient::Status::processWriting));
	if(!taskSync){
		return;
	}
	
	const std::string path(reader.ReadString16());
	const int indexBlock = reader.ReadUInt();
	const derlProtocol::FileDataReceivedResult result = (derlProtocol::FileDataReceivedResult)reader.ReadByte();
	
	{
	std::unique_lock guard(taskSync->GetMutex());
	derlTaskFileWrite::Map &tasksWrite = taskSync->GetTasksWriteFile();
	
	derlTaskFileWrite::Map::const_iterator iterWrite(tasksWrite.find(path));
	if(iterWrite == tasksWrite.cend()){
		std::stringstream log;
		log << "Write file data response received with invalid path: " << path;
		Log(denLogger::LogSeverity::warning, "pProcessFileDataReceived", log.str());
		return;
	}
	
	derlTaskFileWrite &taskWrite = *iterWrite->second;
	if(taskWrite.GetStatus() != derlTaskFileWrite::Status::processing){
		std::stringstream log;
		log << "Write file data response received but it is not processing: " << path;
		Log(denLogger::LogSeverity::warning, "pProcessFileDataReceived", log.str());
		return;
	}
	
	{
	derlTaskFileWriteBlock::List &blocks = taskWrite.GetBlocks();
	derlTaskFileWriteBlock::List::const_iterator iterBlock(std::find_if(
		blocks.cbegin(), blocks.cend(), [indexBlock](const derlTaskFileWriteBlock::Ref &block){
			return block->GetIndex() == indexBlock;
		}));
	
	if(iterBlock == blocks.cend()){
		std::stringstream log;
		log << "Write file data response received with invalid block: "
			<< path << " block " << indexBlock;
		Log(denLogger::LogSeverity::warning, "pProcessFileDataReceived", log.str());
		return;
	}
	
	derlTaskFileWriteBlock &block = **iterBlock;
	if(block.GetStatus() != derlTaskFileWriteBlock::Status::dataSent){
		std::stringstream log;
		log << "Write file data response received but block is not dataSent: "
			<< path << " block " << indexBlock;
		Log(denLogger::LogSeverity::warning, "pProcessFileDataReceived", log.str());
		return;
	}
	
	blocks.erase(iterBlock);
	if(pCountInProgressBlocks > 0){
		pCountInProgressBlocks--;
	}
	}
	}
	
	if(result == derlProtocol::FileDataReceivedResult::success){
		SendNextWriteRequestsFailSync(*taskSync);
		
	}else{
		std::stringstream ss;
		ss << "Failed sending data: " << path << " block " << indexBlock;
		Log(denLogger::LogSeverity::error, "pProcessResponseSendFileData", ss.str());
		pClient->FailSynchronization(ss.str());
	}
}

void derlRemoteClientConnection::pProcessResponseFinishWriteFile(denMessageReader &reader){
	const derlTaskSyncClient::Ref taskSync(pGetSyncTask(
		"pProcessResponseFinishWriteFile", derlTaskSyncClient::Status::processWriting));
	if(!taskSync){
		return;
	}
	
	const std::string path(reader.ReadString16());
	const derlProtocol::WriteFileResult result = (derlProtocol::WriteFileResult)reader.ReadByte();
	
	{
	const std::lock_guard guard(taskSync->GetMutex());
	derlTaskFileWrite::Map &tasksWrite = taskSync->GetTasksWriteFile();
	
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
		std::stringstream ss;
		ss << "File written: " << path;
		Log(denLogger::LogSeverity::info, "pProcessResponseFinishWriteFile", ss.str());
		pCheckFinishedWrite(taskSync);
		
	}else{
		std::stringstream ss;
		ss << "Writing file failed: " << path;
		Log(denLogger::LogSeverity::error, "pProcessResponseFinishWriteFile", ss.str());
		pClient->FailSynchronization(ss.str());
	}
}

void derlRemoteClientConnection::pProcessResponseSystemProperty(denMessageReader &reader){
	const std::string property(reader.ReadString8());
	const std::string value(reader.ReadString16());
	pClient->OnSystemProperty(property, value);
}

void derlRemoteClientConnection::pSendRequestWriteFile(const derlTaskFileWrite &task){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
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

void derlRemoteClientConnection::pSendSendFileData(derlTaskFileWriteBlock &block){
	if(pEnableDebugLog){
		std::stringstream log;
		log << "Send file data: " << block.GetParentTask().GetPath()
			<< " block " << block.GetIndex() << " size " << block.GetSize();
		LogDebug("pSendSendFileData", log.str());
	}
	
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::sendFileData);
		writer.WriteString16(block.GetParentTask().GetPath());
		writer.WriteUInt((uint32_t)block.GetIndex());
		writer.Write((void*)block.GetData().c_str(), block.GetSize());
	}
	pQueueSend.Add(message);
}

void derlRemoteClientConnection::pSendRequestFinishWriteFile(const derlTaskFileWrite &task){
	const derlFile::Ref file(pClient->GetFileLayoutServer()->GetFileAt(task.GetPath()));
	if(!file){
		std::stringstream ss;
		ss << "File missing in layout: " << task.GetPath();
		throw std::runtime_error(ss.str());
	}
	
	const std::lock_guard guard(derlGlobal::mutexNetwork);
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

derlTaskSyncClient::Ref derlRemoteClientConnection::pGetSyncTask(
const std::string &functionName, derlTaskSyncClient::Status status){
	const derlTaskSyncClient::Ref task(pClient->GetTaskSyncClient());
	
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

derlTaskSyncClient::Ref derlRemoteClientConnection::pGetSyncTask(const std::string &functionName,
derlTaskSyncClient::Status status1, derlTaskSyncClient::Status status2){
	const derlTaskSyncClient::Ref task(pClient->GetTaskSyncClient());
	
	if(!task){
		Log(denLogger::LogSeverity::warning, functionName,
			"Received response but no sync task is present");
		return nullptr;
	}
	
	if(task->GetStatus() != status1 && task->GetStatus() != status2){
		std::stringstream ss;
		ss << "Received response but sync task is not in the right state: "
			<< (int)task->GetStatus() << " instead of " << (int)status1 << " or " << (int)status2;
		Log(denLogger::LogSeverity::warning, functionName, ss.str());
		return nullptr;
	}
	
	return task;
}

void derlRemoteClientConnection::pCheckFinishedHashes(const derlTaskSyncClient::Ref &task){
	{
	const std::lock_guard guard(task->GetMutex());
	if(!task->GetTasksFileBlockHashes().empty()){
		return;
	}
	
	task->SetStatus(derlTaskSyncClient::Status::prepareTasksWriting);
	}
	
	pClient->AddPendingTaskSync(task);
}

void derlRemoteClientConnection::pCheckFinishedWrite(const derlTaskSyncClient::Ref &task){
	bool finished;
	{
	const std::lock_guard guard(task->GetMutex());
	finished = task->GetTasksDeleteFile().empty() && task->GetTasksWriteFile().empty();
	if(finished){
		task->SetStatus(derlTaskSyncClient::Status::success);
	}
	}
	
	if(finished){
		pClient->SucceedSynchronization();
		return;
	}
	
	try{
		SendNextWriteRequests(*task);
		
	}catch(const std::exception &e){
		LogException("SendNextWriteRequests", e, "Failed");
		std::stringstream ss;
		ss << "Synchronize client failed: " << e.what();
		pClient->FailSynchronization(ss.str());
		
	}catch(...){
		Log(denLogger::LogSeverity::error, "SendNextWriteRequests", "Failed");
		pClient->FailSynchronization();
	}
}
