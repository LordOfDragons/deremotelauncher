/**
 * MIT License
 * 
 * Copyright (c) 2024 DragonDreams (info@dragondreams.ch)
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

#ifndef _DERLPROTOCOL_H_
#define _DERLPROTOCOL_H_

namespace derlProtocol{
	/**
	 * \brief Connect request signatures.
	 */
	static const char * const signatureClient = "DERemLaunchCnt-0";
	static const char * const signatureServer = "DERemLaunchSrv-0";
	
	/**
	 * \brief Message codes
	 */
	enum class MessageCodes{
		connectRequest = 1,
		connectAccepted = 2,
		requestFileLayout = 3,
		responseFileLayout = 4,
		requestFileBlockHashes = 5,
		responseFileBlockHashes = 6,
		requestDeleteFile = 7,
		responseDeleteFile = 8,
		requestWriteFile = 9,
		responseWriteFile = 10,
		sendFileData = 11,
		fileDataReceived = 12,
		requestFinishWriteFile = 13,
		responseFinishWriteFile = 14,
		startApplication = 15,
		stopApplication = 16,
		logs = 17,
		requestSystemProperty = 18,
		responseSystemProperty = 19,
		keepAlive = 20
	};
	
	/**
	 * \brief Delete file result.
	 */
	enum class DeleteFileResult{
		success = 0,
		failure = 1
	};
	
	/**
	 * \brief Write file result.
	 */
	enum class WriteFileResult{
		success = 0,
		failure = 1
	};
	
	/**
	 * \brief File data received result.
	 */
	enum class FileDataReceivedResult{
		success = 0,
		failure = 1
	};
	
	/**
	 * \brief Finish write file result.
	 */
	enum class FinishWriteFileResult{
		success = 0,
		failure = 1,
		validationFailed = 2
	};
	
	/**
	 * \brief Stop application mode.
	 */
	enum class StopApplicationMode{
		requestClose = 0,
		killProcess = 1
	};
	
	/**
	 * \brief Log level.
	 */
	enum class LogLevel{
		info = 0,
		warning = 1,
		error = 2
	};
	
	/**
	 * \brief Link codes.
	 */
	enum class LinkCodes{
		runState = 1
	};
	
	/**
	 * \brief Run state status.
	 */
	enum class RunStateStatus{
		stopped = 0,
		running = 1
	};
	
	/**
	 * \brief System property names.
	 */
	namespace SystemPropertyNames{
		/** \brief List of supported property names as newline separated string. */
		static const char * const propertyNames = "properties.names";
		
		/** \brief List of available profile names as newline separated string. */
		static const char * const profileNames = "profiles.names";
		
		/** \brief Default profile name. */
		static const char * const defaultProfile = "profiles.default";
	};
}

#endif
