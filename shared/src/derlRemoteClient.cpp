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
#include "internal/derlRemoteClientConnection.h"


// Class derlRemoteClient
///////////////////////////

derlRemoteClient::derlRemoteClient(derlServer &server, derlRemoteClientConnection *connection) :
pServer(server),
pConnection(connection){
}

derlRemoteClient::~derlRemoteClient(){
	StopTaskProcessors();
}


// Management
///////////////

void derlRemoteClient::DropConnection(){
	pConnection = nullptr;
}

const std::string &derlRemoteClient::GetName() const{
	if(pConnection){
		return pConnection->GetName();
		
	}else{
		static const std::string name;
		return name;
	}
}

void derlRemoteClient::SetFileLayout(const derlFileLayout::Ref &layout){
	pFileLayout = layout;
}

void derlRemoteClient::SetTaskSyncClient(const derlTaskSyncClient::Ref &task){
	pTaskSyncClient = task;
}

const denLogger::Ref &derlRemoteClient::GetLogger() const{
	return pLogger;
}

void derlRemoteClient::SetLogger(const denLogger::Ref &logger){
	pLogger = logger;
	if(pConnection){
		pConnection->SetLogger(logger);
	}
	if(pTaskProcessor){
		pTaskProcessor->SetLogger(logger);
	}
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
	if(pConnection){
		pConnection->Disconnect();
	}
}

void derlRemoteClient::Update(float elapsed){
	if(pConnection){
		pConnection->Update(elapsed);
		pConnection->FinishPendingOperations();
	}
}


// Events
///////////

void derlRemoteClient::OnConnectionEstablished(){
}

void derlRemoteClient::OnConnectionClosed(){
}
