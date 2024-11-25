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
#include <condition_variable>

#include "derlFileLayout.h"
#include "internal/derlRemoteClientConnection.h"
#include "processor/derlTaskProcessorRemoteClient.h"
#include "task/derlTaskFileLayout.h"
#include "task/derlTaskSyncClient.h"


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
	
	/** \brief Synchronize status. */
	enum class SynchronizeStatus{
		/** \brief Client has not synchronized yet. */
		pending,
		
		/** \brief Client is synchronizing. */
		processing,
		
		/** \brief Client is synchronized. */
		ready,
		
		/** \brief Client synchronization failed. */
		failed
	};
	
	/** \brief Run state status. */
	enum class RunStatus{
		stopped = 0,
		running = 1
	};
	
	
protected:
	std::string pLogClassName;
	
	
private:
	friend class derlRemoteClientConnection;
	
	derlServer &pServer;
	const derlRemoteClientConnection::Ref pConnection;
	denLogger::Ref pLogger;
	
	std::filesystem::path pPathDataDir;
	SynchronizeStatus pSynchronizeStatus;
	std::string pSynchronizeDetails;
	
	derlFileLayout::Ref pFileLayoutServer, pFileLayoutClient;
	
	derlTaskSyncClient::Ref pTaskSyncClient;
	
	derlBaseTask::Queue pPendingTasks;
	std::mutex pMutexPendingTasks;
	std::condition_variable pConditionPendingTasks;
	
	derlTaskProcessorRemoteClient::Ref pTaskProcessor;
	std::unique_ptr<std::thread> pThreadTaskProcessor;
	bool pTaskProcessorsRunning;
	
	std::mutex pMutex;
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/**
	 * \brief Create remote launcher.
	 */
	derlRemoteClient(derlServer &server, const derlRemoteClientConnection::Ref &connection);
	
	/** \brief Clean up remote launcher support. */
	virtual ~derlRemoteClient() noexcept;
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Server the client is connected to. */
	inline derlServer &GetServer(){ return pServer; }
	inline const derlServer &GetServer() const{ return pServer; }
	
	/**
	 * \brief Connection.
	 * \warning For internal use only.
	 */
	inline derlRemoteClientConnection &GetConnection(){ return *pConnection; }
	inline const derlRemoteClientConnection &GetConnection() const{ return *pConnection; }
	
	/** \brief Name identifying the client. */
	const std::string &GetName() const;
	
	
	
	/** \brief Server file layout or nullptr. */
	inline const derlFileLayout::Ref &GetFileLayoutServer() const{ return pFileLayoutServer; }
	
	/** \brief Server file layout or nullptr while locking mutex. */
	derlFileLayout::Ref GetFileLayoutServerSync();
	
	/** \brief Set server file layout. */
	void SetFileLayoutServer(const derlFileLayout::Ref &layout);
	
	/** \brief Client file layout or nullptr. */
	inline const derlFileLayout::Ref &GetFileLayoutClient() const{ return pFileLayoutClient; }
	
	/** \brief Client file layout or nullptr while locking mutex. */
	derlFileLayout::Ref GetFileLayoutClientSync();
	
	/** \brief Set client file layout. */
	void SetFileLayoutClient(const derlFileLayout::Ref &layout);
	
	
	
	/** \brief Path to data directory. */
	inline const std::filesystem::path &GetPathDataDir() const{ return pPathDataDir; }
	
	/** \brief Part size. */
	int GetPartSize() const;
	
	/** \brief Mutex for accessing client members. */
	inline std::mutex &GetMutex(){ return pMutex; }
	
	
	
	/** \brief Sync client task. */
	derlTaskSyncClient::Ref GetTaskSyncClient();
	
	
	
	/** \brief Pending tasks queue. */
	inline derlBaseTask::Queue &GetPendingTasks(){ return pPendingTasks; }
	inline std::mutex &GetMutexPendingTasks(){ return pMutexPendingTasks; }
	inline std::condition_variable &GetConditionPendingTasks(){ return pConditionPendingTasks; }
	
	/**
	 * \brief Remove all tasks of specific type.
	 * \note Caller has to lock GetMutexPendingTasks().
	 */
	void RemovePendingTaskWithType(derlBaseTask::Type type);
	
	/**
	 * \brief One or more pending tasks are present matching type.
	 * \note Caller has to lock GetMutexPendingTasks().
	 */
	bool HasPendingTasksWithType(derlBaseTask::Type type);
	
	/** \brief Add pending task while holding mutex. */
	void AddPendingTaskSync(const derlBaseTask::Ref &task);
	
	/** \brief Notify waiters a pending task has been added. */
	void NotifyPendingTaskAdded();
	
	
	
	/** \brief Logger or null. */
	const denLogger::Ref &GetLogger() const;
	
	/** \brief Set logger or nullptr to clear. */
	void SetLogger(const denLogger::Ref &logger);
	
	/** \brief Debug logging is enabled. */
	bool GetEnableDebugLog() const;
	
	/** \brief Set if debug logging is enabled. */
	void SetEnableDebugLog(bool enable);
	
	
	
	/**
	 * \brief Synchronize status.
	 * \note Lock mutex while calling this function.
	 */
	inline SynchronizeStatus GetSynchronizeStatus() const{ return pSynchronizeStatus; }
	
	/**
	 * \brief Last synchronize details for display.
	 * \note Lock mutex while calling this function.
	 */
	inline const std::string &GetSynchronizeDetails() const{ return pSynchronizeDetails; }
	
	/** \brief Set synchronize status and details for display while locking mutex. */
	void SetSynchronizeStatus(SynchronizeStatus status, const std::string &details);
	
	/** \brief Synchronize client. */
	virtual void Synchronize();
	
	
	
	/** \brief Run status. */
	RunStatus GetRunStatus() const;
	
	/** \brief Set run status. */
	void SetRunStatus(RunStatus status);
	
	/**
	 * \brief Start application.
	 * 
	 * Send start application request to client. Monitor GetRunStatus() or listen to
	 * the OnRunStatusChanged() event to know when the application is running.
	 * In case of error logs will be send but no explicit error condition is send.
	 */
	void StartApplication(const derlRunParameters &params);
	
	/**
	 * \brief Stop application.
	 * 
	 * Send stop application request to client to gracefully stop application.
	 * Monitor GetRunStatus() or listen to the OnRunStatusChanged() event to know when
	 * the application is stopped. In case of error logs will be send but no explicit
	 * error condition is send.
	 */
	void StopApplication();
	
	/**
	 * \brief Kill application.
	 * 
	 * Send stop application request to client to kill application. Monitor GetRunStatus()
	 * or listen to the OnRunStatusChanged() event to know when the application is stopped.
	 * In case of error logs will be send but no explicit error condition is send.
	 */
	void KillApplication();
	
	
	
	/**
	 * \brief Start task processors.
	 * 
	 * Default implementation starts running a single derlTaskProcessorLauncherClient
	 * instance in a thread. Overwrite method to start any number of task processors instead.
	 * 
	 * This method has to be safe being called multiple times.
	 * 
	 * \note Called automatically once connection is established and client is accepted.
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
	 * \note Method is called if disconnected or by class destructor. User can also call it earlier.
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
	
	/** \brief Client is connected. */
	bool GetConnected();
	
	/** \brief Client is disconnected. */
	bool GetDisconnected();
	
	
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
	virtual void Update(float elapsed);
	
	/** \brief Fail synchonization. */
	void FailSynchronization(const std::string &error);
	void FailSynchronization();
	
	/** \brief Succeed synchonization. */
	void SucceedSynchronization();
	
	
	/** \brief Log exception. */
	void LogException(const std::string &functionName, const std::exception &exception,
		const std::string &message);
	
	/** \brief Log message. */
	virtual void Log(denLogger::LogSeverity severity, const std::string &functionName,
		const std::string &message);
	
	/** \brief Debug log message only printed if debugging is enabled. */
	void LogDebug(const std::string &functionName, const std::string &message);
	/*@}*/
	
	
	
	/** \name Events */
	/*@{*/
	/** \brief Connection established. */
	virtual void OnConnectionEstablished();
	
	/** \brief Connection closed either by calling Disconnect() or by server. */
	virtual void OnConnectionClosed();
	
	/** \brief Begin synchronize. */
	virtual void OnSynchronizeBegin();
	
	/** \brief Synchronize update. */
	virtual void OnSynchronizeUpdate();
	
	/** \brief Synchronize finished. */
	virtual void OnSynchronizeFinished();
	
	/** \brief Run status changed. */
	virtual void OnRunStatusChanged();
	/*@}*/
	
	
	
private:
	void pInternalStartTaskProcessors();
};

#endif
