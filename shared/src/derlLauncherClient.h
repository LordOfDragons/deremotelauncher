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

#include "derlFileLayout.h"
#include "derlFileOperation.h"

#include <denetwork/denConnection.h>


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
	
	
private:
	friend class derlLauncherClientConnection;
	
	std::unique_ptr<derlLauncherClientConnection> pConnection;
	
	std::string pName;
	
	derlFileLayout::Ref pFileLayout;
	derlFileOperation::Map pFileOpsDeleteFiles, pFileOpsWriteFiles;
	
	
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
	
	/** \brief File layout or nullptr. */
	inline const derlFileLayout::Ref &GetFileLayout() const{ return pFileLayout; }
	void SetFileLayout( const derlFileLayout::Ref &layout );
	
	/** \brief Delete files file operations. */
	inline const derlFileOperation::Map &GetFileOpsDeleteFiles() const{ return pFileOpsDeleteFiles; }
	inline derlFileOperation::Map &GetFileOpsDeleteFiles(){ return pFileOpsDeleteFiles; }
	
	/** \brief Write files file operations. */
	inline const derlFileOperation::Map &GetFileOpsWriteFiles() const{ return pFileOpsWriteFiles; }
	inline derlFileOperation::Map &GetFileOpsWriteFiles(){ return pFileOpsWriteFiles; }
	
	
	
	/** \brief Connection state. */
	denConnection::ConnectionState GetConnectionState() const;
	
	/** \brief Connection to a remote host is established. */
	bool GetConnected() const;
	
	/** \brief Logger or null. */
	const denLogger::Ref &GetLogger() const;
	
	/** \brief Set logger or nullptr to clear. */
	void SetLogger(const denLogger::Ref &logger);
	
	/** \brief Connect to connection object on host at address. */
	void ConnectTo(const std::string &address);
	
	/** \brief Disconnect from remote connection if connected. */
	void Disconnect();
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
	
	
	
private:
};

#endif
