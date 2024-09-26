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

#ifndef _DERLFILEBLOCK_H_
#define _DERLFILEBLOCK_H_

#include <stdint.h>
#include <string>
#include <memory>


/**
 * \brief File block.
 */
class derlFileBlock{
public:
	/** \brief Reference type. */
	typedef std::shared_ptr<derlFileBlock> Ref;
	
	
private:
	const uint64_t pOffset;
	const uint64_t pSize;
	std::string pHash;
	
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/**
	 * \brief Create file block.
	 */
	derlFileBlock( uint64_t offset, uint64_t size );
	
	/** \brief Clean up remote launcher support. */
	~derlFileBlock();
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Offset in bytes. */
	inline uint64_t GetOffset() const{ return pOffset; }
	
	/** \brief Size in bytes. */
	inline uint64_t GetSize() const{ return pSize; }
	
	/** \brief Hash (SHA-256). */
	inline const std::string &GetHash() const{ return pHash; }
	void SetHash( const std::string &hash );
	/*@}*/
	
	
	
private:
};

#endif
