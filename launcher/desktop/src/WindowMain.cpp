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

#include <algorithm>
#include <sstream>
#include <iostream>

#include "WindowMain.h"
#include "Application.h"
#include "Logger.h"
#include "resources/icons.h"

#include <deremotelauncher/derlProtocol.h>

#ifdef OS_W32
#include <dragengine/app/deOSWindows.h>
#endif


FXDEFMAP(WindowMain) WindowMainMap[] = {
	FXMAPFUNC(SEL_CLOSE, 0, WindowMain::onClose),
	FXMAPFUNC(SEL_MINIMIZE, 0, WindowMain::onMinimized),
	FXMAPFUNC(SEL_RESTORE, 0, WindowMain::onRestored),
	FXMAPFUNC(SEL_MAXIMIZE, 0, WindowMain::onMaximized),
	
	FXMAPFUNC(SEL_COMMAND, WindowMain::ID_CONNECT, WindowMain::onBtnConnect),
	FXMAPFUNC(SEL_COMMAND, WindowMain::ID_DISCONNECT, WindowMain::onBtnDisconnect),
	
	FXMAPFUNC(SEL_COMMAND, WindowMain::ID_MSG_LOGS_ADDED, WindowMain::onMsgLogsAdded),
	FXMAPFUNC(SEL_COMMAND, WindowMain::ID_MSG_UPDATE_UI_STATES, WindowMain::onMsgUpdateUIStates),
	FXMAPFUNC(SEL_COMMAND, WindowMain::ID_MSG_START_APP, WindowMain::onMsgStartApp),
	FXMAPFUNC(SEL_COMMAND, WindowMain::ID_MSG_STOP_APP, WindowMain::onMsgStopApp),
	FXMAPFUNC(SEL_COMMAND, WindowMain::ID_MSG_KILL_APP, WindowMain::onMsgKillApp),
	FXMAPFUNC(SEL_COMMAND, WindowMain::ID_MSG_SYSPROP_PROFILENAMES, WindowMain::onMsgSysPropProfileNames),
	FXMAPFUNC(SEL_COMMAND, WindowMain::ID_MSG_SYSPROP_DEFAULTPROFILE, WindowMain::onMsgSysPropDefaultProfile),
	
	FXMAPFUNC(SEL_TIMEOUT, WindowMain::ID_TIMER_PULSE, WindowMain::onTimerPulse)
};

FXIMPLEMENT(WindowMain, FXMainWindow, WindowMainMap, ARRAYNUMBER(WindowMainMap))


// pulse time in nano-seconds. 1s pulse time
#define PULSE_TIME 1000000000


// Constructors
/////////////////

WindowMain::WindowMain(){
}

WindowMain::WindowMain(Application &app) :
FXMainWindow(&app, "Drag[en]gine Remote Launcher",
	new FXBMPIcon(&app, icon_appicon), new FXBMPIcon(&app, icon_appicon),
	DECOR_ALL, 0, 0, 800, 600),
pHostAddress(app.reg().readStringEntry("settings", "hostAddress", "localhost")),
pClientName(app.reg().readStringEntry("settings", "clientName", "Client")),
pDataPath(app.reg().readStringEntry("settings", "dataPath", GetDefaultDataPath().text())),
pMessageChannel(std::make_shared<FXMessageChannel>(&app)),
pMaxLogLineCount(100),
pTargetHostAddress(pHostAddress),
pTargetClientName(pClientName),
pTargetDataPath(pDataPath)
{
	pCreateContent();
	create();
	show(FX::PLACEMENT_SCREEN);
	
	pLogger = std::make_shared<Logger>(*this);
	
	pClient = std::make_shared<Client>(*this, pLogger);
	pLauncher = std::make_shared<Launcher>(*this, pLogger);
	
	getApp()->addTimeout(this, ID_TIMER_PULSE, PULSE_TIME);
}

WindowMain::~WindowMain(){
	getApp()->removeTimeout(this, ID_TIMER_PULSE);
}

// Management
///////////////

void WindowMain::UpdateLogs(){
	{
	const std::lock_guard guard(pMutex);
	if(pAddLogs.empty()){
		return;
	}
	
	while(!pAddLogs.empty()){
		pLogLines.push_back(pAddLogs.front());
		pAddLogs.pop_front();
	}
	}
	
	while(pLogLines.size() > pMaxLogLineCount){
		pLogLines.pop_front();
	}
	
	pStreamLogs.str("");
	bool addNewline = false;
	for(const std::string &log : pLogLines){
		if(addNewline){
			pStreamLogs << std::endl;
			
		}else{
			addNewline = true;
		}
		pStreamLogs << log;
	}
	
	pEditLogs->setText(pStreamLogs.str().c_str());
	pEditLogs->makePositionVisible(std::max(pEditLogs->getText().length() - 1, 0));
}

bool WindowMain::CloseWindow(){
	return true;
}

void WindowMain::Close(){
	SaveSettings();
	
	// we use close() on purpose instead of delete because fox requires this
	close(false);
}

FXString WindowMain::GetDefaultDataPath() const{
#ifdef OS_W32
	return deOSWindows::ParseNativePath("@LocalAppData\\DERemoteLauncher\\data").GetString();
#else
	return FXPath::expand("~/.cache/deremotelauncher/data");
#endif
}

void WindowMain::SaveSettings() const{
	FXRegistry &r = getApp()->reg();
	r.writeStringEntry("settings", "hostAddress", pHostAddress.text());
	r.writeStringEntry("settings", "clientName", pClientName.text());
	r.writeStringEntry("settings", "dataPath", pDataPath.text());
}

void WindowMain::UpdateUIStates(){
	if(pLauncher->GetState() != Launcher::State::ready){
		pEditClientName->disable();
		pEditDataPath->disable();
		pEditHostAddress->disable();
		pBtnConnect->disable();
		pBtnDisconnect->disable();
		return;
	}
	
	if(pClient->IsDisconnected()){
		pEditClientName->enable();
		pEditDataPath->enable();
		pEditHostAddress->enable();
		
		pBtnConnect->enable();
		pBtnDisconnect->disable();
		
	}else{
		pEditClientName->disable();
		pEditDataPath->disable();
		pEditHostAddress->disable();
		
		pBtnConnect->disable();
		pBtnDisconnect->enable();
	}
}

void WindowMain::RequestUpdateUIStates(){
	pMessageChannel->message(this, FXSEL(SEL_COMMAND, ID_MSG_UPDATE_UI_STATES));
}

void WindowMain::AddLogs(const std::string &logs){
	{
	const std::lock_guard guard(pMutex);
	pAddLogs.push_back(logs);
	}
	
	pMessageChannel->message(this, FXSEL(SEL_COMMAND, ID_MSG_LOGS_ADDED));
}

void WindowMain::StartApp(const derlRunParameters &params){
	{
	const std::lock_guard guard(pMutex);
	pRunParams = params;
	}
	
	pMessageChannel->message(this, FXSEL(SEL_COMMAND, ID_MSG_START_APP));
}

void WindowMain::StopApp(){
	pMessageChannel->message(this, FXSEL(SEL_COMMAND, ID_MSG_STOP_APP));
}

void WindowMain::KillApp(){
	pMessageChannel->message(this, FXSEL(SEL_COMMAND, ID_MSG_KILL_APP));
}

void WindowMain::RequestProfileNames(){
	pMessageChannel->message(this, FXSEL(SEL_COMMAND, ID_MSG_SYSPROP_PROFILENAMES));
}

void WindowMain::RequestDefaultProfileName(){
	pMessageChannel->message(this, FXSEL(SEL_COMMAND, ID_MSG_SYSPROP_DEFAULTPROFILE));
}

// Events
///////////

long WindowMain::onClose(FXObject*, FXSelector, void*){
	if(CloseWindow()){
		Close();
	}
	return 1;
}

long WindowMain::onMinimized(FXObject*, FXSelector, void*){
	return 0;
}

long WindowMain::onRestored(FXObject*, FXSelector, void*){
	return 0;
}

long WindowMain::onMaximized(FXObject*, FXSelector, void*){
	return 0;
}

long WindowMain::onBtnConnect(FXObject*, FXSelector, void*){
	try{
		pClient->ConnectToHost(pClientName.text(), pDataPath.text(), pHostAddress.text());
		
	}catch(const std::exception &e){
		pLogger->Log(denLogger::LogSeverity::error, e.what());
		FXMessageBox::error(this, MBOX_OK, "Connect Error", "%s", e.what());
	}
	
	UpdateUIStates();
	return 1;
}

long WindowMain::onBtnDisconnect(FXObject*, FXSelector, void*){
	try{
		pClient->DisconnectFromHost();
		
	}catch(const std::exception &e){
		pLogger->Log(denLogger::LogSeverity::error, e.what());
		FXMessageBox::error(this, MBOX_OK, "Disconnect Error", "%s", e.what());
	}
	
	UpdateUIStates();
	return 1;
}

long WindowMain::onMsgLogsAdded(FXObject*, FXSelector, void*){
	UpdateLogs();
	return 1;
}

long WindowMain::onMsgUpdateUIStates(FXObject*, FXSelector, void*){
	UpdateUIStates();
	return 1;
}

long WindowMain::onMsgStartApp(FXObject*, FXSelector, void*){
	pLogger->Log(denLogger::LogSeverity::info, "Start running application");
	try{
		pLauncher->RunGame(pDataPath.text(), pRunParams);
		pClient->SetRunStatus(derlLauncherClient::RunStatus::running);
		
	}catch(const deException &e){
		pLogException(e, "Start application failed");
		
	}catch(const std::exception &e){
		pLogException(e, "Start application failed");
	}
	UpdateUIStates();
	return 1;
}

long WindowMain::onMsgStopApp(FXObject*, FXSelector, void*){
	pLogger->Log(denLogger::LogSeverity::info, "Stop running application");
	try{
		pLauncher->StopGame();
		pClient->SetRunStatus(derlLauncherClient::RunStatus::stopped);
		
	}catch(const deException &e){
		pLogException(e, "Stop application failed");
		
	}catch(const std::exception &e){
		pLogException(e, "Stop application failed");
	}
	UpdateUIStates();
	return 1;
}

long WindowMain::onMsgKillApp(FXObject*, FXSelector, void*){
	pLogger->Log(denLogger::LogSeverity::info, "Kill running application");
	try{
		pLauncher->KillGame();
		pClient->SetRunStatus(derlLauncherClient::RunStatus::stopped);
		
	}catch(const deException &e){
		pLogException(e, "Kill application failed");
		
	}catch(const std::exception &e){
		pLogException(e, "Kill application failed");
	}
	UpdateUIStates();
	return 1;
}

long WindowMain::onMsgSysPropProfileNames(FXObject*, FXSelector, void*){
	std::stringstream names;
	const delGameProfileList &profiles = pLauncher->GetGameManager().GetProfiles();
	const int count = profiles.GetCount();
	int i;
	for(i=0; i<count; i++){
		if(i > 0){
			names << '\n';
		}
		names << profiles.GetAt(i)->GetName().GetString();
	}
	pClient->SendSystemProperty(derlProtocol::SystemPropertyNames::profileNames, names.str());
	
	return 1;
}

long WindowMain::onMsgSysPropDefaultProfile(FXObject*, FXSelector, void*){
	const delGameProfile * const profile = pLauncher->GetGameManager().GetDefaultProfile();
	pClient->SendSystemProperty(derlProtocol::SystemPropertyNames::defaultProfile,
		profile ? profile->GetName().GetString() : "");
	return 1;
}

long WindowMain::onTimerPulse(FXObject*, FXSelector, void*){
	getApp()->addTimeout(this, ID_TIMER_PULSE, PULSE_TIME);
	
	pLauncher->Pulse();
	
	if(pLauncher->GetState() == Launcher::State::running){
		pClient->SetRunStatus(derlLauncherClient::RunStatus::running);
		
	}else{
		pClient->SetRunStatus(derlLauncherClient::RunStatus::stopped);
	}
	return 1;
}


// Private Functions
//////////////////////

void WindowMain::pCreateContent(){
	FXVerticalFrame * const f = new FXVerticalFrame(this, LAYOUT_FILL);
	pCreatePanelConnect(f);
	pCreatePanelLogs(f);
}

void WindowMain::pCreatePanelConnect(FXComposite *container){
	FXHorizontalFrame * const f = new FXHorizontalFrame(container, LAYOUT_FILL_X);
	
	FXMatrix * const f2 = new FXMatrix(f, 2, MATRIX_BY_COLUMNS | LAYOUT_FILL);
	
	pLabHostAddress = new FXLabel(f2, "Host Address:", nullptr, LAYOUT_FILL_ROW | LAYOUT_FILL_Y);
	pEditHostAddress = new FXTextField(f2, 20, &pTargetHostAddress, FXDataTarget::ID_VALUE,
		TEXTFIELD_NORMAL | LAYOUT_FILL_ROW | LAYOUT_FILL_COLUMN | LAYOUT_FILL);
	pEditHostAddress->disable();
	
	pLabClientName = new FXLabel(f2, "Client Name:", nullptr, LAYOUT_FILL_ROW | LAYOUT_FILL_Y);
	pEditClientName = new FXTextField(f2, 20, &pTargetClientName, FXDataTarget::ID_VALUE,
		TEXTFIELD_NORMAL | LAYOUT_FILL_ROW | LAYOUT_FILL_COLUMN | LAYOUT_FILL);
	pEditClientName->disable();
	
	pLabDataPath = new FXLabel(f2, "Data Path:", nullptr, LAYOUT_FILL_ROW | LAYOUT_FILL_Y);
	pEditDataPath = new FXTextField(f2, 20, &pTargetDataPath, FXDataTarget::ID_VALUE,
		TEXTFIELD_NORMAL | LAYOUT_FILL_ROW | LAYOUT_FILL_COLUMN | LAYOUT_FILL);
	pEditDataPath->disable();
	
	FXMatrix * const f3 = new FXMatrix(f, 1, MATRIX_BY_COLUMNS | LAYOUT_FILL_Y);
	
	pBtnConnect = new FXButton(f3, "Connect", nullptr, this, ID_CONNECT,
		BUTTON_NORMAL | LAYOUT_FILL_COLUMN | LAYOUT_FILL_ROW | LAYOUT_FILL);
	pBtnConnect->disable();
	
	pBtnDisconnect = new FXButton(f3, "Disconnect", nullptr, this, ID_DISCONNECT,
		BUTTON_NORMAL | LAYOUT_FILL_COLUMN | LAYOUT_FILL_ROW | LAYOUT_FILL);
	pBtnDisconnect->disable();
}

void WindowMain::pCreatePanelLogs(FXComposite *container){
	pEditLogs = new FXText(container, this, ID_LOGS,
		TEXT_READONLY | TEXT_WORDWRAP | TEXT_SHOWACTIVE /*| TEXT_AUTOSCROLL*/ | LAYOUT_FILL);
	
	FXFontDesc fd(getApp()->getNormalFont()->getFontDesc());
	
#ifdef OS_W32
	strcpy_s(fd.face, sizeof(fd.face), "courier");
#else
	strcpy(fd.face, "courier");
#endif
	fd.setwidth = 30; // normal=50, condensed=30
	pEditLogs->setFont(new FXFont(getApp(), fd));
}

void WindowMain::pLogException(const std::exception &exception, const std::string &message){
	pLogger->Log(denLogger::LogSeverity::error, exception.what());
}

void WindowMain::pLogException(const deException &exception, const std::string &message){
	const decStringList lines(exception.FormatOutput());
	const int count = lines.GetCount();
	std::stringstream ss;
	int i;
	
	ss << message << ": ";
	
	for(i=0; i<count; i++){
		if(i > 0){
			ss << std::endl;
		}
		ss << lines.GetAt(i).GetString();
	}
	
	pLogger->Log(denLogger::LogSeverity::error, ss.str());
}
