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

#ifndef _DERLTASKPROCESSOR_H_
#define _DERLTASKPROCESSOR_H_

#include <chrono>
#include <fstream>
#include <filesystem>

#include "derlFile.h"
#include "derlFileLayout.h"
#include "task/derlTaskFileBlockHashes.h"
#include "task/derlTaskFileDelete.h"
#include "task/derlTaskFileWrite.h"
#include "task/derlTaskFileWriteBlock.h"

#include <denetwork/denLogger.h>

class derlLauncherClient;


/** \brief Process tasks. */
class derlTaskProcessor{
public:
	/** \brief Reference type. */
	typedef std::shared_ptr<derlTaskProcessor> Ref;
	
	/** \brief List type. */
	typedef std::vector<Ref> List;
	
	/** \brief Directory entry. */
	struct DirectoryEntry{
		std::string filename;
		std::string path;
		uint64_t fileSize;
		bool isDirectory;
	};
	
	/** \brief List directory entries. */
	typedef std::vector<DirectoryEntry> ListDirEntries;
	
	
private:
	derlLauncherClient &pClient;
	bool pExit;
	std::chrono::milliseconds pNoTaskDelay;
	std::filesystem::path pBaseDir, pFilePath;
	std::fstream pFileStream;
	uint64_t pFileHashReadSize;
	std::string pLogClassName;
	denLogger::Ref pLogger;
	bool pEnableDebugLog;
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/** \brief Create processor. */
	derlTaskProcessor(derlLauncherClient &client);
	
	/** \brief Clean up processor. */
	virtual ~derlTaskProcessor() noexcept;
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Client. */
	inline derlLauncherClient &GetClient() const{ return pClient; }
	
	/** \brief Base directory. */
	inline const std::filesystem::path &GetBaseDirectory() const{ return pBaseDir; }
	
	/** \brief Set base directory. */
	void SetBaseDirectory(const std::filesystem::path &path);
	
	/** \brief Request task processor to exit the next time possible. */
	void Exit();
	
	/** \brief Process tasks until Exit() is called. */
	void Run();
	
	/**
	 * \brief Process one task if possible.
	 * \returns true if task is run or false otherwise.
	 */
	bool RunTask();
	
	
	
	/**
	 * \brief Find next file block hashes task.
	 * \returns true if task is found otherwise false.
	 */
	bool FindNextTaskFileBlockHashes(derlTaskFileBlockHashes::Ref &task) const;
	
	/**
	 * \brief Find next delete file task.
	 * \returns true if task is found otherwise false.
	 */
	bool FindNextTaskDelete(derlTaskFileDelete::Ref &task) const;
	
	/**
	 * \brief Find next write file block task.
	 * \returns true if task is found otherwise false.
	 */
	bool FindNextTaskWriteFileBlock(derlTaskFileWrite::Ref &task, derlTaskFileWriteBlock::Ref &block) const;
	
	
	
	/** \brief Process task file block hashes. */
	virtual void ProcessFileBlockHashes(derlTaskFileBlockHashes &task);
	
	/** \brief Process task delete file. */
	virtual void ProcessDeleteFile(derlTaskFileDelete &task);
	
	/** \brief Process task write file block. */
	virtual void ProcessWriteFileBlock(derlTaskFileWrite &task, derlTaskFileWriteBlock &block);
	
	
	
	/**
	 * \brief Calculate file layout.
	 */
	derlFileLayout::Ref CalcFileLayout();
	
	/**
	 * \brief Calculate file layout.
	 */
	void CalcFileLayout(derlFileLayout &layout, const std::string &pathDir);
	
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
	 * \brief Delete file.
	 * 
	 * Default implementation deletes file using standard library functionality.
	 */
	virtual void DeleteFile(const derlTaskFileDelete &task);
	
	/**
	 * \brief Open file for reading or writing.
	 * 
	 * Default implementation opens an std::filestream for reading/writing binary data.
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
	 * \brief Write data to open file.
	 * 
	 * Default implementation writes to open std::filestream.
	 */
	virtual void WriteFile(const void *data, uint64_t offset, uint64_t size);
	
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
	
	/** \brief Debug logging is enabled. */
	inline bool GetEnableDebugLog() const{ return pEnableDebugLog; }
	
	/** \brief Set if debug logging is enabled. */
	void SetEnableDebugLog(bool enable);
	
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
