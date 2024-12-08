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

#include "derlRemoteClient.h"
#include "derlServer.h"
#include "derlGlobal.h"
#include "internal/derlRemoteClientConnection.h"


// Class derlRemoteClient
///////////////////////////

derlRemoteClient::derlRemoteClient(derlServer &server,
	const derlRemoteClientConnection::Ref &connection) :
pLogClassName("derlRemoteClient"),
pServer(server),
pConnection(connection),
pSynchronizeStatus(SynchronizeStatus::pending),
pStartTaskProcessorCount(1),
pTaskProcessorsRunning(false){
}

derlRemoteClient::~derlRemoteClient(){
	pConnection->SetClient(nullptr);
}


// Management
///////////////

const std::string &derlRemoteClient::GetName() const{
	return pConnection->GetName();
}

const std::string &derlRemoteClient::GetAddress() const{
	return pConnection->GetRemoteAddress();
}

derlFileLayout::Ref derlRemoteClient::GetFileLayoutServerSync(){
	const std::lock_guard guard(pMutex);
	return pFileLayoutServer;
}

void derlRemoteClient::SetFileLayoutServer(const derlFileLayout::Ref &layout){
	const std::lock_guard guard(pMutex);
	pFileLayoutServer = layout;
}

derlFileLayout::Ref derlRemoteClient::GetFileLayoutClientSync(){
	const std::lock_guard guard(pMutex);
	return pFileLayoutClient;
}

void derlRemoteClient::SetFileLayoutClient(const derlFileLayout::Ref &layout){
	const std::lock_guard guard(pMutex);
	pFileLayoutClient = layout;
}

derlTaskSyncClient::Ref derlRemoteClient::GetTaskSyncClient(){
	const std::lock_guard guard(pMutex);
	return pTaskSyncClient;
}

void derlRemoteClient::RemovePendingTaskWithType(derlBaseTask::Type type){
	const std::lock_guard guard(pMutexPendingTasks);
	derlBaseTask::Queue filtered;
	for(const derlBaseTask::Ref &task : pPendingTasks){
		if(task->GetType() != type){
			filtered.push_back(task);
		}
	}
	pPendingTasks.swap(filtered);
}

bool derlRemoteClient::HasPendingTasksWithType(derlBaseTask::Type type){
	return std::find_if(pPendingTasks.cbegin(), pPendingTasks.cend(),
		[type](const derlBaseTask::Ref &task){
			return task->GetType() == type;
		}) != pPendingTasks.cend();
}

void derlRemoteClient::AddPendingTaskSync(const derlBaseTask::Ref &task){
	{
	const std::lock_guard guard(pMutexPendingTasks);
	pPendingTasks.push_back(task);
	}
	NotifyPendingTaskAdded();
}

void derlRemoteClient::NotifyPendingTaskAdded(){
	pConditionPendingTasks.notify_all();
}

const denLogger::Ref &derlRemoteClient::GetLogger() const{
	return pLogger;
}

void derlRemoteClient::SetLogger(const denLogger::Ref &logger){
	pLogger = logger;
	pConnection->SetLogger(logger);
	for(const derlTaskProcessorRemoteClient::Ref &processor : pTaskProcessors){
		processor->SetLogger(logger);
	}
}

bool derlRemoteClient::GetEnableDebugLog() const{
	return pConnection->GetEnableDebugLog();
}

void derlRemoteClient::SetEnableDebugLog(bool enable){
	pConnection->SetEnableDebugLog(enable);
}

void derlRemoteClient::SetSynchronizeStatus(SynchronizeStatus status, const std::string & details){
	const std::lock_guard guard(pMutex);
	pSynchronizeStatus = status;
	pSynchronizeDetails = details;
}

void derlRemoteClient::Synchronize(){
	{
	const std::lock_guard guard(pMutex);
	if(pTaskSyncClient && pTaskSyncClient->GetStatus() == derlTaskSyncClient::Status::failure){
		pTaskSyncClient = nullptr;
	}
	
	if(pTaskSyncClient){
		return;
	}
	
	pPathDataDir = GetServer().GetPathDataDir();
	
	pSynchronizeDetails.clear();
	pSynchronizeStatus = SynchronizeStatus::processing;
	pSynchronizeDetails = "Scanning file systems...";
	
	pFileLayoutServer = nullptr;
	pFileLayoutClient = nullptr;
	
	pTaskSyncClient = std::make_shared<derlTaskSyncClient>();
	
	{
	const std::lock_guard guardPending(pMutexPendingTasks);
	pPendingTasks.push_back(pTaskSyncClient->GetTaskFileLayoutServer());
	}
	}
	
	pConnection->SendRequestLayout();
	NotifyPendingTaskAdded();
	
	OnSynchronizeBegin();
}

derlRemoteClient::RunStatus derlRemoteClient::GetRunStatus() const{
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	switch((derlProtocol::RunStateStatus)pConnection->GetValueRunStatus()->GetValue()){
	case derlProtocol::RunStateStatus::running:
		return RunStatus::running;
		
	case derlProtocol::RunStateStatus::stopped:
	default:
		return RunStatus::stopped;
	}
}

void derlRemoteClient::SetRunStatus(RunStatus status){
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

void derlRemoteClient::RequestSystemProperty(const std::string &property){
	pConnection->SendRequestSystemProperty(property);
}

void derlRemoteClient::StartApplication(const derlRunParameters &params){
	pConnection->SendStartApplication(params);
}

void derlRemoteClient::StopApplication(){
	pConnection->SendStopApplication(derlProtocol::StopApplicationMode::requestClose);
}

void derlRemoteClient::KillApplication(){
	pConnection->SendStopApplication(derlProtocol::StopApplicationMode::killProcess);
}

void derlRemoteClient::StartTaskProcessors(){
	if(pTaskProcessors.empty()){
		Log(denLogger::LogSeverity::info, "StartTaskProcessors", "Create task processors");
		int i;
		for(i=0; i<pStartTaskProcessorCount; i++){
			const derlTaskProcessorRemoteClient::Ref processor(
				std::make_shared<derlTaskProcessorRemoteClient>(*this));
			processor->SetLogger(GetLogger());
			pTaskProcessors.push_back(processor);
		}
	}
	
	if(pTaskProcessorThreads.empty()){
		Log(denLogger::LogSeverity::info, "StartTaskProcessors", "Run task processor threads");
		for(const derlTaskProcessorRemoteClient::Ref &processor : pTaskProcessors){
			pTaskProcessorThreads.push_back(std::make_unique<std::thread>(
				[&](derlTaskProcessorRemoteClient::Ref processor){
					processor->Run();
				}, processor));
		}
	}
}

void derlRemoteClient::StopTaskProcessors(){
	if(!pTaskProcessors.empty()){
		Log(denLogger::LogSeverity::info, "StopTaskProcessors", "Exit task processors");
		for(const derlTaskProcessorRemoteClient::Ref &processor : pTaskProcessors){
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



void derlRemoteClient::Disconnect(){
	const std::lock_guard guard(derlGlobal::mutexNetwork);
	pConnection->Disconnect();
}

bool derlRemoteClient::GetConnected(){
	return pConnection->GetConnected();
}

bool derlRemoteClient::GetDisconnected(){
	return pConnection->GetConnectionState() == denConnection::ConnectionState::disconnected;
}

void derlRemoteClient::Update(float elapsed){
	pConnection->SendQueuedMessages();
	pConnection->ProcessReceivedMessages();
	
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

void derlRemoteClient::FailSynchronization(const std::string &error){
	{
	const std::lock_guard guard(pMutex);
	if(!pTaskSyncClient){
		return;
	}
	
	{
	const std::lock_guard guard2(pTaskSyncClient->GetMutex());
	pTaskSyncClient->SetStatus(derlTaskSyncClient::Status::failure);
	
	if(pTaskSyncClient->GetError().empty()){
		pSynchronizeDetails = "Synchronize failed.";
		
	}else{
		pSynchronizeDetails = pTaskSyncClient->GetError();
	}
	}
	
	pSynchronizeStatus = SynchronizeStatus::failed;
	pTaskSyncClient = nullptr;
	}
	
	{
	const std::lock_guard guard(pMutexPendingTasks);
	pPendingTasks.clear();
	}
	
	OnSynchronizeFinished();
}

void derlRemoteClient::FailSynchronization(){
	FailSynchronization("Synchronize client failed: unknown error");
}

void derlRemoteClient::SucceedSynchronization(){
	{
	const std::lock_guard guard(pMutex);
	if(!pTaskSyncClient){
		return;
	}
	
	pTaskSyncClient->SetStatus(derlTaskSyncClient::Status::success);
	
	pSynchronizeDetails = "Synchronized.";
	pSynchronizeStatus = SynchronizeStatus::ready;
	pTaskSyncClient = nullptr;
	}
	
	OnSynchronizeFinished();
}

void derlRemoteClient::LogException(const std::string &functionName,
const std::exception &exception, const std::string &message){
	std::stringstream ss;
	ss << message << ": " << exception.what();
	Log(denLogger::LogSeverity::error, functionName, ss.str());
}

void derlRemoteClient::Log(denLogger::LogSeverity severity,
const std::string &functionName, const std::string &message){
	if(!GetLogger()){
		return;
	}
	
	std::stringstream ss;
	ss << "[" << pLogClassName << "::" << functionName << "] " << message;
	GetLogger()->Log(severity, ss.str());
}

void derlRemoteClient::LogDebug(const std::string &functionName, const std::string &message){
	if(GetEnableDebugLog()){
		Log(denLogger::LogSeverity::debug, functionName, message);
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

void derlRemoteClient::OnRunStatusChanged(){
}

void derlRemoteClient::OnSystemProperty(const std::string &property, const std::string &value){
}

// Private Functions
//////////////////////

void derlRemoteClient::pInternalStartTaskProcessors(){
	StartTaskProcessors();
	pTaskProcessorsRunning = true;
}
