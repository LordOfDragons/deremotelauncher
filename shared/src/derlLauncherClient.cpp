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

#include "derlLauncherClient.h"
#include "derlGlobal.h"
#include "internal/derlLauncherClientConnection.h"


// Class derlLauncherClient
/////////////////////////////

derlLauncherClient::derlLauncherClient() :
pLogClassName("derlLauncherClient"),
pConnection(std::make_unique<derlLauncherClientConnection>(*this)),
pName("Client"){
}

derlLauncherClient::~derlLauncherClient(){
	StopTaskProcessors();
}


// Management
///////////////

void derlLauncherClient::SetName(const std::string &name){
	pName = name;
}

void derlLauncherClient::SetPathDataDir(const std::filesystem::path &path){
	if(pConnection->GetConnectionState() != denConnection::ConnectionState::disconnected){
		throw std::invalid_argument("is not disconnected");
	}
	
	pPathDataDir = path;
}

int derlLauncherClient::GetPartSize() const{
	return pConnection->GetPartSize();
}

void derlLauncherClient::SetFileLayout(const derlFileLayout::Ref &layout){
	pFileLayout = layout;
}

void derlLauncherClient::SetTaskFileLayout(const derlTaskFileLayout::Ref &task){
	pTaskFileLayout = task;
}

denConnection::ConnectionState derlLauncherClient::GetConnectionState() const{
	return pConnection->GetConnectionState();
}

bool derlLauncherClient::GetConnected() const{
	return pConnection->GetConnected();
}

const denLogger::Ref &derlLauncherClient::GetLogger() const{
	return pConnection->GetLogger();
}

void derlLauncherClient::SetLogger(const denLogger::Ref &logger){
	pConnection->SetLogger(logger);
	if(pTaskProcessor){
		pTaskProcessor->SetLogger(logger);
	}
}

bool derlLauncherClient::GetEnableDebugLog() const{
	return pConnection->GetEnableDebugLog();
}

void derlLauncherClient::SetEnableDebugLog(bool enable){
	pConnection->SetEnableDebugLog(enable);
}



void derlLauncherClient::StartTaskProcessors(){
	if(!pTaskProcessor){
		Log(denLogger::LogSeverity::info, "StartTaskProcessors", "Create task processor");
		pTaskProcessor = std::make_shared<derlTaskProcessorLauncherClient>(*this);
		pTaskProcessor->SetLogger(GetLogger());
	}
	
	if(!pThreadTaskProcessor){
		Log(denLogger::LogSeverity::info, "StartTaskProcessors", "Run task processor thread");
		pThreadTaskProcessor = std::make_unique<std::thread>([](derlTaskProcessorLauncherClient &processor){
			processor.Run();
		}, std::ref(*pTaskProcessor));
	}
}

void derlLauncherClient::StopTaskProcessors(){
	if(pTaskProcessor){
		Log(denLogger::LogSeverity::info, "StopTaskProcessors", "Exit task processor");
		pTaskProcessor->Exit();
	}
	
	if(pThreadTaskProcessor){
		Log(denLogger::LogSeverity::info, "StopTaskProcessors", "Join task processor thread");
		pThreadTaskProcessor->join();
		pThreadTaskProcessor = nullptr;
	}
	pTaskProcessor = nullptr;
}



void derlLauncherClient::ConnectTo(const std::string &address){
	if(pPathDataDir.empty()){
		throw std::invalid_argument("data directory path is empty");
	}
	
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	pConnection->ConnectTo(address);
}

void derlLauncherClient::Disconnect(){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	pConnection->Disconnect();
}

void derlLauncherClient::Update(float elapsed){
	pConnection->SendQueuedMessages();
	
	{
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	pConnection->Update(elapsed);
	}
}

void derlLauncherClient::ProcessReceivedMessages(){
	pConnection->ProcessReceivedMessages();
}

void derlLauncherClient::FinishPendingOperations(){
	pConnection->FinishPendingOperations();
}

void derlLauncherClient::LogException(const std::string &functionName,
const std::exception &exception, const std::string &message){
	std::stringstream ss;
	ss << message << ": " << exception.what();
	Log(denLogger::LogSeverity::error, functionName, ss.str());
}

void derlLauncherClient::Log(denLogger::LogSeverity severity,
const std::string &functionName, const std::string &message){
	if(!GetLogger()){
		return;
	}
	
	std::stringstream ss;
	ss << "[" << pLogClassName << "::" << functionName << "] " << message;
	GetLogger()->Log(severity, ss.str());
}

void derlLauncherClient::LogDebug(const std::string &functionName, const std::string &message){
	if(GetEnableDebugLog()){
		Log(denLogger::LogSeverity::debug, functionName, message);
	}
}

// Events
///////////

void derlLauncherClient::OnConnectionEstablished(){
}

void derlLauncherClient::OnConnectionFailed(denConnection::ConnectionFailedReason reason){
}

void derlLauncherClient::OnConnectionClosed(){
}


// Private Functions
//////////////////////
