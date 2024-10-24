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

#include "derlFile.h"


/**
 * \brief File layout.
 */
class derlFileLayout{
public:
	/** \brief Reference type. */
	typedef std::shared_ptr<derlFileLayout> Ref;
	
	
private:
	derlFile::Map pFiles;
	
	
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
	
	/** \brief File with path or nullptr. */
	derlFile::Ref GetFileAt(const std::string &path) const;
	
	/** \brief Add file. */
	void AddFile(const derlFile::Ref &file);
	
	/** \brief Remove file. */
	void RemoveFile(const std::string &path);
	
	/** \brief Remove all files. */
	void RemoveAllFiles();
	/*@}*/
	
	
	
private:
};

#endif
