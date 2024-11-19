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

#include "derlRemoteClient.h"
#include "derlServer.h"
#include "internal/derlRemoteClientConnection.h"


// Class derlRemoteClient
///////////////////////////

derlRemoteClient::derlRemoteClient(derlServer &server,
	const derlRemoteClientConnection::Ref &connection) :
pServer(server),
pConnection(connection),
pSynchronizeStatus(SynchronizeStatus::pending){
}

derlRemoteClient::~derlRemoteClient(){
	StopTaskProcessors();
}


// Management
///////////////

const std::string &derlRemoteClient::GetName() const{
	return pConnection->GetName();
}

void derlRemoteClient::SetFileLayoutServer(const derlFileLayout::Ref &layout){
	pFileLayoutServer = layout;
}

void derlRemoteClient::SetFileLayoutClient(const derlFileLayout::Ref &layout){
	pFileLayoutClient = layout;
}

void derlRemoteClient::SetTaskFileLayoutServer(const derlTaskFileLayout::Ref &task){
	pTaskFileLayoutServer = task;
}

void derlRemoteClient::SetTaskFileLayoutClient(const derlTaskFileLayout::Ref &task){
	pTaskFileLayoutClient = task;
}

void derlRemoteClient::SetTaskSyncClient(const derlTaskSyncClient::Ref &task){
	pTaskSyncClient = task;
}

int derlRemoteClient::GetPartSize() const{
	return pConnection->GetPartSize();
}

const denLogger::Ref &derlRemoteClient::GetLogger() const{
	return pLogger;
}

void derlRemoteClient::SetLogger(const denLogger::Ref &logger){
	pLogger = logger;
	pConnection->SetLogger(logger);
	if(pTaskProcessor){
		pTaskProcessor->SetLogger(logger);
	}
}

void derlRemoteClient::SetSynchronizeDetails(const std::string &details){
	pSynchronizeDetails = details;
}

void derlRemoteClient::Synchronize(){
	{
	std::lock_guard guard(pMutex);
	
	{
	derlServer &server = GetServer();
	std::lock_guard guard2(server.GetMutex());
	pPathDataDir = server.GetPathDataDir();
	}
	
	pSynchronizeDetails.clear();
	pSynchronizeStatus = SynchronizeStatus::processing;
	
	pFileLayoutServer = nullptr;
	if(/*!pFileLayoutServer &&*/ !pTaskFileLayoutServer){
		pTaskFileLayoutServer = std::make_shared<derlTaskFileLayout>();
	}
	
	if(!pFileLayoutClient && !pTaskFileLayoutClient){
		pTaskFileLayoutClient = std::make_shared<derlTaskFileLayout>();
	}
	
	if(pTaskSyncClient && pTaskSyncClient->GetStatus() == derlTaskSyncClient::Status::failure){
		pTaskSyncClient = nullptr;
	}
	
	if(!pTaskSyncClient){
		pTaskSyncClient = std::make_shared<derlTaskSyncClient>();
	}
	}
	
	OnSynchronizeBegin();
}

void derlRemoteClient::StartTaskProcessors(){
	if(!pTaskProcessor){
		pTaskProcessor = std::make_shared<derlTaskProcessorRemoteClient>(*this);
		pTaskProcessor->SetLogger(GetLogger());
	}
	
	if(!pThreadTaskProcessor){
		pThreadTaskProcessor = std::make_unique<std::thread>([](derlTaskProcessorRemoteClient &processor){
			processor.Run();
		}, std::ref(*pTaskProcessor));
	}
}

void derlRemoteClient::StopTaskProcessors(){
	if(pTaskProcessor){
		pTaskProcessor->Exit();
	}
	
	if(pThreadTaskProcessor){
		pThreadTaskProcessor->join();
		pThreadTaskProcessor = nullptr;
	}
	pTaskProcessor = nullptr;
}



void derlRemoteClient::Disconnect(){
	pConnection->Disconnect();
}

void derlRemoteClient::Update(float elapsed){
	std::lock_guard guard(pConnection->GetMutex());
	pConnection->SendQueuedMessages();
	pConnection->Update(elapsed);
}

void derlRemoteClient::ProcessReceivedMessages(){
	pConnection->ProcessReceivedMessages();
}

void derlRemoteClient::FinishPendingOperations(){
	pConnection->FinishPendingOperations();
	
	bool sendEventSyncUpdate = false;
	bool sendEventSyncEnd = false;
	
	{
	std::lock_guard guard(pMutex);
	
	if(pTaskSyncClient){
		pProcessTaskSyncClient(*pTaskSyncClient);
		
		if(pTaskSyncClient){
			sendEventSyncUpdate = true;
			
		}else{
			sendEventSyncEnd = true;
		}
	}
	}
	
	if(sendEventSyncUpdate){
		OnSynchronizeUpdate();
		
	}else if(sendEventSyncEnd){
		OnSynchronizeFinished();
	}
}

// Events
///////////

void derlRemoteClient::OnConnectionEstablished(){
}

void derlRemoteClient::OnConnectionClosed(){
}

void derlRemoteClient::OnSynchronizeBegin(){
}

void derlRemoteClient::OnSynchronizeUpdate(){
}

void derlRemoteClient::OnSynchronizeFinished(){
}

// Private Functions
//////////////////////

void derlRemoteClient::pProcessTaskSyncClient(derlTaskSyncClient &task){
	if(!pFileLayoutServer || !pFileLayoutClient){
		pSynchronizeDetails = "Scanning file systems...";
	}
	
	switch(task.GetStatus()){
	case derlTaskSyncClient::Status::success:
		pSynchronizeDetails = "Synchronized.";
		pSynchronizeStatus = SynchronizeStatus::ready;
		pTaskSyncClient = nullptr;
		break;
		
	case derlTaskSyncClient::Status::failure:
		if(task.GetError().empty()){
			pSynchronizeDetails = "Synchronize failed.";
			
		}else{
			pSynchronizeDetails = task.GetError();
		}
		pSynchronizeStatus = SynchronizeStatus::failed;
		pTaskSyncClient = nullptr;
		break;
		
	default:
		pSynchronizeDetails = "Synchronize...";
		break;
	}
}
