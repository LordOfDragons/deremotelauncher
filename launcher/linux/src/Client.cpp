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
#include "WindowMain.h"


// Constructors
/////////////////

Client::Client(WindowMain &windowMain) :
pWindowMain(windowMain),
pLastTime(std::chrono::steady_clock::now()){
}

// Management
///////////////

void Client::OnFrameUpdate(){
	const std::chrono::steady_clock::time_point now(std::chrono::steady_clock::now());
	const int64_t elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(now - pLastTime).count();
	
	pLastTime = now;
	
	Update((float)elapsed_us / 1e6f);
}

void Client::StartApplication(const derlRunParameters &params){
	Log(denLogger::LogSeverity::info, "StartApplication", "Run");
}

void Client::StopApplication(){
	Log(denLogger::LogSeverity::info, "StopApplication", "Run");
}

void Client::KillApplication(){
	Log(denLogger::LogSeverity::info, "KillApplication", "Run");
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
