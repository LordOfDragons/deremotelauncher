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

#include <sstream>
#include <chrono>
#include <iomanip>

#include "Logger.h"


// Constructors
/////////////////

Logger::Logger(std::mutex &mutex, std::deque<std::string> &addLogs) :
pMutex(mutex),
pAddLogs(addLogs){
}


// Management
///////////////

void Logger::Log(LogSeverity severity, const std::string &message){
	std::stringstream ss;
	
	switch(severity){
	case LogSeverity::debug:
		ss << "[DD] ";
		break;
		
	case LogSeverity::warning:
		ss << "[WW] ";
		break;
		
	case LogSeverity::error:
		ss << "[EE] ";
		break;
		
	case LogSeverity::info:
	default:
		ss << "[II] ";
	}
	
	const auto tp = std::chrono::system_clock::now();
	const auto t = std::chrono::system_clock::to_time_t(tp);
	ss << "[" << std::put_time(std::localtime(&t), "%Y-%m-%d %H-%M-%S") << "] ";
	
	ss << message;
	
	const std::lock_guard guard(pMutex);
	pAddLogs.push_back(ss.str());
}
