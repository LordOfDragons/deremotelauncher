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

#include <memory>

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
	
	const denState::Ref pStateRun;
	const denValueInt::Ref pValueRunStatus;
	
	
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
	
	/** \brief Process finished pending operations. */
	void FinishPendingOperations();
	/*@}*/
	
	
private:
	void pProcessRequestLogs(denMessageReader &reader);
};

#endif
