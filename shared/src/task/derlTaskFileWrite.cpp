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

#include "derlTaskFileWrite.h"


// Class derlTaskFileWrite
////////////////////////////

derlTaskFileWrite::derlTaskFileWrite(const std::string &path) :
pPath(path),
pStatus(Status::pending),
pFileSize(0L),
pBlockSize(0L),
pBlockCount(0){
}

derlTaskFileWrite::~derlTaskFileWrite(){
}


// Management
///////////////

void derlTaskFileWrite::SetStatus(Status status){
	pStatus = status;
}

void derlTaskFileWrite::SetFileSize(uint64_t fileSize){
	pFileSize = fileSize;
}

void derlTaskFileWrite::SetBlockSize(uint64_t blockSize){
	pBlockSize = blockSize;
}

void derlTaskFileWrite::SetBlockCount(int blockCount){
	pBlockCount = blockCount;
}