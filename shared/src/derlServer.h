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

#ifndef _DERLSERVER_H_
#define _DERLSERVER_H_

#include <memory>
#include <vector>
#include <mutex>
#include <thread>
#include <filesystem>

#include "derlFileLayout.h"
#include "derlRemoteClient.h"
#include "processor/derlTaskProcessorServer.h"
#include "task/derlTaskFileLayout.h"
#include <denetwork/denConnection.h>

class derlServerServer;


/**
 * \brief Drag[en]gine server.
 * 
 * Implements DERemoteLauncher Network Protocol and does the heavy lifting of being a server.
 * Subclass and overwrite the respective functions to react to the events.
 */
class derlServer{
public:
	/** \brief Reference type. */
	typedef std::shared_ptr<derlServer> Ref;
	
	
private:
	std::unique_ptr<derlServerServer> pServer;
	
	std::filesystem::path pPathDataDir;
	
	derlFileLayout::Ref pFileLayout;
	
	derlTaskFileLayout::Ref pTaskFileLayout;
	std::mutex pMutex;
	
	derlTaskProcessorServer::Ref pTaskProcessor;
	std::unique_ptr<std::thread> pThreadTaskProcessor;
	
	derlRemoteClient::List pClients;
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/**
	 * \brief Create server.
	 */
	derlServer();
	
	/** \brief Clean up server. */
	virtual ~derlServer() noexcept;
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Path to data directory. */
	inline const std::filesystem::path &GetPathDataDir() const{ return pPathDataDir; }
	
	/**
	 * \brief Set path to data directory.
	 * \throws std::invalid_argument Connected to server.
	 */
	void SetPathDataDir(const std::filesystem::path &path);
	
	/** \brief File layout or nullptr. */
	inline const derlFileLayout::Ref &GetFileLayout() const{ return pFileLayout; }
	void SetFileLayout( const derlFileLayout::Ref &layout );
	
	/** \brief File layout task. */
	inline const derlTaskFileLayout::Ref &GetTaskFileLayout() const{ return pTaskFileLayout; }
	void SetTaskFileLayout(const derlTaskFileLayout::Ref &task);
	
	/** \brief Mutex. */
	inline std::mutex &GetMutex(){ return pMutex; }
	
	/** \brief Logger or null. */
	const denLogger::Ref &GetLogger() const;
	
	/** \brief Set logger or nullptr to clear. */
	void SetLogger(const denLogger::Ref &logger);
	
	
	
	/** \brief Remote clients. */
	inline derlRemoteClient::List &GetClients(){ return pClients; }
	inline const derlRemoteClient::List &GetClients() const{ return pClients; }
	
	/** \brief Create client for connection. */
	virtual derlRemoteClient::Ref CreateClient(derlRemoteClientConnection *connection);
	
	
	
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
	inline const derlTaskProcessorServer::Ref &GetTaskProcessor() const{ return pTaskProcessor; }
	
	
	
	/**
	 * \brief Start listening on address for incoming connections.
	 * \throws std::invalid_argument Data directory path is empty.
	 * \param[in] address Address is in the format "hostnameOrIP" or "hostnameOrIP:port".
	 *                    You can use a resolvable hostname or an IPv4. If the port is not
	 *                    specified the default port 3413 is used. You can use any port
	 *                    you you like.
	 */
	void ListenOn(const std::string &address);
	
	/** \brief Stop listening. */
	void StopListening();
	
	
	
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
	/*@}*/
};

#endif
