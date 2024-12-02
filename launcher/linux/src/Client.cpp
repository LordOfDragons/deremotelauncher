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

#include <deremotelauncher/derlGlobal.h>

#include "Client.h"
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
	const std::lock_guard guard(pMutexClient);
	return GetConnectionState() == denConnection::ConnectionState::disconnected;
}

void Client::ConnectToHost(const char *name, const char *pathDataDir, const char *address){
	const std::lock_guard guard(pMutexClient);
	if(GetConnectionState() != denConnection::ConnectionState::disconnected){
		return;
	}
	
	SetName(name);
	SetPathDataDir(pathDataDir);
	
	ConnectTo(address);
}

void Client::DisconnectFromHost(){
	const std::lock_guard guard(pMutexClient);
	if(GetConnectionState() == denConnection::ConnectionState::disconnected){
		Disconnect();
	}
}

void Client::pFrameUpdate(){
	{
	const std::lock_guard guard(pMutexClient);
	
	const std::chrono::steady_clock::time_point now(std::chrono::steady_clock::now());
	const int64_t elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(now - pLastTime).count();
	const float elapsed = (float)elapsed_us / 1e6f;
	pLastTime = now;
	
	Update(elapsed);
	}
	
	std::this_thread::yield();
	//std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

void Client::StartApplication(const derlRunParameters &params){
	pWindowMain.StartApp(params);
}

void Client::StopApplication(){
	pWindowMain.StopApp();
}

void Client::KillApplication(){
	pWindowMain.KillApp();
}

void Client::SetRunStatus(RunStatus status){
	derlLauncherClient::SetRunStatus(status);
}

void Client::SendLog(denLogger::LogSeverity severity, const std::string &source,
const std::string &log){
	const std::lock_guard guard(pMutexClient);
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
