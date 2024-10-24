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

#ifndef _DERLTASKFILEWRITEBLOCK_H_
#define _DERLTASKFILEWRITEBLOCK_H_

#include <memory>
#include <vector>
#include <string>


/**
 * \brief File write task block.
 */
class derlTaskFileWriteBlock{
public:
	/** \brief Reference type. */
	typedef std::shared_ptr<derlTaskFileWriteBlock> Ref;
	
	/** \brief Reference list. */
	typedef std::vector<Ref> List;
	
	/** \brief Status. */
	enum class Status{
		pending,
		processing,
		success,
		failure
	};
	
	
private:
	Status pStatus;
	uint64_t pOffset, pSize;
	std::string pData;
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/** \brief Create block. */
	derlTaskFileWriteBlock(uint64_t offset, uint64_t size, const std::string &data);
	
	/** \brief Clean up block. */
	~derlTaskFileWriteBlock() noexcept;
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Status. */
	inline Status GetStatus() const{ return pStatus; }
	void SetStatus(Status status);
	
	/** \brief Offset. */
	inline uint64_t GetOffset() const{ return pOffset; }
	
	/** \brief Size. */
	inline uint64_t GetSize() const{ return pSize; }
	
	/** \brief Data. */
	inline const std::string &GetData() const{ return pData; }
	
	/** \brief Drop data after writing is finished to free memory. */
	void DropData();
	/*@}*/
};

#endif
