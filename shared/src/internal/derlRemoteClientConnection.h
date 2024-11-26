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

#ifndef _DERLREMOTECLIENTCONNECTION_H_
#define _DERLREMOTECLIENTCONNECTION_H_

#include <mutex>
#include <memory>

#include "../derlMessageQueue.h"
#include "../derlRunParameters.h"
#include "../derlProtocol.h"
#include "../task/derlTaskFileWrite.h"
#include "../task/derlTaskFileDelete.h"
#include "../task/derlTaskFileBlockHashes.h"
#include "../task/derlTaskSyncClient.h"

#include <denetwork/denConnection.h>
#include <denetwork/state/denState.h>
#include <denetwork/value/denValueInteger.h>

class derlServer;
class derlRemoteClient;
class derlFileLayout;
class derlFile;

class denMessageReader;


/**
 * \brief Remote client connection.
 * 
 * For internal use.
 */
class derlRemoteClientConnection : public denConnection{
public:
	/** \brief Shared pointer. */
	typedef std::shared_ptr<derlRemoteClientConnection> Ref;
	
	/** \brief Run state. */
	class StateRun : public denState{
	private:
		derlRemoteClientConnection &pConnection;
		
	public:
		const denValueInt::Ref valueRunStatus;
		
		StateRun(derlRemoteClientConnection &connection);
		void RemoteValueChanged(denValue &value) override;
	};
	
	
private:
	derlServer &pServer;
	derlRemoteClient *pClient;
	std::string pName;
	const uint32_t pSupportedFeatures;
	uint32_t pEnabledFeatures;
	bool pEnableDebugLog;
	
	const std::shared_ptr<StateRun> pStateRun;
	
	int pMaxInProgressFiles, pCountInProgressFiles;
	int pMaxInProgressBlocks, pCountInProgressBlocks;
	
	derlMessageQueue pQueueReceived, pQueueSend;
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/** \brief Create remote client. */
	derlRemoteClientConnection(derlServer &server);
	
	/** \brief Clean up remote launcher support. */
	~derlRemoteClientConnection() noexcept override;
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Client or nullptr if connection has not been accepted yet. */
	inline derlRemoteClient *GetClient() const{ return pClient; }
	
	/** \brief Set client or nullptr if connection has not been accepted yet. */
	void SetClient(derlRemoteClient *client);
	
	/** \brief Name of client. */
	inline const std::string &GetName() const{ return pName; }
	
	
	/** \brief Received message queue. */
	inline derlMessageQueue &GetQueueReceived(){ return pQueueReceived; }
	
	/** \brief Send message queue. */
	inline derlMessageQueue &GetQueueSend(){ return pQueueSend; }
	
	/** \brief Debug logging is enabled. */
	inline bool GetEnableDebugLog() const{ return pEnableDebugLog; }
	
	/** \brief Set if debug logging is enabled. */
	void SetEnableDebugLog(bool enable);
	
	
	/** \brief Run status network vaue. */
	const denValueInt::Ref &GetValueRunStatus() const;
	
	
	/** \brief Set logger or nullptr to clear. */
	void SetLogger(const denLogger::Ref &logger);
	
	/**
	 * \brief Connection closed.
	 * 
	 * This is called if Disconnect() is called or the remote closes the connection.
	 */
	void ConnectionClosed() override;
	
	/** \brief Long message is in progress of receiving. */
	void MessageProgress(size_t bytesReceived) override;
	
	/**
	 * \brief Message received.
	 * \param[in] message Received message. Reference can be stored for later use.
	 */
	void MessageReceived(const denMessage::Ref &message) override;
	
	/**
	 * \brief Send queues messages.
	 * \warning Caller has to lock derlGlobal::mutexNetwork while calling this method.
	 * */
	void SendQueuedMessages();
	
	/** \brief Process received messages. */
	void ProcessReceivedMessages();
	
	/** \brief Send next write requests if possible. */
	void SendNextWriteRequests(derlTaskSyncClient &taskSync);
	
	/**
	 * \brief Send next write requests if possible.
	 * 
	 * Same as SendNextWriteRequests but wrapped in try-catch failing synchronize on catch.
	 */
	void SendNextWriteRequestsFailSync(derlTaskSyncClient &taskSync);
	
	/** \brief Log exception. */
	void LogException(const std::string &functionName, const std::exception &exception,
		const std::string &message);
	
	/** \brief Log message. */
	virtual void Log(denLogger::LogSeverity severity, const std::string &functionName,
		const std::string &message);
	
	/** \brief Debug log message only printed if debugging is enabled. */
	void LogDebug(const std::string &functionName, const std::string &message);
	
	void SendRequestLayout();
	void SendRequestFileBlockHashes(const derlTaskFileBlockHashes &task);
	void SendRequestDeleteFile(const derlTaskFileDelete &task);
	void SendStartApplication(const derlRunParameters &parameters);
	void SendStopApplication(derlProtocol::StopApplicationMode mode);
	/*@}*/
	
	
private:
	void pMessageReceivedConnect(denMessage &message);
	
	void pProcessRequestLogs(denMessageReader &reader);
	void pProcessResponseFileLayout(denMessageReader &reader);
	void pProcessResponseFileBlockHashes(denMessageReader &reader);
	void pProcessResponseDeleteFile(denMessageReader &reader);
	void pProcessResponseWriteFile(denMessageReader &reader);
	void pProcessFileDataReceived(denMessageReader &reader);
	void pProcessResponseFinishWriteFile(denMessageReader &reader);
	
	void pSendRequestWriteFile(const derlTaskFileWrite &task);
	void pSendSendFileData(derlTaskFileWriteBlock &block);
	void pSendRequestFinishWriteFile(const derlTaskFileWrite &task);
	
	derlTaskSyncClient::Ref pGetSyncTask(const std::string &functionName,
		derlTaskSyncClient::Status status);
	void pCheckFinishedHashes(const derlTaskSyncClient::Ref &task);
	void pCheckFinishedWrite(const derlTaskSyncClient::Ref &task);
};

#endif
