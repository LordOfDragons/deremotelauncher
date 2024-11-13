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
	
	const derlRemoteClient::Ref client(pClient);
	pClient = nullptr;
	
	derlRemoteClient::List &clients = pServer.GetClients();
	clients.erase(std::find(clients.begin(), clients.end(), client));
	
	pClient->OnConnectionClosed();
}

void derlRemoteClientConnection::MessageProgress(size_t bytesReceived){
}

void derlRemoteClientConnection::MessageReceived(const denMessage::Ref &message){
	denMessageReader reader(message->Item());
	const derlProtocol::MessageCodes code = (derlProtocol::MessageCodes)reader.ReadByte();
	
	if(!pClient){
		if(!GetParentServer()){
			GetLogger()->Log(denLogger::LogSeverity::error,
				"Server link missing (internal error), disconnecting.");
			Disconnect();
			return;
		}
		
		if(code != derlProtocol::MessageCodes::connectRequest){
			GetLogger()->Log(denLogger::LogSeverity::error,
				"Client send request other than ConnectRequest, disconnecting.");
			Disconnect();
			return;
		}
		
		std::string signature(16, 0);
		reader.Read((void*)signature.c_str(), 16);
		if(signature != derlProtocol::signatureClient){
			GetLogger()->Log(denLogger::LogSeverity::error,
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
		
		GetLogger()->Log(denLogger::LogSeverity::error,
			"Connection not found (internal error), disconnecting.");
		Disconnect();
		return;
	}
	
	switch(code){
	case derlProtocol::MessageCodes::logs:
		pProcessRequestLogs(reader);
		break;
		
	case derlProtocol::MessageCodes::requestFileLayout:
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
	if(!taskSync || taskSync->GetStatus() != derlTaskSyncClient::Status::processing){
		return;
	}
	
	// TODO send requests for tasks in "pending" state promoting them to "processing" state
	
	// TODO if all tasks are in "success" or "failure" state promote taskSync to "success" or "failure" state
}

// Private Functions
//////////////////////

void derlRemoteClientConnection::pProcessRequestLogs(denMessageReader &reader){
	const int count = (int)reader.ReadUShort();
	denLogger &logger = *GetLogger();
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
		
		logger.Log(severity, ss.str());
	}
}

void derlRemoteClientConnection::pProcessResponseFileLayout(denMessageReader &reader){
	const derlTaskSyncClient::Ref task(pGetProcessingSyncTask());
	if(!task){
		return;
	}
	
	if(pClient->GetFileLayout()){
		GetLogger()->Log(denLogger::LogSeverity::warning,
			"Received ResponseFileLayout but layout has been already received");
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
	
	pClient->SetFileLayout(layout);
	GetLogger()->Log(denLogger::LogSeverity::info, "File layout received.");
}

void derlRemoteClientConnection::pProcessResponseFileBlockHashes(denMessageReader &reader){
	const derlTaskSyncClient::Ref task(pGetProcessingSyncTask());
	if(!task){
		return;
	}
	
	const std::string path(reader.ReadString16());
	
	derlTaskFileBlockHashes::Map::iterator iterTaskHashes(task->GetTasksFileBlockHashes().find(path));
	if(iterTaskHashes == task->GetTasksFileBlockHashes().end()){
		std::stringstream log;
		log << "Block hashes for file received but task is absent: " << path;
		GetLogger()->Log(denLogger::LogSeverity::warning, log.str());
		return;
	}
	
	derlTaskFileBlockHashes &taskHashes = *iterTaskHashes->second;
	if(taskHashes.GetStatus() != derlTaskFileBlockHashes::Status::processing){
		std::stringstream log;
		log << "Block hashes for file received but task is not processing: " << path;
		GetLogger()->Log(denLogger::LogSeverity::warning, log.str());
		return;
	}
	
	if(!pClient->GetFileLayout()){
		taskHashes.SetStatus(derlTaskFileBlockHashes::Status::failure);
		std::stringstream log;
		log << "Block hashes for file received but file layout is not present: " << path;
		GetLogger()->Log(denLogger::LogSeverity::warning, log.str());
		return;
	}
	
	const derlFile::Ref file(pClient->GetFileLayout()->GetFileAt(path));
	if(!file){
		taskHashes.SetStatus(derlTaskFileBlockHashes::Status::failure);
		std::stringstream log;
		log << "Block hashes for file received but file does not exist in layout: " << path;
		GetLogger()->Log(denLogger::LogSeverity::warning, log.str());
		return;
	}
	
	const int blockCount = (int)reader.ReadUInt();
	const uint32_t blockSize = reader.ReadUInt();
	
	if(file->GetBlockCount() != blockCount){
		taskHashes.SetStatus(derlTaskFileBlockHashes::Status::failure);
		std::stringstream log;
		log << "Block hashes for file received but block count does not match: " << path;
		GetLogger()->Log(denLogger::LogSeverity::warning, log.str());
		return;
	}
	
	if(file->GetBlockSize() != blockSize){
		taskHashes.SetStatus(derlTaskFileBlockHashes::Status::failure);
		std::stringstream log;
		log << "Block hashes for file received but block size does not match: " << path;
		GetLogger()->Log(denLogger::LogSeverity::warning, log.str());
		return;
	}
	
	derlFileBlock::List::const_iterator iterBlock;
	for(iterBlock=file->GetBlocksBegin(); iterBlock!=file->GetBlocksEnd(); iterBlock++){
		(*iterBlock)->SetHash(reader.ReadString8());
	}
	
	taskHashes.SetStatus(derlTaskFileBlockHashes::Status::success);
	
	std::stringstream log;
	log << "Block hashes: " << path;
	GetLogger()->Log(denLogger::LogSeverity::info, log.str());
}

void derlRemoteClientConnection::pProcessResponseDeleteFiles(denMessageReader &reader){
	const derlTaskSyncClient::Ref task(pGetProcessingSyncTask());
	if(!task){
		return;
	}
	
	derlTaskFileDelete::Map &tasksDelete = task->GetTasksDeleteFile();
	const int count = (int)reader.ReadUInt();
	int i;
	
	for(i=0; i<count; i++){
		const std::string path(reader.ReadString16());
		const derlProtocol::DeleteFileResult result = (derlProtocol::DeleteFileResult)reader.ReadByte();
		
		derlTaskFileDelete::Map::const_iterator iterDelete(tasksDelete.find(path));
		if(iterDelete == tasksDelete.cend()){
			std::stringstream log;
			log << "Delete file response received with invalid path: " << path;
			GetLogger()->Log(denLogger::LogSeverity::warning, log.str());
			continue;
		}
		
		derlTaskFileDelete &taskDelete = *iterDelete->second;
		if(taskDelete.GetStatus() != derlTaskFileDelete::Status::pending){
			std::stringstream log;
			log << "Delete file response received but it is not pending: " << path;
			GetLogger()->Log(denLogger::LogSeverity::warning, log.str());
			continue;
		}
		
		if(result == derlProtocol::DeleteFileResult::success){
			taskDelete.SetStatus(derlTaskFileDelete::Status::success);
			
			std::stringstream log;
			log << "File deleted: " << path;
			GetLogger()->Log(denLogger::LogSeverity::info, log.str());
			
		}else{
			taskDelete.SetStatus(derlTaskFileDelete::Status::failure);
			
			std::stringstream log;
			log << "Failed deleting file: " << path;
			GetLogger()->Log(denLogger::LogSeverity::error, log.str());
		}
	}
}

void derlRemoteClientConnection::pProcessResponseWriteFiles(denMessageReader &reader){
	const derlTaskSyncClient::Ref task(pGetProcessingSyncTask());
	if(!task){
		return;
	}
	
	derlTaskFileWrite::Map &tasksWrite = task->GetTasksWriteFile();
	const int count = (int)reader.ReadUInt();
	int i;
	
	for(i=0; i<count; i++){
		const std::string path(reader.ReadString16());
		const derlProtocol::WriteFileResult result = (derlProtocol::WriteFileResult)reader.ReadByte();
		
		derlTaskFileWrite::Map::const_iterator iterWrite(tasksWrite.find(path));
		if(iterWrite == tasksWrite.cend()){
			std::stringstream log;
			log << "Write file response received with invalid path: " << path;
			GetLogger()->Log(denLogger::LogSeverity::warning, log.str());
			continue;
		}
		
		derlTaskFileWrite &taskWrite = *iterWrite->second;
		if(taskWrite.GetStatus() != derlTaskFileWrite::Status::pending){
			std::stringstream log;
			log << "Write file response received but it is not pending: " << path;
			GetLogger()->Log(denLogger::LogSeverity::warning, log.str());
			continue;
		}
		
		if(result == derlProtocol::WriteFileResult::success){
			taskWrite.SetStatus(derlTaskFileWrite::Status::processing);
			
		}else{
			taskWrite.SetStatus(derlTaskFileWrite::Status::failure);
			
			std::stringstream log;
			log << "Failed writing file: " << path;
			GetLogger()->Log(denLogger::LogSeverity::error, log.str());
		}
	}
}

void derlRemoteClientConnection::pProcessResponseSendFileData(denMessageReader &reader){
	const derlTaskSyncClient::Ref task(pGetProcessingSyncTask());
	if(!task){
		return;
	}
	
	derlTaskFileWrite::Map &tasksWrite = task->GetTasksWriteFile();
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
			GetLogger()->Log(denLogger::LogSeverity::warning, log.str());
			continue;
		}
		
		derlTaskFileWrite &taskWrite = *iterWrite->second;
		if(taskWrite.GetStatus() != derlTaskFileWrite::Status::processing){
			std::stringstream log;
			log << "Write file data response received but it is not processing: " << path;
			GetLogger()->Log(denLogger::LogSeverity::warning, log.str());
			continue;
		}
		
		const derlTaskFileWriteBlock::List &blocks = taskWrite.GetBlocks();
		derlTaskFileWriteBlock::List::const_iterator iterBlock;
		for(iterBlock=blocks.cbegin(); iterBlock!=blocks.cend(); iterBlock++){
			derlTaskFileWriteBlock &block = **iterBlock;
			if(block.GetOffset() == offset && block.GetSize() == size){
				break;
			}
		}
		
		if(iterBlock == blocks.cend()){
			std::stringstream log;
			log << "Write file data response received with invalid block: "
				<< path << " offset " << offset << " size " << size;
			GetLogger()->Log(denLogger::LogSeverity::warning, log.str());
			continue;
		}
		
		derlTaskFileWriteBlock &block = **iterBlock;
		if(block.GetStatus() != derlTaskFileWriteBlock::Status::processing){
			std::stringstream log;
			log << "Write file data response received but block is not processing: "
				<< path << " offset " << offset << " size " << size;
			GetLogger()->Log(denLogger::LogSeverity::warning, log.str());
			continue;
		}
		
		if(result == derlProtocol::WriteDataResult::success){
			block.SetStatus(derlTaskFileWriteBlock::Status::success);
			
		}else{
			block.SetStatus(derlTaskFileWriteBlock::Status::failure);
			
			std::stringstream log;
			log << "Failed sending data: " << path << " offset " << offset << " size " << size;
			GetLogger()->Log(denLogger::LogSeverity::error, log.str());
		}
	}
}

void derlRemoteClientConnection::pProcessResponseFinishWriteFiles(denMessageReader &reader){
	const derlTaskSyncClient::Ref task(pGetProcessingSyncTask());
	if(!task){
		return;
	}
	
	derlTaskFileWrite::Map &tasksWrite = task->GetTasksWriteFile();
	const int count = (int)reader.ReadUInt();
	int i;
	
	for(i=0; i<count; i++){
		const std::string path(reader.ReadString16());
		const derlProtocol::WriteFileResult result = (derlProtocol::WriteFileResult)reader.ReadByte();
		
		derlTaskFileWrite::Map::const_iterator iterWrite(tasksWrite.find(path));
		if(iterWrite == tasksWrite.cend()){
			std::stringstream log;
			log << "Finish write file response received with invalid path: " << path;
			GetLogger()->Log(denLogger::LogSeverity::warning, log.str());
			continue;
		}
		
		derlTaskFileWrite &taskWrite = *iterWrite->second;
		if(taskWrite.GetStatus() != derlTaskFileWrite::Status::processing){
			std::stringstream log;
			log << "Finish write file response received but it is not processing: " << path;
			GetLogger()->Log(denLogger::LogSeverity::warning, log.str());
			continue;
		}
		
		if(result == derlProtocol::WriteFileResult::success){
			taskWrite.SetStatus(derlTaskFileWrite::Status::success);
			
			std::stringstream log;
			log << "File written: " << path;
			GetLogger()->Log(denLogger::LogSeverity::info, log.str());
			
		}else{
			taskWrite.SetStatus(derlTaskFileWrite::Status::failure);
			
			std::stringstream log;
			log << "Writing file failed: " << path;
			GetLogger()->Log(denLogger::LogSeverity::error, log.str());
		}
	}
}

void derlRemoteClientConnection::pSendRequestLayout(){
	GetLogger()->Log(denLogger::LogSeverity::info, "Request file layout");
	
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
	GetLogger()->Log(denLogger::LogSeverity::info, log.str());
	
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
			GetLogger()->Log(denLogger::LogSeverity::info, log.str());
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
			GetLogger()->Log(denLogger::LogSeverity::info, log.str());
		}
	}
	SendReliableMessage(message);
}

void derlRemoteClientConnection::pSendSendFileData(const derlTaskFileWrite &task, const derlTaskFileWriteBlock &block){
	std::stringstream log;
	log << "Send file data: " << task.GetPath() << " offset " << block.GetOffset() << " size " << block.GetSize();
	GetLogger()->Log(denLogger::LogSeverity::info, log.str());
	
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
			GetLogger()->Log(denLogger::LogSeverity::info, log.str());
		}
	}
	SendReliableMessage(message);
}

void derlRemoteClientConnection::pSendStartApplication(const derlRunParameters &parameters){
	std::stringstream log;
	log << "Start application: ";
	GetLogger()->Log(denLogger::LogSeverity::info, log.str());
	
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
	GetLogger()->Log(denLogger::LogSeverity::info, log.str());
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::stopApplication);
		writer.WriteByte((uint8_t)mode);
	}
	SendReliableMessage(message);
}

derlTaskSyncClient::Ref derlRemoteClientConnection::pGetProcessingSyncTask() const{
	const derlTaskSyncClient::Ref &task = pClient->GetTaskSyncClient();
	
	if(!task){
		GetLogger()->Log(denLogger::LogSeverity::warning,
			"Received ResponseFileLayout but no sync task is present");
		return nullptr;
	}
	
	if(task->GetStatus() != derlTaskSyncClient::Status::processing){
		GetLogger()->Log(denLogger::LogSeverity::warning,
			"Received ResponseFileLayout but sync task is not processing");
		return nullptr;
	}
	
	return task;
}
