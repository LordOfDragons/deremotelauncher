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

#include <memory>

#include <denetwork/denConnection.h>

#include "../derlFileOperation.h"


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
	derlLauncherClient &pClient;
	bool pConnectionAccepted;
	uint32_t pEnabledFeatures;
	
	
	
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
	 * \param[in] message Received message. Reference can be stored for later use.
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
	/*@}*/
	
	
private:
	void pProcessRequestLayout();
	void pProcessRequestFileBlockHashes(denMessageReader &reader);
	void pProcessRequestDeleteFiles(denMessageReader &reader);
	void pProcessRequestWriteFiles(denMessageReader &reader);
	void pProcessSendFileData(denMessageReader &reader);
	void pProcessRequestFinishWriteFiles(denMessageReader &reader);
	void pProcessStartApplication(denMessageReader &reader);
	void pProcessStopApplication(denMessageReader &reader);
	
	void pSendResponseFileLayout(const derlFileLayout &layout);
	void pSendResponseFileBlockHashes(const std::string &path, uint32_t blockSize);
	void pSendResponseFileBlockHashes(const derlFile &file);
	void pSendResponseDeleteFiles(const derlFileOperation::Map &files);
	void pSendResponseWriteFiles(const derlFileOperation::Map &files);
	void pSendResponseFinishWriteFiles(const derlFileOperation::List &files);
};

#endif
