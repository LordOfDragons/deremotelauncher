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

#include <stdexcept>

#include "derlFileLayout.h"


// Class derlFileLayout
/////////////////////////

derlFileLayout::derlFileLayout(){
}

derlFileLayout::~derlFileLayout(){
}

// Management
///////////////

int derlFileLayout::GetFileCount() const{
	return pFiles.size();
}

derlFile::Map::const_iterator derlFileLayout::GetFilesBegin() const{
	return pFiles.cbegin();
}

derlFile::Map::const_iterator derlFileLayout::GetFilesEnd() const{
	return pFiles.cend();
}

derlFile::Ref derlFileLayout::GetFileAt(const std::string &path) const{
	derlFile::Map::const_iterator iter(pFiles.find(path));
	return iter != pFiles.cend() ? iter->second : nullptr;
}

derlFile::Ref derlFileLayout::GetFileAtSync(const std::string &path){
	const std::lock_guard guard(pMutex);
	return GetFileAt(path);
}

void derlFileLayout::SetFileAt(const std::string &path, const derlFile::Ref &file){
	pFiles[path] = file;
}

void derlFileLayout::SetFileAtSync(const std::string &path, const derlFile::Ref &file){
	const std::lock_guard guard(pMutex);
	SetFileAt(path, file);
}

derlFileLayout::ListPath derlFileLayout::GetAllPath() const{
	derlFile::Map::const_iterator iter;
	ListPath list;
	for(iter = pFiles.cbegin(); iter != pFiles.cend(); iter++){
		list.push_back(iter->first);
	}
	return list;
}

void derlFileLayout::AddFile(const derlFile::Ref &file){
	pFiles[file->GetPath()] = file;
}

void derlFileLayout::RemoveFile(const std::string &path){
	derlFile::Map::iterator iter(pFiles.find(path));
	if(iter == pFiles.end()){
		throw std::runtime_error("file absent");
	}
	pFiles.erase(iter);
}

void derlFileLayout::AddFileSync(const derlFile::Ref &file){
	const std::lock_guard guard(pMutex);
	AddFile(file);
}

void derlFileLayout::RemoveFileIfPresent(const std::string &path){
	derlFile::Map::iterator iter(pFiles.find(path));
	if(iter != pFiles.end()){
		pFiles.erase(iter);
	}
}

void derlFileLayout::RemoveFileIfPresentSync(const std::string &path){
	const std::lock_guard guard(pMutex);
	RemoveFileIfPresent(path);
}

void derlFileLayout::RemoveAllFiles(){
	pFiles.clear();
}
