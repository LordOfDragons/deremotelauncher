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

#ifndef _DERLFILEOPERATION_H_
#define _DERLFILEOPERATION_H_

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "derlFileReader.h"
#include "derlFileWriter.h"


/**
 * \brief File operation.
 */
class derlFileOperation{
public:
	/** \brief Reference type. */
	typedef std::shared_ptr<derlFileOperation> Ref;
	
	/** \brief Reference list. */
	typedef std::vector<Ref> List;
	
	/** \brief Reference map keyed by path. */
	typedef std::unordered_map<std::string, Ref> Map;
	
	/** \brief Status. */
	enum class Status{
		pending,
		success,
		failure
	};
	
	
private:
	const std::string pPath;
	Status pStatus;
	uint64_t pFileSize;
	derlFileReader::Ref pReader;
	derlFileWriter::Ref pWriter;
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/**
	 * \brief Create file operation.
	 */
	derlFileOperation(const std::string &path);
	
	/** \brief Clean up file operation. */
	~derlFileOperation() noexcept;
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Path. */
	inline const std::string &GetPath() const{ return pPath; }
	
	/** \brief Status. */
	inline Status GetStatus() const{ return pStatus; }
	void SetStatus(Status status);
	
	/** \brief File size. */
	inline uint64_t GetFileSize() const{ return pFileSize; }
	void SetFileSize(uint64_t fileSize);
	
	/** \brief File reader or nullptr. */
	inline const derlFileReader::Ref &GetReader() const{ return pReader; }
	void SetReader(const derlFileReader::Ref &reader);
	
	/** \brief File writer or nullptr. */
	inline const derlFileWriter::Ref &GetWriter() const{ return pWriter; }
	void SetWriter(const derlFileWriter::Ref &writer);
	/*@}*/
	
	
	
private:
};

#endif
