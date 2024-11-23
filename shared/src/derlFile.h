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

#ifndef _DERLFILE_H_
#define _DERLFILE_H_

#include <stdint.h>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

#include "derlFileBlock.h"


/**
 * \brief File.
 */
class derlFile{
public:
	/** \brief Reference type. */
	typedef std::shared_ptr<derlFile> Ref;
	
	/** \brief List type. */
	typedef std::vector<Ref> List;
	
	/** \brief Map type. */
	typedef std::unordered_map<std::string,Ref> Map;
	
	
private:
	const std::string pPath;
	uint64_t pSize;
	std::string pHash;
	
	derlFileBlock::List pBlocks;
	bool pHasBlocks;
	uint32_t pBlockSize;
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/** \brief Create file. */
	derlFile(const std::string &path);
	
	/** \brief Copy file. */
	derlFile(const derlFile &file);
	
	/** \brief Clean up remote launcher support. */
	~derlFile() noexcept;
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Path. */
	inline const std::string &GetPath() const{ return pPath; }
	
	/** \brief Size in bytes. */
	inline uint64_t GetSize() const{ return pSize; }
	void SetSize(uint64_t size);
	
	/** \brief Hash (SHA-256). */
	inline const std::string &GetHash() const{ return pHash; }
	void SetHash(const std::string &hash);
	
	
	/** \brief Has blocks. */
	inline bool GetHasBlocks() const{ return pHasBlocks; }
	void SetHasBlocks(bool hasBlocks);
	
	/** \brief Count of blocks. */
	int GetBlockCount() const;
	
	/** \brief Block at index. */
	derlFileBlock::Ref GetBlockAt(int index) const;
	
	/** \brief Block matching range or nullptr. */
	derlFileBlock::Ref BlockMatching(uint64_t offset, uint64_t size) const;
	
	/** \brief Add block. */
	void AddBlock(const derlFileBlock::Ref &block);
	
	/** \brief Remove block. */
	void RemoveBlock(const derlFileBlock::Ref &block);
	
	/** \brief Remove all blocks. */
	void RemoveAllBlocks();
	
	/** \brief Set blocks. */
	void SetBlocks(const derlFileBlock::List &blocks);
	
	/** \brief Block iterators. */
	derlFileBlock::List::const_iterator GetBlocksBegin() const;
	derlFileBlock::List::const_iterator GetBlocksEnd() const;
	
	/** Size of blocks in bytes. */
	inline uint32_t GetBlockSize() const{ return pBlockSize; }
	
	/** Set size of block sin bytes. */
	void SetBlockSize(uint32_t size);
	/*@}*/
	
	
	
private:
};

#endif
