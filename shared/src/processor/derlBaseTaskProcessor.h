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

#ifndef _DERLBASETASKPROCESSOR_H_
#define _DERLBASETASKPROCESSOR_H_

#include <chrono>
#include <fstream>
#include <filesystem>
#include <atomic>

#include "../derlFile.h"
#include "../derlFileLayout.h"

#include <denetwork/denLogger.h>


/** \brief Base class for task processors. */
class derlBaseTaskProcessor{
public:
	/** \brief Directory entry. */
	struct DirectoryEntry{
		std::string filename;
		std::string path;
		uint64_t fileSize;
		bool isDirectory;
	};
	
	/** \brief List directory entries. */
	typedef std::vector<DirectoryEntry> ListDirEntries;
	
	
protected:
	std::atomic<bool> pExit;
	std::filesystem::path pBaseDir, pFilePath;
	std::fstream pFileStream;
	uint64_t pFileHashReadSize;
	std::string pLogClassName;
	denLogger::Ref pLogger;
	bool pEnableDebugLog;
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/** \brief Create base task processor. */
	derlBaseTaskProcessor();
	
	/** \brief Clean up base task processor. */
	virtual ~derlBaseTaskProcessor() noexcept = default;
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Base directory. */
	inline const std::filesystem::path &GetBaseDirectory() const{ return pBaseDir; }
	
	/** \brief Set base directory. */
	void SetBaseDirectory(const std::filesystem::path &path);
	
	/** \brief Task processor has been requested to exit the next time possible. */
	inline bool HasExitRequested() const{ return pExit; }
	
	/** \brief Request task processor to exit the next time possible. */
	void Exit();
	
	/** \brief Process tasks until Exit() is called. */
	void Run();
	
	/** \brief Process one task if possible. */
	virtual void RunTask() = 0;
	
	
	
	/**
	 * \brief Calculate file layout.
	 */
	void CalcFileLayout(derlFileLayout &layout, const std::string &pathDir);
	
	/**
	 * \brief Path exists and refers to an existing directory.
	 * 
	 * Default implementation uses std::filesystem::is_directory.
	 */
	virtual bool IsPathDirectory(const std::string &pathDir);
	
	/**
	 * \brief List all files in directory.
	 * 
	 * Default implementation uses std::filesystem::directory_iterator.
	 */
	virtual void ListDirectoryFiles(ListDirEntries &entries, const std::string &pathDir);
	
	/**
	 * \brief Calculate file hash.
	 */
	void CalcFileHash(derlFile &file);
	
	/**
	 * \brief Calculate file block hashes.
	 */
	void CalcFileBlockHashes(derlFileBlock::List &blocks, const std::string &path, uint64_t blockSize);
	
	/**
	 * \brief Truncate file.
	 * 
	 * Default implementation opens and closes an std::filestream to truncate file.
	 */
	virtual void TruncateFile(const std::string &path);
	
	/**
	 * \brief Open file for reading or writing.
	 * 
	 * Default implementation opens an std::filestream for reading/writing binary data.
	 * If parent directories do not exist they are created first.
	 * If file is open CloseFile() is called first.
	 */
	virtual void OpenFile(const std::string &path, bool write);
	
	/**
	 * \brief Get size of open file.
	 * 
	 * Default implementation gets file size from std::filestream using seek/tell.
	 */
	virtual uint64_t GetFileSize();
	
	/**
	 * \brief Read data from open file.
	 * 
	 * Default implementation reads from open std::filestream.
	 */
	virtual void ReadFile(void *data, uint64_t offset, uint64_t size);
	
	/**
	 * \brief Close open file.
	 * 
	 * Default implementation closes open std::filestream. Has no effect if file is not open.
	 */
	virtual void CloseFile();
	
	
	
	/** \brief Logging class name. */
	inline const std::string &GetLogClassName() const{ return pLogClassName; }
	
	/** \brief Set logging class name. */
	void SetLogClassName(const std::string &name);
	
	/** \brief Logger or nullptr. */
	inline const denLogger::Ref &GetLogger() const{ return pLogger; }
	
	/** \brief Set logger. */
	void SetLogger(const denLogger::Ref &logger);
	
	/** \brief Log exception. */
	void LogException(const std::string &functionName, const std::exception &exception,
		const std::string &message);
	
	/** \brief Log message. */
	virtual void Log(denLogger::LogSeverity severity, const std::string &functionName,
		const std::string &message);
	
	/** \brief Debug log message only printed if debugging is enabled. */
	void LogDebug(const std::string &functionName, const std::string &message);
};

#endif
