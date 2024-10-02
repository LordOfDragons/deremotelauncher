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

#include <sstream>

#include "derlLauncherClientConnection.h"
#include "../derlLauncherClient.h"
#include "../derlProtocol.h"

#include <denetwork/message/denMessage.h>
#include <denetwork/message/denMessageReader.h>
#include <denetwork/message/denMessageWriter.h>


#define DERL_MAX_UINT_SIZE 0xffffffff
#define DERL_MAX_UINT ((int)DERL_MAX_UINT_SIZE)


// Class derlLauncherClientConnection
///////////////////////////////////////

derlLauncherClientConnection::derlLauncherClientConnection(derlLauncherClient &client) :
pClient(client),
pConnectionAccepted(false),
pEnabledFeatures(0){
}

derlLauncherClientConnection::~derlLauncherClientConnection(){
}


// Management
///////////////

void derlLauncherClientConnection::ConnectionEstablished(){
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		uint32_t supportedFeatures = 0;
		
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::connectRequest);
		writer.Write("DERemLaunchCnt-0", 16);
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
			if(signature != "DERemLaunchSrv-0"){
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
		
		return nullptr;
		
	default:
		return nullptr; // ignore all other messages
	}
}

// Private Functions
//////////////////////

void derlLauncherClientConnection::pProcessRequestLayout(){
	if(!pClient.GetFileLayout()){
		// TODO request update
		return;
	}
	
	pSendResponseFileLayout(*pClient.GetFileLayout());
}

void derlLauncherClientConnection::pProcessRequestFileBlockHashes(denMessageReader &reader){
	if(!pClient.GetFileLayout()){
		// TODO request update
		return;
	}
	
	const std::string path(reader.ReadString16());
	const derlFile::Ref file(pClient.GetFileLayout()->GetFileAt(path));
	if(!file){
		std::stringstream log;
		log << "Block hashes for non-existing file requested: "
			<< path << ". Answering with empty file.";
		GetLogger()->Log(denLogger::LogSeverity::warning, log.str());
		pSendResponseFileBlockHashes(path);
		return;
	}
	
	const uint32_t blockSize = reader.ReadUInt();
	if(!file->GetHasBlocks() || file->GetBlockSize() != blockSize){
		file->RemoveAllBlocks();
		file->SetBlockSize(blockSize);
		// TODO request blocks
		return;
	}
	
	pSendResponseFileBlockHashes(*file);
}

void derlLauncherClientConnection::pProcessRequestDeleteFiles(denMessageReader &reader){
	derlFileOperation::Map &deleteFiles = pClient.GetFileOpsDeleteFiles();
	const int count = reader.ReadUInt();
	int i;
	
	for(i=0; i<count; i++){
		const std::string path(reader.ReadString16());
		deleteFiles[path] = std::make_shared<derlFileOperation>(path);
	}
}

void derlLauncherClientConnection::pProcessRequestWriteFiles(denMessageReader &reader){
	derlFileOperation::Map &writeFiles = pClient.GetFileOpsWriteFiles();
	const int count = reader.ReadUInt();
	int i;
	
	for(i=0; i<count; i++){
		const derlFileOperation::Ref fileop(
			std::make_shared<derlFileOperation>(reader.ReadString16()));
		fileop->SetFileSize(reader.ReadULong());
		writeFiles[fileop->GetPath()] = fileop;
	}
}

void derlLauncherClientConnection::pProcessSendFileData(denMessageReader &reader){
	const std::string path(reader.ReadString16());
	
	const derlFileOperation::Map::const_iterator iter(pClient.GetFileOpsWriteFiles().find(path));
	if(iter == pClient.GetFileOpsWriteFiles().cend()){
		return; // ignore
	}
	
	const derlFileOperation::Ref fileop(iter->second);
	const uint64_t offset = reader.ReadULong();
	const uint64_t size = (uint64_t)(reader.GetLength() - reader.GetPosition());
	
	(void)offset; (void)size;
	// reader.Read(buffer, size)
}

void derlLauncherClientConnection::pProcessRequestFinishWriteFiles(denMessageReader &reader){
	derlFileOperation::Map &writeFiles = pClient.GetFileOpsWriteFiles();
	derlFileOperation::List finishedFiles;
	const int count = reader.ReadUInt();
	int i;
	
	for(i=0; i<count; i++){
		const std::string path(reader.ReadString16());
		const derlFileOperation::Map::iterator iter(writeFiles.find(path));
		
		if(iter != writeFiles.end()){
			const derlFileOperation::Ref fileop(iter->second);
			if(fileop->GetStatus() == derlFileOperation::Status::pending){
				fileop->SetStatus(derlFileOperation::Status::success);
			}
			finishedFiles.push_back(fileop);
			writeFiles.erase(iter);
			
		}else{
			const derlFileOperation::Ref fileop(std::make_shared<derlFileOperation>(path));
			fileop->SetStatus(derlFileOperation::Status::failure);
			finishedFiles.push_back(fileop);
		}
	}
	
	pSendResponseFinishWriteFiles(finishedFiles);
}

void derlLauncherClientConnection::pProcessStartApplication(denMessageReader &reader){
	const std::string gameConfig(reader.ReadString16());
	const std::string profileName(reader.ReadString8());
	const std::string arguments(reader.ReadString16());
	
}

void derlLauncherClientConnection::pProcessStopApplication(denMessageReader &reader){
	const derlProtocol::StopApplicationMode mode = (derlProtocol::StopApplicationMode)reader.ReadByte();
	(void)mode;
}

void derlLauncherClientConnection::pSendResponseFileLayout(const derlFileLayout &layout){
	if(layout.GetFileCount() > DERL_MAX_UINT){
		throw std::runtime_error("Too many files in layout");
	}
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseFileLayout);
		writer.WriteUInt((uint32_t)layout.GetFileCount());
		
		derlFileLayout::MapFiles::const_iterator iter;
		for(iter=layout.GetFilesBegin(); iter!=layout.GetFilesEnd(); iter++){
			const derlFile &file = *iter->second.get();
			writer.WriteString16(file.GetPath());
			writer.WriteULong(file.GetSize());
			writer.WriteString8(file.GetHash());
		}
	}
	SendReliableMessage(message);
}

void derlLauncherClientConnection::pSendResponseFileBlockHashes(const std::string &path, uint32_t blockSize){
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
	if(count > DERL_MAX_UINT){
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

void derlLauncherClientConnection::pSendResponseDeleteFiles(const derlFileOperation::Map &files){
	if(files.size() > DERL_MAX_UINT_SIZE){
		throw std::runtime_error("Too many files");
	}
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseDeleteFiles);
		writer.WriteUInt((uint32_t)files.size());
		
		derlFileOperation::Map::const_iterator iter;
		for(iter=files.cbegin(); iter!=files.cend(); iter++){
			const derlFileOperation &fileop = *iter->second.get();
			writer.WriteString16(fileop.GetPath());
			
			switch(fileop.GetStatus()){
			case derlFileOperation::Status::success:
				writer.WriteByte((uint8_t)derlProtocol::DeleteFileResult::success);
				break;
				
			case derlFileOperation::Status::failure:
			default:
				writer.WriteByte((uint8_t)derlProtocol::DeleteFileResult::failure);
			}
		}
	}
	SendReliableMessage(message);
}

void derlLauncherClientConnection::pSendResponseWriteFiles(const derlFileOperation::Map &files){
	if(files.size() > DERL_MAX_UINT_SIZE){
		throw std::runtime_error("Too many files");
	}
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseWriteFiles);
		writer.WriteUInt((uint32_t)files.size());
		
		derlFileOperation::Map::const_iterator iter;
		for(iter=files.cbegin(); iter!=files.cend(); iter++){
			const derlFileOperation &fileop = *iter->second.get();
			writer.WriteString16(fileop.GetPath());
			
			switch(fileop.GetStatus()){
			case derlFileOperation::Status::success:
			case derlFileOperation::Status::pending:
				writer.WriteByte((uint8_t)derlProtocol::WriteFileResult::success);
				break;
				
			case derlFileOperation::Status::failure:
			default:
				writer.WriteByte((uint8_t)derlProtocol::WriteFileResult::failure);
			}
		}
	}
	SendReliableMessage(message);
}

void derlLauncherClientConnection::pSendResponseFinishWriteFiles(const derlFileOperation::List &files){
	if(files.size() > DERL_MAX_UINT_SIZE){
		throw std::runtime_error("Too many files");
	}
	
	const denMessage::Ref message(denMessage::Pool().Get());
	{
		denMessageWriter writer(message->Item());
		writer.WriteByte((uint8_t)derlProtocol::MessageCodes::responseFinishWriteFiles);
		writer.WriteUInt((uint32_t)files.size());
		
		derlFileOperation::List::const_iterator iter;
		for(iter=files.cbegin(); iter!=files.cend(); iter++){
			const derlFileOperation &fileop = **iter;
			writer.WriteString16(fileop.GetPath());
			
			switch(fileop.GetStatus()){
			case derlFileOperation::Status::success:
				writer.WriteByte((uint8_t)derlProtocol::FinishWriteFileResult::success);
				break;
				
			case derlFileOperation::Status::pending:
			case derlFileOperation::Status::failure:
			default:
				writer.WriteByte((uint8_t)derlProtocol::FinishWriteFileResult::failure);
			}
		}
	}
	SendReliableMessage(message);
}
