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
	if(pClient){
		pClient->DropConnection();
	}
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
	
	pClient->DropConnection();
	
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
		
		const derlRemoteClient::Ref client(pServer.CreateClient(this));
		pServer.GetClients().push_back(client);
		pClient = client.get();
		
		pClient->OnConnectionEstablished();
		return;
	}
	
	switch(code){
	case derlProtocol::MessageCodes::logs:
		pProcessRequestLogs(reader);
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
