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
#include <memory>
#include <stdexcept>
#include <deremotelauncher/derlRunParameters.h>
#include <denetwork/denLogger.h>
#include "foxtoolkit.h"

#include "Client.h"
#include "Launcher.h"

class Application;

/**
 * Main window.
 */
class WindowMain : public FXMainWindow{
	FXDECLARE(WindowMain)
protected:
	WindowMain();
	
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
	
	std::shared_ptr<FXMessageChannel> pMessageChannel;
	
	std::deque<std::string> pLogLines, pAddLogs;
	std::stringstream pStreamLogs;
	int pMaxLogLineCount;
	std::mutex pMutex;
	
	denLogger::Ref pLogger;
	Client::Ref pClient;
	Launcher::Ref pLauncher;
	derlRunParameters pRunParams;
	
	
public:
	enum eIDs{
		ID_HOST_ADDRESS = FXMainWindow::ID_LAST + 1,
		ID_CLIENT_NAME,
		ID_DATA_PATH,
		ID_LOGS,
		ID_CONNECT,
		ID_DISCONNECT,
		ID_MSG_LOGS_ADDED,
		ID_MSG_UPDATE_UI_STATES,
		ID_MSG_START_APP,
		ID_MSG_STOP_APP,
		ID_MSG_KILL_APP,
		ID_MSG_SYSPROP_PROFILENAMES,
		ID_MSG_SYSPROP_DEFAULTPROFILE,
		ID_TIMER_PULSE,
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
	
	/** \brief Client. */
	inline const Client::Ref &GetClient() const{ return pClient; }
	
	/** \brief Launcher. */
	inline const Launcher::Ref &GetLauncher() const{ return pLauncher; }
	
	/** \brief Update UI states. */
	void UpdateUIStates();
	
	/** \brief Asynchronously request update UP states. */
	void RequestUpdateUIStates();
	
	/** \brief Enqueue logs. */
	void AddLogs(const std::string &logs);
	
	/** \brief Start application. */
	void StartApp(const derlRunParameters &params);
	
	/** \brief Stop application. */
	void StopApp();
	
	/** \brief Kill application. */
	void KillApp();
	
	/** \brief Request profile names and send them to server. */
	void RequestProfileNames();
	
	/** \brief Request default profile name and send it to server. */
	void RequestDefaultProfileName();
	/*@}*/
	
	
	
	/** \name Events */
	/*@{*/
	long onClose(FXObject*, FXSelector, void*);
	long onMinimized(FXObject*, FXSelector, void*);
	long onRestored(FXObject*, FXSelector, void*);
	long onMaximized(FXObject*, FXSelector, void*);
	
	long onBtnConnect(FXObject*, FXSelector, void*);
	long onBtnDisconnect(FXObject*, FXSelector, void*);
	
	long onMsgLogsAdded(FXObject*, FXSelector, void*);
	long onMsgUpdateUIStates(FXObject*, FXSelector, void*);
	long onMsgStartApp(FXObject*, FXSelector, void*);
	long onMsgStopApp(FXObject*, FXSelector, void*);
	long onMsgKillApp(FXObject*, FXSelector, void*);
	long onMsgSysPropProfileNames(FXObject*, FXSelector, void*);
	long onMsgSysPropDefaultProfile(FXObject*, FXSelector, void*);
	
	long onTimerPulse(FXObject*, FXSelector, void*);
	/*@}*/
	
	
private:
	void pCreateContent();
	void pCreatePanelConnect(FXComposite *container);
	void pCreatePanelLogs(FXComposite *container);
	
	void pLogException(const std::exception &exception, const std::string &message);
	void pLogException(const deException &exception, const std::string &message);
};

#endif
