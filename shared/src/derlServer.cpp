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
#include <stdexcept>

#include "derlServer.h"
#include "derlGlobal.h"
#include "internal/derlServerServer.h"


// Class derlServer
/////////////////////

derlServer::derlServer() :
pServer(std::make_unique<derlServerServer>(*this)){
}

derlServer::~derlServer(){
	StopTaskProcessors();
}


// Management
///////////////

void derlServer::SetPathDataDir(const std::filesystem::path &path){
	if(pServer->IsListening()){
		throw std::invalid_argument("is listening");
	}
	
	pPathDataDir = path;
}

const denLogger::Ref &derlServer::GetLogger() const{
	return pServer->GetLogger();
}

void derlServer::SetLogger(const denLogger::Ref &logger){
	pServer->SetLogger(logger);
	if(pTaskProcessor){
		pTaskProcessor->SetLogger(logger);
	}
}

derlRemoteClient::Ref derlServer::CreateClient(const derlRemoteClientConnection::Ref &connection){
	return std::make_shared<derlRemoteClient>(*this, connection);
}

void derlServer::StartTaskProcessors(){
	if(!pTaskProcessor){
		pTaskProcessor = std::make_shared<derlTaskProcessorServer>(*this);
		pTaskProcessor->SetLogger(GetLogger());
	}
	
	if(!pThreadTaskProcessor){
		pThreadTaskProcessor = std::make_unique<std::thread>([](derlTaskProcessorServer &processor){
			processor.Run();
		}, std::ref(*pTaskProcessor));
	}
}

void derlServer::StopTaskProcessors(){
	if(pTaskProcessor){
		pTaskProcessor->Exit();
	}
	
	if(pThreadTaskProcessor){
		pThreadTaskProcessor->join();
		pThreadTaskProcessor = nullptr;
	}
	pTaskProcessor = nullptr;
}

void derlServer::ListenOn(const std::string &address){
	if(pPathDataDir.empty()){
		throw std::invalid_argument("data directory path is empty");
	}
	
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	pServer->ListenOn(address);
}

void derlServer::StopListening(){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	pServer->StopListening();
}

void derlServer::Update(float elapsed){
	{
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	pServer->Update(elapsed);
	}
	
	derlRemoteClient::List closed;
	derlRemoteClient::List::const_iterator iter;
	for(iter=pClients.cbegin(); iter!=pClients.cend(); iter++){
		(*iter)->Update(elapsed);
		if(!(*iter)->GetConnected()){
			closed.push_back(*iter);
		}
	}
	
	for(iter=closed.cbegin(); iter!=closed.cend(); iter++){
		pClients.erase(std::find(pClients.cbegin(), pClients.cend(), *iter));
	}
}

// Events
///////////

