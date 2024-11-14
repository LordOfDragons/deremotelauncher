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
#include <mutex>

#include "derlTaskProcessorRemoteClient.h"
#include "../derlRemoteClient.h"
#include "../derlServer.h"
#include "../derlFile.h"
#include "../derlFileBlock.h"
#include "../derlFileLayout.h"


// Class derlTaskProcessorRemoteClient
////////////////////////////////////////

derlTaskProcessorRemoteClient::derlTaskProcessorRemoteClient(derlRemoteClient &client) :
pClient(client){
	pLogClassName = "derlTaskProcessorRemoteClient";
}

// Management
///////////////

bool derlTaskProcessorRemoteClient::RunTask(){
	derlTaskFileLayout::Ref taskLayoutServer;
	derlTaskSyncClient::Ref taskSyncClient;
	
	{
	std::lock_guard guard(pClient.GetMutex());
	pBaseDir = pClient.GetPathDataDir();
	
	FindNextTaskLayoutServer(taskLayoutServer)
	|| !pClient.GetFileLayoutServer()
	|| !pClient.GetFileLayoutClient()
	|| FindNextTaskSyncClient(taskSyncClient);
	}
	
	if(taskLayoutServer){
		ProcessFileLayoutServer(*taskLayoutServer);
		return true;
		
	}else if(taskSyncClient){
		ProcessSyncClient(*taskSyncClient);
		return true;
		
	}else{
		return false;
	}
}

bool derlTaskProcessorRemoteClient::FindNextTaskLayoutServer(derlTaskFileLayout::Ref &task) const{
	const derlTaskFileLayout::Ref checkTask(pClient.GetTaskFileLayoutServer());
	if(checkTask && checkTask->GetStatus() == derlTaskFileLayout::Status::pending){
		task = checkTask;
		task->SetStatus(derlTaskFileLayout::Status::processing);
		return true;
	}
	return false;
}

bool derlTaskProcessorRemoteClient::FindNextTaskSyncClient(derlTaskSyncClient::Ref &task) const{
	const derlTaskSyncClient::Ref checkTask(pClient.GetTaskSyncClient());
	if(checkTask && checkTask->GetStatus() == derlTaskSyncClient::Status::pending){
		task = checkTask;
		task->SetStatus(derlTaskSyncClient::Status::preparing);
		return true;
	}
	return false;
}

void derlTaskProcessorRemoteClient::ProcessFileLayoutServer(derlTaskFileLayout &task){
	if(pEnableDebugLog){
		LogDebug("ProcessFileLayoutServer", "Build file layout");
	}
	
	derlTaskFileLayout::Status status;
	derlFileLayout::Ref layout;
	std::string syncError;
	
	try{
		layout = std::make_shared<derlFileLayout>();
		CalcFileLayout(*layout, "");
		status = derlTaskFileLayout::Status::success;
		
	}catch(const std::exception &e){
		LogException("ProcessFileLayoutServer", e, "Failed");
		status = derlTaskFileLayout::Status::failure;
		
		std::stringstream ss;
		ss << "Build server file layout failed: " << e.what();
		syncError = ss.str();
		
	}catch(...){
		Log(denLogger::LogSeverity::error, "ProcessFileLayoutServer", "Failed");
		status = derlTaskFileLayout::Status::failure;
		syncError = "Build server file layout failed: unknown error";
	}
	
	std::lock_guard guard(pClient.GetMutex());
	
	switch(status){
	case derlTaskFileLayout::Status::success:
		pClient.SetFileLayoutServer(layout);
		pClient.SetTaskFileLayoutServer(nullptr);
		break;
		
	case derlTaskFileLayout::Status::failure:
		pClient.SetFileLayoutServer(nullptr);
		pClient.SetTaskFileLayoutServer(nullptr);
		
		if(pClient.GetTaskSyncClient()){
			pClient.GetTaskSyncClient()->SetError(syncError);
			pClient.GetTaskSyncClient()->SetStatus(derlTaskSyncClient::Status::failure);
		}
		break;
		
	default:
		break;
	}
}

void derlTaskProcessorRemoteClient::ProcessSyncClient(derlTaskSyncClient &task){
	derlTaskSyncClient::Status status;
	std::string syncError;
	
	try{
		derlFileLayout::Ref layoutServer, layoutClient;
		{
		std::lock_guard guard(pClient.GetMutex());
		layoutServer = pClient.GetFileLayoutServer();
		layoutClient = pClient.GetFileLayoutClient();
		}
		
		if(!layoutServer || !layoutClient){
			return;
		}
		
		// TODO
		
		status = derlTaskSyncClient::Status::preparing;
		
	}catch(const std::exception &e){
		LogException("ProcessSyncClient", e, "Failed");
		status = derlTaskSyncClient::Status::failure;
		
		std::stringstream ss;
		ss << "Synchronize client failed: " << e.what();
		syncError = ss.str();
		
	}catch(...){
		Log(denLogger::LogSeverity::error, "ProcessSyncClient", "Failed");
		status = derlTaskSyncClient::Status::failure;
		syncError = "Synchronize client failed: unknown error";
	}
	
	std::lock_guard guard(pClient.GetMutex());
	
	task.SetStatus(status);
	if(status == derlTaskSyncClient::Status::failure){
		task.SetError(syncError);
	}
}
