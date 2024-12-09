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

#include "derlLauncherClient.h"
#include "derlGlobal.h"
#include "derlProtocol.h"
#include "internal/derlLauncherClientConnection.h"


// Class derlLauncherClient
/////////////////////////////

derlLauncherClient::derlLauncherClient() :
pLogClassName("derlLauncherClient"),
pConnection(std::make_unique<derlLauncherClientConnection>(*this)),
pName("Client"),
pDirtyFileLayout(false),
pStartTaskProcessorCount(1),
pTaskProcessorsRunning(false),
pKeepAliveInterval(10.0f),
pKeepAliveElapsed(0.0f){
}

derlLauncherClient::~derlLauncherClient(){
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

derlFileLayout::Ref derlLauncherClient::GetFileLayoutSync(){
	const std::lock_guard guard(pMutex);
	return pFileLayout;
}

void derlLauncherClient::SetFileLayoutSync(const derlFileLayout::Ref &layout){
	const std::lock_guard guard(pMutex);
	pNextFileLayout = layout;
}

void derlLauncherClient::SetDirtyFileLayoutSync(bool dirty){
	const std::lock_guard guard(pMutex);
	pDirtyFileLayout = dirty;
}

void derlLauncherClient::RemovePendingTaskWithType(derlBaseTask::Type type){
	const std::lock_guard guard(pMutexPendingTasks);
	derlBaseTask::Queue filtered;
	for(const derlBaseTask::Ref &task : pPendingTasks){
		if(task->GetType() != type){
			filtered.push_back(task);
		}
	}
	pPendingTasks.swap(filtered);
}

bool derlLauncherClient::HasPendingTasksWithType(derlBaseTask::Type type){
	return std::find_if(pPendingTasks.cbegin(), pPendingTasks.cend(),
		[type](const derlBaseTask::Ref &task){
			return task->GetType() == type;
		}) != pPendingTasks.cend();
}

void derlLauncherClient::AddPendingTaskSync(const derlBaseTask::Ref &task){
	{
	const std::lock_guard guard(pMutexPendingTasks);
	pPendingTasks.push_back(task);
	}
	NotifyPendingTaskAdded();
}

void derlLauncherClient::NotifyPendingTaskAdded(){
	pConditionPendingTasks.notify_all();
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
	for(const derlTaskProcessorLauncherClient::Ref &processor : pTaskProcessors){
		processor->SetLogger(logger);
	}
}

bool derlLauncherClient::GetEnableDebugLog() const{
	return pConnection->GetEnableDebugLog();
}

void derlLauncherClient::SetEnableDebugLog(bool enable){
	pConnection->SetEnableDebugLog(enable);
}

derlLauncherClient::RunStatus derlLauncherClient::GetRunStatus() const{
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	switch((derlProtocol::RunStateStatus)pConnection->GetValueRunStatus()->GetValue()){
	case derlProtocol::RunStateStatus::running:
		return RunStatus::running;
		
	case derlProtocol::RunStateStatus::stopped:
	default:
		return RunStatus::stopped;
	}
}

void derlLauncherClient::SetRunStatus(RunStatus status){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	switch(status){
	case RunStatus::running:
		pConnection->GetValueRunStatus()->SetValue((uint64_t)derlProtocol::RunStateStatus::running);
		break;
		
	case RunStatus::stopped:
	default:
		pConnection->GetValueRunStatus()->SetValue((uint64_t)derlProtocol::RunStateStatus::stopped);
	}
}

void derlLauncherClient::StartTaskProcessors(){
	if(pTaskProcessors.empty()){
		Log(denLogger::LogSeverity::info, "StartTaskProcessors", "Create task processors");
		int i;
		for(i=0; i<pStartTaskProcessorCount; i++){
			const derlTaskProcessorLauncherClient::Ref processor(CreateTaskProcessor());
			processor->SetLogger(GetLogger());
			pTaskProcessors.push_back(processor);
		}
	}
	
	if(pTaskProcessorThreads.empty()){
		Log(denLogger::LogSeverity::info, "StartTaskProcessors", "Run task processor threads");
		for(const derlTaskProcessorLauncherClient::Ref &processor : pTaskProcessors){
			pTaskProcessorThreads.push_back(std::make_shared<std::thread>(
				[&](derlTaskProcessorLauncherClient::Ref processor){
					processor->Run();
				}, processor));
		}
	}
}

derlTaskProcessorLauncherClient::Ref derlLauncherClient::CreateTaskProcessor(){
	return std::make_shared<derlTaskProcessorLauncherClient>(*this);
}

void derlLauncherClient::StopTaskProcessors(){
	if(!pTaskProcessors.empty()){
		Log(denLogger::LogSeverity::info, "StopTaskProcessors", "Exit task processors");
		for(const derlTaskProcessorLauncherClient::Ref &processor : pTaskProcessors){
			processor->Exit();
		}
	}
	
	NotifyPendingTaskAdded();
	
	if(!pTaskProcessorThreads.empty()){
		Log(denLogger::LogSeverity::info, "StopTaskProcessors", "Join task processor threads");
		for(const std::shared_ptr<std::thread> &thread : pTaskProcessorThreads){
			thread->join();
		}
		pTaskProcessorThreads.clear();
	}
	
	pTaskProcessors.clear();
}



void derlLauncherClient::ConnectTo(const std::string &address){
	if(pPathDataDir.empty()){
		throw std::invalid_argument("data directory path is empty");
	}
	
	StartTaskProcessors();
	pTaskProcessorsRunning = true;
	
	try{
		const std::lock_guard guard(derlGlobal::mutexNetwork);
		pConnection->ConnectTo(address);
		
	}catch(...){
		StopTaskProcessors();
		pTaskProcessorsRunning = false;
		throw;
	}
}

void derlLauncherClient::Disconnect(){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	pConnection->Disconnect();
}

void derlLauncherClient::Update(float elapsed){
	UpdateLayoutChanged();
	
	pConnection->SendQueuedMessages();
	
	if(pConnection->ProcessReceivedMessages()){
		pKeepAliveElapsed = 0.0f;
		
	}else{
		pKeepAliveElapsed += elapsed;
		if(pKeepAliveElapsed >= pKeepAliveInterval){
			pKeepAliveElapsed = 0.0f;
			pConnection->SendKeepAlive();
		}
	}
	
	{
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	pConnection->Update(elapsed);
	}
	
	if(pTaskProcessorsRunning
	&& pConnection->GetConnectionState() == denConnection::ConnectionState::disconnected){
		StopTaskProcessors();
		pTaskProcessorsRunning = false;
	}
}

void derlLauncherClient::UpdateLayoutChanged(){
	bool notifyLayoutChanged = false;
	
	{
	const std::lock_guard guard(pMutex);
	if(pDirtyFileLayout && pFileLayout){
		Log(denLogger::LogSeverity::info, "Update", "File layout dirty, dropped.");
		pNextFileLayout = nullptr;
		pDirtyFileLayout = false;
	}
	
	if(pNextFileLayout != pFileLayout){
		pFileLayout = pNextFileLayout;
		pDirtyFileLayout = false;
		notifyLayoutChanged = true;
	}
	}
	
	if(notifyLayoutChanged){
		pConnection->OnFileLayoutChanged();
	}
}

std::unique_ptr<std::string> derlLauncherClient::GetSystemProperty(const std::string &property){
	return std::make_unique<std::string>();
}

void derlLauncherClient::SendLog(denLogger::LogSeverity severity,
const std::string &source, const std::string &log){
	pConnection->SendLog(severity, source, log);
}

void derlLauncherClient::SendSystemProperty(const std::string &property, const std::string &value){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	pConnection->SendResponseSystemPropertyNoLock(property, value);
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
