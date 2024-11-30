/**
 * MIT License
 * 
 * Copyright (c) 2024 DragonDreams (info@dragondreams.ch)
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

#ifndef _WINDOWMAIN_H_
#define _WINDOWMAIN_H_

#include <atomic>
#include <deque>
#include <string>
#include <sstream>
#include <mutex>
#include <denetwork/denLogger.h>
#include "foxtoolkit.h"

#include "Client.h"

class Application;

/**
 * Main window.
 */
class WindowMain : public FXMainWindow{
	FXDECLARE(WindowMain)
protected:
	WindowMain();
	
public:
	enum eIDs{
		ID_HOST_ADDRESS = FXMainWindow::ID_LAST + 1,
		ID_CLIENT_NAME,
		ID_DATA_PATH,
		ID_LOGS,
		ID_CONNECT,
		ID_DISCONNECT,
		ID_LAST
	};
	
	
	/** \name Constructors and Destructors */
	/*@{*/
	/** \brief Create main window. */
	WindowMain(Application &app);
	
	/** \brief Free main window. */
	~WindowMain() override;
	/*@}*/
	
	
	
	/** \name Management */
	/*@{*/
	/** \brief Frame update. */
	void OnFrameUpdate();
	
	/** \brief Update logs. */
	void UpdateLogs();
	
	/** \brief Determine is window can be closed. */
	bool CloseWindow();
	
	/** \brief Close window. */
	void Close();
	
	/** \brief Default data path. */
	FXString GetDefaultDataPath() const;
	
	/** \brief Save settings. */
	void SaveSettings() const;
	
	/** \brief Update UI states. */
	void UpdateUIStates();
	
	/** \brief Asynchronously request update UP states. */
	void RequestUpdateUIStates();
	/*@}*/
	
	
	
	/** \name Events */
	/*@{*/
	long onClose(FXObject*, FXSelector, void*);
	long onMinimized(FXObject*, FXSelector, void*);
	long onRestored(FXObject*, FXSelector, void*);
	long onMaximized(FXObject*, FXSelector, void*);
	
	long onBtnConnect(FXObject*, FXSelector, void*);
	long onBtnDisconnect(FXObject*, FXSelector, void*);
	/*@}*/
	
private:
	FXString pHostAddress;
	FXString pClientName;
	FXString pDataPath;
	
	FXDataTarget pTargetHostAddress;
	FXDataTarget pTargetClientName;
	FXDataTarget pTargetDataPath;
	
	FXLabel *pLabHostAddress = nullptr;
	FXLabel *pLabClientName = nullptr;
	FXLabel *pLabDataPath = nullptr;
	FXTextField *pEditHostAddress = nullptr;
	FXTextField *pEditClientName = nullptr;
	FXTextField *pEditDataPath = nullptr;
	FXButton *pBtnConnect = nullptr;
	FXButton *pBtnDisconnect = nullptr;
	FXText *pEditLogs = nullptr;
	
	std::deque<std::string> pLogLines, pAddLogs;
	std::stringstream pStreamLogs;
	int pMaxLogLineCount;
	std::mutex pMutexLogs;
	
	std::atomic<bool> pNeedUpdateUIStates;
	
	denLogger::Ref pLogger;
	std::shared_ptr<Client> pClient;
	
	void pCreateContent();
	void pCreatePanelConnect(FXComposite *container);
	void pCreatePanelLogs(FXComposite *container);
};

#endif
