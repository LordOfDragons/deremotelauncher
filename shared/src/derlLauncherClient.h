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

#ifndef _DERLLAUNCHERCLIENT_H_
#define _DERLLAUNCHERCLIENT_H_

#include <memory>
#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <filesystem>

#include <denetwork/denConnection.h>

#include "derlFileLayout.h"
#include "derlRunParameters.h"
#include "processor/derlTaskProcessorLauncherClient.h"
#include "task/derlBaseTask.h"


class derlLauncherClientConnection;


/**
 * \brief Drag[en]gine remote launcher client.
 * 
 * Implements DERemoteLauncher Network Protocol and does the heavy lifting of being a client.
 * Subclass and overwrite the respective functions to react to the events.
 */
class derlLauncherClient{
public:
	/** \brief Reference type. */
	typedef std::shared_ptr<derlLauncherClient> Ref;
	
	/** \brief Run state status. */
	enum class RunStatus{
		stopped = 0,
		running = 1
	};
	
	
protected:
	std::string pLogClassName;
	
	
private:
	friend class derlLauncherClientConnection;
	
	std::unique_ptr<derlLauncherClientConnection> pConnection;
	
	std::string pName;
	std::filesystem::path pPathDataDir;
	
	derlFileLayout::Ref pFileLayout, pNextFileLayout;
	bool pDirtyFileLayout;
	
	std::mutex pMutex;
	
	derlBaseTask::Queue pPendingTasks;
	std::mutex pMutexPendingTasks;
	std::condition_variable pConditionPendingTasks;
	
	int pStartTaskProcessorCount;
	derlTaskProcessorLauncherClient::List pTaskProcessors;
	std::vector<std::shared_ptr<std::thread>> pTaskProcessorThreads;
	bool pTaskProcessorsRunning;
	
	float pKeepAliveInterval, pKeepAliveElapsed;
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/**
	 * \brief Create remote launcher.
	 */
	derlLauncherClient();
	
	/** \brief Clean up remote launcher support. */
	virtual ~derlLauncherClient() noexcept;
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Name identifying the client. */
	inline const std::string &GetName() const{ return pName; }
	
	/**
	 * \brief Name identifying the client.
	 * 
	 * Name change takes effect the next time a connection is established.
	 */
	void SetName( const std::string &name );
	
	/** \brief Path to data directory. */
	inline const std::filesystem::path &GetPathDataDir() const{ return pPathDataDir; }
	
	/**
	 * \brief Set path to data directory.
	 * \throws std::invalid_argument Connected to server.
	 */
	void SetPathDataDir(const std::filesystem::path &path);
	
	
	
	/** \brief File layout or nullptr. */
	inline const derlFileLayout::Ref &GetFileLayout() const{ return pFileLayout; }
	
	/** \brief File layout or nullptr while locking muted. */
	derlFileLayout::Ref GetFileLayoutSync();
	
	/**
	 * \brief Set file layout while locking mutex.
	 * 
	 * Actual change happens during the next Update() call.
	 */
	void SetFileLayoutSync(const derlFileLayout::Ref &layout);
	
	/**
	 * \brief Set file layout dirty while locking mutex.
	 * 
	 * Actual change happens during the next Update() call.
	 */
	void SetDirtyFileLayoutSync(bool dirty);
	
	
	
	/** \brief Mutex for accessing client members. */
	inline std::mutex &GetMutex(){ return pMutex; }
	
	
	
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
	
	/**
	 * \brief Connection.
	 * \warning For internal use only.
	 */
	inline derlLauncherClientConnection &GetConnection(){ return *pConnection; }
	inline const derlLauncherClientConnection &GetConnection() const{ return *pConnection; }
	
	/** \brief Connection state. */
	denConnection::ConnectionState GetConnectionState() const;
	
	/** \brief Connection to a remote host is established. */
	bool GetConnected() const;
	
	/** \brief Logger or null. */
	const denLogger::Ref &GetLogger() const;
	
	/** \brief Set logger or nullptr to clear. */
	void SetLogger(const denLogger::Ref &logger);
	
	/** \brief Debug logging is enabled. */
	bool GetEnableDebugLog() const;
	
	/** \brief Set if debug logging is enabled. */
	void SetEnableDebugLog(bool enable);
	
	
	
	/**
	 * \brief Start task processors.
	 * 
	 * Default implementation starts running a single CreateTaskProcessor() instance in a thread.
	 * Overwrite method to start any number of task processors instead.
	 * 
	 * This method has to be safe being called multiple times.
	 * 
	 * \note User has to call this method after creating the launcher client before connecting.
	 */
	virtual void StartTaskProcessors();
	
	/**
	 * \brief Create task processors.
	 * 
	 * Default implementation creates instance of derlTaskProcessorLauncherClient.
	 * Overwrite method to create own task processors instead.
	 */
	virtual derlTaskProcessorLauncherClient::Ref CreateTaskProcessor();
	
	/**
	 * \brief Stop task processors.
	 * 
	 * Default implementation stops running the single derlTaskProcessorLauncherClient instance
	 * if running. Overwrite method to stop all task processors started by StartTaskProcessors().
	 * 
	 * This method has to be safe being called multiple times.
	 * 
	 * \note Method is called by class destructor. User can also call it earlier.
	 */
	virtual void StopTaskProcessors();
	
	/**
	 * \brief Task processors.
	 * 
	 * Valid if StartTaskProcessors() has been called and subclass did not overwrite it.
	 */
	inline const derlTaskProcessorLauncherClient::List &GetTaskProcessors() const{ return pTaskProcessors; }
	
	
	
	/**
	 * \brief Connect to connection object on host at address.
	 * \throws std::invalid_argument Data directory path is empty.
	 * \warning StartTaskProcessors() has to be called once before connecting.
	 */
	void ConnectTo(const std::string &address);
	
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
	
	/** \brief Apply layout change if pending. */
	void UpdateLayoutChanged();
	
	
	
	/** \brief Run status. */
	RunStatus GetRunStatus() const;
	
	/** \brief Set run status. */
	void SetRunStatus(RunStatus status);
	
	/**
	 * \brief Start application.
	 * 
	 * Called if start application request has been received. Subclass has to begin the
	 * process of starting the application if the application is not yet running or in
	 * progress of starting. The application has to return immediately from this function.
	 * The starting and running process has to be done outside this function. Subclass
	 * has to call SetRunStatus() to change the run status to running once the application
	 * is up and running. Failure to run the application has to he send as logs to the
	 * server. No error is expected to be thrown.
	 */
	virtual void StartApplication(const derlRunParameters &params) = 0;
	
	/**
	 * \brief Stop application.
	 * 
	 * Called if stop application request has been received. Subclass has to begin the
	 * process of stopping the application grafecully if the application is running or
	 * in the progress of starting. The application has to return immediately from this
	 * function. The stopping process has to be done outside this function. Subclass has
	 * to call SetRunStatus() to change the run status to stopped once the appliaction
	 * has been stopped. Failure to stop the application has to he send as logs to the
	 * server. No error is expected to be thrown.
	 */
	virtual void StopApplication() = 0;
	
	/**
	 * \brief Kill application.
	 * 
	 * Called if stop application request has been received with the kill flag set.
	 * Subclass has to begin the process of killing the application if the application
	 * is running or in the progress of starting. The application has to return
	 * immediately from this function. The killing process has to be done outside this
	 * function. Subclass has to call SetRunStatus() to change the run status to stopped
	 * once the appliaction has been killed. Failure to kill the application has to he
	 * send as logs to the server. No error is expected to be thrown.
	 */
	virtual void KillApplication() = 0;
	
	/**
	 * \brief Server requests system property.
	 * 
	 * If the value of the property can be determined right in the callback return the value.
	 * If the value can not be determined return nullptr then later on call
	 * SendSystemProperty() with the result.
	 * 
	 * If the property is not supported treat this as if the property has empty value.
	 * 
	 * For the special property name "properties.names" return a newline separated list
	 * of supported property names (excluding "properties.names").
	 * 
	 * Default implementation returns empty string.
	 */
	virtual std::unique_ptr<std::string> GetSystemProperty(const std::string &property);
	
	/** \brief Send log to server. */
	void SendLog(denLogger::LogSeverity severity, const std::string &source, const std::string &log);
	
	/** \brief Send system property to server. */
	void SendSystemProperty(const std::string &property, const std::string &value);
	
	
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
	
	/** \brief Connection failed. */
	virtual void OnConnectionFailed(denConnection::ConnectionFailedReason reason);
	
	/** \brief Connection closed either by calling Disconnect() or by server. */
	virtual void OnConnectionClosed();
	/*@}*/
};

#endif
