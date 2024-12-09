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

#ifndef _DERLLAUNCHERCLIENTCONNECTION_H_
#define _DERLLAUNCHERCLIENTCONNECTION_H_

#include <mutex>
#include <memory>

#include <denetwork/denConnection.h>
#include <denetwork/state/denState.h>
#include <denetwork/value/denValueInteger.h>

#include "../derlMessageQueue.h"
#include "../task/derlTaskFileWrite.h"
#include "../task/derlTaskFileDelete.h"
#include "../task/derlTaskFileBlockHashes.h"


class derlLauncherClient;
class derlFileLayout;
class derlFile;

class denMessageReader;


/**
 * \brief Remote launcher client connection.
 * 
 * For internal use.
 */
class derlLauncherClientConnection : public denConnection{
private:
	typedef std::unique_lock<std::mutex> Lock;
	
	derlLauncherClient &pClient;
	bool pConnectionAccepted;
	uint32_t pEnabledFeatures;
	bool pEnableDebugLog;
	
	const denState::Ref pStateRun;
	const denValueInt::Ref pValueRunStatus;
	
	bool pPendingRequestLayout;
	derlTaskFileWrite::Map pWriteFileTasks;
	
	derlMessageQueue pQueueReceived, pQueueSend;
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/**
	 * \brief Create remote launcher.
	 */
	derlLauncherClientConnection(derlLauncherClient &client);
	
	/** \brief Clean up remote launcher support. */
	~derlLauncherClientConnection() noexcept override;
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Received message queue. */
	inline derlMessageQueue &GetQueueReceived(){ return pQueueReceived; }
	
	/** \brief Send message queue. */
	inline derlMessageQueue &GetQueueSend(){ return pQueueSend; }
	
	/** \brief Debug logging is enabled. */
	inline bool GetEnableDebugLog() const{ return pEnableDebugLog; }
	
	/** \brief Set if debug logging is enabled. */
	void SetEnableDebugLog(bool enable);
	
	
	/** \brief Run status network vaue. */
	inline const denValueInt::Ref &GetValueRunStatus(){ return pValueRunStatus; }
	
	
	/** \brief Set logger or nullptr to clear. */
	void SetLogger(const denLogger::Ref &logger);
	
	/** \brief Connection established. */
	void ConnectionEstablished() override;
	
	/** \brief Connection failed or timeout out. */
	void ConnectionFailed(ConnectionFailedReason reason) override;
	
	/**
	 * \brief Connection closed.
	 * 
	 * This is called if Disconnect() is called or the server closes the connection.
	 */
	void ConnectionClosed() override;
	
	/** \brief Long message is in progress of receiving. */
	void MessageProgress(size_t bytesReceived) override;
	
	/**
	 * \brief Message received.
	 * 
	 * Stores the message in the received message queue to avoid stalling.
	 */
	void MessageReceived(const denMessage::Ref &message) override;
	
	/**
	 * \brief Host send state to link.
	 * 
	 * Overwrite to create states requested by the server. States synchronize a fixed set
	 * of values between the server and the client. The client can have read-write or
	 * read-only access to the state. Create an instance of a subclass of denState to handle
	 * individual states. It is not necessary to create a subclass of denState if you intent
	 * to subclass denValue* instead.
	 * 
	 * If you do not support a state requested by the server you can return nullptr.
	 * In this case the state is not linked and state values are not synchronized.
	 * You can not re-link a state later on if you rejected it here. If you need re-linking
	 * a state make the server resend the link request. This will be a new state link.
	 * 
	 * \returns State or nullptr to reject.
	 */
	denState::Ref CreateState(const denMessage::Ref &message, bool readOnly) override;
	
	/**
	 * \brief Send queues messages.
	 * \warning Caller has to lock derlGlobal::mutexNetwork while calling this method.
	 * */
	void SendQueuedMessages();
	
	/**
	 * \brief Process received messages.
	 * \returns true if any message has been processed or false otherwise.
	 */
	bool ProcessReceivedMessages();
	
	/** \brief File layout changed. */
	void OnFileLayoutChanged();
	
	/** \brief Log exception. */
	void LogException(const std::string &functionName, const std::exception &exception,
		const std::string &message);
	
	/** \brief Log message. */
	virtual void Log(denLogger::LogSeverity severity, const std::string &functionName,
		const std::string &message);
	
	/** \brief Debug log message only printed if debugging is enabled. */
	void LogDebug(const std::string &functionName, const std::string &message);
	
	void SendResponseFileBlockHashes(const std::string &path, uint32_t blockSize);
	void SendResponseFileBlockHashes(const derlFile &file);
	void SendResponseDeleteFile(const derlTaskFileDelete &task);
	void SendFileDataReceived(const derlTaskFileWriteBlock &block);
	void SendResponseWriteFile(const derlTaskFileWrite &task);
	void SendFailResponseWriteFile(const std::string &path);
	void SendResponseFinishWriteFile(const derlTaskFileWrite &task);
	void SendResponseSystemPropertyNoLock(const std::string &property, const std::string &value);
	void SendLog(denLogger::LogSeverity severity, const std::string &source, const std::string &log);
	void SendKeepAlive();
	/*@}*/
	
	
private:
	void pMessageReceivedConnect(denMessage &message);
	
	void pProcessRequestLayout();
	void pProcessRequestFileBlockHashes(denMessageReader &reader);
	void pProcessRequestDeleteFile(denMessageReader &reader);
	void pProcessRequestWriteFile(denMessageReader &reader);
	void pProcessSendFileData(denMessageReader &reader);
	void pProcessRequestFinishWriteFile(denMessageReader &reader);
	void pProcessStartApplication(denMessageReader &reader);
	void pProcessStopApplication(denMessageReader &reader);
	void pProcessRequestSystemProperty(denMessageReader &reader);
	
	void pSendResponseFileLayout(const derlFileLayout &layout);
};

#endif
