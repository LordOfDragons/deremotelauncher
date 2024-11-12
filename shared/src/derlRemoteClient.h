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

#ifndef _DERLREMOTECLIENT_H_
#define _DERLREMOTECLIENT_H_

#include <memory>
#include <vector>
#include <mutex>
#include <thread>
#include <filesystem>

#include "derlFileLayout.h"
#include "processor/derlTaskProcessorRemoteClient.h"
#include "task/derlTaskSyncClient.h"
#include <denetwork/denConnection.h>


class derlRemoteClientConnection;
class derlServer;


/**
 * \brief Drag[en]gine remote client.
 * 
 * Implements DERemoteLauncher Network Protocol and does the heavy lifting of managing a client.
 * Subclass and overwrite the respective functions to react to the events.
 */
class derlRemoteClient{
public:
	/** \brief Reference type. */
	typedef std::shared_ptr<derlRemoteClient> Ref;
	
	/** \brief List reference type. */
	typedef std::vector<Ref> List;
	
	
private:
	friend class derlRemoteClientConnection;
	
	derlServer &pServer;
	derlRemoteClientConnection *pConnection;
	denLogger::Ref pLogger;
	
	derlFileLayout::Ref pFileLayout;
	
	derlTaskSyncClient::Ref pTaskSyncClient;
	std::mutex pMutex;
	
	derlTaskProcessorRemoteClient::Ref pTaskProcessor;
	std::unique_ptr<std::thread> pThreadTaskProcessor;
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/**
	 * \brief Create remote launcher.
	 */
	derlRemoteClient(derlServer &server, derlRemoteClientConnection *connection);
	
	/** \brief Clean up remote launcher support. */
	virtual ~derlRemoteClient() noexcept;
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Server the client is connected to. */
	inline derlServer &GetServer(){ return pServer; }
	inline const derlServer &GetServer() const{ return pServer; }
	
	/** \brief Drop connection. */
	void DropConnection();
	
	/** \brief Name identifying the client. */
	const std::string &GetName() const;
	
	/** \brief File layout or nullptr. */
	inline const derlFileLayout::Ref &GetFileLayout() const{ return pFileLayout; }
	void SetFileLayout( const derlFileLayout::Ref &layout );
	
	/** \brief Sync client task. */
	inline const derlTaskSyncClient::Ref &GetTaskSyncClient() const{ return pTaskSyncClient; }
	void SetTaskSyncClient(const derlTaskSyncClient::Ref &task);
	
	/** \brief Mutex. */
	inline std::mutex &GetMutex(){ return pMutex; }
	
	
	
	/** \brief Logger or null. */
	const denLogger::Ref &GetLogger() const;
	
	/** \brief Set logger or nullptr to clear. */
	void SetLogger(const denLogger::Ref &logger);
	
	
	
	/**
	 * \brief Start task processors.
	 * 
	 * Default implementation starts running a single derlTaskProcessorLauncherClient instance in a thread.
	 * Overwrite method to start any number of task processors instead.
	 * 
	 * This method has to be safe being called multiple times.
	 * 
	 * \note User has to call this method after creating the launcher client before connecting.
	 */
	virtual void StartTaskProcessors();
	
	/**
	 * \brief Stop task processors.
	 * 
	 * Default implementation stops running the single derlTaskProcessorLauncherClient instance if running.
	 * Overwrite method to stop all task processors started by StartTaskProcessors().
	 * 
	 * This method has to be safe being called multiple times.
	 * 
	 * \note Method is called by class destructor. User can also call it earlier.
	 */
	virtual void StopTaskProcessors();
	
	/**
	 * \brief Task processor or nullptr.
	 * 
	 * Valid if StartTaskProcessors() has been called and subclass did not overwrite it.
	 */
	inline const derlTaskProcessorRemoteClient::Ref &GetTaskProcessor() const{ return pTaskProcessor; }
	
	
	
	/** \brief Disconnect from remote connection if connected. */
	void Disconnect();
	
	
	/**
	 * \brief Update launcher client.
	 * 
	 * Call this on frame update or at regular intervals. This call does the following:
	 * - processes network events.
	 * - add pending file operations to be processed by a task processor.
	 * - finish requests if pending file operations are finished.
	 * 
	 * The caller is responsible to operate task processors himself if not using the default one.
	 * 
	 * \param[in] elapsed Elapsed time since last update call.
	 */
	void Update(float elapsed);
	/*@}*/
	
	
	
	/** \name Events */
	/*@{*/
	/** \brief Connection established. */
	virtual void OnConnectionEstablished();
	
	/** \brief Connection closed either by calling Disconnect() or by server. */
	virtual void OnConnectionClosed();
	/*@}*/
	
	
	
private:
};

#endif
