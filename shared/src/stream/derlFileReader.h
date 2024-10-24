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

#ifndef _DERLFILEREADER_H_
#define _DERLFILEREADER_H_

#include <memory>
#include <string>


/**
 * \brief File reader interface.
 */
class derlFileReader{
public:
	/** \brief Reference type. */
	typedef std::shared_ptr<derlFileReader> Ref;
	
	/** \brief Seek mode. */
	enum class SeekMode{
		set,
		begin,
		end
	};
	
	
private:
	const std::string pPath;
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/**
	 * \brief Create file reader.
	 */
	derlFileReader(const std::string &path);
	
	/** \brief Clean up file reader. */
	virtual ~derlFileReader() = 0;
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Path. */
	inline const std::string &GetPath() const{ return pPath; }
	
	/** \brief File size in bytes. */
	virtual uint64_t GetSize() = 0;
	
	/** \brief Read position in bytes. */
	virtual uint64_t GetPosition() = 0;
	
	/** \brief Set read position in bytes. */
	virtual void SetPosition(uint64_t position, SeekMode mode) = 0;
	
	/** \brief Read bytes throwing exception if failed. */
	virtual void Read(void *buffer, uint64_t size) = 0;
	/*@}*/
};

#endif
