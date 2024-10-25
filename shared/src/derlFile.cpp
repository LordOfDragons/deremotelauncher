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

#include <algorithm>
#include <stdexcept>

#include "derlFile.h"


// Class derlFile
///////////////////

derlFile::derlFile(const std::string &path) :
pPath(path),
pSize(0),
pHasBlocks(false),
pBlockSize(0){
}

derlFile::~derlFile(){
}


// Management
///////////////

void derlFile::SetSize(uint64_t size){
	pSize = size;
}

void derlFile::SetHash(const std::string &hash){
	pHash = hash;
}

void derlFile::SetHasBlocks(bool hasBlocks){
	pHasBlocks = hasBlocks;
}

int derlFile::GetBlockCount() const{
	return pBlocks.size();
}

derlFileBlock::Ref derlFile::GetBlockAt(int index) const{
	return pBlocks.at(index);
}

derlFileBlock::Ref derlFile::BlockMatching(uint64_t offset, uint64_t size) const{
	derlFileBlock::List::const_iterator iter;
	
	for(iter=pBlocks.cbegin(); iter!=pBlocks.cend(); iter++){
		const derlFileBlock::Ref &block = *iter;
		if(block->GetOffset() == offset && block->GetSize() == size){
			return block;
		}
	}
	
	return nullptr;
}

void derlFile::AddBlock(const derlFileBlock::Ref &block){
	pBlocks.push_back(block);
}

void derlFile::RemoveBlock(const derlFileBlock::Ref &block){
	derlFileBlock::List::iterator iter(std::find(pBlocks.begin(), pBlocks.end(), block));
	if(iter == pBlocks.end()){
		throw std::runtime_error("block absent");
	}
	pBlocks.erase(iter);
}

void derlFile::RemoveAllBlocks(){
	pBlocks.clear();
}

void derlFile::SetBlocks(const derlFileBlock::List &blocks){
	pBlocks = blocks;
}

derlFileBlock::List::const_iterator derlFile::GetBlocksBegin() const{
	return pBlocks.cbegin();
}

derlFileBlock::List::const_iterator derlFile::GetBlocksEnd() const{
	return pBlocks.cend();
}

void derlFile::SetBlockSize(uint32_t size){
	pBlockSize = size;
}


// Private Functions
//////////////////////
