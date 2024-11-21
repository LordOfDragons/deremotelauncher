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

#ifndef _DERLTASKPROCESSORLAUNCHERCLIENT_H_
#define _DERLTASKPROCESSORLAUNCHERCLIENT_H_

#include "derlBaseTaskProcessor.h"

#include "../task/derlTaskFileBlockHashes.h"
#include "../task/derlTaskFileDelete.h"
#include "../task/derlTaskFileWrite.h"
#include "../task/derlTaskFileWriteBlock.h"
#include "../task/derlTaskFileLayout.h"

class derlLauncherClient;


/** \brief Process tasks. */
class derlTaskProcessorLauncherClient : public derlBaseTaskProcessor{
public:
	/** \brief Reference type. */
	typedef std::shared_ptr<derlTaskProcessorLauncherClient> Ref;
	
	/** \brief List type. */
	typedef std::vector<Ref> List;
	
	
protected:
	derlLauncherClient &pClient;
	
	
public:
	/** \name Constructors and Destructors */
	/*@{*/
	/** \brief Create processor. */
	derlTaskProcessorLauncherClient(derlLauncherClient &client);
	
	/** \brief Clean up processor. */
	virtual ~derlTaskProcessorLauncherClient() noexcept;
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Client. */
	inline derlLauncherClient &GetClient() const{ return pClient; }
	
	/**
	 * \brief Process one task if possible.
	 * \returns true if task is run or false otherwise.
	 */
	bool RunTask() override;
	
	
	
	/**
	 * \brief Find next file layout task.
	 * \returns true if task is found otherwise false.
	 */
	bool FindNextTaskFileLayout(derlTaskFileLayout::Ref &task) const;
	
	/**
	 * \brief Find next file block hashes task.
	 * \returns true if task is found otherwise false.
	 */
	bool FindNextTaskFileBlockHashes(derlTaskFileBlockHashes::Ref &task) const;
	
	/**
	 * \brief Find next delete file task.
	 * \returns true if task is found otherwise false.
	 */
	bool FindNextTaskDelete(derlTaskFileDelete::Ref &task) const;
	
	/**
	 * \brief Find next write file task.
	 * \returns true if task is found otherwise false.
	 */
	bool FindNextTaskWriteFile(derlTaskFileWrite::Ref &task) const;
	
	/**
	 * \brief Find next write file block task.
	 * \returns true if task is found otherwise false.
	 */
	bool FindNextTaskWriteFileBlock(derlTaskFileWrite::Ref &task, derlTaskFileWriteBlock::Ref &block) const;
	
	
	
	/** \brief Process task file layout. */
	virtual void ProcessFileLayout(derlTaskFileLayout &task);
	
	/** \brief Process task file block hashes. */
	virtual void ProcessFileBlockHashes(derlTaskFileBlockHashes &task);
	
	/** \brief Process task delete file. */
	virtual void ProcessDeleteFile(derlTaskFileDelete &task);
	
	/** \brief Process task write file. */
	virtual void ProcessWriteFile(derlTaskFileWrite &task);
	
	/** \brief Process task write file block. */
	virtual void ProcessWriteFileBlock(derlTaskFileWrite &task, derlTaskFileWriteBlock &block);
	
	
	
	/**
	 * \brief Delete file.
	 * 
	 * Default implementation deletes file using standard library functionality.
	 */
	virtual void DeleteFile(const derlTaskFileDelete &task);
	
	/**
	 * \brief Write data to open file.
	 * 
	 * Default implementation writes to open std::filestream.
	 */
	virtual void WriteFile(const void *data, uint64_t offset, uint64_t size);
};

#endif
