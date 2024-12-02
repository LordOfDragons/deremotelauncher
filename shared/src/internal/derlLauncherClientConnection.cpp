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
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	pQueueReceived.PopAll(messages);
	}
	
	for(const denMessage::Ref &message : messages){
		denMessageReader reader(message->Item());
		const derlProtocol::MessageCodes code = (derlProtocol::MessageCodes)reader.ReadByte();
		
		switch(code){
		case derlProtocol::MessageCodes::requestFileLayout:
			pProcessRequestLayout();
			break;
			
		case derlProtocol::MessageCodes::requestFileBlockHashes:
			pProcessRequestFileBlockHashes(reader);
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
			pProcessRequestFinishWriteFile(reader);
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
	
	{
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	messages.clear();
	}
}

void derlLauncherClientConnection::OnFileLayoutChanged(){
	if(!pPendingRequestLayout){
		return;
	}
	
	const derlFileLayout::Ref layout(pClient.GetFileLayoutSync());
	if(layout){
		pPendingRequestLayout = false;
		if(GetConnected()){
			pSendResponseFileLayout(*layout);
		}
		
	}else{
		pClient.AddPendingTaskSync(std::make_shared<derlTaskFileLayout>());
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

void derlLauncherClientConnection::SendResponseFileBlockHashes(
const std::string &path, uint32_t blockSize){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	if(!GetConnected()){
		return;
	}
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseFileBlockHashes);
		writer.WriteString16(path);
		writer.WriteUInt(0);
	}
	pQueueSend.Add(message);
}

void derlLauncherClientConnection::SendResponseFileBlockHashes(const derlFile &file){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	if(!GetConnected()){
		return;
	}
	
	const std::string &path = file.GetPath();
	const int count = file.GetBlockCount();
	int i;
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseFileBlockHashes);
		writer.WriteString16(path);
		writer.WriteUInt((uint32_t)count);
		for(i=0; i<count; i++){
			writer.WriteString8(file.GetBlockAt(i)->GetHash());
		}
		pQueueSend.Add(message);
	}
}

void derlLauncherClientConnection::SendResponseDeleteFile(const derlTaskFileDelete &task){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	if(!GetConnected()){
		return;
	}
	
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

void derlLauncherClientConnection::SendFileDataReceived(const derlTaskFileWriteBlock &block){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	if(!GetConnected()){
		return;
	}
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::fileDataReceived);
		writer.WriteString16(block.GetParentTask().GetPath());
		writer.WriteUInt((uint32_t)block.GetIndex());
		
		if(block.GetStatus() == derlTaskFileWriteBlock::Status::success){
			writer.WriteByte((uint8_t)derlProtocol::FileDataReceivedResult::success);
			
		}else{
			writer.WriteByte((uint8_t)derlProtocol::FileDataReceivedResult::failure);
		}
	}
	pQueueSend.Add(message);
	
	LogDebug("SendFileDataReceived", "Block finished");
}

void derlLauncherClientConnection::SendResponseWriteFile(const derlTaskFileWrite &task){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	if(!GetConnected()){
		return;
	}
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseWriteFile);
		writer.WriteString16(task.GetPath());
		
		if(task.GetStatus() == derlTaskFileWrite::Status::processing){
			writer.WriteByte((uint8_t)derlProtocol::WriteFileResult::success);
			
		}else{
			writer.WriteByte((uint8_t)derlProtocol::WriteFileResult::failure);
		}
	}
	pQueueSend.Add(message);
}

void derlLauncherClientConnection::SendFailResponseWriteFile(const std::string &path){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	if(!GetConnected()){
		return;
	}
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseWriteFile);
		writer.WriteString16(path);
		writer.WriteByte((uint8_t)derlProtocol::WriteFileResult::failure);
	}
	pQueueSend.Add(message);
}

void derlLauncherClientConnection::SendResponseFinishWriteFile(const derlTaskFileWrite &task){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	if(!GetConnected()){
		return;
	}
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseFinishWriteFile);
		writer.WriteString16(task.GetPath());
		
		switch(task.GetStatus()){
		case derlTaskFileWrite::Status::success:
			writer.WriteByte((uint8_t)derlProtocol::FinishWriteFileResult::success);
			break;
			
		case derlTaskFileWrite::Status::validationFailed:
			writer.WriteByte((uint8_t)derlProtocol::FinishWriteFileResult::validationFailed);
			break;
			
		default:
			writer.WriteByte((uint8_t)derlProtocol::FinishWriteFileResult::failure);
		}
	}
	pQueueSend.Add(message);
}

void derlLauncherClientConnection::SendLog(denLogger::LogSeverity severity,
const std::string &source, const std::string &log){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	if(!GetConnected()){
		return;
	}
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::logs);
		
		switch(severity){
		case denLogger::LogSeverity::error:
			writer.WriteByte((uint8_t)derlProtocol::LogLevel::error);
			break;
			
		case denLogger::LogSeverity::warning:
			writer.WriteByte((uint8_t)derlProtocol::LogLevel::warning);
			break;
			
		case denLogger::LogSeverity::info:
		default:
			writer.WriteByte((uint8_t)derlProtocol::LogLevel::info);
			break;
		}
		
		writer.WriteString8(source);
		writer.WriteString16(log);
	}
	pQueueSend.Add(message);
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

void derlLauncherClientConnection::pProcessRequestLayout(){
	Log(denLogger::LogSeverity::info, "pProcessRequestLayout", "Layout request received");
	const derlFileLayout::Ref layout(pClient.GetFileLayoutSync());
	if(layout){
		pPendingRequestLayout = false;
		pSendResponseFileLayout(*layout);
		
	}else{
		pPendingRequestLayout = true;
		{
		const std::lock_guard guard(pClient.GetMutexPendingTasks());
		if(!pClient.HasPendingTasksWithType(derlBaseTask::Type::fileLayout)){
			pClient.GetPendingTasks().push_back(std::make_shared<derlTaskFileLayout>());
		}
		}
		pClient.NotifyPendingTaskAdded();
	}
}

void derlLauncherClientConnection::pProcessRequestFileBlockHashes(denMessageReader &reader){
	const std::string path(reader.ReadString16());
	const uint32_t blockSize = reader.ReadUInt();
	
	{
	std::stringstream ss;
	ss << "Calculate file block hashes received: " << path << " blockSize " << (int)blockSize;
	Log(denLogger::LogSeverity::info, "pProcessRequestFileBlockHashes", ss.str());
	}
	
	const derlFileLayout::Ref layout(pClient.GetFileLayoutSync());
	if(!layout){
		std::stringstream log;
		log << "Block hashes for file requested but file layout is not present: "
			<< path << ". Answering with empty file.";
		Log(denLogger::LogSeverity::warning, "pProcessRequestFileBlockHashes", log.str());
		SendResponseFileBlockHashes(path);
		return;
	}
	
	derlFile::Ref file(layout->GetFileAtSync(path));
	if(!file){
		std::stringstream log;
		log << "Block hashes for non-existing file requested: "
			<< path << ". Answering with empty file.";
		Log(denLogger::LogSeverity::warning, "pProcessRequestFileBlockHashes", log.str());
		SendResponseFileBlockHashes(path);
		return;
	}
	
	if(file->GetHasBlocks() && file->GetBlockSize() == blockSize){
		SendResponseFileBlockHashes(*file);
		
	}else{
		file = std::make_shared<derlFile>(*file);
		file->RemoveAllBlocks();
		file->SetBlockSize(blockSize);
		layout->SetFileAtSync(path, file);
		
		pClient.AddPendingTaskSync(std::make_shared<derlTaskFileBlockHashes>(path, blockSize));
	}
}

void derlLauncherClientConnection::pProcessRequestDeleteFile(denMessageReader &reader){
	const std::string path(reader.ReadString16());
	
	{
	std::stringstream ss;
	ss << "Delete file request received: " << path;
	Log(denLogger::LogSeverity::info, "pProcessRequestDeleteFile", ss.str());
	}
	
	pClient.AddPendingTaskSync(std::make_shared<derlTaskFileDelete>(path));
}

void derlLauncherClientConnection::pProcessRequestWriteFile(denMessageReader &reader){
	const std::string path(reader.ReadString16());
	
	{
	std::stringstream ss;
	ss << "Write file request received: " << path;
	Log(denLogger::LogSeverity::info, "pProcessRequestWriteFile", ss.str());
	}
	
	const derlFileLayout::Ref layout(pClient.GetFileLayoutSync());
	if(!layout){
		std::stringstream log;
		log << "Write file requested but file layout is not present: " << path;
		Log(denLogger::LogSeverity::warning, "pProcessRequestWriteFile", log.str());
		pClient.SetDirtyFileLayoutSync(true);
		SendFailResponseWriteFile(path);
		return;
	}
	
	const derlFile::Ref file(layout->GetFileAtSync(path));
	
	const derlTaskFileWrite::Ref task(std::make_shared<derlTaskFileWrite>(path));
	task->SetFileSize(reader.ReadULong());
	task->SetBlockSize(reader.ReadULong());
	task->SetBlockCount((int)reader.ReadUInt());
	task->SetTruncate(file && file->GetSize() != task->GetFileSize());
	
	pWriteFileTasks[path] = task;
	pClient.AddPendingTaskSync(task);
	
	if(pEnableDebugLog){
		std::stringstream log;
		log << "Request write file received: " << path << " fileSize " << task->GetFileSize()
			<< " blockSize " << task->GetBlockSize() << " blockCount " << task->GetBlockCount()
			<< " truncate " << task->GetTruncate();
		LogDebug("pProcessRequestWriteFile", log.str());
	}
}

void derlLauncherClientConnection::pProcessSendFileData(denMessageReader &reader){
	const std::string path(reader.ReadString16());
	const int indexBlock = reader.ReadUInt();
	
	const uint64_t size = (uint64_t)(reader.GetLength() - reader.GetPosition());
	
	const derlTaskFileWrite::Map::const_iterator iterWrite(pWriteFileTasks.find(path));
	if(iterWrite == pWriteFileTasks.cend()){
		std::stringstream log;
		log << "Send file data received but task does not exist: " << path;
		Log(denLogger::LogSeverity::warning, "pProcessSendFileData", log.str());
		return; // ignore
	}
	
	derlTaskFileWrite &taskWrite = *iterWrite->second;
	if(indexBlock < 0 || indexBlock >= taskWrite.GetBlockCount()){
		std::stringstream log;
		log << "Send file data received but block index is out of range: "
			<< path << " index " << indexBlock << " count " << taskWrite.GetBlockCount();
		Log(denLogger::LogSeverity::warning, "pProcessSendFileData", log.str());
		taskWrite.SetStatus(derlTaskFileWrite::Status::failure);
		return;
	}
	
	const uint64_t blockOffset = taskWrite.GetBlockSize() * indexBlock;
	const uint64_t blockSize = std::min(taskWrite.GetBlockSize(), taskWrite.GetFileSize() - blockOffset);
	
	derlTaskFileWriteBlock::Ref taskBlock(std::make_shared<derlTaskFileWriteBlock>(
		taskWrite, indexBlock, blockSize));
	taskBlock->GetData().assign(blockSize, 0);
	taskBlock->SetStatus(derlTaskFileWriteBlock::Status::dataReady);
	reader.Read((void*)taskBlock->GetData().c_str(), size);
	
	pClient.AddPendingTaskSync(taskBlock);
	
	if(pEnableDebugLog){
		std::stringstream log;
		log << "Send file data received: " << path << " block " << indexBlock;
		LogDebug("pProcessSendFileData", log.str());
	}
}

void derlLauncherClientConnection::pProcessRequestFinishWriteFile(denMessageReader &reader){
	const std::string path(reader.ReadString16());
	{
	std::stringstream ss;
	ss << "Finish write file request received: " << path;
	Log(denLogger::LogSeverity::info, "pProcessRequestFinishWriteFile", ss.str());
	}
	
	const derlTaskFileWrite::Map::const_iterator iterTask(pWriteFileTasks.find(path));
	if(iterTask == pWriteFileTasks.cend()){
		std::stringstream log;
		log << "Finish write file request received but no task is present: " << path;
		Log(denLogger::LogSeverity::warning, "pProcessRequestFinishWriteFile", log.str());
		return;
	}
	
	{
	derlTaskFileWrite &task = *iterTask->second;
	if(task.GetStatus() != derlTaskFileWrite::Status::processing){
		std::stringstream log;
		log << "Finish write file request received task is not processing: " << path;
		Log(denLogger::LogSeverity::warning, "pProcessRequestFinishWriteFile", log.str());
		return;
	}
	
	task.SetHash(reader.ReadString8());
	task.SetStatus(derlTaskFileWrite::Status::finishing);
	}
	
	pClient.AddPendingTaskSync(iterTask->second);
	
	pWriteFileTasks.erase(iterTask);
}

void derlLauncherClientConnection::pProcessStartApplication(denMessageReader &reader){
	derlRunParameters params;
	params.SetGameConfig(reader.ReadString16());
	params.SetProfileName(reader.ReadString8());
	params.SetArguments(reader.ReadString16());
	
	Log(denLogger::LogSeverity::info, "pProcessStartApplication", "Start application request received");
	pClient.StartApplication(params);
}

void derlLauncherClientConnection::pProcessStopApplication(denMessageReader &reader){
	const derlProtocol::StopApplicationMode mode = (derlProtocol::StopApplicationMode)reader.ReadByte();
	
	switch(mode){
	case derlProtocol::StopApplicationMode::requestClose:
		Log(denLogger::LogSeverity::info, "pProcessStopApplication", "Stop application request received => stop");
		pClient.StopApplication();
		break;
		
	case derlProtocol::StopApplicationMode::killProcess:
		Log(denLogger::LogSeverity::info, "pProcessStopApplication", "Stop application request received => kill");
		pClient.KillApplication();
		break;
	}
}

void derlLauncherClientConnection::pSendResponseFileLayout(const derlFileLayout &layout){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	if(!GetConnected()){
		return;
	}

	if(layout.GetFileCount() == 0){
		const denMessage::Ref message(denMessage::Pool().Get());
		{
			denMessageWriter writer(message->Item());
			writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseFileLayout);
			writer.WriteUInt(0);
		}
		pQueueSend.Add(message);
		return;
	}
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
	denMessageWriter writer(message->Item());
	writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseFileLayout);
	
	const int count = layout.GetFileCount();
	writer.WriteUInt((uint32_t)count);
	
	derlFile::Map::const_iterator iter;
	for(iter=layout.GetFilesBegin(); iter!=layout.GetFilesEnd(); iter++){
		const derlFile &file = *iter->second.get();
		
		writer.WriteString16(file.GetPath());
		writer.WriteULong(file.GetSize());
		writer.WriteString8(file.GetHash());
	}
	
	}
	pQueueSend.Add(message);
}
