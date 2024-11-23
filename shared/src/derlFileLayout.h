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

#ifndef _DERLFILELAYOUT_H_
#define _DERLFILELAYOUT_H_

#include <memory>
#include <mutex>

#include "derlFile.h"


/**
 * \brief File layout.
 */
class derlFileLayout{
public:
	/** \brief Reference type. */
	typedef std::shared_ptr<derlFileLayout> Ref;
	
	/** \brief Path list. */
	typedef std::vector<std::string> ListPath;
	
	
private:
	derlFile::Map pFiles;
	std::mutex pMutex;
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/**
	 * \brief Create file layout.
	 */
	derlFileLayout();
	
	/** \brief Clean up remote launcher support. */
	~derlFileLayout() noexcept;
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Count of files. */
	int GetFileCount() const;
	
	/** \brief File iterators. */
	derlFile::Map::const_iterator GetFilesBegin() const;
	derlFile::Map::const_iterator GetFilesEnd() const;
	
	/** \brief All path as list. */
	ListPath GetAllPath() const;
	
	/** \brief File with path or nullptr. */
	derlFile::Ref GetFileAt(const std::string &path) const;
	
	/** \brief File with path or nullptr while locking mutex. */
	derlFile::Ref GetFileAtSync(const std::string &path);
	
	/** \brief Set file with path. */
	void SetFileAt(const std::string &path, const derlFile::Ref &file);
	
	/** \brief Set file with path while locking mutex. */
	void SetFileAtSync(const std::string &path, const derlFile::Ref &file);
	
	/** \brief Add file. */
	void AddFile(const derlFile::Ref &file);
	
	/** \brief Add file while locking mutex. */
	void AddFileSync(const derlFile::Ref &file);
	
	/** \brief Remove file. */
	void RemoveFile(const std::string &path);
	
	/** \brief Remove file if present. */
	void RemoveFileIfPresent(const std::string &path);
	
	/** \brief Remove file if present while locking mutex. */
	void RemoveFileIfPresentSync(const std::string &path);
	
	/** \brief Remove all files. */
	void RemoveAllFiles();
	
	/** \brief Mutex */
	inline std::mutex &GetMutex(){ return pMutex; }
	/*@}*/
	
	
	
private:
};

#endif
