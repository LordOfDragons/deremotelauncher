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
////////////////////////////

derlTaskProcessorRemoteClient::derlTaskProcessorRemoteClient(derlRemoteClient &client) :
pClient(client){
	pLogClassName = "derlTaskProcessorRemoteClient";
}

// Management
///////////////

bool derlTaskProcessorRemoteClient::RunTask(){
	derlTaskSyncClient::Ref taskSyncClient;
	
	{
	std::lock_guard guard(pClient.GetMutex());
	std::lock_guard guard2(pClient.GetServer().GetMutex());
	pBaseDir = pClient.GetServer().GetPathDataDir();
	
	!pClient.GetServer().GetFileLayout()
	|| FindNextTaskSyncClient(taskSyncClient);
	}
	
	if(taskSyncClient){
		ProcessSyncClient(*taskSyncClient);
		return true;
		
	}else{
		return false;
	}
}

bool derlTaskProcessorRemoteClient::FindNextTaskSyncClient(derlTaskSyncClient::Ref &task) const{
	const derlTaskSyncClient::Ref checkTask(pClient.GetTaskSyncClient());
	if(checkTask && checkTask->GetStatus() == derlTaskSyncClient::Status::pending){
		task = checkTask;
		return true;
	}
	return false;
}

void derlTaskProcessorRemoteClient::ProcessSyncClient(derlTaskSyncClient &task){
	derlTaskSyncClient::Status status = task.GetStatus();
	
	try{
		
	}catch(const std::exception &e){
		LogException("ProcessSyncClient", e, "Failed");
		status = derlTaskSyncClient::Status::failure;
		
	}catch(...){
		Log(denLogger::LogSeverity::error, "ProcessFileBlockHashes", "Failed");
		status = derlTaskSyncClient::Status::failure;
	}
	
	std::lock_guard guard(pClient.GetMutex());
	if(status == derlTaskSyncClient::Status::success){
		
	}
	task.SetStatus(status);
}
