/**
 * MIT License
 * 
 * Copyright (c) 2024 DragonDreams (info@dragondreams.ch)
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

#include "Client.h"
#include "Launcher.h"
#include "WindowMain.h"


// Constructors, destructors
//////////////////////////////

Client::Client(WindowMain &windowMain, const denLogger::Ref &logger) :
pWindowMain(windowMain),
pLastTime(std::chrono::steady_clock::now())
{
	SetLogger(logger);
	
	pThreadUpdater = std::make_shared<std::thread>([&](){
		while(!pExitUpdaterThread){
			pFrameUpdate();
		}
	});
}

Client::~Client(){
	if(pThreadUpdater){
		pExitUpdaterThread = true;
		pThreadUpdater->join();
	}
	
	StopTaskProcessors();
}


// Management
///////////////

bool Client::IsDisconnected(){
	const std::lock_guard guard(pMutexUpdater);
	return GetConnectionState() == denConnection::ConnectionState::disconnected;
}

void Client::ConnectToHost(const char *name, const char *pathDataDir, const char *address){
	const std::lock_guard guard(pMutexUpdater);
	if(GetConnectionState() != denConnection::ConnectionState::disconnected){
		return;
	}
	
	SetName(name);
	SetPathDataDir(pathDataDir);
	ConnectTo(address);
}

void Client::DisconnectFromHost(){
	const std::lock_guard guard(pMutexUpdater);
	if(GetConnectionState() == denConnection::ConnectionState::disconnected){
		return;
	}
	
	Disconnect();
}

void Client::pFrameUpdate(){
	const std::lock_guard guard(pMutexUpdater);
	
	const std::chrono::steady_clock::time_point now(std::chrono::steady_clock::now());
	const int64_t elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(now - pLastTime).count();
	
	pLastTime = now;
	
	pCheckAppRunState();
	Update((float)elapsed_us / 1e6f);
	
	std::this_thread::yield();
	//std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

void Client::pCheckAppRunState(){
	const Launcher::Ref launcher(pWindowMain.GetLauncher());
	if(launcher && launcher->GetState() == Launcher::State::running){
		SetRunStatus(RunStatus::running);
		
	}else{
		SetRunStatus(RunStatus::stopped);
	}
}

void Client::StartApplication(const derlRunParameters &params){
	Log(denLogger::LogSeverity::info, "StartApplication", "Start running application");
	try{
		pWindowMain.GetLauncher()->RunGame(GetPathDataDir(), params);
		SetRunStatus(RunStatus::running);
		
	}catch(const deException &e){
		pLogException("StartApplication", e, "Start application failed");
		
	}catch(const std::exception &e){
		LogException("StartApplication", e, "Start application failed");
	}
	pWindowMain.RequestUpdateUIStates();
}

void Client::StopApplication(){
	Log(denLogger::LogSeverity::info, "StopApplication", "Stop running application");
	try{
		pWindowMain.GetLauncher()->StopGame();
		SetRunStatus(RunStatus::stopped);
		
	}catch(const deException &e){
		pLogException("StopApplication", e, "Stop application failed");
		
	}catch(const std::exception &e){
		LogException("StopApplication", e, "Stop application failed");
	}
	pWindowMain.RequestUpdateUIStates();
}

void Client::KillApplication(){
	Log(denLogger::LogSeverity::info, "KillApplication", "Kill running application");
	try{
		pWindowMain.GetLauncher()->KillGame();
		SetRunStatus(RunStatus::stopped);
		
	}catch(const deException &e){
		pLogException("KillApplication", e, "Kill application failed");
		
	}catch(const std::exception &e){
		LogException("KillApplication", e, "Kill application failed");
	}
	pWindowMain.RequestUpdateUIStates();
}

void Client::SendLog(denLogger::LogSeverity severity, const std::string &source,
const std::string &log){
	derlLauncherClient::SendLog(severity, source, log);
}

void Client::OnConnectionEstablished(){
	pWindowMain.RequestUpdateUIStates();
}

void Client::OnConnectionFailed(denConnection::ConnectionFailedReason reason){
	pWindowMain.RequestUpdateUIStates();
}

void Client::OnConnectionClosed(){
	pWindowMain.RequestUpdateUIStates();
}


// Private Functions
//////////////////////

void Client::pLogException(const std::string &functionName,
const deException &exception, const std::string &message){
	const decStringList lines(exception.FormatOutput());
	const int count = lines.GetCount();
	std::stringstream ss;
	int i;
	
	ss << message << ": ";
	
	for(i=0; i<count; i++){
		if(i > 0){
			ss << std::endl;
		}
		ss << lines.GetAt(i).GetString();
	}
	
	Log(denLogger::LogSeverity::error, functionName, ss.str());
}
