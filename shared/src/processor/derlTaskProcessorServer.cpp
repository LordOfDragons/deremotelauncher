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

#include "derlTaskProcessorServer.h"
#include "../derlServer.h"
#include "../derlFileLayout.h"


// Class derlTaskProcessorServer
////////////////////////////

derlTaskProcessorServer::derlTaskProcessorServer(derlServer &server) :
pServer(server){
	pLogClassName = "derlTaskProcessorServer";
}

derlTaskProcessorServer::~derlTaskProcessorServer(){
}

// Management
///////////////

bool derlTaskProcessorServer::RunTask(){
	derlTaskFileLayout::Ref taskFileLayout;
	
	{
	std::lock_guard guard(pServer.GetMutex());
	pBaseDir = pServer.GetPathDataDir();
	
	FindNextTaskFileLayout(taskFileLayout)
	|| !pServer.GetFileLayout();
	}
	
	if(taskFileLayout){
		ProcessFileLayout(*taskFileLayout);
		return true;
		
	}else{
		return false;
	}
}



bool derlTaskProcessorServer::FindNextTaskFileLayout(derlTaskFileLayout::Ref &task) const{
	const derlTaskFileLayout::Ref checkTask(pServer.GetTaskFileLayout());
	if(checkTask && checkTask->GetStatus() == derlTaskFileLayout::Status::pending){
		task = checkTask;
		return true;
	}
	return false;
}



void derlTaskProcessorServer::ProcessFileLayout(derlTaskFileLayout &task){
	if(pEnableDebugLog){
		LogDebug("ProcessFileLayout", "Build file layout");
	}
	
	derlTaskFileLayout::Status status;
	derlFileLayout::Ref layout;
	
	try{
		layout = std::make_shared<derlFileLayout>();
		CalcFileLayout(*layout, "");
		status = derlTaskFileLayout::Status::success;
		
	}catch(const std::exception &e){
		LogException("ProcessFileLayout", e, "Failed");
		status = derlTaskFileLayout::Status::failure;
		
	}catch(...){
		Log(denLogger::LogSeverity::error, "ProcessFileLayout", "Failed");
		status = derlTaskFileLayout::Status::failure;
	}
	
	std::lock_guard guard(pServer.GetMutex());
	task.SetStatus(status);
	if(status == derlTaskFileLayout::Status::success){
		task.SetLayout(layout);
	}
}
