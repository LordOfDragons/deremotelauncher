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

#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <chrono>
#include <mutex>
#include <thread>
#include <atomic>

#include "foxtoolkit.h"
#include <deremotelauncher/derlLauncherClient.h>
#include <dragengine/common/exceptions.h>

class WindowMain;

/**
 * Client.
 */
class Client : protected derlLauncherClient{
public:
	/** \brief Reference. */
	typedef std::shared_ptr<Client> Ref;
	
	
private:
	WindowMain &pWindowMain;
	
	std::chrono::steady_clock::time_point pLastTime;
	
	std::shared_ptr<std::thread> pThreadUpdater;
	std::atomic<bool> pExitUpdaterThread = false;
	std::mutex pMutexClient;
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/** \brief Create client. */
	Client(WindowMain &windowMain, const denLogger::Ref &logger);
	
	/** \brief Clean up client. */
	~Client() override;
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Is disconnected. */
	bool IsDisconnected();
	
	/** \brief Connect to host. */
	void ConnectToHost(const char *name, const char *pathDataDir, const char *address);
	
	/** \brief Disconnect from host. */
	void DisconnectFromHost();
	
	/** \brief Start application. */
	void StartApplication(const derlRunParameters &params) override;
	
	/** \brief Stop application. */
	void StopApplication() override;
	
	/** \brief Kill application. */
	void KillApplication() override;
	
	/** \brief Set run status. */
	void SetRunStatus(RunStatus status);
	
	/** \brief Send log to server. */
	void SendLog(denLogger::LogSeverity severity, const std::string &source, const std::string &log);
	
	/** \brief Connection established. */
	void OnConnectionEstablished() override;
	
	/** \brief Connection failed. */
	void OnConnectionFailed(denConnection::ConnectionFailedReason reason) override;
	
	/** \brief Connection closed either by calling Disconnect() or by server. */
	void OnConnectionClosed() override;
	
	/**
	 * \brief Server requests system property.
	 * 
	 * If the property return the value of the property in string form. If not supported
	 * return empty string.
	 * 
	 * For the special property name "properties.names" return a newline separated list
	 * of supported property names (excluding "properties.names").
	 * 
	 * Default implementation returns empty string.
	 */
	std::string GetSystemProperty(const std::string &property) override;
	/*@}*/
	
	
protected:
	void pFrameUpdate();
};

#endif
