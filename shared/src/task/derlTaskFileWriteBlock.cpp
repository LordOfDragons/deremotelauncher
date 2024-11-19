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

#include "derlTaskFileWriteBlock.h"
#include "../derlFileBlock.h"


// Class derlTaskFileWriteBlock
/////////////////////////////////

derlTaskFileWriteBlock::derlTaskFileWriteBlock(int index, uint64_t size) :
pStatus(Status::pending),
pIndex(index),
pSize(size),
pNextPartIndex(0),
pPartCount(0),
pBatchesFinished(0){
}

derlTaskFileWriteBlock::derlTaskFileWriteBlock(
	int index, uint64_t size, const std::string &data) :
pStatus(Status::pending),
pIndex(index),
pSize(size),
pData(data),
pNextPartIndex(0),
pPartCount(0),
pBatchesFinished(0){
}

derlTaskFileWriteBlock::~derlTaskFileWriteBlock(){
}


// Management
///////////////

void derlTaskFileWriteBlock::SetStatus(Status status){
	pStatus = status;
}

void derlTaskFileWriteBlock::SetNextPartIndex(int index){
	pNextPartIndex = index;
}

void derlTaskFileWriteBlock::SetPartCount(int count){
	pPartCount = count;
}

void derlTaskFileWriteBlock::CalcPartCount(int partSize){
	pPartCount = (int)((pSize - 1L) / (uint64_t)partSize) + 1;
}

uint8_t *derlTaskFileWriteBlock::PartDataPointer(int partSize, int indexPart) const{
	return (uint8_t*)pData.c_str() + (uint64_t)partSize * indexPart;
}

void derlTaskFileWriteBlock::SetBatchesFinished(int count){
	pBatchesFinished = count;
}
