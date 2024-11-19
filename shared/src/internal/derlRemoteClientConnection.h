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
	
	/** \brief Run state status. */
	enum class RunStatus{
		stopped = 0,
		running = 1
	};
	
	
	
private:
	derlServer &pServer;
	derlRemoteClient *pClient;
	std::string pName;
	const uint32_t pSupportedFeatures;
	uint32_t pEnabledFeatures;
	int pPartSize;
	
	const denState::Ref pStateRun;
	const denValueInt::Ref pValueRunStatus;
	
	int pMaxInProgressFiles;
	int pMaxInProgressBlocks;
	int pMaxInProgressParts;
	
	int pCountInProgressFiles;
	int pCountInProgressBlocks;
	int pCountInProgressParts;
	
	std::mutex pMutex;
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
	
	
	/** \brief Part size. */
	inline int GetPartSize() const{ return pPartSize; }
	
	/** \brief Mutex to lock all access to denConnection resources. */
	inline std::mutex &GetMutex(){ return pMutex; }
	
	/** \brief Received message queue. */
	inline derlMessageQueue &GetQueueReceived(){ return pQueueReceived; }
	
	/** \brief Send message queue. */
	inline derlMessageQueue &GetQueueSend(){ return pQueueSend; }
	
	
	/** \brief Get run status. */
	RunStatus GetRunStatus() const;
	
	
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
	 * \warning Caller has to lock GetMutex() while calling this method.
	 * */
	void SendQueuedMessages();
	
	/** \brief Process received messages. */
	void ProcessReceivedMessages();
	
	/** \brief Process finished pending operations. */
	void FinishPendingOperations();
	
	/** \brief Log exception. */
	void LogException(const std::string &functionName, const std::exception &exception,
		const std::string &message);
	
	/** \brief Log message. */
	virtual void Log(denLogger::LogSeverity severity, const std::string &functionName,
		const std::string &message);
	
	/** \brief Debug log message only printed if debugging is enabled. */
	void LogDebug(const std::string &functionName, const std::string &message);
	/*@}*/
	
	
private:
	void pMessageReceivedConnect(const denMessage::Ref &message);
	
	void pProcessRequestLogs(denMessageReader &reader);
	void pProcessResponseFileLayout(denMessageReader &reader);
	void pProcessResponseFileBlockHashes(denMessageReader &reader);
	void pProcessResponseDeleteFile(denMessageReader &reader);
	void pProcessResponseWriteFile(denMessageReader &reader);
	void pProcessFileDataReceived(denMessageReader &reader);
	void pProcessResponseFinishWriteFile(denMessageReader &reader);
	
	void pSendRequestLayout();
	void pSendRequestFileBlockHashes(const std::string &path, uint32_t blockSize);
	void pSendRequestsDeleteFile(const derlTaskFileDelete::List &tasks);
	void pSendRequestsWriteFile(const derlTaskFileWrite::List &tasks);
	void pSendSendFileData(const derlTaskFileWrite &task, const derlTaskFileWriteBlock &block);
	void pSendRequestsFinishWriteFile(const derlTaskFileWrite::List &tasks);
	void pSendStartApplication(const derlRunParameters &parameters);
	void pSendStopApplication(derlProtocol::StopApplicationMode mode);
	
	derlTaskSyncClient::Ref pGetPendingSyncTask(const std::string &functionName);
	derlTaskSyncClient::Ref pGetProcessingSyncTask(const std::string &functionName);
};

#endif
