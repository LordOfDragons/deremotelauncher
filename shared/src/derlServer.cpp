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

derlServer::~derlServer() noexcept{
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
}

derlRemoteClient::Ref derlServer::CreateClient(const derlRemoteClientConnection::Ref &connection){
	return std::make_shared<derlRemoteClient>(*this, connection);
}

bool derlServer::IsListening() const{
	return pServer->IsListening();
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

void derlServer::WaitAllClientsDisconnected(){
	std::chrono::steady_clock::time_point last(std::chrono::steady_clock::now());
	while(!pClients.empty()){
		std::chrono::steady_clock::time_point now(std::chrono::steady_clock::now());
		const int64_t elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(now - last).count();
		
		if(elapsed_us > 10){
			last = now;
			Update((float)elapsed_us / 1e6f);
		}
	}
}

void derlServer::Update(float elapsed){
	{
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	pServer->Update(elapsed);
	}
	
	derlRemoteClient::List closed;
	for(const derlRemoteClient::Ref &each : pClients){
		each->Update(elapsed);
		if(each->GetDisconnected()){
			each->StopTaskProcessors();
			closed.push_back(each);
		}
	}
	
	for(const derlRemoteClient::Ref &each : closed){
		pClients.erase(std::find(pClients.cbegin(), pClients.cend(), each));
	}
}

// Events
///////////

